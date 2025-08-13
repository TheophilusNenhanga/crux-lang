#ifndef STD_H
#define STD_H

#include "../object.h"
#include "../table.h"

typedef struct {
	const char *name;
	CruxCallable function;
	int arity;
} Callable;

typedef struct {
	const char *name;
	CruxInfallibleCallable function;
	int arity;
} InfallibleCallable;

bool initializeStdLib(VM *vm);

bool registerNativeMethod(VM *vm, Table *methodTable, const char *methodName,
			  CruxCallable methodFunction, int arity);

bool registerNativeInfallibleMethod(VM *vm, Table *methodTable,
				    const char *methodName,
				    CruxInfallibleCallable methodFunction,
				    int arity);

#endif // STD_H
