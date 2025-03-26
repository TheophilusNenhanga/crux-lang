#ifndef TYPES_H
#define TYPES_H

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

extern Callable stringMethods[];
extern Callable arrayMethods[];
extern InfallibleCallable arrayInfallibleMethods[];
extern Callable tableMethods[];
extern Callable errorMethods[];
extern InfallibleCallable builtinInfallibleCallables[];
extern InfallibleCallable randomInfallibleMethods[];
extern Callable randomMethods[];
extern InfallibleCallable randomInfallibleMethods[];

bool defineNativeCallable(VM *vm, Table *methodTable, const char *methodName, StellaNativeCallable methodFunction, int arity);

bool defineNativeInfallibleMethod(VM *vm, Table *methodTable, const char *methodName, StellaInfallibleCallable methodFunction, int arity);

bool defineMethods(VM *vm, Table *methodTable, Callable *methods);

bool defineInfallibleMethods(VM *vm, Table *methodTable, InfallibleCallable *methods);

bool defineNativeFunctions(VM *vm, Table *callableTable);

/**
 * @brief Defines a native infallible function in the given function table.
 *
 * This function creates a new ObjectNativeInfallibleFunction with the provided name, 
 * implementation, and arity, and adds it to the provided function table.
 *
 * @param vm The virtual machine.
 * @param functionTable The table to add the function to.
 * @param functionName The name of the function.
 * @param function The implementation of the function.
 * @param arity The expected number of arguments.
 * @param isPublic Whether the function is public.
 *
 * @return true if the function was added successfully, false otherwise.
 */
bool defineNativeInfallibleFunction(VM *vm, Table *functionTable, const char *functionName, StellaInfallibleCallable function, int arity, bool isPublic);

/**
 * @brief Defines a set of native infallible functions in the given function table.
 *
 * This function iterates through an array of InfallibleCallable structures and adds
 * each as a native infallible function to the provided function table.
 *
 * @param vm The virtual machine.
 * @param callableTable The table to add the functions to.
 * @param infallibles Array of InfallibleCallable structures, terminated by a NULL name.
 *
 * @return true if all functions were added successfully, false otherwise.
 */
bool defineNativeInfallibleFunctions(VM *vm, Table *callableTable, InfallibleCallable *infallibles);

bool initNativeModule(VM* vm, Callable *globals, InfallibleCallable *infallibleCallables, char* moduleName);

bool defineStandardLibrary(VM *vm);

#endif // TYPES_H
