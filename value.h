//
// Created by theon on 08/09/2024.
//

#ifndef VALUE_H
#define VALUE_H

#include "common.h"

#define EPSILON 1e-9

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum { VAL_BOOL, VAL_NIL, VAL_INT, VAL_FLOAT, VAL_OBJECT } ValueType;

typedef struct {
	ValueType type;
	union {
		bool _bool;
		double _float;
		int64_t _int;
		Object *_object;
	} as;
	bool isMutable;
} Value;

// Check a given value's type
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_INT(value) ((value).type == VAL_INT)
#define IS_FLOAT(value) ((value).type == VAL_FLOAT)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

// Make a StellaC type a value
#define AS_BOOL(value) ((value).as._bool)
#define AS_INT(value) ((value).as._int)
#define AS_FLOAT(value) ((value).as._float)
#define AS_OBJECT(value) ((value).as._object)

// Make a value a StellaC type
#define BOOL_VAL(value) ((Value){VAL_BOOL, {._bool = value}, true})
#define NIL_VAL ((Value){VAL_NIL, {._int = 0}, true})
#define INT_VAL(value) ((Value){VAL_INT, {._int = value}, true})
#define FLOAT_VAL(value) ((Value){VAL_FLOAT, {._float = value}, true})
#define OBJECT_VAL(value) ((Value){VAL_OBJECT, {._object = value}, true})

typedef struct {
	int capacity;
	int count;
	Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif // VALUE_H
