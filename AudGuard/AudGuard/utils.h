#pragma once
#include <ntifs.h>
#include <ntddk.h>

namespace utils
{
	bool PsGetImageNameByPid(ULONG ProcessId, PUNICODE_STRING* PsImageName);
	void PrintGuidValues(LPCGUID lpGuid);
}