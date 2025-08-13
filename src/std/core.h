#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../object.h"
#include "../value.h"

ObjectResult *lengthFunction(VM *vm, int argCount, const Value *args);
ObjectResult *intFunction(VM *vm, int argCount, const Value *args);
ObjectResult *floatFunction(VM *vm, int argCount, const Value *args);
ObjectResult *stringFunction(VM *vm, int argCount, const Value *args);
ObjectResult *arrayFunction(VM *vm, int argCount, const Value *args);
ObjectResult *tableFunction(VM *vm, int argCount, const Value *args);

Value lengthFunction_(VM *vm, int argCount, const Value *args);
Value intFunction_(VM *vm, int argCount, const Value *args);
Value floatFunction_(VM *vm, int argCount, const Value *args);
Value stringFunction_(VM *vm, int argCount, const Value *args);
Value arrayFunction_(VM *vm, int argCount, const Value *args);
Value tableFunction_(VM *vm, int argCount, const Value *args);

#endif
