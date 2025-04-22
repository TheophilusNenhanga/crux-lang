#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
Value timeSecondsFunction_(VM *vm, int argCount, Value *args);
Value timeMillisecondsFunction_(VM *vm, int argCount, Value *args);

// Sleep functions
ObjectResult *sleepSecondsFunction(VM *vm, int argCount, Value *args);
ObjectResult *sleepMillisecondsFunction(VM *vm, int argCount, Value *args);

// Date/Time functions
Value yearFunction_(VM *vm, int argCount, Value *args);
Value monthFunction_(VM *vm, int argCount, Value *args);
Value dayFunction_(VM *vm, int argCount, Value *args);
Value hourFunction_(VM *vm, int argCount, Value *args);
Value minuteFunction_(VM *vm, int argCount, Value *args);
Value secondFunction_(VM *vm, int argCount, Value *args);
Value weekdayFunction_(VM *vm, int argCount, Value *args);
Value dayOfYearFunction_(VM *vm, int argCount, Value *args);

#endif // TIME_H
