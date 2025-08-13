#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
Value timeSecondsFunction_(VM *vm, int argCount, const Value *args);
Value timeMillisecondsFunction_(VM *vm, int argCount, const Value *args);

// Sleep functions
ObjectResult *sleepSecondsFunction(VM *vm, int argCount, const Value *args);
ObjectResult *sleepMillisecondsFunction(VM *vm, int argCount,
					const Value *args);

// Date/Time functions
Value yearFunction_(VM *vm, int argCount, const Value *args);
Value monthFunction_(VM *vm, int argCount, const Value *args);
Value dayFunction_(VM *vm, int argCount, const Value *args);
Value hourFunction_(VM *vm, int argCount, const Value *args);
Value minuteFunction_(VM *vm, int argCount, const Value *args);
Value secondFunction_(VM *vm, int argCount, const Value *args);
Value weekdayFunction_(VM *vm, int argCount, const Value *args);
Value dayOfYearFunction_(VM *vm, int argCount, const Value *args);

#endif // TIME_H
