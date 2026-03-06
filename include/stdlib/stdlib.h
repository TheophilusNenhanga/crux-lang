#ifndef STD_H
#define STD_H

#include "object.h"
#include "table.h"

typedef struct {
	const char *name;
	CruxCallable function;
	int arity;
	TypeRecord **arg_types; // heap-allocated, may be NULL for 0-arity
	TypeRecord *return_type; // arena-allocated
} Callable;

bool initialize_std_lib(VM *vm);

#endif // STD_H
