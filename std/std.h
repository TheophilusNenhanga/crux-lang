#ifndef TYPES_H
#define TYPES_H

#include "../object.h"
#include "../table.h"

typedef struct {
	const char *name;
	NativeFn function;
	int arity;
} Callable;

extern Callable stringMethods[];
extern Callable arrayMethods[];
extern Callable tableMethods[];
extern Callable errorMethods[];

bool defineNativeCallable(Table* methodTable, const char * methodName, NativeFn methodFunction, int arity);

bool defineMethods(Table* methodTable, Callable* methods);

bool defineNativeFunctions(Table *callableTable);

#endif //TYPES_H
