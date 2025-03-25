#ifndef RANDOM_H
#define RANDOM_H

#include "../vm.h"
#include "../object.h"

ObjectResult* randomSeedMethod(VM* vm, int argCount, Value *args);
Value randomNextMethod(VM* vm, int argCount, Value *args);
Value RandomInitFunction(VM* vm, int argCount, Value *args);

#endif

