#include <ntifs.h>
#include "client.h"
#include "config.h"
#include "csq.h"
#include "globals.h"
#include "apc.h"


NTSTATUS client::create_close(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS client::device_control(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	int ProtectionServiceConfig;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION irpStack;
	PIRP KsIrp;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	pCsqIrpQueue DevExt = reinterpret_cast<pCsqIrpQueue>(DeviceObject->DeviceExtension);

	switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_AUDGUARD_POLL_EVENTS:

		// insert client event IRP to event queue
		// IoCsqInsertIrp marks the IRP as pending
		IoCsqInsertIrp(&DevExt->CsqObject, Irp, nullptr);
		status = STATUS_PENDING;
		break;

	case IOCTL_AUDGUARD_USER_DIALOG:

		KsIrp = IoCsqRemoveNextIrp(&globals::g_pKsPropertyQueue->CsqObject, nullptr);
		if (!KsIrp)
			break;

		ProtectionServiceConfig = *reinterpret_cast<int*>(Irp->AssociatedIrp.SystemBuffer);

		// complete the previously pended IOCTL_KS_PROPERTY 
		// we have to do it from the caller's context since ksthunk (below us) will try to map user addresses
		if (apc::queue_completion_kernel_apc(KsIrp, ProtectionServiceConfig))
			status = STATUS_SUCCESS;

		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = status;

	if (status != STATUS_PENDING)
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;

}


NTSTATUS client::forward(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

	if (IoStackLocation->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		return client::device_control(DeviceObject, Irp);

	else if (IoStackLocation->MajorFunction == IRP_MJ_CREATE || IoStackLocation->MajorFunction == IRP_MJ_CLOSE)
		return client::create_close(DeviceObject, Irp);

	else
		return STATUS_INVALID_DEVICE_REQUEST;

}
