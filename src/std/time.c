#include "../panic.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../memory.h"
#include "time.h"

Value time_seconds_function_(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args __attribute__((unused)))
{
	return FLOAT_VAL((double)time(NULL));
}

Value time_milliseconds_function_(VM *vm __attribute__((unused)),
				  int arg_count __attribute__((unused)),
				  const Value *args __attribute__((unused)))
{
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
	uint64_t ms = (uint64_t)ts.tv_sec * 1000 +
		      (uint64_t)ts.tv_nsec / 1000000;
#endif
	return FLOAT_VAL((double)ms);
}

ObjectResult *sleep_seconds_function(VM *vm,
				     int arg_count __attribute__((unused)),
				     const Value *args)
{
	if (!IS_INT(args[0]) || IS_FLOAT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Parameter <duration> must be of type 'int' | 'float'.", TYPE);
	}

	const double seconds = AS_FLOAT(args[0]);
	if (seconds < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Sleep duration cannot be negative.", VALUE);
	}

#ifdef _WIN32
	Sleep((DWORD)(seconds * 1000));
#else
	sleep((unsigned int)seconds);
#endif
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *sleep_milliseconds_function(VM *vm,
					  int arg_count __attribute__((unused)),
					  const Value *args)
{
	if (!IS_INT(args[0]) || IS_FLOAT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Parameter <duration> must be of type 'int' | 'float'.", TYPE);
	}

	const double milliseconds = AS_FLOAT(args[0]);
	if (milliseconds < 0) {
		return MAKE_GC_SAFE_ERROR(vm , "Sleep duration cannot be negative.", VALUE);
	}

#ifdef _WIN32
	Sleep((DWORD)milliseconds);
#else
	usleep((useconds_t)(milliseconds * 1000));
#endif

	return new_ok_result(vm, NIL_VAL);
}

static time_t get_current_time(void)
{
	return time(NULL);
}

Value year_function_(VM *vm __attribute__((unused)),
		     int arg_count __attribute__((unused)),
		     const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_year + 1900);
}

Value month_function_(VM *vm __attribute__((unused)),
		      int arg_count __attribute__((unused)),
		      const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mon + 1);
}

Value day_function_(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)),
		    const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mday);
}

Value hour_function_(VM *vm __attribute__((unused)),
		     int arg_count __attribute__((unused)),
		     const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_hour);
}

Value minute_function_(VM *vm __attribute__((unused)),
		       int arg_count __attribute__((unused)),
		       const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_min);
}

Value second_function_(VM *vm __attribute__((unused)),
		       int arg_count __attribute__((unused)),
		       const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_sec);
}

Value weekday_function_(VM *vm __attribute__((unused)),
			int arg_count __attribute__((unused)),
			const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	// 1 (Monday) - 7 (Sunday)
	const int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
	return INT_VAL(weekday);
}

Value day_of_year_function_(VM *vm __attribute__((unused)),
			    int arg_count __attribute__((unused)),
			    const Value *args __attribute__((unused)))
{
	const time_t t = get_current_time();
	const struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_yday + 1);
}
