#include "wnf.h"
#include "config.h"
#include "client.h"
#include "globals.h"
#include "apc.h"

NTSTATUS wnf::Subscribe(ULONG64 StateIdentifier)
{
    WNF_STATE_NAME StateName;
    NTSTATUS Status;

    StateName.Data[0] = (ULONG)(StateIdentifier & 0xFFFFFFFF);  // Gets lower 32 bits (0x3BC4075)
    StateName.Data[1] = (ULONG)(StateIdentifier >> 32);         // Gets upper 32 bits (0x2821B2CA)

    return ExSubscribeWnfStateChange(&globals::g_WnfSubsription, &StateName, 0x1, NULL, &wnf::Callback, NULL);
}

NTSTATUS wnf::Callback(PWNF_SUBSCRIPTION Subscription, PWNF_STATE_NAME StateName, ULONG SubscribedEventSet, WNF_CHANGE_STAMP ChangeStamp, PWNF_TYPE_ID TypeId, PVOID CallbackContext)
{

    NTSTATUS Status = STATUS_SUCCESS;
    ULONG BufSize = 0;
    PVOID pStateData = nullptr;
    pAudioStateData AudioCaptureStateData = nullptr;
    WNF_CHANGE_STAMP changeStamp = 0;

    Status = ExQueryWnfStateData(Subscription, &changeStamp, NULL, &BufSize);
    if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        pStateData = ExAllocatePoolWithTag(NonPagedPool, BufSize, TAG);
        if (!pStateData)
            return STATUS_UNSUCCESSFUL;

        Status = ExQueryWnfStateData(Subscription, &ChangeStamp, pStateData, &BufSize);
        if (NT_SUCCESS(Status))
        {

            AudioCaptureStateData = reinterpret_cast<pAudioStateData>(pStateData);

            for (int i = 0; i < AudioCaptureStateData->NumberOfEntries; i++)
            {
                DbgPrint("[*] AudioGuard :: Wnf :: process capturing audio -> 0x%x\n", AudioCaptureStateData->Entries[i]);
            }

        }

        ExFreePoolWithTag(pStateData, TAG);
    }

    return Status;
}