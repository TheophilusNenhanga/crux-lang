#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "garbage_collector.h"
#include "panic.h"
#include "stdlib/time.h"

/**
 * Returns the current Unix timestamp in seconds
 * Returns Float
 */
Value time_seconds_function_(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;
	return FLOAT_VAL((double)time(NULL));
}

/**
 * Returns the current Unix timestamp in milliseconds
 * Returns Float
 */
Value time_milliseconds_function_(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	const uint64_t ms = (uli.QuadPart - 116444736000000000ULL) / 10000;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	const uint64_t ms = (uint64_t)ts.tv_sec * 1000 +
			    (uint64_t)ts.tv_nsec / 1000000;
#endif
	return FLOAT_VAL((double)ms);
}

/**
 * Pauses execution for the specified number of seconds
 * arg0 -> seconds: Float
 * Returns Result<Nil>
 */
Value sleep_seconds_function(VM *vm, const Value *args)
{
	const double seconds = TO_DOUBLE(args[0]);
	if (seconds < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Sleep duration cannot be negative.",
					  VALUE);
	}

#ifdef _WIN32
	Sleep((DWORD)(seconds * 1000));
#else
	sleep((unsigned int)seconds);
#endif
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Pauses execution for the specified number of milliseconds
 * arg0 -> milliseconds: Float
 * Returns Result<Nil>
 */
Value sleep_milliseconds_function(VM *vm, const Value *args)
{
	const double milliseconds = TO_DOUBLE(args[0]);
	if (milliseconds < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Sleep duration cannot be negative.",
					  VALUE);
	}

#ifdef _WIN32
	Sleep((DWORD)milliseconds);
#else
	usleep((useconds_t)(milliseconds * 1000));
#endif

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

static time_t get_current_time(void)
{
	return time(NULL);
}

/**
 * Returns the current year
 * Returns Int
 */
Value year_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_year + 1900);
}

/**
 * Returns the current month (1-12)
 * Returns Int
 */
Value month_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mon + 1);
}

/**
 * Returns the current day of the month (1-31)
 * Returns Int
 */
Value day_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mday);
}

/**
 * Returns the current hour (0-23)
 * Returns Int
 */
Value hour_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_hour);
}

/**
 * Returns the current minute (0-59)
 * Returns Int
 */
Value minute_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_min);
}

/**
 * Returns the current second (0-59)
 * Returns Int
 */
Value second_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_sec);
}

/**
 * Returns the current day of the week (1=Monday, 7=Sunday)
 * Returns Int
 */
Value weekday_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	// 1 (Monday) - 7 (Sunday)
	const int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
	return INT_VAL(weekday);
}

/**
 * Returns the current day of the year (1-366)
 * Returns Int
 */
Value day_of_year_function_(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_yday + 1);
}
