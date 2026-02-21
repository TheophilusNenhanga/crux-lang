#ifndef MATH_H
#define MATH_H

#include "value.h"

Value pow_function(VM *vm, const Value *args);
Value sqrt_function(VM *vm, const Value *args);
Value abs_function(VM *vm, const Value *args);

Value sin_function(VM *vm, const Value *args);
Value cos_function(VM *vm, const Value *args);
Value tan_function(VM *vm, const Value *args);

Value asin_function(VM *vm, const Value *args);
Value acos_function(VM *vm, const Value *args);
Value atan_function(VM *vm, const Value *args);

Value exp_function(VM *vm, const Value *args);
Value ln_function(VM *vm, const Value *args);
Value log10_function(VM *vm, const Value *args);

Value ceil_function(VM *vm, const Value *args);
Value floor_function(VM *vm, const Value *args);
Value round_function(VM *vm, const Value *args);

Value min_function(VM *vm, const Value *args);
Value max_function(VM *vm, const Value *args);

Value pi_function(VM *vm, const Value *args);
Value e_function(VM *vm, const Value *args);
Value nan_function(VM *vm, const Value *args);
Value inf_function(VM *vm, const Value *args);

#endif // MATH_H
