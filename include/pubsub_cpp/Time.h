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
	uint64_t usec;

	Duration()
	{

	}

    Duration(uint64_t usec_)
    {
      usec = usec_;
    }

	Duration(double seconds)
		: usec(seconds*1000000.0)
	{

	}

	Duration(uint32_t sec, uint32_t usec)
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
	uint64_t usec;

	Time()
	{

	}

	Time(uint64_t usec)
		: usec(usec)
	{

	}


	Time(uint32_t sec, uint32_t usec)
		: usec(static_cast<uint64_t>(sec)*1000000 + static_cast<uint64_t>(usec))
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
		static uint64_t start = 0;
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
			start = ((uint64_t)(system_time.wMilliseconds * 1000)) + ((ularge.QuadPart - epoch) / 10ULL);

			// subtract out uptime
			start -= GetTickCount64() * 1000;
		}
		uint64_t start_sec = start / 1000000;
		uint64_t count = GetTickCount64();// todo use higher accuracy timer
		uint64_t usec = count * 1000 + start;
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
      time_t t = usec / 1000000;// toSec();

	  std::string str = ctime(&t);
      str.pop_back();
      return str;
	}
};
}
