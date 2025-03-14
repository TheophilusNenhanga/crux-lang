#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
Value _time_s(VM *vm, int argCount, Value *args);
Value _time_ms(VM *vm, int argCount, Value *args);

// Sleep functions
ObjectResult* _sleep_s(VM *vm, int argCount, Value *args);
ObjectResult* _sleep_ms(VM *vm, int argCount, Value *args);

// Date/Time functions
Value _year(VM *vm, int argCount, Value *args);
Value _month(VM *vm, int argCount, Value *args);
Value _day(VM *vm, int argCount, Value *args);
Value _hour(VM *vm, int argCount, Value *args);
Value _minute(VM *vm, int argCount, Value *args);
Value _second(VM *vm, int argCount, Value *args);
Value _weekday(VM *vm, int argCount, Value *args);
Value _day_of_year(VM *vm, int argCount, Value *args);

#endif // TIME_H
