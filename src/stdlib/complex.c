#include <math.h>

#include "object.h"
#include "panic.h"
#include "stdlib/complex.h"

/**
 * Returns the real part of a complex number
 * arg0 -> complex: Complex
 * Returns Float
 */
Value complex_real_method(VM *vm, const Value *args)
{
	(void)vm;

	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->real);
}

/**
 * Returns the imaginary part of a complex number
 * arg0 -> complex: Complex
 * Returns Float
 */
Value complex_imag_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->imag);
}

/**
 * Creates a new complex number with real and imaginary parts
 * arg0 -> real: Float
 * arg1 -> imag: Float
 * Returns Complex
 */
Value new_complex_function(VM *vm, const Value *args)
{
	const double real = TO_DOUBLE(args[0]);
	const double imag = TO_DOUBLE(args[1]);
	ObjectComplex *comp = new_complex_number(vm, real, imag);
	return OBJECT_VAL(comp);
}

Value complex_add_value(VM *vm, const ObjectComplex *lhs, const ObjectComplex *rhs)
{
	(void)vm;
	return OBJECT_VAL(new_complex_number(vm, lhs->real + rhs->real, lhs->imag + rhs->imag));
}

Value complex_subtract_value(VM *vm, const ObjectComplex *lhs, const ObjectComplex *rhs)
{
	(void)vm;
	return OBJECT_VAL(new_complex_number(vm, lhs->real - rhs->real, lhs->imag - rhs->imag));
}

Value complex_multiply_value(VM *vm, const ObjectComplex *lhs, const ObjectComplex *rhs)
{
	const double real = lhs->real * rhs->real - lhs->imag * rhs->imag;
	const double imag = lhs->real * rhs->imag + lhs->imag * rhs->real;
	return OBJECT_VAL(new_complex_number(vm, real, imag));
}

Value complex_divide_value(VM *vm, const ObjectComplex *lhs, const ObjectComplex *rhs)
{
	const double c = rhs->real;
	const double d = rhs->imag;
	const double denom = c * c + d * d;
	return OBJECT_VAL(new_complex_number(vm, (lhs->real * c + lhs->imag * d) / denom,
										 (lhs->imag * c - lhs->real * d) / denom));
}

Value complex_scalar_multiply_value(VM *vm, const ObjectComplex *value, const double scalar)
{
	return OBJECT_VAL(new_complex_number(vm, value->real * scalar, value->imag * scalar));
}

Value complex_scalar_divide_value(VM *vm, const ObjectComplex *value, const double scalar)
{
	if (fabs(scalar) < 1e-10) {
		return MAKE_GC_SAFE_ERROR(vm, "Division by zero.", MATH);
	}
	return OBJECT_VAL(new_complex_number(vm, value->real / scalar, value->imag / scalar));
}

/**
 * Adds two complex numbers together
 * arg0 -> complex: Complex
 * arg1 -> other: Complex
 * Returns Complex
 */
Value add_complex_number_method(VM *vm, const Value *args)
{
	return complex_add_value(vm, AS_CRUX_COMPLEX(args[0]), AS_CRUX_COMPLEX(args[1]));
}

/**
 * Subtracts another complex number from this one
 * arg0 -> complex: Complex
 * arg1 -> other: Complex
 * Returns Complex
 */
Value sub_complex_number_method(VM *vm, const Value *args)
{
	return complex_subtract_value(vm, AS_CRUX_COMPLEX(args[0]), AS_CRUX_COMPLEX(args[1]));
}

/**
 * Multiplies two complex numbers together
 * arg0 -> complex: Complex
 * arg1 -> other: Complex
 * Returns Complex
 */
Value mul_complex_number_method(VM *vm, const Value *args)
{
	return complex_multiply_value(vm, AS_CRUX_COMPLEX(args[0]), AS_CRUX_COMPLEX(args[1]));
}

/**
 * Divides this complex number by another
 * arg0 -> complex: Complex
 * arg1 -> other: Complex
 * Returns Complex
 */
Value div_complex_number_method(VM *vm, const Value *args)
{
	return complex_divide_value(vm, AS_CRUX_COMPLEX(args[0]), AS_CRUX_COMPLEX(args[1]));
}

/**
 * Scales a complex number by a scalar value
 * arg0 -> complex: Complex
 * arg1 -> scalar: Float
 * Returns Complex
 */
Value scale_complex_number_method(VM *vm, const Value *args)
{
	return complex_scalar_multiply_value(vm, AS_CRUX_COMPLEX(args[0]), TO_DOUBLE(args[1]));
}

/**
 * Returns the magnitude (modulus) of a complex number
 * arg0 -> complex: Complex
 * Returns Float
 */
Value magnitude_complex_number_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = sqrt(a * a + b * b);
	return FLOAT_VAL(magnitude);
}

/**
 * Returns the squared magnitude of a complex number
 * arg0 -> complex: Complex
 * Returns Float
 */
Value square_magnitude_complex_number_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = a * a + b * b;
	return FLOAT_VAL(magnitude);
}

/**
 * Returns the complex conjugate of a complex number
 * arg0 -> complex: Complex
 * Returns Complex
 */
Value conjugate_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	ObjectComplex *conjugate = new_complex_number(vm, complex_number->real,
						      -complex_number->imag);
	return OBJECT_VAL(conjugate);
}
