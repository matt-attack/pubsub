#pragma once

#ifdef _WIN32
#include <Windows.h>
void ps_sleep(unsigned int time_ms)
{
	Sleep(time_ms);
}
#else
void ps_sleep(unsigned int time_ms)
{
	//usleep(time_ms*1000);
}

unsigned int GetTickCount()
{
	return 0;
}
#endif