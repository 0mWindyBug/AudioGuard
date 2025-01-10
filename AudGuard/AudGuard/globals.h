#pragma once
#include <ntddk.h>
#include "csq.h"
#include "wnf.h"


namespace globals
{
	extern pCsqIrpQueue   g_pKsPropertyQueue;
	extern PDEVICE_OBJECT ClientDeviceObject;
	extern EX_RUNDOWN_REF PendingOperations;
	extern PWNF_SUBSCRIPTION g_WnfSubsription;


}