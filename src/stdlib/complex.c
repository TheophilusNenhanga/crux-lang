#include <math.h>

#include "object.h"
#include "panic.h"
#include "stdlib/complex.h"

Value complex_real_method(VM *vm, const Value *args)
{
	(void)vm;

	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->real);
}

Value complex_imag_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->imag);
}

Value new_complex_function(VM *vm, const Value *args)
{
	const double real = TO_DOUBLE(args[0]);
	const double imag = TO_DOUBLE(args[1]);
	ObjectComplex *comp = new_complex_number(vm, real, imag);
	return OBJECT_VAL(comp);
}

Value add_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	ObjectComplex *result_complex =
		new_complex_number(vm, other->real + complex_number->real,
				   other->imag + complex_number->imag);
	return OBJECT_VAL(result_complex);
}

Value sub_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *self = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	ObjectComplex *result_complex = new_complex_number(
		vm, self->real - other->real, self->imag - other->imag);
	return OBJECT_VAL(result_complex);
}

Value mul_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *self = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	const double real = self->real * other->real - self->imag * other->imag;
	const double imag = self->real * other->imag + self->imag * other->real;

	ObjectComplex *result_complex = new_complex_number(vm, real, imag);
	return OBJECT_VAL(result_complex);
}

Value div_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *self = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	const double a = self->real;
	const double b = self->imag;
	const double c = other->real;
	const double d = other->imag;

	const double real_part = (a * c + b * d) / (c * c + d * d);
	const double imag_part = (b * c - a * d) / (c * c + d * d);

	ObjectComplex *result_complex = new_complex_number(vm, real_part,
							   imag_part);
	return OBJECT_VAL(result_complex);
}

Value scale_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	const double scale_factor = TO_DOUBLE(args[1]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	ObjectComplex *result_complex = new_complex_number(vm, a * scale_factor,
							   b * scale_factor);
	return OBJECT_VAL(result_complex);
}

Value magnitude_complex_number_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = sqrt(a * a + b * b);
	return FLOAT_VAL(magnitude);
}

Value square_magnitude_complex_number_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = a * a + b * b;
	return FLOAT_VAL(magnitude);
}

Value conjugate_complex_number_method(VM *vm, const Value *args)
{
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	ObjectComplex *conjugate = new_complex_number(vm, complex_number->real,
						      -complex_number->imag);
	return OBJECT_VAL(conjugate);
}
