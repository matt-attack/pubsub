#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif

namespace pubsub
{
class Time
{
	unsigned int sec;
	unsigned int usec;
public:

	Time()
	{

	}

	Time(unsigned int sec, unsigned int usec)
		: sec(sec), usec(usec)
	{

	}

	static Time now()
	{
#ifdef _WIN32
		int count = GetTickCount();
		return Time(count / 1000, (count % 1000)*1000);
#else
		struct timeval tv;
		gettimeofday(&tv, 0);

		return Time(tv.tv_sec, tv.tv_usec);
#endif
	}

	double toSec()
	{
		return sec + usec / 1000000.0;
	}
};
}