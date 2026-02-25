#include "stdlib/random.h"
#include "panic.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from
// https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/august/test-run-lightweight-random-number-generation

/**
 * Seeds the random number generator with a specific value
 * arg0 -> random: Random
 * arg1 -> seed: Int
 * Returns Nil
 */
Value random_seed_method(VM *vm, const Value *args)
{
	(void)vm;
	const uint64_t seedInt = (uint64_t)AS_INT(args[1]);
	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	random->seed = seedInt;
	return NIL_VAL;
}

int next(uint64_t *seed, const int bits)
{
	*seed = (*seed * A + C) & ((1ULL << 48) - 1);
	return (int)(*seed >> (48 - bits));
}

double get_next(ObjectRandom *random)
{
	const int bits26 = next(&random->seed, 26);
	const int bits27 = next(&random->seed, 27);

	const double result = (((uint64_t)bits26 << 27) + bits27) /
			      (double)(1ULL << 53);
	return result;
}

/**
 * Returns the next random float in the range [0, 1)
 * arg0 -> random: Random
 * Returns Float
 */
Value random_next_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	return FLOAT_VAL(get_next(random));
}

/**
 * Creates a new Random number generator instance
 * Returns Random
 */
Value random_init_function(VM *vm, const Value *args)
{
	(void)args;
	ObjectRandom *random_obj = new_random(vm);
	return OBJECT_VAL(random_obj);
}

/**
 * Returns a random integer in the range [min, max] inclusive
 * arg0 -> random: Random
 * arg1 -> min: Int
 * arg2 -> max: Int
 * Returns Result<Int>
 */
Value random_int_method(VM *vm, const Value *args)
{
	const Value min = args[1];
	const Value max = args[2];

	const int32_t minInt = AS_INT(min);
	const int32_t maxInt = AS_INT(max);

	if (minInt > maxInt) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<min> must be less than or equal to <max>",
			RUNTIME);
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const uint64_t range = (uint64_t)maxInt - (uint64_t)minInt + 1;
	const int32_t result = minInt + (int32_t)(r * range);

	return OBJECT_VAL(new_ok_result(vm, INT_VAL(result)));
}

/**
 * Returns a random float in the range [min, max]
 * arg0 -> random: Random
 * arg1 -> min: Float
 * arg2 -> max: Float
 * Returns Result<Float>
 */
Value random_float_method(VM *vm, const Value *args)
{
	const Value min = args[1];
	const Value max = args[2];

	const double minDouble = IS_FLOAT(min) ? AS_FLOAT(min)
					       : (double)AS_INT(min);
	const double maxDouble = IS_FLOAT(max) ? AS_FLOAT(max)
					       : (double)AS_INT(max);

	if (minDouble > maxDouble) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Parameter <min> must be less than "
					  "or equal to parameter <max>.",
					  RUNTIME);
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const double result = minDouble + r * (maxDouble - minDouble);

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(result)));
}

// Returns true with probability p (0 <= p <= 1)
/**
 * Returns true with probability p (0 <= p <= 1)
 * arg0 -> random: Random
 * arg1 -> probability: Float
 * Returns Result<Bool>
 */
Value random_bool_method(VM *vm, const Value *args)
{
	const double prob = TO_DOUBLE(args[1]);

	if (prob < 0 || prob > 1) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Probability must be between 0 and 1",
					  RUNTIME);
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);

	return OBJECT_VAL(new_ok_result(vm, BOOL_VAL(r < prob)));
}

// Returns a random element from the array
/**
 * Returns a random element from an array
 * arg0 -> random: Random
 * arg1 -> array: Array
 * Returns Result<Any>
 */
Value random_choice_method(VM *vm, const Value *args)
{
	const ObjectArray *arr = AS_CRUX_ARRAY(args[1]);
	if (arr->size == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Array cannot be empty", RUNTIME);
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const uint32_t index = (uint32_t)(r * arr->size);

	return OBJECT_VAL(new_ok_result(vm, arr->values[index]));
}
