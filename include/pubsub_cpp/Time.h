#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif

namespace pubsub
{
class Duration
{
public:
	unsigned long long usec;

	Duration()
	{

	}

	Duration(double seconds)
		: usec(seconds*1000000.0)
	{

	}

	Duration(unsigned int sec, unsigned int usec)
		: usec(sec * 1000000 + usec)
	{

	}

	double toSec()
	{
		return usec / 1000000.0;
	}
};

class Time
{
public:
	unsigned long long usec;

	Time()
	{

	}

	Time(unsigned int sec, unsigned int usec)
		: usec(sec*1000000 + usec)
	{

	}

	Duration operator-(const Time& rhs) // otherwise, both parameters may be const references
	{
		Duration out;
		out.usec = this->usec - rhs.usec;
		return out; // return the result by value (uses move constructor)
	}

	bool operator<(const Time& rhs) const// otherwise, both parameters may be const references
	{
		return this->usec < rhs.usec;
	}

	bool operator>(const Time& rhs) const// otherwise, both parameters may be const references
	{
		return this->usec > rhs.usec;
	}

	Time operator+(const Duration& rhs) // otherwise, both parameters may be const references
	{
		Time out;
		out.usec = this->usec + rhs.usec;
		return out; // return the result by value (uses move constructor)
	}

	static Time now()
	{
#ifdef _WIN32
		unsigned int count = GetTickCount();
		return Time(count / 1000, (count % 1000)*1000);
#else
		struct timeval tv;
		gettimeofday(&tv, 0);

		return Time(tv.tv_sec, tv.tv_usec);
#endif
	}

	double toSec()
	{
		return usec / 1000000.0;
	}
};
}