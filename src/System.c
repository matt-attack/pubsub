#include <pubsub/System.h>

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
