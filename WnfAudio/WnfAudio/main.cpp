#include <ntddk.h>
#include "wnf.h"
#include "config.h"
#include "globals.h"

PWNF_SUBSCRIPTION globals::g_WnfSubsription = nullptr;



void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	if (globals::g_WnfSubsription)
	{
		ExUnsubscribeWnfStateChange(globals::g_WnfSubsription);
		globals::g_WnfSubsription = nullptr;
	}

	DbgPrint("[*] WnfAudio :: unloaded!\n");
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS Status;

	DriverObject->DriverUnload = DriverUnload;

	Status = wnf::Subscribe(WNF_AUDC_CPATURE);
	if (!NT_SUCCESS(Status))
		return Status;

	DbgPrint("[*] WnfAudio :: loaded!\n");

    return Status;
}