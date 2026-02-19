#include "stdlib/math.h"
#include <math.h>
#include "panic.h"
#include "value.h"

Value pow_function(VM *vm, const Value *args)
{
	(void) vm;
	const double base = TO_DOUBLE(args[0]);
	const double exponent = TO_DOUBLE(args[1]);

	return FLOAT_VAL(pow(base, exponent));
}

Value sqrt_function(VM *vm, const Value *args)
{
	const double number = TO_DOUBLE(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Cannot calculate square root of a negative number.",
			VALUE);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(sqrt(number))));
}

static int32_t absolute_int(const int32_t x)
{
	if (x < 0) {
		return -x;
	}
	return x;
}

static double absolute_float(const double x)
{
	if (x < 0) {
		return -x;
	}
	return x;
}

Value abs_function(VM *vm, const Value *args)
{
	(void) vm;
	return IS_INT(args[0])
	? INT_VAL(absolute_int(AS_INT(args[0])))
	: FLOAT_VAL(absolute_float(AS_FLOAT(args[0])));
}

Value sin_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(sin(TO_DOUBLE(args[0])));
}

Value cos_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(cos(TO_DOUBLE(args[0])));
}

Value tan_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(tan(TO_DOUBLE(args[0])));
}

Value asin_function(VM *vm, const Value *args)
{
	const double num = TO_DOUBLE(args[0]);
	if (num < -1 || num > 1) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Argument must be between -1 and 1.",
					  VALUE);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(asin(num))));
}

Value acos_function(VM *vm, const Value *args)
{
	const double num = TO_DOUBLE(args[0]);
	if (num < -1 || num > 1) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Argument must be between -1 and 1.",
					  VALUE);
	}
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(acos(num))));
}

Value atan_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(atan(TO_DOUBLE(args[0])));
}

Value exp_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(exp(TO_DOUBLE(args[0])));
}

Value ln_function(VM *vm, const Value *args)
{
	const double number = TO_DOUBLE(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate natural logarithm "
					  "of non positive number.",
					  VALUE);
	}
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(log(AS_FLOAT(args[0])))));
}

Value log10_function(VM *vm, const Value *args)
{
	const double number = TO_DOUBLE(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate base 10 logarithm "
					  "of non positive number.",
					  VALUE);
	}

	return OBJECT_VAL(
		new_ok_result(vm, FLOAT_VAL(log10(AS_FLOAT(args[0])))));
}

Value ceil_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(ceil(TO_DOUBLE(args[0])));
}

Value floor_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(floor(TO_DOUBLE(args[0])));
}

Value round_function(VM *vm, const Value *args)
{
	(void) vm;
	return FLOAT_VAL(round(TO_DOUBLE(args[0])));
}

Value pi_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(M_PI);
}

Value e_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(M_E);
}

Value nan_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(NAN);
}

Value inf_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(INFINITY);
}

Value min_function(VM *vm, const Value *args)
{
	(void) vm;
	const double a = TO_DOUBLE(args[0]);
	const double b = TO_DOUBLE(args[1]);
	return FLOAT_VAL(a < b ? args[0]
				: args[1]);
}

Value max_function(VM *vm, const Value *args)
{
	(void) vm;
	const double a = TO_DOUBLE(args[0]);
	const double b = TO_DOUBLE(args[1]);
	return FLOAT_VAL(a > b ? args[0]
				: args[1]);
}
