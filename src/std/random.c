#include "random.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from 

ObjectResult* randomSeedMethod(VM* vm, int argCount, Value *args) {
    Value seed = args[1];
    if (!IS_NUMBER(seed)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Seed must be a number.", 22), RUNTIME, false));
    }

    double seedDouble = AS_NUMBER(seed);
    uint64_t seedInt = (uint64_t) seedDouble;

    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    random->seed = seedInt;
    return stellaOk(vm, NIL_VAL);
}

int next(uint64_t* seed, int bits) {
    *seed = (*seed * A + C) & ((1ULL << 48) - 1);
    return (int)(*seed >> (48 - bits));
}

double getNext(ObjectRandom *random) {
    int bits26 = next(&random->seed, 26);
    int bits27 = next(&random->seed, 27);
    
    double result = (((uint64_t)bits26 << 27) + bits27) / (double)(1ULL << 53);
    return result;
}

Value randomNextMethod(VM* vm, int argCount, Value *args) {
    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    return NUMBER_VAL(getNext(random));
}   

Value RandomInitFunction(VM* vm, int argCount, Value *args) {
    return OBJECT_VAL(newRandom(vm));
}


