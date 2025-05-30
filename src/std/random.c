#include "random.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from
// https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/august/test-run-lightweight-random-number-generation

ObjectResult *randomSeedMethod(VM *vm, int argCount, const Value *args) {
  const Value seed = args[1];
  if (!IS_INT(seed)) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Seed must be a number.", 22), RUNTIME,
                     false));
  }

  const uint64_t seedInt = (uint64_t)AS_INT(seed);

  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  random->seed = seedInt;
  return newOkResult(vm, NIL_VAL);
}

int next(uint64_t *seed, const int bits) {
  *seed = (*seed * A + C) & ((1ULL << 48) - 1);
  return (int)(*seed >> (48 - bits));
}

double getNext(ObjectRandom *random) {
  const int bits26 = next(&random->seed, 26);
  const int bits27 = next(&random->seed, 27);

  const double result =
      (((uint64_t)bits26 << 27) + bits27) / (double)(1ULL << 53);
  return result;
}

Value randomNextMethod(VM *vm, int argCount, const Value *args) {
  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  return FLOAT_VAL(getNext(random));
}

Value randomInitFunction(VM *vm, int argCount, const Value *args) {
  return OBJECT_VAL(newRandom(vm));
}

// Returns a random integer in the range [min, max] inclusive
ObjectResult *randomIntMethod(VM *vm, int argCount, const Value *args) {
  const Value min = args[1];
  const Value max = args[2];

  if (!IS_INT(min) || !IS_INT(max)) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Arguments must be of type 'int'.", 32),
                     TYPE, false));
  }

  const int32_t minInt = AS_INT(min);
  const int32_t maxInt = AS_INT(max);

  if (minInt > maxInt) {
    return newErrorResult(
        vm, newError(
                vm, copyString(vm, "Min must be less than or equal to max", 37),
                RUNTIME, false));
  }

  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  const double r = getNext(random);
  const uint64_t range = (uint64_t)maxInt - (uint64_t)minInt + 1;
  const int32_t result = minInt + (int32_t)(r * range);

  return newOkResult(vm, INT_VAL(result));
}

// Returns a random double in the range [min, max]
ObjectResult *randomDoubleMethod(VM *vm, int argCount, const Value *args) {
  const Value min = args[1];
  const Value max = args[2];

  if (!IS_FLOAT(min) && !IS_INT(max)) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Parameter <min> must be a number.", 33),
                 RUNTIME, false));
  }

  if (!IS_FLOAT(max) && !IS_INT(max)) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Parameter <max> must be a number.", 33),
                 RUNTIME, false));
  }

  const double minDouble = IS_FLOAT(min) ? AS_FLOAT(min) : (double)AS_INT(min);
  const double maxDouble = IS_FLOAT(max) ? AS_FLOAT(max) : (double)AS_INT(max);

  if (minDouble > maxDouble) {
    return newErrorResult(
        vm, newError(vm,
                     copyString(vm,
                                "Parameter <min> must be less than or equal to "
                                "parameter <max>.",
                                62),
                     RUNTIME, false));
  }

  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  const double r = getNext(random);
  const double result = minDouble + r * (maxDouble - minDouble);

  return newOkResult(vm, FLOAT_VAL(result));
}

// Returns true with probability p (0 <= p <= 1)
ObjectResult *randomBoolMethod(VM *vm, int argCount, const Value *args) {
  const Value p = args[1];
  if (!IS_FLOAT(p) && !IS_INT(p)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 41),
            RUNTIME, false));
  }

  const double prob = IS_FLOAT(p) ? AS_FLOAT(p) : (double)AS_INT(p);

  if (prob < 0 || prob > 1) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Probability must be between 0 and 1", 37),
                 RUNTIME, false));
  }

  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  const double r = getNext(random);

  return newOkResult(vm, BOOL_VAL(r < prob));
}

// Returns a random element from the array
ObjectResult *randomChoiceMethod(VM *vm, int argCount, const Value *args) {
  const Value array = args[1];
  if (!IS_CRUX_ARRAY(array)) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Argument must be an array", 25),
                     RUNTIME, false));
  }

  const ObjectArray *arr = AS_CRUX_ARRAY(array);
  if (arr->size == 0) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Array cannot be empty", 20), RUNTIME,
                     false));
  }

  ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
  const double r = getNext(random);
  const uint32_t index = (uint32_t)(r * arr->size);

  return newOkResult(vm, arr->array[index]);
}
