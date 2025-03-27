#ifndef RANDOM_H
#define RANDOM_H

#include "../vm.h"
#include "../object.h"

ObjectResult* randomSeedMethod(VM* vm, int argCount, Value *args);
Value randomNextMethod(VM* vm, int argCount, Value *args);
Value randomInitFunction(VM* vm, int argCount, Value *args);
ObjectResult* randomIntMethod(VM* vm, int argCount, Value *args);
ObjectResult* randomDoubleMethod(VM* vm, int argCount, Value *args);
ObjectResult* randomBoolMethod(VM* vm, int argCount, Value *args);
ObjectResult* randomChoiceMethod(VM* vm, int argCount, Value *args);

#endif

