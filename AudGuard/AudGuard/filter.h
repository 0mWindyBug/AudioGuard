#include <ntifs.h>
#include <ntddk.h>
#include <windef.h>
#include <initguid.h>
#include <ks.h>
#include <ksmedia.h>
#include <minwindef.h>


const GUID GUID_PROPSETID_Connection = { STATIC_KSPROPSETID_Connection };


namespace filter {
    NTSTATUS DispatchForward(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PDO);
    bool KsPropertyHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IoStackLocation);

    typedef struct _DeviceExtension
    {
        IO_REMOVE_LOCK RemoveLock;
        PDEVICE_OBJECT LowerDeviceObject;
    } DeviceExtension,* pDeviceExtension;


}