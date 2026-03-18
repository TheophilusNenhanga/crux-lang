#include "stdlib/math.h"
#include <math.h>
#include "panic.h"
#include "value.h"

/**
 * Calculates the value of base raised to the power of exponent
 * arg0 -> base: Float
 * arg1 -> exponent: Float
 * Returns Float
 */
Value pow_function(VM *vm, const Value *args)
{
	(void)vm;
	const double base = TO_DOUBLE(args[0]);
	const double exponent = TO_DOUBLE(args[1]);

	return FLOAT_VAL(pow(base, exponent));
}

/**
 * Calculates the square root of a number. Returns an error for negative
 * numbers. arg0 -> number: Float Returns Result<Float>
 */
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

/**
 * Calculates the absolute value of a number
 * arg0 -> x: Float | Int
 * Returns Int or Float (same type as input)
 */
Value abs_function(VM *vm, const Value *args)
{
	(void)vm;
	return IS_INT(args[0]) ? INT_VAL(absolute_int(AS_INT(args[0])))
			       : FLOAT_VAL(absolute_float(AS_FLOAT(args[0])));
}

/**
 * Calculates the sine of an angle in radians
 * arg0 -> angle: Float
 * Returns Float
 */
Value sin_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(sin(TO_DOUBLE(args[0])));
}

/**
 * Calculates the cosine of an angle in radians
 * arg0 -> angle: Float
 * Returns Float
 */
Value cos_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(cos(TO_DOUBLE(args[0])));
}

/**
 * Calculates the tangent of an angle in radians
 * arg0 -> angle: Float
 * Returns Float
 */
Value tan_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(tan(TO_DOUBLE(args[0])));
}

/**
 * Calculates the arcsine (inverse sine) of a value. Argument must be between -1
 * and 1. arg0 -> value: Float Returns Result<Float>
 */
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

/**
 * Calculates the arccosine (inverse cosine) of a value. Argument must be
 * between -1 and 1. arg0 -> value: Float Returns Result<Float>
 */
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

/**
 * Calculates the arctangent (inverse tangent) of a value
 * arg0 -> value: Float
 * Returns Float
 */
Value atan_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(atan(TO_DOUBLE(args[0])));
}

/**
 * Calculates e (Euler's number) raised to the power of x
 * arg0 -> x: Float
 * Returns Float
 */
Value exp_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(exp(TO_DOUBLE(args[0])));
}

/**
 * Calculates the natural logarithm (base e) of a number. Argument must be
 * positive. arg0 -> number: Float Returns Result<Float>
 */
Value ln_function(VM *vm, const Value *args)
{
	const double number = TO_DOUBLE(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate natural logarithm "
					  "of non positive number.",
					  VALUE);
	}
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(log(number))));
}

/**
 * Calculates the base 10 logarithm of a number. Argument must be positive.
 * arg0 -> number: Float
 * Returns Result<Float>
 */
Value log10_function(VM *vm, const Value *args)
{
	const double number = TO_DOUBLE(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate base 10 logarithm "
					  "of non positive number.",
					  VALUE);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(log10(number))));
}

/**
 * Rounds a number up to the nearest integer
 * arg0 -> number: Float
 * Returns Float
 */
Value ceil_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(ceil(TO_DOUBLE(args[0])));
}

/**
 * Rounds a number down to the nearest integer
 * arg0 -> number: Float
 * Returns Float
 */
Value floor_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(floor(TO_DOUBLE(args[0])));
}

/**
 * Rounds a number to the nearest integer
 * arg0 -> number: Float
 * Returns Float
 */
Value round_function(VM *vm, const Value *args)
{
	(void)vm;
	return FLOAT_VAL(round(TO_DOUBLE(args[0])));
}

/**
 * Returns the mathematical constant pi (π)
 * Returns Float
 */
Value pi_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(M_PI);
}

/**
 * Returns the mathematical constant e (Euler's number)
 * Returns Float
 */
Value e_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(M_E);
}

/**
 * Returns NaN (Not a Number) - a special floating-point value
 * Returns Float
 */
Value nan_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(NAN);
}

/**
 * Returns positive infinity
 * Returns Float
 */
Value inf_function(VM *vm, const Value *args)
{
	(void)args;
	(void)vm;
	return FLOAT_VAL(INFINITY);
}

/**
 * Returns the smaller of two values
 * arg0 -> a: Float
 * arg1 -> b: Float
 * Returns Float
 */
Value min_function(VM *vm, const Value *args)
{
	(void)vm;
	const double a = TO_DOUBLE(args[0]);
	const double b = TO_DOUBLE(args[1]);
	return FLOAT_VAL(a < b ? a : b);
}

/**
 * Returns the larger of two values
 * arg0 -> a: Float
 * arg1 -> b: Float
 * Returns Float
 */
Value max_function(VM *vm, const Value *args)
{
	(void)vm;
	const double a = TO_DOUBLE(args[0]);
	const double b = TO_DOUBLE(args[1]);
	return FLOAT_VAL(a > b ? a : b);
}
