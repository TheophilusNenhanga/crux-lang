#ifndef ERROR_H
#define ERROR_H

#include "../object.h"
#include "../value.h"

// std library functions
ObjectResult* errorNative(VM *vm, int argCount, Value *args);
ObjectResult* panicNative(VM *vm, int argCount, Value *args);
ObjectResult* assertNative(VM *vm, int argCount, Value *args);

// error type methods
ObjectResult* errorMessageMethod(VM *vm, int argCount, Value *args);
ObjectResult* errorTypeMethod(VM *vm, int argCount, Value *args);

ObjectResult* _err(VM *vm, int argCount, Value *args);
ObjectResult* _ok(VM *vm, int argCount, Value *args);

#endif // ERROR_H
