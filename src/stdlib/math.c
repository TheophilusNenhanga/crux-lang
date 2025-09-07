#include "stdlib/math.h"
#include <math.h>
#include "panic.h"

static bool numberArgs(const Value *args, const int arg_count)
{
	for (int i = 0; i < arg_count; i++) {
		if (!IS_INT(args[i]) && !IS_FLOAT(args[i])) {
			return false;
		}
	}
	return true;
}

#define NUMBER_ERROR_MESSAGE "Arguments must be of type 'int' | 'float'."

ObjectResult *check_args(VM *vm, const Value *args, const int arg_count)
{
	if (!numberArgs(args, arg_count)) {
		return MAKE_GC_SAFE_ERROR(vm, NUMBER_ERROR_MESSAGE, TYPE);
	}
	return NULL;
}

ObjectResult *pow_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}

	const double base = AS_FLOAT(args[0]);
	const double exponent = AS_FLOAT(args[1]);

	return new_ok_result(vm, FLOAT_VAL(pow(base, exponent)));
}

ObjectResult *sqrt_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}

	const double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Cannot calculate square root of a negative number.",
			VALUE);
	}

	return new_ok_result(vm, FLOAT_VAL(sqrt(number)));
}

static int32_t absolute_value(const int32_t x)
{
	if (x < 0) {
		return -x;
	}
	return x;
}

ObjectResult *abs_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	if (IS_INT(args[0])) {
		return new_ok_result(vm,
				     INT_VAL(absolute_value(AS_INT(args[0]))));
	}
	return new_ok_result(vm, FLOAT_VAL(fabs(AS_FLOAT(args[0]))));
}

ObjectResult *sin_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(sin(AS_FLOAT(args[0]))));
}

ObjectResult *cos_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(cos(AS_FLOAT(args[0]))));
}

ObjectResult *tan_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(tan(AS_FLOAT(args[0]))));
}

ObjectResult *asin_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}

	const double num = AS_FLOAT(args[0]);
	if (num < -1 || num > 1) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Argument must be between -1 and 1.",
					  VALUE);
	}

	return new_ok_result(vm, FLOAT_VAL(asin(num)));
}

ObjectResult *acos_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	const double num = AS_FLOAT(args[0]);
	if (num < -1 || num > 1) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Argument must be between -1 and 1.",
					  VALUE);
	}
	return new_ok_result(vm, FLOAT_VAL(acos(num)));
}

ObjectResult *atan_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(atan(AS_FLOAT(args[0]))));
}

ObjectResult *exp_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(exp(AS_FLOAT(args[0]))));
}

ObjectResult *ln_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}

	const double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate natural logarithm "
					  "of non positive number.",
					  VALUE);
	}
	return new_ok_result(vm, FLOAT_VAL(log(AS_FLOAT(args[0]))));
}

ObjectResult *log10_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}

	const double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot calculate base 10 logarithm "
					  "of non positive number.",
					  VALUE);
	}

	return new_ok_result(vm, FLOAT_VAL(log10(AS_FLOAT(args[0]))));
}

ObjectResult *ceil_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(ceil(AS_FLOAT(args[0]))));
}

ObjectResult *floor_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(floor(AS_FLOAT(args[0]))));
}

ObjectResult *round_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	return new_ok_result(vm, FLOAT_VAL(round(AS_FLOAT(args[0]))));
}

Value pi_function(VM *vm __attribute__((unused)),
		  int arg_count __attribute__((unused)),
		  const Value *args __attribute__((unused)))
{
	return FLOAT_VAL(3.14159265358979323846);
}

Value e_function(VM *vm __attribute__((unused)),
		 int arg_count __attribute__((unused)),
		 const Value *args __attribute__((unused)))
{
	return FLOAT_VAL(2.71828182845904523536);
}

ObjectResult *min_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	const double a = IS_FLOAT(args[0]) ? AS_FLOAT(args[0])
					   : (double)AS_INT(args[0]);
	const double b = IS_FLOAT(args[1]) ? AS_FLOAT(args[1])
					   : (double)AS_INT(args[1]);
	return a < b ? new_ok_result(vm, args[0]) : new_ok_result(vm, args[1]);
}

ObjectResult *max_function(VM *vm, const int arg_count, const Value *args)
{
	ObjectResult *res;
	if ((res = check_args(vm, args, arg_count)) != NULL) {
		return res;
	}
	const double a = IS_FLOAT(args[0]) ? AS_FLOAT(args[0])
					   : (double)AS_INT(args[0]);
	const double b = IS_FLOAT(args[1]) ? AS_FLOAT(args[1])
					   : (double)AS_INT(args[1]);
	return a > b ? new_ok_result(vm, args[0]) : new_ok_result(vm, args[1]);
}
