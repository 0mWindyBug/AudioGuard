#include "filter.h"
#include "config.h"
#include "utils.h"
#include "globals.h"
#include "csq.h"


NTSTATUS filter::AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PDO)
{
    DbgPrint("[*] AudioGuard :: AddDevice!\n");
    PDEVICE_OBJECT DeviceObject;
    NTSTATUS status = IoCreateDevice(DriverObject, sizeof(filter::DeviceExtension), nullptr, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status))
        return status;
    
    filter::pDeviceExtension ext = reinterpret_cast<filter::pDeviceExtension>(DeviceObject->DeviceExtension);
    IoInitializeRemoveLock(&ext->RemoveLock, TAG, NULL, NULL);

    status = IoAttachDeviceToDeviceStackSafe(DeviceObject, PDO, &ext->LowerDeviceObject);

    DeviceObject->DeviceType = ext->LowerDeviceObject->DeviceType;
    DeviceObject->Flags |= ext->LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    DeviceObject->Flags |= DO_POWER_PAGABLE;

    return status;
}


NTSTATUS filter::DispatchForward(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    filter::pDeviceExtension ext = reinterpret_cast<filter::pDeviceExtension>(DeviceObject->DeviceExtension);

    auto status = IoAcquireRemoveLock(&ext->RemoveLock, Irp);
    if (!NT_SUCCESS(status))
    {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    
    if (IoStackLocation->MajorFunction == IRP_MJ_DEVICE_CONTROL)
    {
        if (IoStackLocation->Parameters.DeviceIoControl.IoControlCode == IOCTL_KS_PROPERTY)
        {
            // get status to know wether we can pass it forward or we decided to queue the request in which case we should properly pend the request : ) 
            if (filter::KsPropertyHandler(DeviceObject, Irp, IoStackLocation) == AUDGUARD_PEND)
                return STATUS_PENDING;
        }
    }

    IoSkipCurrentIrpStackLocation(Irp);
    status = IofCallDriver(ext->LowerDeviceObject, Irp);
    IoReleaseRemoveLock(&ext->RemoveLock, Irp);
    return status;

}

NTSTATUS filter::DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    filter::pDeviceExtension ext = reinterpret_cast<filter::pDeviceExtension>(DeviceObject->DeviceExtension);

    PoStartNextPowerIrp(Irp);

    auto status = IoAcquireRemoveLock(&ext->RemoveLock, Irp);
    if (!NT_SUCCESS(status))
    {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    status = IofCallDriver(ext->LowerDeviceObject, Irp);
    IoReleaseRemoveLock(&ext->RemoveLock, Irp);

    return status;
}

NTSTATUS filter::DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    filter::pDeviceExtension ext = reinterpret_cast<filter::pDeviceExtension>(DeviceObject->DeviceExtension);

    auto status = IoAcquireRemoveLock(&ext->RemoveLock, Irp);
    if (!NT_SUCCESS(status))
    {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    auto IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    UCHAR Minor = IoStackLocation->MinorFunction;

    IoSkipCurrentIrpStackLocation(Irp);
    status = IofCallDriver(ext->LowerDeviceObject, Irp);

    if (Minor == IRP_MN_REMOVE_DEVICE)
    {
        IoReleaseRemoveLockAndWait(&ext->RemoveLock, Irp);
        IoDetachDevice(ext->LowerDeviceObject);
        IoDeleteDevice(DeviceObject);

    }
    else
        IoReleaseRemoveLock(&ext->RemoveLock, Irp);

    return status;
}


bool filter::KsPropertyHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IoStackLocation)
{
    GUID PropertysetConnection = GUID_PROPSETID_Connection;
    ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;

    if (!InputBufferLength || !OutputBufferLength)
        return AUDGUARD_COMPLETE;

    PVOID InputBuffer = IoStackLocation->Parameters.DeviceIoControl.Type3InputBuffer;
    PVOID OutputBuffer = Irp->UserBuffer;

    // IOCTL_KS_PROPERTY is method neither, we are provider with the user addresses as is  
    // since AudioGuard is attached at the top of the stack we can access these buffers directly 
    // must be done in a try except as the buffer might get freed any time by the user thread 

    __try
    {

        ProbeForRead(InputBuffer, InputBufferLength, sizeof(UCHAR));

        PKSIDENTIFIER KsIdentifier = reinterpret_cast<PKSIDENTIFIER>(InputBuffer);

        if (IsEqualGUID(KsIdentifier->Set, PropertysetConnection))
        {

            if (KsIdentifier->Id == KSPROPERTY_CONNECTION_STATE && KsIdentifier->Flags == KSPROPERTY_TYPE_SET)
            {
                KSSTATE KsStatePtr = *reinterpret_cast<PKSSTATE>(OutputBuffer);

                switch (KsStatePtr)
                {
                case KSSTATE_STOP:
                    DbgPrint("[*] AudioGuard :: request to set KSSTATE_STOP\n");
                    break;

                case KSSTATE_ACQUIRE:
                    DbgPrint("[*] AudioGuard :: request to set KSSTATE_ACQUIRE\n");
                    break;

                case KSSTATE_PAUSE:
                    DbgPrint("[*] AudioGuard :: request to set KSSTATE_PAUSE\n");
                    break;

                    // sent on capture start!
                    // handle it by placing the IRP in an IRP queue and prompt the user asynchronously 
                    // since we are not going to touch the buffers anymore we don't have to map them
                    // in case the user allows processing to proceed we will call IofCallDriver in an apc, allowing ksthunk to map these user addresses

                case KSSTATE_RUN:
                    DbgPrint("[*] AudioGuard :: request to set KSSTATE_RUN\n");

                    // notify service of KS request
                    pCsqIrpQueue ClientIrpQueue = reinterpret_cast<pCsqIrpQueue>(globals::ClientDeviceObject->DeviceExtension);
                    PIRP ClientIrp = IoCsqRemoveNextIrp(&ClientIrpQueue->CsqObject, nullptr);
                    if (!ClientIrp)
                    {
                        return AUDGUARD_COMPLETE;
                    }


                    Irp->Tail.Overlay.DriverContext[0] = DeviceObject;

                    // IOCsqInsertIrp marks the IRP as pending 
                    IoCsqInsertIrp(&globals::g_pKsPropertyQueue->CsqObject, Irp, nullptr);

                    ClientIrp->IoStatus.Status = STATUS_SUCCESS;
                    ClientIrp->IoStatus.Information = 0;
                    IoCompleteRequest(ClientIrp, IO_NO_INCREMENT);

                    return AUDGUARD_PEND;
                }

            }
        }

    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("[*] AudioGuard :: exception accessing buffer in ksproperty handler\n");
    }

    return AUDGUARD_COMPLETE;
}
