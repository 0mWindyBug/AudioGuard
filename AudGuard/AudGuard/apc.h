#pragma once
#include <ntddk.h>


typedef enum _KAPC_ENVIRONMENT {
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT, * PKAPC_ENVIRONMENT;

typedef VOID(NTAPI* PKNORMAL_ROUTINE)(
    _In_ PVOID NormalContext,
    _In_ PVOID SystemArgument1,
    _In_ PVOID SystemArgument2
    );

typedef VOID KKERNEL_ROUTINE(
    _In_ PRKAPC Apc,
    _Inout_opt_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_opt_ PVOID* NormalContext,
    _Inout_ PVOID* SystemArgument1,
    _Inout_ PVOID* SystemArgument2
);

typedef KKERNEL_ROUTINE(NTAPI* PKKERNEL_ROUTINE);

typedef VOID(NTAPI* PKRUNDOWN_ROUTINE)(_In_ PRKAPC Apc);

extern "C" VOID NTAPI KeInitializeApc(
    _Out_ PRKAPC Apc,
    _In_ PRKTHREAD Thread,
    _In_ KAPC_ENVIRONMENT Environment,
    _In_ PKKERNEL_ROUTINE KernelRoutine,
    _In_opt_ PKRUNDOWN_ROUTINE RundownRoutine,
    _In_opt_ PKNORMAL_ROUTINE NormalRoutine,
    _In_opt_ KPROCESSOR_MODE ProcessorMode,
    _In_opt_ PVOID NormalContext
);

extern "C" BOOLEAN NTAPI KeInsertQueueApc(
    _Inout_ PRKAPC Apc,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2,
    _In_ KPRIORITY Increment
);


namespace apc
{
    void kernel_routine(PKAPC Apc, PKNORMAL_ROUTINE* NormalRoutine, PVOID* Arg1, PVOID* Arg2, PVOID* Arg3);
    void rundown_routine(PKAPC Apc);
    void normal_routine(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument2);
    bool queue_completion_kernel_apc(PIRP Irp, int ProtectionServiceConfig);
    typedef struct _ApcContext 
    {
        int ProtectionServiceConfig;
        PIRP Irp;
    } ApcContext, * pApcContext;
}

