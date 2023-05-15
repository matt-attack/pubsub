#include <pubsub/System.h>

#include <stdbool.h>

#ifndef _WIN32
#ifndef ARDUINO
#include <time.h>
#endif
#endif

#ifdef ARDUINO
unsigned long ps_get_tick_count()
{
	return millis();
}
#else
uint64_t ps_get_tick_count()
{
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
#endif
}
#endif

#ifdef _WIN32
#include <Windows.h>
void ps_sleep(unsigned int time_ms)
{
  timeBeginPeriod(1);
	Sleep(time_ms);
}

uint64_t GetTimeMs()
{
	return GetTickCount64();
}
#else
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
void ps_sleep(unsigned int time_ms)
{
	usleep(time_ms*1000);
}

uint64_t GetTimeMs()
{
    struct timeval te; 
    gettimeofday(&te, 0); // get current time
    uint64_t milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}
#endif

void ps_print_socket_error(const char* description)
{
#ifdef _WIN32
	static char message[256] = {0};
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0, WSAGetLastError(), 0, message, 256, 0);
    char *nl = strrchr(message, '\n');
    if (nl)
	{
		*nl = 0;
	}

	if (description != 0)
	{
		printf("%s: \n", errstr);
	}
	else
	{
		printf("%s\n", errstr);
	}
#else
	perror(description);
#endif
}

static bool initialized = false;
void ps_networking_init()
{
#ifdef _WIN32
	if (!initialized)
	{
		struct WSAData wsaData;
		int retval;
		if ((retval = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
		{
			char sz[156];
			sprintf_s(sz, 156, "WSAStartup failed with error %d\n", retval);
			OutputDebugStringA(sz);
			WSACleanup();
		}
		initialized = true;
	}
#endif
}
