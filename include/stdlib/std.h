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

bool initialize_std_lib(VM *vm);

bool register_native_method(VM *vm, Table *method_table, const char *method_name,
			  CruxCallable method_function, int arity);

bool register_native_infallible_method(VM *vm, Table *method_table,
				    const char *method_name,
				    CruxInfallibleCallable method_function,
				    int arity);

#endif // STD_H
