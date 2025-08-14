#include "random.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from
// https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/august/test-run-lightweight-random-number-generation

ObjectResult *random_seed_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	const Value seed = args[1];
	if (!IS_INT(seed)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Seed must be a number.", 22),
				 RUNTIME, false));
	}

	const uint64_t seedInt = (uint64_t)AS_INT(seed);

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	random->seed = seedInt;
	return new_ok_result(vm, NIL_VAL);
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

Value random_next_method(VM *vm __attribute__((unused)),
			 int arg_count __attribute__((unused)),
			 const Value *args)
{
	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	return FLOAT_VAL(get_next(random));
}

Value random_init_function(VM *vm, int arg_count __attribute__((unused)),
			   const Value *args __attribute__((unused)))
{
	return OBJECT_VAL(new_random(vm));
}

// Returns a random integer in the range [min, max] inclusive
ObjectResult *random_int_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	const Value min = args[1];
	const Value max = args[2];

	if (!IS_INT(min) || !IS_INT(max)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Arguments must be of type 'int'.",
					    32),
				 TYPE, false));
	}

	const int32_t minInt = AS_INT(min);
	const int32_t maxInt = AS_INT(max);

	if (minInt > maxInt) {
		return new_error_result(
			vm,
			new_error(
				vm,
				copy_string(
					vm,
					"Min must be less than or equal to max",
					37),
				RUNTIME, false));
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const uint64_t range = (uint64_t)maxInt - (uint64_t)minInt + 1;
	const int32_t result = minInt + (int32_t)(r * range);

	return new_ok_result(vm, INT_VAL(result));
}

// Returns a random double in the range [min, max]
ObjectResult *random_double_method(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	const Value min = args[1];
	const Value max = args[2];

	if (!IS_FLOAT(min) && !IS_INT(max)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Parameter <min> must be a number.",
					    33),
				 RUNTIME, false));
	}

	if (!IS_FLOAT(max) && !IS_INT(max)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Parameter <max> must be a number.",
					    33),
				 RUNTIME, false));
	}

	const double minDouble = IS_FLOAT(min) ? AS_FLOAT(min)
					       : (double)AS_INT(min);
	const double maxDouble = IS_FLOAT(max) ? AS_FLOAT(max)
					       : (double)AS_INT(max);

	if (minDouble > maxDouble) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Parameter <min> must be less "
						"than or equal to "
						"parameter <max>.",
						62),
				     RUNTIME, false));
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const double result = minDouble + r * (maxDouble - minDouble);

	return new_ok_result(vm, FLOAT_VAL(result));
}

// Returns true with probability p (0 <= p <= 1)
ObjectResult *random_bool_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	const Value p = args[1];
	if (!IS_FLOAT(p) && !IS_INT(p)) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Argument must be of type "
						"'int' | 'float'.",
						41),
				     RUNTIME, false));
	}

	const double prob = IS_FLOAT(p) ? AS_FLOAT(p) : (double)AS_INT(p);

	if (prob < 0 || prob > 1) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "Probability must be between 0 and 1",
					 37),
				 RUNTIME, false));
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);

	return new_ok_result(vm, BOOL_VAL(r < prob));
}

// Returns a random element from the array
ObjectResult *random_choice_method(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	const Value array = args[1];
	if (!IS_CRUX_ARRAY(array)) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Argument must be an array",
						25),
				     RUNTIME, false));
	}

	const ObjectArray *arr = AS_CRUX_ARRAY(array);
	if (arr->size == 0) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Array cannot be empty", 20),
				 RUNTIME, false));
	}

	ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
	const double r = get_next(random);
	const uint32_t index = (uint32_t)(r * arr->size);

	return new_ok_result(vm, arr->values[index]);
}
