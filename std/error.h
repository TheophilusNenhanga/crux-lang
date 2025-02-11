#ifndef ERROR_H
#define ERROR_H

#include "../object.h"
#include "../value.h"

// std library functions
NativeReturn errorNative(VM *vm, int argCount, Value *args);
NativeReturn panicNative(VM *vm, int argCount, Value *args);
NativeReturn assertNative(VM *vm, int argCount, Value *args);

// error type methods
NativeReturn errorMessageMethod(VM *vm, int argCount, Value *args);
NativeReturn errorTypeMethod(VM *vm, int argCount, Value *args);

Value _err(VM *vm, int argCount, Value *args);
Value _ok(VM *vm, int argCount, Value *args);

#endif // ERROR_H
