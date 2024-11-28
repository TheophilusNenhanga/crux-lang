#ifndef TYPES_H
#define TYPES_H

#include "../table.h"
#include "../object.h"

typedef struct {
	const char *name;
	NativeFn function;
	int arity;
} Method;

bool defineNativeMethod(Table* methodTable, const char * methodName, NativeFn methodFunction, int arity);

bool defineStringMethods(Table* methodTable);

#endif //TYPES_H
