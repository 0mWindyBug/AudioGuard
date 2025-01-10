#include <iostream>
#include <Windows.h>
#include <thread>
#include <string>
#include "communicate.h"
#include "config.h"

int main()
{
    std::cout << "[*] [debug] config -> " << DEFAULT_AUDGUARD_CONFIG << std::endl;

    HANDLE hDevice = CreateFileW(L"\\\\.\\AudGuard", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cout << "[*] Failed to obtain device handle" << std::endl;
        return -1;
    }

    std::thread poll_events_thread(communicate::wait_for_event, hDevice);

    poll_events_thread.join();

    CloseHandle(hDevice);

    return 0;
}