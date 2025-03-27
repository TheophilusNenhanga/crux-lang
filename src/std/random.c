#include "random.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/august/test-run-lightweight-random-number-generation

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

Value randomInitFunction(VM* vm, int argCount, Value *args) {
    return OBJECT_VAL(newRandom(vm));
}

// Returns a random integer in the range [min, max] inclusive
ObjectResult* randomIntMethod(VM* vm, int argCount, Value *args) {
    if (argCount != 3) {
        return stellaErr(vm, newError(vm, copyString(vm, "Expected 2 arguments: min and max", 35), RUNTIME, false));
    }

    Value min = args[1];
    Value max = args[2];
    
    if (!IS_NUMBER(min) || !IS_NUMBER(max)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Arguments must be numbers", 25), RUNTIME, false));
    }

    int minInt = (int)AS_NUMBER(min);
    int maxInt = (int)AS_NUMBER(max);

    if (minInt > maxInt) {
        return stellaErr(vm, newError(vm, copyString(vm, "Min must be less than or equal to max", 37), RUNTIME, false));
    }

    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    double r = getNext(random);
    int result = minInt + (int)(r * (maxInt - minInt + 1));
    
    return stellaOk(vm, NUMBER_VAL(result));
}

// Returns a random double in the range [min, max]
ObjectResult* randomDoubleMethod(VM* vm, int argCount, Value *args) {
    Value min = args[1];
    Value max = args[2];

    if (!IS_NUMBER(min)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <min> must be a number.", 33), RUNTIME, false));
    }

    if (!IS_NUMBER(max)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <max> must be a number.", 33), RUNTIME, false));
    }
    

    double minDouble = AS_NUMBER(min);
    double maxDouble = AS_NUMBER(max);

    if (minDouble > maxDouble) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <min> must be less than or equal to parameter <max>.", 62), RUNTIME, false));
    }

    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    double r = getNext(random);
    double result = minDouble + r * (maxDouble - minDouble);
    
    return stellaOk(vm, NUMBER_VAL(result));
}

// Returns true with probability p (0 <= p <= 1)
ObjectResult* randomBoolMethod(VM* vm, int argCount, Value *args) {
    Value p = args[1];
    if (!IS_NUMBER(p)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Argument must be a number", 25), RUNTIME, false));
    }

    double prob = AS_NUMBER(p);
    if (prob < 0 || prob > 1) {
        return stellaErr(vm, newError(vm, copyString(vm, "Probability must be between 0 and 1", 37), RUNTIME, false));
    }

    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    double r = getNext(random);
    
    return stellaOk(vm, BOOL_VAL(r < prob));
}

// Returns a random element from the array
ObjectResult* randomChoiceMethod(VM* vm, int argCount, Value *args) {
    Value array = args[1];
    if (!IS_STL_ARRAY(array)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Argument must be an array", 25), RUNTIME, false));
    }

    ObjectArray *arr = AS_STL_ARRAY(array);
    if (arr->size == 0) {
        return stellaErr(vm, newError(vm, copyString(vm, "Array cannot be empty", 20), RUNTIME, false));
    }

    ObjectRandom *random = AS_STL_RANDOM(args[0]);
    double r = getNext(random);
    int index = (int)(r * arr->size);
    
    return stellaOk(vm, arr->array[index]);
}


