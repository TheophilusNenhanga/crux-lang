#ifndef ERROR_H
#define ERROR_H

#include "../object.h"
#include "../value.h"

// std library functions
NativeReturn errorNative(int argCount, Value *args);
NativeReturn panicNative(int argCount, Value *args);
NativeReturn assertNative(int argCount, Value *args);

// error type methods
NativeReturn errorMessageMethod(int argCount, Value *args);
NativeReturn errorCreatorMethod(int argCount, Value *args);
NativeReturn errorTypeMethod(int argCount, Value *args);

#endif // ERROR_H
