#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value) isObjectType(value, OBJECT_STRING)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE)

#define AS_STRING(value) ((ObjectString *) AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *) AS_OBJECT(value))->chars)
#define AS_FUNCTION(value) ((ObjectFunction *) AS_OBJECT(value))
#define AS_NATIVE(value) (((ObjectNative *) AS_OBJECT(value))->function)
#define AS_NATIVE_FN(value) ((ObjectNative *) value.as._object)

typedef enum {
	OBJECT_STRING,
	OBJECT_FUNCTION,
	OBJECT_NATIVE,
} ObjectType;

struct Object {
	ObjectType type;
	Object *next;
};

struct ObjectString {
	Object Object;
	int length;
	char *chars;
	uint32_t hash;
};

typedef struct {
	Object object;
	int arity;
	Chunk chunk;
	ObjectString *name;
} ObjectFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
	Object object;
	NativeFn function;
	int arity;
} ObjectNative;

ObjectNative *newNative(NativeFn function, int arity);
ObjectFunction *newFunction();
ObjectString *takeString(const char *chars, int length);
ObjectString *copyString(const char *chars, int length);
void printObject(Value value);

static bool isObjectType(Value value, ObjectType type) { return IS_OBJECT(value) && AS_OBJECT(value)->type == type; }

#endif

// For heap allocated types
