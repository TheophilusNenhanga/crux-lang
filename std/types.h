#ifndef TYPES_H
#define TYPES_H

#include "../table.h"
#include "../object.h"

typedef struct {
	const char *name;
	NativeFn function;
	int arity;
} Method;

extern Method stringMethods[];
extern Method arrayMethods[];

bool defineNativeMethod(Table* methodTable, const char * methodName, NativeFn methodFunction, int arity);

bool defineMethods(Table* methodTable, Method* methods);

#endif //TYPES_H
