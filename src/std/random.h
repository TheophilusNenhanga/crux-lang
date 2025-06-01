#ifndef RANDOM_H
#define RANDOM_H

#include "../object.h"
#include "../vm/vm.h"

ObjectResult *randomSeedMethod(VM *vm, int argCount, const Value *args);
ObjectResult *randomIntMethod(VM *vm, int argCount, const Value *args);
ObjectResult *randomDoubleMethod(VM *vm, int argCount, const Value *args);
ObjectResult *randomBoolMethod(VM *vm, int argCount, const Value *args);
ObjectResult *randomChoiceMethod(VM *vm, int argCount, const Value *args);

Value randomNextMethod(VM *vm, int argCount, const Value *args);
Value randomInitFunction(VM *vm, int argCount, const Value *args);
#endif // RANDOM_H
