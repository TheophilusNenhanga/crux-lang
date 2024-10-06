#include "value.h"

#include <math.h>
#include <stdio.h>


#include "memory.h"
#include "object.h"

void initValueArray(ValueArray *array) {
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void writeValueArray(ValueArray *array, const Value value) {
	if (array->capacity < array->count + 1) {
		const int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void freeValueArray(ValueArray *array) {
	FREE_ARRAY(Value, array->values, array->capacity);
	initValueArray(array);
}

void printValue(const Value value) {
	switch (value.type) {
		case VAL_BOOL:
			printf(AS_BOOL(value) ? "true" : "false");
			break;
		case VAL_NIL:
			printf("nil");
			break;
		case VAL_INT:
			printf("%d", AS_INT(value));
			break;
		case VAL_FLOAT:
			printf("%g", AS_FLOAT(value));
			break;
		case VAL_OBJECT:
			printObject(value);
			break;
	}
}

bool valuesEqual(const Value a, const Value b) {
	if (a.type != b.type) {
		// Special case: comparing int and float
		if ((a.type == VAL_INT && b.type == VAL_FLOAT) || (a.type == VAL_FLOAT && b.type == VAL_INT)) {
			const double aNum = (a.type == VAL_INT) ? (double) AS_INT(a) : AS_FLOAT(a);
			const double bNum = (b.type == VAL_INT) ? (double) AS_INT(b) : AS_FLOAT(b);
			return fabs(aNum - bNum) < EPSILON;
		}
		return false;
	}

	switch (a.type) {
		case VAL_BOOL:
			return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NIL:
			return true;
		case VAL_INT:
			return AS_INT(a) == AS_INT(b);
		case VAL_FLOAT:
			return fabs(AS_FLOAT(a) - AS_FLOAT(b)) < EPSILON;
		case VAL_OBJECT:
			return AS_OBJECT(a) == AS_OBJECT(b);
		default:
			return false; // Unknown type. Unreachable
	}
}
