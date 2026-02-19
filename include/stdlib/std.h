#ifndef STD_H
#define STD_H

#include "object.h"
#include "table.h"

typedef struct {
	const char *name;
	CruxCallable function;
	int arity;
	ValueType arg_types[NATIVE_FUNCTION_MAX_ARGS];
} Callable;

bool initialize_std_lib(VM *vm);

bool register_native_method(VM *vm, Table *method_table, const char *method_name,
			  CruxCallable method_function, int arity, const ValueType *arg_types);

#endif // STD_H
