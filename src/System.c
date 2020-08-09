#include <../include/pubsub/System.h>

#ifdef _WIN32
#include <Windows.h>
void ps_sleep(unsigned int time_ms)
{
	Sleep(time_ms);
}

unsigned long long GetTimeMs()
{
	return GetTickCount64();
}
#else
#include <sys/time.h>
void ps_sleep(unsigned int time_ms)
{
	usleep(time_ms*1000);
}

unsigned long long GetTimeMs()
{
    struct timeval te; 
    gettimeofday(&te, 0); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}
#endif
