#ifndef MATH_H
#define MATH_H

#include "../value.h"
#include "std.h"

ObjectResult *pow_function(VM *vm, int arg_count, const Value *args);
ObjectResult *sqrt_function(VM *vm, int arg_count, const Value *args);
ObjectResult *abs_function(VM *vm, int arg_count, const Value *args);

ObjectResult *sin_function(VM *vm, int arg_count, const Value *args);
ObjectResult *cos_function(VM *vm, int arg_count, const Value *args);
ObjectResult *tan_function(VM *vm, int arg_count, const Value *args);

ObjectResult *asin_function(VM *vm, int arg_count, const Value *args);
ObjectResult *acos_function(VM *vm, int arg_count, const Value *args);
ObjectResult *atan_function(VM *vm, int arg_count, const Value *args);

ObjectResult *exp_function(VM *vm, int arg_count, const Value *args);
ObjectResult *ln_function(VM *vm, int arg_count, const Value *args);
ObjectResult *log10_function(VM *vm, int arg_count, const Value *args);

ObjectResult *ceil_function(VM *vm, int arg_count, const Value *args);
ObjectResult *floor_function(VM *vm, int arg_count, const Value *args);
ObjectResult *round_function(VM *vm, int arg_count, const Value *args);

ObjectResult *min_function(VM *vm, int arg_count, const Value *args);
ObjectResult *max_function(VM *vm, int arg_count, const Value *args);

Value pi_function(VM *vm, int arg_count, const Value *args);
Value e_function(VM *vm, int arg_count, const Value *args);

#endif // MATH_H
