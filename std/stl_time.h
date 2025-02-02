#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
NativeReturn _time_s(VM *vm, int argCount, Value *args);
NativeReturn _time_ms(VM *vm, int argCount, Value *args);

// Sleep functions
NativeReturn _sleep_s(VM *vm, int argCount, Value *args);
NativeReturn _sleep_ms(VM *vm, int argCount, Value *args);

// Date/Time functions
NativeReturn _year(VM *vm, int argCount, Value *args);
NativeReturn _month(VM *vm, int argCount, Value *args);
NativeReturn _day(VM *vm, int argCount, Value *args);
NativeReturn _hour(VM *vm, int argCount, Value *args);
NativeReturn _minute(VM *vm, int argCount, Value *args);
NativeReturn _second(VM *vm, int argCount, Value *args);
NativeReturn _weekday(VM *vm, int argCount, Value *args);
NativeReturn _day_of_year(VM *vm, int argCount, Value *args);

#endif // TIME_H
