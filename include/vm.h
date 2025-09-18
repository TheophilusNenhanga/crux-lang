#ifndef VM_H
#define VM_H

#include "../include/value.h"
#include "table.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModuleRecord ObjectModuleRecord;
typedef struct ObjectResult ObjectResult;
typedef struct ObjectStructInstance ObjectStructInstance;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

/**
 * An ongoing function call
 */
typedef struct {
	ObjectClosure *closure;
	uint8_t *ip;
	Value *slots;
} CallFrame;

typedef struct {
	ObjectString *name;
	Table *names;
} NativeModule;

typedef struct {
	Value match_target;
	Value match_bind;
	bool is_match_target;
	bool is_match_bind;
} MatchHandler;

typedef struct {
	NativeModule *modules;
	uint8_t capacity;
	uint8_t count;
} NativeModules;

typedef struct {
	const char **argv;
	int argc;
} Args;

typedef struct {
	ObjectString **paths; // resolved module path strings
	uint32_t count;
	uint32_t capacity;
} ImportStack;

typedef struct {
	ObjectStructInstance **structs;
	uint32_t count;
	uint32_t capacity;
} StructInstanceStack;

typedef enum {
	PAUSED,
	RUNNING,
} GC_STATUS;

typedef struct {
	PoolObject *objects;
	size_t count;
	size_t capacity;
	size_t *free_list;
	size_t free_top;
} ObjectPool;

struct VM {
	Object *objects;

	ObjectPool* object_pool;

	Table strings;

	Table module_cache;
	ObjectModuleRecord *current_module_record;
	ImportStack import_stack;

	Table random_type;
	Table string_type;
	Table array_type;
	Table table_type;
	Table error_type;
	Table file_type;
	Table result_type;
	Table vec2_type;
	Table vec3_type;

	StructInstanceStack struct_instance_stack;

	NativeModules native_modules;
	MatchHandler match_handler;
	Args args;

	size_t bytes_allocated;
	size_t next_gc;
	Object **gray_stack;
	int gray_capacity;
	int gray_count;
	GC_STATUS gc_status;

	int import_count;
};

#define push(module_record, value)                                             \
	do {                                                                   \
		if (__builtin_expect((module_record)->stack_top >=             \
					     (module_record)->stack_limit,     \
				     0)) {                                     \
			runtime_panic((module_record), true, STACK_OVERFLOW,   \
				      "Stack overflow error");                 \
		}                                                              \
		*(module_record)->stack_top++ = (value);                       \
	} while (0)

#define pop(module_record)                                                     \
	({                                                                     \
		if (__builtin_expect((module_record)->stack_top <=             \
					     (module_record)->stack,           \
				     0)) {                                     \
			runtime_panic((module_record), true, RUNTIME,          \
				      "Stack underflow error");                \
		}                                                              \
		*--(module_record)->stack_top;                                 \
	})

#define PEEK(module_record, distance)                                          \
	((module_record)->stack_top[-1 - (distance)])

VM *new_vm(int argc, const char **argv);

void init_vm(VM *vm, int argc, const char **argv);

void free_vm(VM *vm);

InterpretResult interpret(VM *vm, char *source);

#endif // VM_H
