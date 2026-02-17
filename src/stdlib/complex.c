#include <math.h>

#include "object.h"
#include "panic.h"
#include "stdlib/complex.h"

Value complex_real_method(VM *vm, int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->real);
}

Value complex_imag_method(VM *vm, int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	return FLOAT_VAL(complex_number->imag);
}

ObjectResult *new_complex_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!(IS_FLOAT(args[0]) || IS_INT(args[0]))) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<real> should be of type 'Int' | 'Float'.", TYPE);
	}
	if (!(IS_FLOAT(args[1]) || IS_INT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<imag> should be of type 'Int' | 'Float'.", TYPE);
	}
	const double real = IS_FLOAT(args[0]) ? AS_FLOAT(args[0])
					      : (double)AS_INT(args[0]);
	const double imag = IS_FLOAT(args[1]) ? AS_FLOAT(args[1])
					      : (double)AS_INT(args[1]);
	ObjectComplex *comp = new_complex_number(vm, real, imag);
	push(vm->current_module_record, OBJECT_VAL(comp));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(comp));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *add_complex_number_method(VM *vm, int arg_count,
					const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_COMPLEX(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<other> must be of type 'Complex'.'",
					  TYPE);
	}
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	ObjectComplex *result_complex =
		new_complex_number(vm, other->real + complex_number->real,
				   other->imag + complex_number->imag);
	push(vm->current_module_record, OBJECT_VAL(result_complex));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_complex));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *sub_complex_number_method(VM *vm, int arg_count,
					const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_COMPLEX(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<other> must be of type 'Complex'.'",
					  TYPE);
	}
	const ObjectComplex *self = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	ObjectComplex *result_complex =
		new_complex_number(vm, self->real - other->real,
				   self->imag - other->imag);
	push(vm->current_module_record, OBJECT_VAL(result_complex));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_complex));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *mul_complex_number_method(VM *vm, int arg_count,
					const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_COMPLEX(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<other> must be of type 'Complex'.'",
					  TYPE);
	}
	const ObjectComplex *self = AS_CRUX_COMPLEX(args[0]);
	const ObjectComplex *other = AS_CRUX_COMPLEX(args[1]);

	const double real = self->real * other->real - self->imag * other->imag;
	const double imag = self->real * other->imag + self->imag * other->real;

	ObjectComplex *result_complex = new_complex_number(vm, real, imag);
	push(vm->current_module_record, OBJECT_VAL(result_complex));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_complex));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *div_complex_number_method(VM *vm, int arg_count,
					const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_COMPLEX(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<other> must be of type 'Complex'.'",
					  TYPE);
	}
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
	push(vm->current_module_record, OBJECT_VAL(result_complex));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_complex));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *scale_complex_number_method(VM *vm, int arg_count,
					  const Value *args)
{
	(void)arg_count;
	if (!(IS_FLOAT(args[1]) || IS_INT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<other> must be of type 'Complex'.'",
					  TYPE);
	}
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	const double scale_factor = IS_FLOAT(args[1]) ? AS_FLOAT(args[1])
						      : (double)AS_INT(args[1]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	ObjectComplex *result_complex = new_complex_number(vm, a * scale_factor,
							   b * scale_factor);
	push(vm->current_module_record, OBJECT_VAL(result_complex));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_complex));
	pop(vm->current_module_record);
	return result;
}

Value magnitude_complex_number_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = sqrt(a * a + b * b);
	return FLOAT_VAL(magnitude);
}

Value square_magnitude_complex_number_method(VM *vm, int arg_count,
					     const Value *args)
{
	(void)arg_count;
	(void)vm;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);

	const double a = complex_number->real;
	const double b = complex_number->imag;

	const double magnitude = a * a + b * b;
	return FLOAT_VAL(magnitude);
}

Value conjugate_complex_number_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	const ObjectComplex *complex_number = AS_CRUX_COMPLEX(args[0]);
	ObjectComplex *conjugate = new_complex_number(vm, complex_number->real,
						      -complex_number->imag);
	return OBJECT_VAL(conjugate);
}
