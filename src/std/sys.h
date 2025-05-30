#ifndef SYS_H
#define SYS_H

#include "../object.h"

ObjectResult *argsFunction(VM *vm, int argCount, const Value *args);
Value platformFunction(VM *vm, int argCount, const Value *args);
Value archFunction(VM *vm, int argCount, const Value *args);
Value pidFunction(VM *vm, int argCount, const Value *args);
ObjectResult *getEnvFunction(VM *vm, int argCount, const Value *args);
ObjectResult *sleepFunction(VM *vm, int argCount,const  Value *args);
Value exitFunction(VM *vm, int argCount, const Value *args);

#endif // SYS_H
