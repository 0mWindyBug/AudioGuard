#include "csq.h"
#include "client.h"


void csq::InsertIrp(PIO_CSQ Csq, PIRP Irp)
{
    pCsqIrpQueue DevExt;
    DevExt = CONTAINING_RECORD(Csq, CsqIrpQueue, CsqObject);
    InsertTailList(&DevExt->IrpQueue, &Irp->Tail.Overlay.ListEntry);
}


void csq::RemoveIrp(PIO_CSQ Csq, PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}


PIRP csq::PeekNextIrp(PIO_CSQ Csq, PIRP Irp, PVOID PeekContext)
{
    pCsqIrpQueue            DevExt;
    PIRP                    NextIrp = nullptr;
    PLIST_ENTRY             NextEntry;
    PLIST_ENTRY             ListHead;
    PIO_STACK_LOCATION      IrpStack;

    DevExt = CONTAINING_RECORD(Csq, CsqIrpQueue, CsqObject);

    ListHead = &DevExt->IrpQueue;

    if (Irp == nullptr)
        NextEntry = ListHead->Flink;

    else
        NextEntry = Irp->Tail.Overlay.ListEntry.Flink;


    while (NextEntry != ListHead)
    {

        NextIrp = CONTAINING_RECORD(NextEntry, IRP, Tail.Overlay.ListEntry);

        IrpStack = IoGetCurrentIrpStackLocation(NextIrp);

        if (PeekContext)
        {
            if (IrpStack->FileObject == reinterpret_cast<PFILE_OBJECT>(PeekContext))
                break;

        }
        else
        {
            break;
        }

        NextIrp = nullptr;
        NextEntry = NextEntry->Flink;
    }

    return NextIrp;
}


void csq::AcquireLock(PIO_CSQ Csq, PKIRQL Irql)
{
    pCsqIrpQueue   DevExt;

    DevExt = CONTAINING_RECORD(Csq, CsqIrpQueue, CsqObject);

    KeAcquireSpinLock(&DevExt->QueueLock, Irql);
}


void csq::ReleaseLock(PIO_CSQ Csq, KIRQL Irql)
{
    pCsqIrpQueue   DevExt;

    DevExt = CONTAINING_RECORD(Csq, CsqIrpQueue, CsqObject);

    KeReleaseSpinLock(&DevExt->QueueLock, Irql);
}


void csq::CompleteCanceledIrp(PIO_CSQ Csq, PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}


