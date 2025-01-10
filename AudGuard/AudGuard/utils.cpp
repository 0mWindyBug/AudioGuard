#include "utils.h"

bool utils::PsGetImageNameByPid(ULONG ProcessId, PUNICODE_STRING* PsImageName)
{
	PEPROCESS Proc;
	NTSTATUS status = PsLookupProcessByProcessId(UlongToHandle(ProcessId), &Proc);
	if (!NT_SUCCESS(status))
		return false;

	status = SeLocateProcessImageName(Proc, PsImageName);
	ObDereferenceObject(Proc);

	if (!NT_SUCCESS(status))
		return false;

	return true;
}

void utils::PrintGuidValues(LPCGUID lpGuid)
{
    KdPrint(("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        lpGuid->Data1,
        lpGuid->Data2,
        lpGuid->Data3,
        lpGuid->Data4[0],
        lpGuid->Data4[1],
        lpGuid->Data4[2],
        lpGuid->Data4[3],
        lpGuid->Data4[4],
        lpGuid->Data4[5],
        lpGuid->Data4[6],
        lpGuid->Data4[7]));
}