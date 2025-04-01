#ifndef ERROR_H
#define ERROR_H

#include "../object.h"
#include "../value.h"

ObjectResult* errorFunction(VM *vm, int argCount, Value *args);
ObjectResult* panicFunction(VM *vm, int argCount, Value *args);
ObjectResult* assertFunction(VM *vm, int argCount, Value *args);
ObjectResult* errorMessageMethod(VM *vm, int argCount, Value *args);
ObjectResult* errorTypeMethod(VM *vm, int argCount, Value *args);

ObjectResult* errFunction(VM *vm, int argCount, Value *args);
ObjectResult* okFunction(VM *vm, int argCount, Value *args);

#endif // ERROR_H
