#include "value.h"

#include <stdio.h>

#include "memory.h"
#include "object.h"

void initValueArray(ValueArray *array)
{
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void writeValueArray(VM *vm, ValueArray *array, const Value value)
{
	if (array->capacity < array->count + 1) {
		const int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(vm, Value, array->values,
					   oldCapacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void freeValueArray(VM *vm, ValueArray *array)
{
	FREE_ARRAY(vm, Value, array->values, array->capacity);
	initValueArray(array);
}

void printValue(const Value value, const bool inCollection)
{
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if IS_FLOAT (value) {
		printf("%g", AS_FLOAT(value));
	} else if (IS_INT(value)) {
		printf("%d", AS_INT(value));
	} else if (IS_CRUX_OBJECT(value)) {
		printObject(value, inCollection);
	}
}

bool valuesEqual(const Value a, const Value b)
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
