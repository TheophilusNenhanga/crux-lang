#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "table.h"
#include "value.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModuleRecord ObjectModuleRecord;
typedef struct ObjectResult ObjectResult;
typedef struct ObjectStructInstance ObjectStructInstance;
typedef struct ObjectTypeRecord ObjectTypeRecord;
typedef struct ObjectTypeTable ObjectTypeTable;
typedef struct SlabAllocator SlabAllocator;
typedef struct Compiler Compiler;

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR, INTERPRET_EXIT } InterpretResult;

/**
 * An ongoing function call
 */
typedef struct {
	ObjectClosure *closure;
	uint16_t *ip;
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
	uint32_t count;
	uint32_t capacity;
	uint32_t *free_list;
	uint32_t free_top;
} ObjectPool;

struct VM {
	ObjectPool *object_pool; // Global object pool

	SlabAllocator *slab_16;
	SlabAllocator *slab_24;
	SlabAllocator *slab_32;
	SlabAllocator *slab_48;
	SlabAllocator *slab_64;

	ObjectModuleRecord *current_module_record;
	ImportStack import_stack;

	Table module_cache;
	Table strings;
	Table core_fns;
	Table random_type;
	Table string_type;
	Table array_type;
	Table table_type;
	Table error_type;
	Table file_type;
	Table result_type;
	Table vector_type;
	Table complex_type;
	Table matrix_type;
	Table range_type;
	Table set_type;
	Table tuple_type;
	Table buffer_type;

	StructInstanceStack struct_instance_stack;

	NativeModules native_modules;
	MatchHandler match_handler;
	Args args;

	double heap_growth_factor;
	size_t bytes_allocated;
	size_t next_gc;
	CruxObject **gray_stack;
	int gray_capacity;
	int gray_count;

	GC_STATUS gc_status;

	bool is_exiting;
	int exit_code;

	int import_count;
	ObjectTypeTable *type_table;
	Compiler *main_compiler;
};

#define push(module_record, value)                                                                                     \
	do {                                                                                                               \
		if (__builtin_expect((module_record)->stack_top >= (module_record)->stack_limit, 0)) {                         \
			runtime_panic((module_record), STACK_OVERFLOW, "Stack overflow error");                              \
		}                                                                                                              \
		*(module_record)->stack_top++ = (value);                                                                       \
	} while (0)

#define pop(module_record)                                                                                             \
	({                                                                                                                 \
		if (__builtin_expect((module_record)->stack_top <= (module_record)->stack, 0)) {                               \
			runtime_panic((module_record), RUNTIME, "Stack underflow error");                                    \
		}                                                                                                              \
		*--(module_record)->stack_top;                                                                                 \
	})

#define PEEK(module_record, distance) ((module_record)->stack_top[-1 - (distance)])

VM *new_vm(int argc, const char **argv);

void init_vm(VM *vm, int argc, const char **argv);

void free_vm(VM *vm);

InterpretResult interpret(VM *vm, char *source);

InterpretResult run(VM *vm, bool is_anonymous_frame);

void reset_stack(ObjectModuleRecord *moduleRecord);

void close_upvalues(ObjectModuleRecord *moduleRecord, const Value *last);

void init_import_stack(VM *vm);

void free_import_stack(VM *vm);

// Returns false on allocation error
bool push_import_stack(VM *vm, ObjectString *path);

void pop_import_stack(VM *vm);

bool is_in_import_stack(const VM *vm, const ObjectString *path);

ObjectResult *execute_callable(VM *vm, Value callable, int arg_count, InterpretResult *result);

bool is_falsy(Value value);

void pop_push(ObjectModuleRecord *moduleRecord, Value value);

#define pop_two(module_record)                                                                                         \
	pop((module_record));                                                                                              \
	pop((module_record))

#define pop_push(module_record, value)                                                                                 \
	pop((module_record));                                                                                              \
	push((module_record), (value))

bool binary_operation(VM *vm, OpCode operation);

bool concatenate(VM *vm);

/**
 * Checks if a previous instruction matches the expected opcode.
 * @param frame The current call frame
 * @param instructions_ago How many instructions to look back
 * @param instruction The opcode to check for
 * @return true if the previous instruction matches, false otherwise
 */
bool check_previous_instruction(const CallFrame *frame, int instructions_ago, OpCode instruction);

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param arg_count Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call_value(VM *vm, Value callee, int arg_count);

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
ObjectUpvalue *capture_upvalue(VM *vm, Value *local);

bool handle_invoke(VM *vm, int arg_count, Value receiver, Value original, Value value);

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param arg_count Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invoke(VM *vm, const ObjectString *name, int arg_count);

/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
void define_method(VM *vm, ObjectString *name);

InterpretResult global_compound_operation(VM *vm, ObjectString *name, OpCode opcode, char *operation);

/**
 * Calls a function closure with the given arguments.
 * @param module_record The currently executing module
 * @param closure The function closure to call
 * @param arg_count Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call(ObjectModuleRecord *module_record, ObjectClosure *closure, int arg_count);

Value typeof_value(VM *vm, Value value);

ObjectStructInstance *pop_struct_stack(VM *vm);
bool pushStructStack(VM *vm, ObjectStructInstance *struct_instance);
ObjectStructInstance *peek_struct_stack(const VM *vm);

bool handle_compound_assignment(ObjectModuleRecord *currentModuleRecord, Value *target, Value operand, OpCode op);

#endif // VM_H
