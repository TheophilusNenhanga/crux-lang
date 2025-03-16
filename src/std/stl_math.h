#ifndef STL_MATH_H
#define STL_MATH_H

#include "../value.h"
#include "std.h"


ObjectResult* _pow(VM *vm, int argCount, Value *args);
ObjectResult* _sqrt(VM *vm, int argCount, Value *args);
ObjectResult* _abs(VM *vm, int argCount, Value *args);

// Trigonometric functions
ObjectResult* _sin(VM *vm, int argCount, Value *args);
ObjectResult* _cos(VM *vm, int argCount, Value *args);
ObjectResult* _tan(VM *vm, int argCount, Value *args);

// Inverse trigonometric functions
ObjectResult* _asin(VM *vm, int argCount, Value *args);
ObjectResult* _acos(VM *vm, int argCount, Value *args);
ObjectResult* _atan(VM *vm, int argCount, Value *args);

// Exponential and logarithmic functions
ObjectResult* _exp(VM *vm, int argCount, Value *args);
ObjectResult* _ln(VM *vm, int argCount, Value *args);
ObjectResult* _log10(VM *vm, int argCount, Value *args);

// Rounding functions
ObjectResult* _ceil(VM *vm, int argCount, Value *args);
ObjectResult* _floor(VM *vm, int argCount, Value *args);
ObjectResult* _round(VM *vm, int argCount, Value *args);

// Constants
Value _pi(VM *vm, int argCount, Value *args);
Value _e(VM *vm, int argCount, Value *args);
#endif //STL_MATH_H
