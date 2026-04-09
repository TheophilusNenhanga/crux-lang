#include "value.h"

#include <stdio.h>

#include "garbage_collector.h"
#include "object.h"
#include "vm.h"

void init_value_array(ValueArray *array)
{
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

// Value must be GC protected
void write_value_array(VM *vm, ValueArray *array, const Value value)
{
	if (array->capacity < array->count + 1) {
		const int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(vm, Value, array->values, oldCapacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void free_value_array(VM *vm, ValueArray *array)
{
	FREE_ARRAY(vm, Value, array->values, array->capacity);
	init_value_array(array);
}

bool values_equal(const Value a, const Value b)
{
	if (IS_INT(a) && IS_INT(b)) {
		return AS_INT(a) == AS_INT(b);
	}
	if (IS_FLOAT(a) && IS_FLOAT(b)) {
		return AS_FLOAT(a) == AS_FLOAT(b);
	}
	if (IS_FLOAT(a) && IS_INT(b)) {
		return AS_FLOAT(a) == (double)AS_INT(b);
	}
	if (IS_INT(a) && IS_FLOAT(b)) {
		return (double)AS_INT(a) == AS_FLOAT(b);
	}
	if (IS_CRUX_RANGE(a) && IS_CRUX_RANGE(b)) {
		ObjectRange* range_a = AS_CRUX_RANGE(a);
		ObjectRange* range_b = AS_CRUX_RANGE(b);
		return (range_a->start == range_b->start) && (range_a->end == range_b->end) && (range_a->step == range_b->step);
	}
	if (IS_CRUX_RANDOM(a) && IS_CRUX_RANDOM(b)) {
		ObjectRandom* random_a = AS_CRUX_RANDOM(a);
		ObjectRandom* random_b = AS_CRUX_RANDOM(b);
		return random_a->seed == random_b->seed;
	}
	if (IS_CRUX_COMPLEX(a) && IS_CRUX_COMPLEX(b)) {
		ObjectComplex* complex_a = AS_CRUX_COMPLEX(a);
		ObjectComplex* complex_b = AS_CRUX_COMPLEX(b);
		return (complex_a->real == complex_b->real) && (complex_a->imag == complex_b->imag);
	}
	if (IS_CRUX_MATRIX(a) && IS_CRUX_MATRIX(b)) {
		ObjectMatrix* matrix_a = AS_CRUX_MATRIX(a);
		ObjectMatrix* matrix_b = AS_CRUX_MATRIX(b);
		if (matrix_a->row_dim != matrix_b->row_dim || matrix_a->col_dim != matrix_b->col_dim) {
			return false;
		}
		for (uint16_t i = 0; i < matrix_a->row_dim * matrix_a->col_dim; i++) {
			if (matrix_a->data[i] != matrix_b->data[i]) {
				return false;
			}
		}
		return true;
	}
	if (IS_CRUX_ARRAY(a) && IS_CRUX_ARRAY(b)) {
		ObjectArray* array_a = AS_CRUX_ARRAY(a);
		ObjectArray* array_b = AS_CRUX_ARRAY(b);
		if (array_a->size != array_b->size) {
			return false;
		}
		for (uint16_t i = 0; i < array_a->size; i++) {
			if (!values_equal(array_a->values[i], array_b->values[i])) {
				return false;
			}
		}
		return true;
	}

	if (IS_CRUX_VECTOR(a) && IS_CRUX_VECTOR(b)) {
		ObjectVector* vector_a = AS_CRUX_VECTOR(a);
		ObjectVector* vector_b = AS_CRUX_VECTOR(b);
		if (vector_a->dimensions != vector_b->dimensions) {
			return false;
		}
		if (vector_a->dimensions > STATIC_VECTOR_SIZE) {
			for (uint16_t i = 0; i < vector_a->dimensions; i++) {
				if (!values_equal(vector_a->as.h_components[i], vector_b->as.h_components[i])) {
					return false;
				}
			}
		} else {
			for (uint16_t i = 0; i < vector_a->dimensions; i++) {
				if (vector_a->as.s_components[i] != vector_b->as.s_components[i]) {
					return false;
				}
			}
		}
		return true;
	}


	// TODO: compare other object types
	return a == b;
}
