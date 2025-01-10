#include <thread>
#include "communicate.h"
#include "config.h"

bool communicate::wait_for_event(HANDLE hDevice)
{
    while (true)
    {
        OVERLAPPED* overlapped = new(std::nothrow) OVERLAPPED();
        if (!overlapped)
            return false;



        overlapped->hEvent = CreateEventW(nullptr, true, false, nullptr);
        if (!overlapped->hEvent)
        {
            std::cout << "[*] Failed to create event: " << GetLastError() << std::endl;
            delete overlapped;
            return false;
        }
        DWORD bytes = 0;
        BOOL status = DeviceIoControl(hDevice, IOCTL_AUDGUARD_POLL_EVENTS, nullptr, 0, nullptr, 0, &bytes, overlapped);
        if (!status && GetLastError() != ERROR_IO_PENDING)
        {
            std::cout << "[*] Failed in DeviceIoControl: " << GetLastError() << std::endl;
            CloseHandle(overlapped->hEvent);
            delete overlapped;
            return false;
        }

        std::cout << "[*] Polling for events from driver..." << std::endl;

        DWORD waitStatus = WaitForSingleObject(overlapped->hEvent, INFINITE);
        if (waitStatus == WAIT_OBJECT_0)
        {
            std::cout << "[*] Event signaled, processing..." << std::endl;

            std::thread set_user_dialog_thread(communicate::initiate_dialog, hDevice);
            set_user_dialog_thread.detach();
        }
        else
        {
            std::cout << "[*] Wait error: " << GetLastError() << std::endl;
            CloseHandle(overlapped->hEvent);
            delete overlapped;
            break;
        }

        CloseHandle(overlapped->hEvent);
        delete overlapped;
    }

    return true;
}


bool communicate::initiate_dialog(HANDLE hDevice)
{

    int Response = DEFAULT_AUDGUARD_CONFIG;


    DWORD bytes = 0;
    BOOL status = DeviceIoControl(
        hDevice,
        IOCTL_AUDGUARD_USER_DIALOG,
        &Response,
        sizeof(Response),
        nullptr,
        0,
        &bytes,
        nullptr
    );

    if (!status)
    {
        std::cout << "[*] Failed to send response. Error: " << GetLastError() << std::endl;
        return false;
    }


    std::cout << "[*] sent response to driver!" << std::endl;
    return true;
    


}