#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "stl_time.h"
#include "../memory.h"

Value _time_s(VM* vm, int argCount, Value* args) {
	return NUMBER_VAL((double)time(NULL));
}

Value _time_ms(VM *vm, int argCount, Value *args) {
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	uint64_t ms = (uli.QuadPart - 116444736000000000ULL) / 10000;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
	return NUMBER_VAL((double)ms);
}

ObjectResult* _sleep_s(VM *vm, int argCount, Value *args) {
		if (!IS_NUMBER(args[0])) {
			return stellaErr(vm, newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, false));
		}

    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) {
        return stellaErr(vm, newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));
    }

    #ifdef _WIN32
        Sleep((DWORD)(seconds * 1000));
    #else
        sleep((unsigned int)seconds);
    #endif

    return stellaOk(vm, NIL_VAL);
}

ObjectResult* _sleep_ms(VM *vm, int argCount, Value *args) {
	if (!IS_NUMBER(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, false));
	}

    double milliseconds = AS_NUMBER(args[0]);
    if (milliseconds < 0) {
        return stellaErr(vm, newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));

    }

    #ifdef _WIN32
        Sleep((DWORD)milliseconds);
    #else
        usleep((useconds_t)(milliseconds * 1000));
    #endif

    return stellaOk(vm, NIL_VAL);
}

static time_t getCurrentTime() {
    return time(NULL);
}

Value _year(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_year + 1900);
}

Value _month(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_mon + 1);
}

Value _day(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_mday);
}

Value _hour(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_hour);
}

Value _minute(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_min);
}

Value _second(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_sec);
}

Value _weekday(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    // 1 (Monday) - 7 (Sunday)
    int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
    return NUMBER_VAL(weekday);
}

Value _day_of_year(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    return NUMBER_VAL(timeInfo->tm_yday + 1);
}
