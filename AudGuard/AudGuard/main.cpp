#include <ntifs.h>
#include <ntddk.h>
#include <wdmsec.h>
#include "filter.h"
#include "csq.h"
#include "client.h"
#include "wnf.h"
#include "config.h"
#include "globals.h"

PDEVICE_OBJECT globals::ClientDeviceObject;
pCsqIrpQueue   globals::g_pKsPropertyQueue = nullptr;
EX_RUNDOWN_REF globals::PendingOperations;
PWNF_SUBSCRIPTION globals::g_WnfSubsription = nullptr;



void AudGuardUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNICODE_STRING AudguardLink = RTL_CONSTANT_STRING(AUDGUARD_SYMLINKNAME);

    if (globals::g_pKsPropertyQueue)
    {
        ExFreePoolWithTag(globals::g_pKsPropertyQueue, TAG);
        globals::g_pKsPropertyQueue = nullptr;
    }

    if (globals::g_WnfSubsription)
    {
        ExUnsubscribeWnfStateChange(globals::g_WnfSubsription);
        globals::g_WnfSubsription = nullptr;
    }

    ExWaitForRundownProtectionRelease(&globals::PendingOperations);

    IoDeleteDevice(globals::ClientDeviceObject);
    IoDeleteSymbolicLink(&AudguardLink);


    DbgPrint("[*] AudioGuard :: going down!\n");
}


NTSTATUS AudGuardIrpDispatcher(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION IoStack = IoGetCurrentIrpStackLocation(Irp);

    // if request is targeted at our client device 
    if (DeviceObject->DeviceType == AUDGUARD_DEVICE_TYPE)
    {
        return client::forward(DeviceObject, Irp);
    }

    // request is targeted at our filter device 
    else
    {
        return filter::DispatchForward(DeviceObject, Irp);
    }
}



extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS Status;
    UNICODE_STRING Ssdl           = RTL_CONSTANT_STRING(ADMIN_ONLY_SD);
    UNICODE_STRING AudguardDevice = RTL_CONSTANT_STRING(AUDGUARD_DEVICENAME);
    UNICODE_STRING AudguardLink   = RTL_CONSTANT_STRING(AUDGUARD_SYMLINKNAME);

    ExInitializeRundownProtection(&globals::PendingOperations);

    Status = wnf::Subscribe(WNF_AUDC_CPATURE);
    if (!NT_SUCCESS(Status))
        return Status;

    globals::g_pKsPropertyQueue = reinterpret_cast<pCsqIrpQueue>(ExAllocatePoolWithTag(NonPagedPool, sizeof(CsqIrpQueue), TAG));
    if (!globals::g_pKsPropertyQueue)
        return STATUS_INSUFFICIENT_RESOURCES;

    KeInitializeSpinLock(&globals::g_pKsPropertyQueue->QueueLock);

    InitializeListHead(&globals::g_pKsPropertyQueue->IrpQueue);

    Status = IoCsqInitialize(&globals::g_pKsPropertyQueue->CsqObject, csq::InsertIrp, csq::RemoveIrp, csq::PeekNextIrp, csq::AcquireLock, csq::ReleaseLock, csq::CompleteCanceledIrp);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(globals::g_pKsPropertyQueue, TAG);
        globals::g_pKsPropertyQueue = nullptr;
        return Status;
    }

    Status = IoCreateDeviceSecure(DriverObject, sizeof(CsqIrpQueue), &AudguardDevice, AUDGUARD_DEVICE_TYPE, 0, FALSE, &Ssdl, NULL, &globals::ClientDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(globals::g_pKsPropertyQueue, TAG);
        globals::g_pKsPropertyQueue = nullptr;
        return Status;
    }

    Status = IoCreateSymbolicLink(&AudguardLink, &AudguardDevice);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(globals::g_pKsPropertyQueue, TAG);
        globals::g_pKsPropertyQueue = nullptr;
        IoDeleteDevice(globals::ClientDeviceObject);
        return Status;
    }

    KeInitializeSpinLock(&globals::g_pKsPropertyQueue->QueueLock);

    InitializeListHead(&globals::g_pKsPropertyQueue->IrpQueue);

    pCsqIrpQueue ClientIrpQueue = reinterpret_cast<pCsqIrpQueue>(globals::ClientDeviceObject->DeviceExtension);

    KeInitializeSpinLock(&ClientIrpQueue->QueueLock);

    InitializeListHead(&ClientIrpQueue->IrpQueue);


    Status = IoCsqInitialize(&ClientIrpQueue->CsqObject, csq::InsertIrp, csq::RemoveIrp, csq::PeekNextIrp, csq::AcquireLock, csq::ReleaseLock, csq::CompleteCanceledIrp);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(globals::g_pKsPropertyQueue, TAG);
        globals::g_pKsPropertyQueue = nullptr;
        IoDeleteDevice(globals::ClientDeviceObject);
        IoDeleteSymbolicLink(&AudguardLink);
        return Status;
    }

    DriverObject->DriverUnload = AudGuardUnload;

    for (int i = 0; i < ARRAYSIZE(DriverObject->MajorFunction); i++)
    {
        DriverObject->MajorFunction[i] = AudGuardIrpDispatcher;
    }

    DriverObject->MajorFunction[IRP_MJ_PNP] = filter::DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = filter::DispatchPower;

    DriverObject->DriverExtension->AddDevice = filter::AddDevice;

    DbgPrint("[*] AudioGuard :: protection is active!\n");

    return STATUS_SUCCESS;
}