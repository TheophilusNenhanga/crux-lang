#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "stl_time.h"
#include "../memory.h"

NativeReturn _time_s(VM* vm, int argCount, Value* args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NUMBER_VAL((double)time(NULL));
	return nativeReturn;
}

NativeReturn _time_ms(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
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
	nativeReturn.values[0] = NUMBER_VAL((double)ms);
	return nativeReturn;
}

NativeReturn _sleep_s(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 2);

		if (!IS_NUMBER(args[0])) {
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, STELLA));
			return nativeReturn;
		}

    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, STELLA));
        return nativeReturn;
    }

    #ifdef _WIN32
        Sleep((DWORD)(seconds * 1000));
    #else
        sleep((unsigned int)seconds);
    #endif

    nativeReturn.values[0] = NIL_VAL;
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

NativeReturn _sleep_ms(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_NUMBER(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, STELLA));
		return nativeReturn;
	}

    double milliseconds = AS_NUMBER(args[0]);
    if (milliseconds < 0) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, STELLA));
        return nativeReturn;
    }

    #ifdef _WIN32
        Sleep((DWORD)milliseconds);
    #else
        usleep((useconds_t)(milliseconds * 1000));
    #endif

    nativeReturn.values[0] = NIL_VAL;
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

static time_t getCurrentTime() {
    return time(NULL);
}

NativeReturn _year(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_year + 1900);
    return nativeReturn;
}

NativeReturn _month(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_mon + 1);
    return nativeReturn;
}

NativeReturn _day(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_mday);
    return nativeReturn;
}

NativeReturn _hour(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_hour);
    return nativeReturn;
}

NativeReturn _minute(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_min);
    return nativeReturn;
}

NativeReturn _second(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_sec);
    return nativeReturn;
}

NativeReturn _weekday(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    // 1 (Monday) - 7 (Sunday)
    int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
    nativeReturn.values[0] = NUMBER_VAL(weekday);
    return nativeReturn;
}

NativeReturn _day_of_year(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 1);
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_yday + 1);
    return nativeReturn;
}
