#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum { VAL_BOOL, VAL_NIL, VAL_NUMBER, VAL_OBJECT } ValueType;

typedef struct {
	ValueType type;
	union {
		bool _bool;
		double _number;
		Object *_object;
	} as;
	bool isMutable;
} Value;

// Check a given value's type
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

// Make a StellaC type a value
#define AS_BOOL(value) ((value).as._bool)
#define AS_NUMBER(value) ((value).as._number)
#define AS_OBJECT(value) ((value).as._object)

// Make a value a StellaC type
#define BOOL_VAL(value) ((Value){VAL_BOOL, {._bool = value}, true})
#define NIL_VAL ((Value){VAL_NIL, {._number = 0}, true})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {._number = value}, true})
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
