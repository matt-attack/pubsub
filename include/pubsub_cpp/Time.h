#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif

#include <time.h>
#include <string>

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

	bool operator<(const Duration& rhs) const// otherwise, both parameters may be const references
	{
		return this->usec < rhs.usec;
	}

	bool operator>(const Duration& rhs) const// otherwise, both parameters may be const references
	{
		return this->usec > rhs.usec;
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

	Time(unsigned long long usec)
		: usec(usec)
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
		static unsigned long long start = 0;
		if (!start)
		{
			/* FILETIME of Jan 1 1970 00:00:00. */
			static const unsigned __int64 epoch = ((unsigned __int64)116444736000000000ULL);

			FILETIME    file_time;
			SYSTEMTIME  system_time;
			ULARGE_INTEGER ularge;

			GetSystemTime(&system_time);
			SystemTimeToFileTime(&system_time, &file_time);
			ularge.LowPart = file_time.dwLowDateTime;
			ularge.HighPart = file_time.dwHighDateTime;

			// the file_time is in units of 100 nanoseconds
			start = ((unsigned long long)(system_time.wMilliseconds * 1000)) + ((ularge.QuadPart - epoch) / 10ULL);

			// subtract out uptime
			start -= GetTickCount64() * 1000;
		}
		long long start_sec = start / 1000000;
		unsigned long long count = GetTickCount64();// todo use higher accuracy timer
		unsigned long long usec = count * 1000 + start;
		return Time(usec);
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

	std::string toString()
	{
      time_t t = usec / 100000;// toSec();

	  std::string str = ctime(&t);
      str.pop_back();
      return str;
	}
};
}