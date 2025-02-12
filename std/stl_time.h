#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
ObjectResult* _time_s(VM *vm, int argCount, Value *args);
ObjectResult* _time_ms(VM *vm, int argCount, Value *args);

// Sleep functions
ObjectResult* _sleep_s(VM *vm, int argCount, Value *args);
ObjectResult* _sleep_ms(VM *vm, int argCount, Value *args);

// Date/Time functions
ObjectResult* _year(VM *vm, int argCount, Value *args);
ObjectResult* _month(VM *vm, int argCount, Value *args);
ObjectResult* _day(VM *vm, int argCount, Value *args);
ObjectResult* _hour(VM *vm, int argCount, Value *args);
ObjectResult* _minute(VM *vm, int argCount, Value *args);
ObjectResult* _second(VM *vm, int argCount, Value *args);
ObjectResult* _weekday(VM *vm, int argCount, Value *args);
ObjectResult* _day_of_year(VM *vm, int argCount, Value *args);

#endif // TIME_H
