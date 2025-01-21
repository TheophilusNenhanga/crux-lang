#ifndef STL_MATH_H
#define STL_MATH_H

#include "../value.h"
#include "std.h"


NativeReturn _pow(VM *vm, int argCount, Value *args);
NativeReturn _sqrt(VM *vm, int argCount, Value *args);
NativeReturn _abs(VM *vm, int argCount, Value *args);

// Trigonometric functions
NativeReturn _sin(VM *vm, int argCount, Value *args);
NativeReturn _cos(VM *vm, int argCount, Value *args);
NativeReturn _tan(VM *vm, int argCount, Value *args);

// Inverse trigonometric functions
NativeReturn _asin(VM *vm, int argCount, Value *args);
NativeReturn _acos(VM *vm, int argCount, Value *args);
NativeReturn _atan(VM *vm, int argCount, Value *args);

// Exponential and logarithmic functions
NativeReturn _exp(VM *vm, int argCount, Value *args);
NativeReturn _ln(VM *vm, int argCount, Value *args);
NativeReturn _log10(VM *vm, int argCount, Value *args);

// Rounding functions
NativeReturn _ceil(VM *vm, int argCount, Value *args);
NativeReturn _floor(VM *vm, int argCount, Value *args);
NativeReturn _round(VM *vm, int argCount, Value *args);

// Constants
NativeReturn _pi(VM *vm, int argCount, Value *args);
NativeReturn _e(VM *vm, int argCount, Value *args);
#endif //STL_MATH_H
