#ifndef TYPES_H
#define TYPES_H

#include "../object.h"
#include "../table.h"

typedef struct {
	const char *name;
	StellaNativeCallable function;
	int arity;
} Callable;

extern Callable stringMethods[];
extern Callable arrayMethods[];
extern Callable tableMethods[];
extern Callable errorMethods[];

bool defineNativeCallable(VM *vm, Table *methodTable, const char *methodName, StellaNativeCallable methodFunction, int arity);

bool defineMethods(VM *vm, Table *methodTable, Callable *methods);

bool defineNativeFunctions(VM *vm, Table *callableTable);

bool initNativeModule(VM* vm, Callable *globals, char* moduleName);

bool defineStandardLibrary(VM *vm);

#endif // TYPES_H
