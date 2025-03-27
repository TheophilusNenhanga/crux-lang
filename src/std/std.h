#ifndef STELLA_STD_H
#define STELLA_STD_H

#include "../object.h"
#include "../table.h"


typedef struct {
	const char *name;
	StellaNativeCallable function;
	int arity;
} Callable;

typedef struct {
	const char *name;
	StellaInfallibleCallable function;
	int arity;
} InfallibleCallable;


bool initializeStdLib(VM *vm);


bool registerNativeMethod(VM *vm, Table *methodTable, const char *methodName, 
						 StellaNativeCallable methodFunction, int arity);


bool registerNativeInfallibleMethod(VM *vm, Table *methodTable, const char *methodName,
								   StellaInfallibleCallable methodFunction, int arity);

#endif // STELLA_STD_H
