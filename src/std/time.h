#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

// Current time functions
Value time_seconds_function_(VM *vm, int arg_count, const Value *args);
Value time_milliseconds_function_(VM *vm, int arg_count, const Value *args);

// Sleep functions
ObjectResult *sleep_seconds_function(VM *vm, int arg_count, const Value *args);
ObjectResult *sleep_milliseconds_function(VM *vm, int arg_count,
					  const Value *args);

// Date/Time functions
Value year_function_(VM *vm, int arg_count, const Value *args);
Value month_function_(VM *vm, int arg_count, const Value *args);
Value day_function_(VM *vm, int arg_count, const Value *args);
Value hour_function_(VM *vm, int arg_count, const Value *args);
Value minute_function_(VM *vm, int arg_count, const Value *args);
Value second_function_(VM *vm, int arg_count, const Value *args);
Value weekday_function_(VM *vm, int arg_count, const Value *args);
Value day_of_year_function_(VM *vm, int arg_count, const Value *args);

#endif // TIME_H
