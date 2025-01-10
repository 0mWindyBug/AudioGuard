#pragma once 
#include <ntddk.h>

#define WNF_AUDC_CPATURE 0x2821B2CA3BC4075

typedef struct _AudioStateData
{
    ULONG Unknown;
    ULONG NumberOfEntries;
    ULONG Entries[];
}AudioStateData, * pAudioStateData;


typedef ULONG WNF_CHANGE_STAMP, * PWNF_CHANGE_STAMP;

typedef struct _WNF_NODE_HEADER
{
    UINT16 NodeTypeCode;
    UINT16 NodeByteSize;
} WNF_NODE_HEADER, * PWNF_NODE_HEADER;

typedef enum _WNF_DATA_SCOPE
{
    WnfDataScopeSystem = 0,
    WnfDataScopeSession = 1,
    WnfDataScopeUser = 2,
    WnfDataScopeProcess = 3,
    WnfDataScopeMachine = 4,
    WnfDataScopePhysicalMachine = 5
} WNF_DATA_SCOPE, * PWNF_DATA_SCOPE;

typedef struct _WNF_LOCK
{
    EX_PUSH_LOCK PushLock;
} WNF_LOCK, * PWNF_LOCK;

typedef struct _WNF_STATE_NAME_STRUCT
{
    UINT64 Version : 4;
    UINT64 NameLifetime : 2;
    UINT64 DataScope : 4;
    UINT64 PermanentData : 1;
    UINT64 Sequence : 53;
} WNF_STATE_NAME_STRUCT, * PWNF_STATE_NAME_STRUCT;

typedef struct _RTL_AVL_TREE
{
    PRTL_BALANCED_NODE Root;
} RTL_AVL_TREE, * PRTL_AVL_TREE;

typedef struct _WNF_SCOPE_INSTANCE
{
    WNF_NODE_HEADER Header;
#if defined(_M_X64)
    UINT8 _PADDING0_[4];
#endif
    EX_RUNDOWN_REF RunRef;
    WNF_DATA_SCOPE DataScope;
    ULONG32 InstanceIdSize;
    PVOID InstanceIdData;
    LIST_ENTRY ResolverListEntry;
    WNF_LOCK NameSetLock;
    RTL_AVL_TREE NameSet;
    PVOID PermanentDataStore;
    PVOID VolatilePermanentDataStore;
} WNF_SCOPE_INSTANCE, * PWNF_SCOPE_INSTANCE;

typedef struct _WNF_TYPE_ID
{
    GUID TypeId;
} WNF_TYPE_ID, * PWNF_TYPE_ID;

typedef struct _WNF_STATE_NAME_REGISTRATION
{
    ULONG32 MaxStateSize;
#if defined(_M_X64)
    UINT8 _PADDING0_[4];
#endif
    PWNF_TYPE_ID TypeId;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
} WNF_STATE_NAME_REGISTRATION, * PWNF_STATE_NAME_REGISTRATION;

typedef struct _WNF_STATE_DATA
{
    WNF_NODE_HEADER Header;
    ULONG32 AllocatedSize;
    ULONG32 DataSize;
    ULONG32 ChangeStamp;
} WNF_STATE_DATA, * PWNF_STATE_DATA;

typedef struct _WNF_NAME_INSTANCE
{
    WNF_NODE_HEADER Header;
#if defined(_M_X64)
    UINT8 _PADDING0_[4];
#endif
    EX_RUNDOWN_REF RunRef;
    RTL_BALANCED_NODE TreeLinks;
#if !defined(_M_X64)
    UINT8 _PADDING0_[4];
#endif
    WNF_STATE_NAME_STRUCT StateName;
    PWNF_SCOPE_INSTANCE ScopeInstance;
    WNF_STATE_NAME_REGISTRATION StateNameInfo;
    WNF_LOCK StateDataLock;
    PWNF_STATE_DATA StateData;
    ULONG32 CurrentChangeStamp;
#if defined(_M_X64)
    UINT8 _PADDING1_[4];
#endif
    PVOID PermanentDataStore;
    WNF_LOCK StateSubscriptionListLock;
    LIST_ENTRY StateSubscriptionListHead;
    LIST_ENTRY TemporaryNameListEntry;
    PEPROCESS CreatorProcess;
    LONG32 DataSubscribersCount;
    LONG32 CurrentDeliveryCount;
} WNF_NAME_INSTANCE, * PWNF_NAME_INSTANCE;

typedef enum _WNF_SUBSCRIPTION_STATE
{
    WNF_SUB_STATE_QUIESCENT = 0,
    WNF_SUB_STATE_READY_TO_DELIVER = 1,
    WNF_SUB_STATE_IN_DELIVERY = 2,
    WNF_SUB_STATE_RETRY = 3
} WNF_SUBSCRIPTION_STATE, * PWNF_SUBSCRIPTION_STATE;

typedef struct _WNF_SUBSCRIPTION
{
    WNF_NODE_HEADER Header;
#if defined(_M_X64)
    UINT8 _PADDING0_[4];
#endif
    EX_RUNDOWN_REF RunRef;
    UINT64 SubscriptionId;
    LIST_ENTRY ProcessSubscriptionListEntry;
    PEPROCESS Process;
    PWNF_NAME_INSTANCE NameInstance;
    WNF_STATE_NAME_STRUCT StateName;
    LIST_ENTRY StateSubscriptionListEntry;
    UINT_PTR CallbackRoutine;
    PVOID CallbackContext;
    ULONG32 CurrentChangeStamp;
    ULONG32 SubscribedEventSet;
    LIST_ENTRY PendingSubscriptionListEntry;
    WNF_SUBSCRIPTION_STATE SubscriptionState;
    ULONG32 SignaledEventSet;
    ULONG32 InDeliveryEventSet;
    UINT8 _PADDING01_[4];
} WNF_SUBSCRIPTION, * PWNF_SUBSCRIPTION;


EXTERN_C NTSTATUS
ExQueryWnfStateData(
    _In_ PWNF_SUBSCRIPTION Subscription,
    _Out_ PWNF_CHANGE_STAMP ChangeStamp,
    _Out_ PVOID OutputBuffer,
    _Out_ PULONG OutputBufferSize
);

typedef
NTSTATUS
(*PWNF_CALLBACK) (
    _In_ PWNF_SUBSCRIPTION Subscription,
    _In_ PWNF_STATE_NAME StateName,
    _In_ ULONG SubscribedEventSet,
    _In_ WNF_CHANGE_STAMP ChangeStamp,
    _In_opt_ PWNF_TYPE_ID TypeId,
    _In_opt_ PVOID CallbackContext
    );

EXTERN_C NTSTATUS
ExSubscribeWnfStateChange(
    _Out_ PWNF_SUBSCRIPTION* Subscription,
    _In_ PWNF_STATE_NAME StateName,
    _In_ ULONG DeliveryOption,
    _In_ WNF_CHANGE_STAMP CurrentChangeStamp,
    _In_ PWNF_CALLBACK Callback,
    _In_opt_ PVOID CallbackContext
);

EXTERN_C NTSTATUS
ExUnsubscribeWnfStateChange(_In_ PWNF_SUBSCRIPTION WnfSubsription);



namespace wnf
{
    NTSTATUS Subscribe(ULONG64 StateIdentifier);
    NTSTATUS Callback(PWNF_SUBSCRIPTION Subscription, PWNF_STATE_NAME StateName, ULONG SubscribedEventSet, WNF_CHANGE_STAMP ChangeStamp, PWNF_TYPE_ID TypeId, PVOID CallbackContext);

}