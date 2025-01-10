#pragma once
#include <Windows.h>
#include <iostream>

#define IOCTL_AUDGUARD_POLL_EVENTS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)		
#define IOCTL_AUDGUARD_USER_DIALOG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)	

namespace communicate
{


	bool wait_for_event(HANDLE hDevice);
	bool initiate_dialog(HANDLE hDevice);

}