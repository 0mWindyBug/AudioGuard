#include "filter.h"
#include "apc.h"
#include "config.h"
#include "globals.h"

void apc::kernel_routine(PKAPC Apc, PKNORMAL_ROUTINE* pNormalRoutine, PVOID* Arg1, PVOID* Arg2, PVOID* Arg3)
{
	UNREFERENCED_PARAMETER(pNormalRoutine);
	UNREFERENCED_PARAMETER(Arg1);
	UNREFERENCED_PARAMETER(Arg2);
	UNREFERENCED_PARAMETER(Arg3);

	ExFreePoolWithTag(Apc, TAG);
	ExReleaseRundownProtection(&globals::PendingOperations);

}

void apc::rundown_routine(PKAPC Apc)
{
	ExFreePoolWithTag(Apc->NormalContext, TAG);
	ExFreePoolWithTag(Apc, TAG);
	ExReleaseRundownProtection(&globals::PendingOperations);

}

void apc::normal_routine(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	pApcContext ApcContx = reinterpret_cast<pApcContext>(NormalContext);
	PDEVICE_OBJECT FilterDeviceObject = reinterpret_cast<PDEVICE_OBJECT>(ApcContx->Irp->Tail.Overlay.DriverContext[0]);
	filter::pDeviceExtension DevExt = reinterpret_cast<filter::pDeviceExtension>(FilterDeviceObject->DeviceExtension);

	int ProtectionServiceConfig = ApcContx->ProtectionServiceConfig;

	// if service is configured to block all access complete with status denied  
	if (ProtectionServiceConfig == AUDGUARD_BLOCK_MIC_ACCESS)
	{
		DbgPrint("[*] AudioGuard :: completing IOCTL_KS_PROPERTY IRP with access denied!\n");
		ApcContx->Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
		ApcContx->Irp->IoStatus.Information = 0;
		IofCompleteRequest(ApcContx->Irp, IO_NO_INCREMENT);
	}

	// otherwise pass the request down 
	else
	{
		DbgPrint("[*] AudioGuard :: passing IOCTL_KS_PROPERTY IRP down the audio stack!\n");
		IoSkipCurrentIrpStackLocation(ApcContx->Irp);
		IofCallDriver(DevExt->LowerDeviceObject, ApcContx->Irp);
	}

	IoReleaseRemoveLock(&DevExt->RemoveLock, ApcContx->Irp);
	ExFreePoolWithTag(ApcContx, TAG);

}


bool apc::queue_completion_kernel_apc(PIRP Irp, int ProtectionServiceConfig)
{
	pApcContext pApcContx = nullptr;

	PKAPC Kapc = reinterpret_cast<PKAPC>(ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), TAG));
	if (!Kapc)
		return false;


	PRKTHREAD CallerThread = reinterpret_cast<PRKTHREAD>(Irp->Tail.Overlay.Thread);
	PDEVICE_OBJECT FilterDeviceObject = reinterpret_cast<PDEVICE_OBJECT>(Irp->Tail.Overlay.DriverContext[0]);

	// shouldn't actually happen, but in case it has we will have to fail the request as we cant properly pass it down at this point 
	if (!CallerThread || !FilterDeviceObject)
	{
		ExFreePoolWithTag(Kapc,TAG);
		Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	pApcContx = reinterpret_cast<pApcContext>(ExAllocatePoolWithTag(NonPagedPool, sizeof(ApcContext), TAG));
	if (!pApcContx)
	{
		ExFreePoolWithTag(Kapc, TAG);
		Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	pApcContx->Irp = Irp;
	pApcContx->ProtectionServiceConfig = ProtectionServiceConfig;
	

	KeInitializeApc(
		Kapc,
		CallerThread,
		OriginalApcEnvironment,
		apc::kernel_routine,
		apc::rundown_routine,
		apc::normal_routine,
		KernelMode,
		pApcContx
	);
	
	ExAcquireRundownProtection(&globals::PendingOperations);

	bool inserted = KeInsertQueueApc(
		Kapc,
		nullptr,
		nullptr,
		0
	);

	if (!inserted)
	{
		ExFreePoolWithTag(Kapc, TAG);
		ExFreePoolWithTag(pApcContx,TAG);
		ExReleaseRundownProtection(&globals::PendingOperations);
		return false;
	}


	return true;
}