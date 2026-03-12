#include "value.h"

#include <stdio.h>

#include "garbage_collector.h"
#include "object.h"

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
		return AS_FLOAT(a) == AS_INT(b);
	}
	if (IS_INT(a) && IS_FLOAT(b)) {
		return AS_FLOAT(a) == AS_FLOAT(b);
	}
	return a == b;
}
