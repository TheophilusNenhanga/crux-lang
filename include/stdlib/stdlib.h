#ifndef STD_H
#define STD_H

#include "object.h"
#include "table.h"

typedef struct {
	const char *name;
	CruxCallable function;
	int arity;
	ObjectTypeRecord **arg_types; // may be NULL for 0-arity
	ObjectTypeRecord *return_type;
} Callable;

bool initialize_std_lib(VM *vm);

#endif // STD_H
