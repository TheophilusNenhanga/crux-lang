#include "vm_helpers.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "stdlib/std.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "vm_run.h"

void init_import_stack(VM *vm)
{
	vm->import_stack.paths = NULL;
	vm->import_stack.count = 0;
	vm->import_stack.capacity = 0;
}

bool pushStructStack(VM *vm, ObjectStructInstance *struct_instance)
{
	if (vm->struct_instance_stack.count ==
	    vm->struct_instance_stack.capacity - 1) {
		return false;
	}
	vm->struct_instance_stack.structs[vm->struct_instance_stack.count] =
		struct_instance;
	vm->struct_instance_stack.count++;
	return true;
}

ObjectStructInstance *pop_struct_stack(VM *vm)
{
	if (vm->struct_instance_stack.count == 0) {
		return NULL;
	}
	vm->struct_instance_stack.count--;
	return vm->struct_instance_stack
		.structs[vm->struct_instance_stack.count];
}

ObjectStructInstance *peek_struct_stack(const VM *vm)
{
	if (vm->struct_instance_stack.count > 0) {
		return vm->struct_instance_stack
			.structs[vm->struct_instance_stack.count - 1];
	}
	return NULL;
}

void free_import_stack(VM *vm)
{
	FREE_ARRAY(vm, ObjectString *, vm->import_stack.paths,
		   vm->import_stack.capacity);
	init_import_stack(vm);
}

bool push_import_stack(VM *vm, ObjectString *path)
{
	ImportStack *stack = &vm->import_stack;

	if (stack->count + 1 > stack->capacity) { // resize
		const uint32_t oldCapacity = stack->capacity;
		stack->capacity = GROW_CAPACITY(oldCapacity);
		stack->paths = GROW_ARRAY(vm, ObjectString *, stack->paths,
					  oldCapacity, stack->capacity);
		if (stack->paths == NULL) {
			fprintf(stderr, "Fatal Error: Could not allocate "
					"memory for import stack.\n");
			stack->capacity = oldCapacity;
			return false;
		}
	}

	stack->paths[stack->count] = path;
	stack->count++;
	return true;
}

void pop_import_stack(VM *vm)
{
	ImportStack *stack = &vm->import_stack;
	if (stack->count == 0) {
		return;
	}
	stack->count--;
}

static bool stringEquals(const ObjectString *a, const ObjectString *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->chars, b->chars, a->length) == 0;
}

bool is_in_import_stack(const VM *vm, const ObjectString *path)
{
	const ImportStack *stack = &vm->import_stack;
	for (uint32_t i = 0; i < stack->count; i++) {
		if (stack->paths[i] == path ||
		    stringEquals(stack->paths[i], path)) {
			return true;
		}
	}
	return false;
}

VM *new_vm(const int argc, const char **argv)
{
	VM *vm = malloc(sizeof(VM));
	if (vm == NULL) {
		fprintf(stderr,
			"Fatal Error: Could not allocate memory for VM\n");
		exit(1);
	}
	init_vm(vm, argc, argv);
	return vm;
}

void reset_stack(ObjectModuleRecord *moduleRecord)
{
	moduleRecord->stack_top = moduleRecord->stack;
	moduleRecord->frame_count = 0;
	moduleRecord->open_upvalues = NULL;
}

bool call(ObjectModuleRecord *module_record, ObjectClosure *closure,
	  const int arg_count)
{
	if (arg_count != closure->function->arity) {
		runtime_panic(module_record, false, ARGUMENT_MISMATCH,
			      "Expected %d arguments, got %d",
			      closure->function->arity, arg_count);
		return false;
	}

	if (module_record->frame_count >= FRAMES_MAX) {
		runtime_panic(module_record, false, STACK_OVERFLOW,
			      "Stack overflow");
		return false;
	}

	CallFrame *frame = &module_record->frames[module_record->frame_count++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = module_record->stack_top - arg_count - 1;
	return true;
}

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param arg_count Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call_value(VM *vm, const Value callee, const int arg_count)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;

#define panic_exit(current_module_record)                                      \
	do {                                                                   \
		runtime_panic((current_module_record), false, TYPE,            \
			      "Only functions can be called.");                \
		return false;                                                  \
	} while (0)

#define check_native_arity(arg_count, native)                                  \
	do {                                                                   \
		if ((arg_count) != (native)->arity) {                          \
			runtime_panic(current_module_record, false,            \
				      ARGUMENT_MISMATCH,                       \
				      "Expected %d argument(s), got %d",       \
				      (native)->arity, (arg_count));           \
			return false;                                          \
		}                                                              \
	} while (0)

	if (!IS_CRUX_OBJECT(callee)) {
		panic_exit(current_module_record);
	}

	switch (OBJECT_TYPE(callee)) {
	case OBJECT_CLOSURE:
		return call(current_module_record, AS_CRUX_CLOSURE(callee),
			    arg_count);
	case OBJECT_NATIVE_METHOD: {
		const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(
			callee);
		check_native_arity(arg_count, native);

		ObjectResult *result = native->function(
			vm, arg_count,
			current_module_record->stack_top - arg_count);

		current_module_record->stack_top -= arg_count + 1;

		if (!result->is_ok && result->as.error->is_panic) {
			runtime_panic(current_module_record, false,
				      result->as.error->type,
				      result->as.error->message->chars);
			return false;
		}
		push(current_module_record, OBJECT_VAL(result));
		return true;
	}
	case OBJECT_NATIVE_FUNCTION: {
		const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(
			callee);
		check_native_arity(arg_count, native);

		ObjectResult *result = native->function(
			vm, arg_count,
			current_module_record->stack_top - arg_count);
		current_module_record->stack_top -= arg_count + 1;

		if (!result->is_ok && result->as.error->is_panic) {
			runtime_panic(current_module_record, false,
				      result->as.error->type,
				      result->as.error->message->chars);
			return false;
		}

		push(current_module_record, OBJECT_VAL(result));
		return true;
	}
	case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
		const ObjectNativeInfallibleFunction *native =
			AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(callee);
		check_native_arity(arg_count, native);

		const Value result = native->function(
			vm, arg_count,
			current_module_record->stack_top - arg_count);
		current_module_record->stack_top -= arg_count + 1;

		push(current_module_record, result);
		return true;
	}
	case OBJECT_NATIVE_INFALLIBLE_METHOD: {
		const ObjectNativeInfallibleMethod *native =
			AS_CRUX_NATIVE_INFALLIBLE_METHOD(callee);
		check_native_arity(arg_count, native);

		const Value result = native->function(
			vm, arg_count,
			current_module_record->stack_top - arg_count);
		current_module_record->stack_top -= arg_count + 1;
		push(current_module_record, result);
		return true;
	}
	default: {
		panic_exit(current_module_record);
	}
	}
#undef check_native_arity
#undef panic_exit
}

bool handle_invoke(VM *vm, const int arg_count, const Value receiver,
		   const Value original, const Value value)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;

	mark_value(vm, original);

	// Save original stack order
	current_module_record->stack_top[-arg_count - 1] = value;
	current_module_record->stack_top[-arg_count] = receiver;

	if (!call_value(vm, value, arg_count)) {
		return false;
	}

	// restore the caller and put the result in the right place
	const Value result = pop(current_module_record);
	push(current_module_record, original);
	push(current_module_record, result);
	return true;
}

#define undefined_method_return(current_module_record, name)                   \
	do {                                                                   \
		runtime_panic((current_module_record), false, NAME,            \
			      "Undefined method '%s'.", (name)->chars);        \
		return false;                                                  \
	} while (0)

typedef bool (*TypeInvokeHandler)(VM *vm, const ObjectString *name,
				  int arg_count, Value original,
				  Value receiver);

static bool handle_string_invoke(VM *vm, const ObjectString *name,
				 const int arg_count, const Value original,
				 const Value receiver)
{
	Value value;
	if (table_get(&vm->string_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_undefined_invoke(VM *vm,
				    const ObjectString *name
				    __attribute__((unused)),
				    int arg_count __attribute__((unused)),
				    Value original __attribute__((unused)),
				    Value receiver __attribute__((unused)))
{
	runtime_panic(vm->current_module_record, false, TYPE,
		      "Only instances have methods");
	return false;
}

static bool handle_array_invoke(VM *vm, const ObjectString *name,
				const int arg_count, const Value original,
				const Value receiver)
{
	Value value;
	if (table_get(&vm->array_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_file_invoke(VM *vm, const ObjectString *name,
			       const int arg_count, const Value original,
			       const Value receiver)
{
	Value value;
	if (table_get(&vm->file_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_error_invoke(VM *vm, const ObjectString *name,
				const int arg_count, const Value original,
				const Value receiver)
{
	Value value;
	if (table_get(&vm->error_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_table_invoke(VM *vm, const ObjectString *name,
				const int arg_count, const Value original,
				const Value receiver)
{
	Value value;
	if (table_get(&vm->table_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_random_invoke(VM *vm, const ObjectString *name,
				 const int arg_count, const Value original,
				 const Value receiver)
{
	Value value;
	if (table_get(&vm->random_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_vec2_invoke(VM *vm, const ObjectString *name,
			       const int arg_count, const Value original,
			       const Value receiver)
{
	Value value;
	if (table_get(&vm->vec2_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_vec3_invoke(VM *vm, const ObjectString *name,
			       const int arg_count, const Value original,
			       const Value receiver)
{
	Value value;
	if (table_get(&vm->vec3_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_result_invoke(VM *vm, const ObjectString *name,
				 const int arg_count, const Value original,
				 const Value receiver)
{
	Value value;
	if (table_get(&vm->result_type, name, &value)) {
		return handle_invoke(vm, arg_count, receiver, original, value);
	}
	undefined_method_return(vm->current_module_record, name);
}

static bool handle_struct_instance_invoke(VM *vm, const ObjectString *name,
					  int arg_count,
					  Value original
					  __attribute__((unused)),
					  const Value receiver)
{
	arg_count--;
	const ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(
		receiver);
	Value indexValue;
	if (table_get(&instance->struct_type->fields, name, &indexValue)) {
		return call_value(
			vm, instance->fields[(uint16_t)AS_INT(indexValue)],
			arg_count);
	}
	undefined_method_return(vm->current_module_record, name);
}

static const TypeInvokeHandler invoke_dispatch_table[] = {
	[OBJECT_STRING] = handle_string_invoke,
	[OBJECT_FUNCTION] = handle_undefined_invoke,
	[OBJECT_NATIVE_FUNCTION] = handle_undefined_invoke,
	[OBJECT_NATIVE_METHOD] = handle_undefined_invoke,
	[OBJECT_CLOSURE] = handle_undefined_invoke,
	[OBJECT_UPVALUE] = handle_undefined_invoke,
	[OBJECT_ARRAY] = handle_array_invoke,
	[OBJECT_TABLE] = handle_table_invoke,
	[OBJECT_ERROR] = handle_error_invoke,
	[OBJECT_RESULT] = handle_result_invoke,
	[OBJECT_NATIVE_INFALLIBLE_FUNCTION] = handle_undefined_invoke,
	[OBJECT_NATIVE_INFALLIBLE_METHOD] = handle_undefined_invoke,
	[OBJECT_RANDOM] = handle_random_invoke,
	[OBJECT_FILE] = handle_file_invoke,
	[OBJECT_MODULE_RECORD] = handle_undefined_invoke,
	[OBJECT_STATIC_ARRAY] = handle_undefined_invoke,
	[OBJECT_STATIC_TABLE] = handle_undefined_invoke,
	[OBJECT_STRUCT] = handle_undefined_invoke,
	[OBJECT_STRUCT_INSTANCE] = handle_struct_instance_invoke,
	[OBJECT_VEC2] = handle_vec2_invoke,
	[OBJECT_VEC3] = handle_vec3_invoke,
};

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param arg_count Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invoke(VM *vm, const ObjectString *name, int arg_count)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const Value receiver = PEEK(current_module_record, arg_count);
	const Value original = PEEK(current_module_record,
				    arg_count + 1); // Store the original caller

	if (!IS_CRUX_OBJECT(receiver)) {
		runtime_panic(current_module_record, false, TYPE,
			      "Only instances have methods");
		return false;
	}

	arg_count++; // for the value that the method will act on
	return invoke_dispatch_table[OBJECT_TYPE(receiver)](vm, name, arg_count,
							    original, receiver);
}

ObjectUpvalue *capture_upvalue(VM *vm, Value *local)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	ObjectUpvalue *prevUpvalue = NULL;
	ObjectUpvalue *upvalue = current_module_record->open_upvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjectUpvalue *createdUpvalue = new_upvalue(vm, local);

	createdUpvalue->next = upvalue;
	if (prevUpvalue == NULL) {
		current_module_record->open_upvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

/**
 * Closes all upvalues up to a certain stack position.
 * @param moduleRecord the currently executing module
 * @param last Pointer to the last variable to close
 */
void close_upvalues(ObjectModuleRecord *moduleRecord, const Value *last)
{
	while (moduleRecord->open_upvalues != NULL &&
	       moduleRecord->open_upvalues->location >= last) {
		ObjectUpvalue *upvalue = moduleRecord->open_upvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		moduleRecord->open_upvalues = upvalue->next;
	}
}

/**
 * Determines if a value is falsy (nil or false).
 * @param value The value to check
 * @return true if the value is falsy, false otherwise
 */
bool is_falsy(const Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
	       (IS_INT(value) && AS_INT(value) == 0) ||
	       (IS_FLOAT(value) && AS_FLOAT(value) == 0.0);
}

/**
 * Concatenates two values as strings.
 * @param vm The virtual machine
 * @return true if concatenation succeeds, false otherwise
 */
bool concatenate(VM *vm)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const Value b = PEEK(current_module_record, 0);
	const Value a = PEEK(current_module_record, 1);

	ObjectString *stringB;
	ObjectString *stringA;

	if (IS_CRUX_STRING(b)) {
		stringB = AS_CRUX_STRING(b);
	} else {
		stringB = to_string(vm, b);
		if (stringB == NULL) {
			runtime_panic(
				current_module_record, false, TYPE,
				"Could not convert right operand to a string.");
			return false;
		}
	}

	if (IS_CRUX_STRING(a)) {
		stringA = AS_CRUX_STRING(a);
	} else {
		stringA = to_string(vm, a);
		if (stringA == NULL) {
			runtime_panic(
				current_module_record, false, TYPE,
				"Could not convert left operand to a string.");
			return false;
		}
	}

	const uint64_t length = stringA->length + stringB->length;
	char *chars = ALLOCATE(vm, char, length + 1);

	if (chars == NULL) {
		runtime_panic(current_module_record, false, MEMORY,
			      "Could not allocate memory for concatenation.");
		return false;
	}

	memcpy(chars, stringA->chars, stringA->length);
	memcpy(chars + stringA->length, stringB->chars, stringB->length);
	chars[length] = '\0';

	ObjectString *result = take_string(vm, chars, length);

	pop_two(current_module_record);
	push(current_module_record, OBJECT_VAL(result));
	return true;
}

void initStructInstanceStack(StructInstanceStack *stack)
{
	stack->structs = NULL;
	stack->capacity = STRUCT_INSTANCE_DEPTH;
	stack->count = 0;
}

void freeStructInstanceStack(StructInstanceStack *stack)
{
	free(stack->structs);
	stack->structs = NULL;
	stack->capacity = 0;
	stack->count = 0;
}

void initMatchHandler(MatchHandler *matchHandler)
{
	matchHandler->is_match_bind = false;
	matchHandler->is_match_target = false;
	matchHandler->match_bind = NIL_VAL;
	matchHandler->match_target = NIL_VAL;
}

void initNativeModules(NativeModules *nativeModules)
{
	nativeModules->modules = NULL;
	nativeModules->capacity = 0;
	nativeModules->count = 0;
}

void freeNativeModules(NativeModules *nativeModules)
{
	free(nativeModules->modules);
	nativeModules->modules = NULL;
	nativeModules->capacity = 0;
	nativeModules->count = 0;
}

void free_object_pool(ObjectPool *pool)
{
	if (pool->objects) {
		free(pool->objects);
	}
	if (pool->free_list) {
		free(pool->free_list);
	}
	pool->objects = NULL;
	pool->free_list = NULL;
}

ObjectPool *init_object_pool(const uint32_t initial_capacity)
{
	ObjectPool *pool = malloc(sizeof(ObjectPool));
	if (!pool)
		return NULL;

	pool->objects = calloc(initial_capacity, sizeof(PoolObject));
	pool->free_list = malloc(initial_capacity * sizeof(uint32_t));

	if (!pool->objects || !pool->free_list) {
		free(pool->objects);
		free(pool->free_list);
		free(pool);
		return NULL;
	}

	pool->capacity = initial_capacity;
	pool->count = 0;
	pool->free_top = 0;

	for (uint32_t i = 0; i < initial_capacity; i++) {
		pool->free_list[i] = i;
	}
	pool->free_top = initial_capacity;

	return pool;
}

void init_vm(VM *vm, const int argc, const char **argv)
{
	const bool is_repl = argc == 1 ? true : false;

	vm->object_pool = init_object_pool(INITIAL_OBJECT_POOL_CAPACITY);

	vm->gc_status = PAUSED;
	vm->bytes_allocated = 0;
	vm->next_gc = 1024 * 1024;
	vm->gray_count = 0;
	vm->gray_capacity = 0;
	vm->gray_stack = NULL;
	vm->struct_instance_stack.structs = NULL;

	vm->current_module_record = new_object_module_record(vm, NULL, is_repl,
							     true);

	reset_stack(vm->current_module_record);

	init_table(&vm->string_type);
	init_table(&vm->array_type);
	init_table(&vm->table_type);
	init_table(&vm->error_type);
	init_table(&vm->random_type);
	init_table(&vm->file_type);
	init_table(&vm->result_type);
	init_table(&vm->vec2_type);
	init_table(&vm->vec3_type);
	init_table(&vm->module_cache);

	init_table(&vm->strings);

	init_import_stack(vm);

	initNativeModules(&vm->native_modules);
	vm->native_modules.modules = (NativeModule *)malloc(
		sizeof(NativeModule) * NATIVE_MODULES_CAPACITY);
	if (vm->native_modules.modules == NULL) {
		fprintf(stderr, "Fatal Error: Could not allocate memory for "
				"native modules.\nShutting Down!\n");
		exit(1);
	}

	initMatchHandler(&vm->match_handler);

	if (!initialize_std_lib(vm)) {
		runtime_panic(vm->current_module_record, true, RUNTIME,
			      "Failed to initialize standard library.");
	}
	vm->import_count = 0;

	initStructInstanceStack(&vm->struct_instance_stack);
	vm->struct_instance_stack.structs = (ObjectStructInstance **)malloc(
		sizeof(ObjectStructInstance *) * STRUCT_INSTANCE_DEPTH);
	if (vm->struct_instance_stack.structs == NULL) {
		fprintf(stderr, "Fatal Error: Could not allocate memory for "
				"stack struct.\nShutting Down!\n");
		exit(1);
	}

	vm->args.argc = argc;
	vm->args.argv = argv;

	ObjectString *path;
	if (argc > 1) {
		path = copy_string(vm, argv[1], strlen(argv[1]));
	} else {
#ifdef _WIN32
		path = copy_string(vm, ".\\", 2);
#else
		path = copy_string(vm, "./", 2);
#endif
	}

	vm->current_module_record->path = path;
	table_set(vm, &vm->module_cache, vm->current_module_record->path,
		  OBJECT_VAL(vm->current_module_record));
	vm->gc_status = RUNNING;
}

void free_vm(VM *vm)
{
	free_table(vm, &vm->strings);

	free_table(vm, &vm->string_type);
	free_table(vm, &vm->array_type);
	free_table(vm, &vm->table_type);
	free_table(vm, &vm->error_type);
	free_table(vm, &vm->random_type);
	free_table(vm, &vm->file_type);
	free_table(vm, &vm->result_type);
	free_table(vm, &vm->vec2_type);
	free_table(vm, &vm->vec3_type);

	for (int i = 0; i < vm->native_modules.count; i++) {
		const NativeModule module = vm->native_modules.modules[i];
		free_table(vm, module.names);
		FREE(vm, Table, module.names);
	}
	freeNativeModules(&vm->native_modules);
	free_table(vm, &vm->module_cache);

	free_import_stack(vm);
	freeStructInstanceStack(&vm->struct_instance_stack);

	free_module_record(vm, vm->current_module_record);

	free_objects(vm);
	free_object_pool(vm->object_pool);
	free(vm->object_pool);

	free(vm);
}

typedef bool (*IntBinaryOp)(ObjectModuleRecord *current_module_record,
			    int32_t intA, int32_t intB);
typedef bool (*FloatBinaryOp)(ObjectModuleRecord *current_module_record,
			      double doubleA, double doubleB);

static bool int_add(ObjectModuleRecord *current_module_record,
		    const int32_t intA, const int32_t intB)
{
	const int64_t result = (int64_t)intA + (int64_t)intB;
	pop_two(current_module_record);
	if (result >= INT32_MIN && result <= INT32_MAX) {
		push(current_module_record, INT_VAL((int32_t)result));
	} else {
		push(current_module_record,
		     FLOAT_VAL((double)result)); // Promote on overflow
	}
	return true;
}

static bool int_subtract(ObjectModuleRecord *current_module_record,
			 const int32_t intA, const int32_t intB)
{
	const int64_t result = (int64_t)intA - (int64_t)intB;
	pop_two(current_module_record);
	if (result >= INT32_MIN && result <= INT32_MAX) {
		push(current_module_record, INT_VAL((int32_t)result));
	} else {
		push(current_module_record,
		     FLOAT_VAL((double)result)); // Promote on overflow
	}
	return true;
}

static bool int_multiply(ObjectModuleRecord *current_module_record,
			 const int32_t intA, const int32_t intB)
{
	const int64_t result = (int64_t)intA * (int64_t)intB;
	pop_two(current_module_record);
	if (result >= INT32_MIN && result <= INT32_MAX) {
		push(current_module_record, INT_VAL((int32_t)result));
	} else {
		push(current_module_record,
		     FLOAT_VAL((double)result)); // Promote on overflow
	}
	return true;
}

static bool int_divide(ObjectModuleRecord *current_module_record,
		       const int32_t intA, const int32_t intB)
{
	if (intB == 0) {
		runtime_panic(current_module_record, false, MATH,
			      "Division by zero.");
		return false;
	}
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL((double)intA / (double)intB));
	return true;
}

static bool int_int_divide(ObjectModuleRecord *current_module_record,
			   const int32_t intA, const int32_t intB)
{
	if (intB == 0) {
		runtime_panic(current_module_record, false, MATH,
			      "Integer division by zero.");
		return false;
	}
	// Edge case: INT32_MIN / -1 overflows int32_t
	if (intA == INT32_MIN && intB == -1) {
		pop_two(current_module_record);
		push(current_module_record,
		     FLOAT_VAL(-(double)INT32_MIN)); // Promote
	} else {
		pop_two(current_module_record);
		push(current_module_record, INT_VAL(intA / intB));
	}
	return true;
}

static bool int_modulus(ObjectModuleRecord *current_module_record,
			const int32_t intA, const int32_t intB)
{
	if (intB == 0) {
		runtime_panic(current_module_record, false, MATH,
			      "Modulo by zero.");
		return false;
	}

	if (intA == INT32_MIN && intB == -1) {
		pop_two(current_module_record);
		push(current_module_record, INT_VAL(0));
	} else {
		pop_two(current_module_record);
		push(current_module_record, INT_VAL(intA % intB));
	}
	return true;
}

static bool int_left_shift(ObjectModuleRecord *current_module_record,
			   const int32_t intA, const int32_t intB)
{
	if (intB < 0 || intB >= 32) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Invalid shift amount (%d) for <<.", intB);
		return false;
	}
	pop_two(current_module_record);
	push(current_module_record, INT_VAL(intA << intB));
	return true;
}

static bool int_right_shift(ObjectModuleRecord *current_module_record,
			    const int32_t intA, const int32_t intB)
{
	if (intB < 0 || intB >= 32) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Invalid shift amount (%d) for >>.", intB);
		return false;
	}
	pop_two(current_module_record);
	push(current_module_record, INT_VAL(intA >> intB));
	return true;
}

static bool int_power(ObjectModuleRecord *current_module_record,
		      const int32_t intA, const int32_t intB)
{
	// Promote int^int to float
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(pow(intA, intB)));
	return true;
}

static bool int_less(ObjectModuleRecord *current_module_record,
		     const int32_t intA, const int32_t intB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(intA < intB));
	return true;
}

static bool int_less_equal(ObjectModuleRecord *current_module_record,
			   const int32_t intA, const int32_t intB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(intA <= intB));
	return true;
}

static bool int_greater(ObjectModuleRecord *current_module_record,
			const int32_t intA, const int32_t intB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(intA > intB));
	return true;
}

static bool int_greater_equal(ObjectModuleRecord *current_module_record,
			      const int32_t intA, const int32_t intB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(intA >= intB));
	return true;
}

static bool float_add(ObjectModuleRecord *current_module_record,
		      const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(doubleA + doubleB));
	return true;
}

static bool float_subtract(ObjectModuleRecord *current_module_record,
			   const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(doubleA - doubleB));
	return true;
}

static bool float_multiply(ObjectModuleRecord *current_module_record,
			   const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(doubleA * doubleB));
	return true;
}

static bool float_divide(ObjectModuleRecord *current_module_record,
			 const double doubleA, const double doubleB)
{
	if (doubleB == 0.0) {
		runtime_panic(current_module_record, false, MATH,
			      "Division by zero.");
		return false;
	}
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(doubleA / doubleB));
	return true;
}

static bool float_power(ObjectModuleRecord *current_module_record,
			const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, FLOAT_VAL(pow(doubleA, doubleB)));
	return true;
}

static bool float_less(ObjectModuleRecord *current_module_record,
		       const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(doubleA < doubleB));
	return true;
}

static bool float_less_equal(ObjectModuleRecord *current_module_record,
			     const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(doubleA <= doubleB));
	return true;
}

static bool float_greater(ObjectModuleRecord *current_module_record,
			  const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(doubleA > doubleB));
	return true;
}

static bool float_greater_equal(ObjectModuleRecord *current_module_record,
				const double doubleA, const double doubleB)
{
	pop_two(current_module_record);
	push(current_module_record, BOOL_VAL(doubleA >= doubleB));
	return true;
}

static bool float_invalid_int_op(ObjectModuleRecord *current_module_record,
				 double doubleA __attribute__((unused)),
				 double doubleB __attribute__((unused)))
{
	runtime_panic(current_module_record, false, TYPE,
		      "Operands for integer operation must both be integers.");
	return false;
}

static const IntBinaryOp int_binary_ops[] = {
	[OP_ADD] = int_add,
	[OP_SUBTRACT] = int_subtract,
	[OP_MULTIPLY] = int_multiply,
	[OP_DIVIDE] = int_divide,
	[OP_INT_DIVIDE] = int_int_divide,
	[OP_MODULUS] = int_modulus,
	[OP_LEFT_SHIFT] = int_left_shift,
	[OP_RIGHT_SHIFT] = int_right_shift,
	[OP_POWER] = int_power,
	[OP_LESS] = int_less,
	[OP_LESS_EQUAL] = int_less_equal,
	[OP_GREATER] = int_greater,
	[OP_GREATER_EQUAL] = int_greater_equal,
};

static const FloatBinaryOp float_binary_ops[] = {
	[OP_ADD] = float_add,
	[OP_SUBTRACT] = float_subtract,
	[OP_MULTIPLY] = float_multiply,
	[OP_DIVIDE] = float_divide,
	[OP_POWER] = float_power,
	[OP_LESS] = float_less,
	[OP_LESS_EQUAL] = float_less_equal,
	[OP_GREATER] = float_greater,
	[OP_GREATER_EQUAL] = float_greater_equal,
	[OP_INT_DIVIDE] = float_invalid_int_op,
	[OP_MODULUS] = float_invalid_int_op,
	[OP_LEFT_SHIFT] = float_invalid_int_op,
	[OP_RIGHT_SHIFT] = float_invalid_int_op,
};

// Function pointer types for compound operations
typedef InterpretResult (*IntCompoundOp)(
	ObjectModuleRecord *current_module_record, const ObjectString *name,
	char *operation, int32_t icurrent, int32_t ioperand,
	Value *resultValue);
typedef InterpretResult (*FloatCompoundOp)(
	ObjectModuleRecord *current_module_record, const ObjectString *name,
	char *operation, double dcurrent, double doperand, Value *resultValue);

// Integer compound operation handlers
static InterpretResult
int_compound_plus(ObjectModuleRecord *current_module_record
		  __attribute__((unused)),
		  const ObjectString *name __attribute__((unused)),
		  char *operation __attribute__((unused)), int32_t icurrent,
		  int32_t ioperand, Value *resultValue)
{
	const int64_t result = (int64_t)icurrent + (int64_t)ioperand;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		*resultValue = INT_VAL((int32_t)result);
	} else {
		*resultValue = FLOAT_VAL((double)result);
	}
	return INTERPRET_OK;
}

static InterpretResult
int_compound_minus(ObjectModuleRecord *current_module_record
		   __attribute__((unused)),
		   const ObjectString *name __attribute__((unused)),
		   char *operation __attribute__((unused)), int32_t icurrent,
		   int32_t ioperand, Value *resultValue)
{
	const int64_t result = (int64_t)icurrent - (int64_t)ioperand;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		*resultValue = INT_VAL((int32_t)result);
	} else {
		*resultValue = FLOAT_VAL((double)result);
	}
	return INTERPRET_OK;
}

static InterpretResult
int_compound_star(ObjectModuleRecord *current_module_record
		  __attribute__((unused)),
		  const ObjectString *name __attribute__((unused)),
		  char *operation __attribute__((unused)), int32_t icurrent,
		  int32_t ioperand, Value *resultValue)
{
	const int64_t result = (int64_t)icurrent * (int64_t)ioperand;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		*resultValue = INT_VAL((int32_t)result);
	} else {
		*resultValue = FLOAT_VAL((double)result);
	}
	return INTERPRET_OK;
}

static InterpretResult
int_compound_slash(ObjectModuleRecord *current_module_record,
		   const ObjectString *name, char *operation, int32_t icurrent,
		   int32_t ioperand, Value *resultValue)
{
	if (ioperand == 0) {
		runtime_panic(current_module_record, false, MATH,
			      "Division by zero in '%s %s'.", name->chars,
			      operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	*resultValue = FLOAT_VAL((double)icurrent / (double)ioperand);
	return INTERPRET_OK;
}

static InterpretResult
int_compound_int_divide(ObjectModuleRecord *current_module_record,
			const ObjectString *name, char *operation,
			int32_t icurrent, int32_t ioperand, Value *resultValue)
{
	if (ioperand == 0) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Division by zero in '%s %s'.", name->chars,
			      operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	if (icurrent == INT32_MIN && ioperand == -1) {
		*resultValue = FLOAT_VAL(-(double)INT32_MIN); // Overflow
	} else {
		*resultValue = INT_VAL(icurrent / ioperand);
	}
	return INTERPRET_OK;
}

static InterpretResult
int_compound_modulus(ObjectModuleRecord *current_module_record,
		     const ObjectString *name, char *operation,
		     int32_t icurrent, int32_t ioperand, Value *resultValue)
{
	if (ioperand == 0) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Division by zero in '%s %s'.", name->chars,
			      operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	if (icurrent == INT32_MIN && ioperand == -1) {
		*resultValue = INT_VAL(0);
	} else {
		*resultValue = INT_VAL(icurrent % ioperand);
	}
	return INTERPRET_OK;
}

// Float compound operation handlers
static InterpretResult
float_compound_plus(ObjectModuleRecord *current_module_record
		    __attribute__((unused)),
		    const ObjectString *name __attribute__((unused)),
		    char *operation __attribute__((unused)), double dcurrent,
		    double doperand, Value *resultValue)
{
	*resultValue = FLOAT_VAL(dcurrent + doperand);
	return INTERPRET_OK;
}

static InterpretResult
float_compound_minus(ObjectModuleRecord *current_module_record
		     __attribute__((unused)),
		     const ObjectString *name __attribute__((unused)),
		     char *operation __attribute__((unused)), double dcurrent,
		     double doperand, Value *resultValue)
{
	*resultValue = FLOAT_VAL(dcurrent - doperand);
	return INTERPRET_OK;
}

static InterpretResult
float_compound_star(ObjectModuleRecord *current_module_record
		    __attribute__((unused)),
		    const ObjectString *name __attribute__((unused)),
		    char *operation __attribute__((unused)), double dcurrent,
		    double doperand, Value *resultValue)
{
	*resultValue = FLOAT_VAL(dcurrent * doperand);
	return INTERPRET_OK;
}

static InterpretResult
float_compound_slash(ObjectModuleRecord *current_module_record,
		     const ObjectString *name, char *operation, double dcurrent,
		     double doperand, Value *resultValue)
{
	if (doperand == 0.0) {
		runtime_panic(current_module_record, false, MATH,
			      "Division by zero in '%s %s'.", name->chars,
			      operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	*resultValue = FLOAT_VAL(dcurrent / doperand);
	return INTERPRET_OK;
}

static InterpretResult
float_compound_invalid_int_op(ObjectModuleRecord *current_module_record,
			      const ObjectString *name __attribute__((unused)),
			      char *operation,
			      double dcurrent __attribute__((unused)),
			      double doperand __attribute__((unused)),
			      Value *resultValue __attribute__((unused)))
{
	runtime_panic(current_module_record, false, TYPE,
		      "Operands for integer compound assignment '%s' must both "
		      "be integers.",
		      operation);
	return INTERPRET_RUNTIME_ERROR;
}

// Dispatch tables for compound operations
static const IntCompoundOp int_compound_ops[] = {
	[OP_SET_GLOBAL_PLUS] = int_compound_plus,
	[OP_SET_GLOBAL_MINUS] = int_compound_minus,
	[OP_SET_GLOBAL_STAR] = int_compound_star,
	[OP_SET_GLOBAL_SLASH] = int_compound_slash,
	[OP_SET_GLOBAL_INT_DIVIDE] = int_compound_int_divide,
	[OP_SET_GLOBAL_MODULUS] = int_compound_modulus,
};

static const FloatCompoundOp float_compound_ops[] = {
	[OP_SET_GLOBAL_PLUS] = float_compound_plus,
	[OP_SET_GLOBAL_MINUS] = float_compound_minus,
	[OP_SET_GLOBAL_STAR] = float_compound_star,
	[OP_SET_GLOBAL_SLASH] = float_compound_slash,
	[OP_SET_GLOBAL_INT_DIVIDE] = float_compound_invalid_int_op,
	[OP_SET_GLOBAL_MODULUS] = float_compound_invalid_int_op,
};

// Function pointer type for typeof operations
typedef Value (*TypeofHandler)(VM *vm, const Value value);

// Object type handlers for typeof
static Value typeof_string(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "string", 6));
}

static Value typeof_function(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "function", 8));
}

static Value typeof_upvalue(VM *vm, const Value value)
{
	const ObjectUpvalue *upvalue = AS_CRUX_UPVALUE(value);
	return typeof_value(vm, upvalue->closed);
}

static Value typeof_array(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "array", 5));
}

static Value typeof_table(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "table", 5));
}

static Value typeof_error(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "error", 5));
}

static Value typeof_result(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "result", 6));
}

static Value typeof_random(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "random", 6));
}

static Value typeof_file(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "file", 4));
}

static Value typeof_module_record(VM *vm,
				  const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "module", 6));
}

static Value typeof_static_array(VM *vm,
				 const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "static array", 12));
}

static Value typeof_static_table(VM *vm,
				 const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "static table", 12));
}

static Value typeof_struct(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "struct", 6));
}

static Value typeof_struct_instance(VM *vm,
				    const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "struct instance", 15));
}

static Value typeof_vec2(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "vec2", 4));
}

static Value typeof_vec3(VM *vm, const Value value __attribute__((unused)))
{
	return OBJECT_VAL(copy_string(vm, "vec3", 4));
}

// Dispatch table for object types
static const TypeofHandler typeof_handlers[] = {
	[OBJECT_STRING] = typeof_string,
	[OBJECT_FUNCTION] = typeof_function,
	[OBJECT_NATIVE_FUNCTION] = typeof_function,
	[OBJECT_NATIVE_METHOD] = typeof_function,
	[OBJECT_CLOSURE] = typeof_function,
	[OBJECT_UPVALUE] = typeof_upvalue,
	[OBJECT_ARRAY] = typeof_array,
	[OBJECT_TABLE] = typeof_table,
	[OBJECT_ERROR] = typeof_error,
	[OBJECT_RESULT] = typeof_result,
	[OBJECT_NATIVE_INFALLIBLE_FUNCTION] = typeof_function,
	[OBJECT_NATIVE_INFALLIBLE_METHOD] = typeof_function,
	[OBJECT_RANDOM] = typeof_random,
	[OBJECT_FILE] = typeof_file,
	[OBJECT_MODULE_RECORD] = typeof_module_record,
	[OBJECT_STATIC_ARRAY] = typeof_static_array,
	[OBJECT_STATIC_TABLE] = typeof_static_table,
	[OBJECT_STRUCT] = typeof_struct,
	[OBJECT_STRUCT_INSTANCE] = typeof_struct_instance,
	[OBJECT_VEC2] = typeof_vec2,
	[OBJECT_VEC3] = typeof_vec3,
};

/**
 * Performs a binary operation on the top two values of the stack.
 * @param vm The virtual machine
 * @param operation The operation code to perform
 * @return true if the operation succeeds, false otherwise
 */
bool binary_operation(VM *vm, const OpCode operation)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const Value b = PEEK(current_module_record, 0);
	const Value a = PEEK(current_module_record, 1);

	const bool aIsInt = IS_INT(a);
	const bool bIsInt = IS_INT(b);
	const bool aIsFloat = IS_FLOAT(a);
	const bool bIsFloat = IS_FLOAT(b);

	if (!((aIsInt || aIsFloat) && (bIsInt || bIsFloat))) {
		if (!(aIsInt || aIsFloat)) {
			runtime_panic(current_module_record, false, TYPE,
				      type_error_message(vm, a,
							 "'int' or 'float'"));
		} else {
			runtime_panic(current_module_record, false, TYPE,
				      type_error_message(vm, b,
							 "'int' or 'float'"));
		}
		return false;
	}

	if (aIsInt && bIsInt) {
		const int32_t intA = AS_INT(a);
		const int32_t intB = AS_INT(b);

		if (operation >= (OpCode)(sizeof(int_binary_ops) /
					  sizeof(int_binary_ops[0])) ||
		    int_binary_ops[operation] == NULL) {
			runtime_panic(
				current_module_record, false, RUNTIME,
				"Unknown binary operation %d for int, int.",
				operation);
			return false;
		}

		return int_binary_ops[operation](current_module_record, intA,
						 intB);
	}
	const double doubleA = aIsFloat ? AS_FLOAT(a) : (double)AS_INT(a);
	const double doubleB = bIsFloat ? AS_FLOAT(b) : (double)AS_INT(b);

	if (operation >= (OpCode)(sizeof(float_binary_ops) /
				  sizeof(float_binary_ops[0])) ||
	    float_binary_ops[operation] == NULL) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Unknown binary operation %d for float/mixed.",
			      operation);
		return false;
	}

	return float_binary_ops[operation](current_module_record, doubleA,
					   doubleB);
	return true;
}

InterpretResult global_compound_operation(VM *vm, ObjectString *name,
					  const OpCode opcode, char *operation)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	Value currentValue;
	if (!table_get(&current_module_record->globals, name, &currentValue)) {
		runtime_panic(
			current_module_record, false, NAME,
			"Undefined variable '%s' for compound assignment.",
			name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	const Value operandValue = PEEK(current_module_record, 0);

	const bool currentIsInt = IS_INT(currentValue);
	const bool currentIsFloat = IS_FLOAT(currentValue);
	const bool operandIsInt = IS_INT(operandValue);
	const bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) &&
	      (operandIsInt || operandIsFloat))) {
		if (!(currentIsInt || currentIsFloat)) {
			runtime_panic(current_module_record, false, TYPE,
				      "Variable '%s' is not a number for '%s' "
				      "operator.",
				      name->chars, operation);
		} else {
			runtime_panic(current_module_record, false, TYPE,
				      "Right-hand operand for '%s' must be an "
				      "'int' or 'float'.",
				      operation);
		}
		return INTERPRET_RUNTIME_ERROR;
	}

	Value resultValue;

	if (currentIsInt && operandIsInt) {
		const int32_t icurrent = AS_INT(currentValue);
		const int32_t ioperand = AS_INT(operandValue);

		if (opcode >= (OpCode)(sizeof(int_compound_ops) /
				       sizeof(int_compound_ops[0])) ||
		    int_compound_ops[opcode] == NULL) {
			runtime_panic(current_module_record, false, RUNTIME,
				      "Unsupported compound assignment opcode "
				      "%d for int/int.",
				      opcode);
			return INTERPRET_RUNTIME_ERROR;
		}

		InterpretResult result = int_compound_ops[opcode](
			current_module_record, name, operation, icurrent,
			ioperand, &resultValue);
		if (result != INTERPRET_OK) {
			return result;
		}
	} else {
		const double dcurrent = currentIsFloat
						? AS_FLOAT(currentValue)
						: (double)AS_INT(currentValue);
		const double doperand = operandIsFloat
						? AS_FLOAT(operandValue)
						: (double)AS_INT(operandValue);

		if (opcode >= (OpCode)(sizeof(float_compound_ops) /
				       sizeof(float_compound_ops[0])) ||
		    float_compound_ops[opcode] == NULL) {
			runtime_panic(current_module_record, false, RUNTIME,
				      "Unsupported compound assignment opcode "
				      "%d for float/mixed.",
				      opcode);
			return INTERPRET_RUNTIME_ERROR;
		}

		const InterpretResult result = float_compound_ops[opcode](
			current_module_record, name, operation, dcurrent,
			doperand, &resultValue);
		if (result != INTERPRET_OK) {
			return result;
		}
	}
	table_set(vm, &current_module_record->globals, name, resultValue);
	return INTERPRET_OK;
}

bool check_previous_instruction(const CallFrame *frame,
				const int instructions_ago,
				const OpCode instruction)
{
	const uint8_t *current = frame->ip;
	const uint8_t *codeStart = frame->closure->function->chunk.code;

	// Check if we have enough bytes before current position,
	if (current - (instructions_ago + 2) < codeStart) {
		return false;
	}

	return *(current - (instructions_ago + 2)) == instruction;
}
InterpretResult interpret(VM *vm, char *source)
{
	ObjectFunction *function = compile(vm, source);
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	if (function == NULL) {
		current_module_record->state = STATE_ERROR;
		return INTERPRET_COMPILE_ERROR;
	}

	push(current_module_record, OBJECT_VAL(function));
	ObjectClosure *closure = new_closure(vm, function);
	vm->current_module_record->module_closure = closure;
	pop(current_module_record);
	push(current_module_record, OBJECT_VAL(closure));
	call(current_module_record, closure, 0);

	const InterpretResult result = run(vm, false);

	return result;
}

/**
 *
 * @param vm
 * @param closure The closure to be executed. The caller must ensure that the
 * arguments are on the stack correctly and match the arity
 * @param arg_count
 * @param result result from executing the function
 * @return
 */
ObjectResult *execute_user_function(VM *vm, ObjectClosure *closure,
				    const int arg_count,
				    InterpretResult *result)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const uint32_t currentFrameCount = current_module_record->frame_count;
	ObjectResult *errorResult = new_error_result(
		vm, new_error(vm, copy_string(vm, "", 0), RUNTIME, true));

	if (!call(current_module_record, closure, arg_count)) {
		runtime_panic(current_module_record, false, RUNTIME,
			      "Failed to execute function");
		*result = INTERPRET_RUNTIME_ERROR;
		return errorResult;
	}

	*result = run(vm, true);

	vm->current_module_record->frame_count = currentFrameCount;
	if (*result == INTERPRET_OK) {
		const Value executionResult = PEEK(current_module_record, 0);
		return new_ok_result(vm, executionResult);
	}
	return errorResult;
}

Value typeof_value(VM *vm, const Value value)
{
	if (IS_CRUX_OBJECT(value)) {
		const ObjectType type = AS_CRUX_OBJECT(value)->type;

		if (type >= sizeof(typeof_handlers) /
				    sizeof(typeof_handlers[0]) ||
		    typeof_handlers[type] == NULL) {
			return OBJECT_VAL(copy_string(vm, "unknown", 7));
		}

		return typeof_handlers[type](vm, value);
	}

	if (IS_INT(value)) {
		return OBJECT_VAL(copy_string(vm, "int", 3));
	}

	if (IS_FLOAT(value)) {
		return OBJECT_VAL(copy_string(vm, "float", 5));
	}

	if (IS_BOOL(value)) {
		return OBJECT_VAL(copy_string(vm, "boolean", 7));
	}

	if (IS_NIL(value)) {
		return OBJECT_VAL(copy_string(vm, "nil", 3));
	}
	__builtin_unreachable();
	return OBJECT_VAL(copy_string(vm, "unknown", 7));
}

bool handle_compound_assignment(ObjectModuleRecord *currentModuleRecord,
				Value *target, const Value operand,const OpCode op)
{
	const bool currentIsInt = IS_INT(*target);
	const bool currentIsFloat = IS_FLOAT(*target);
	const bool operandIsInt = IS_INT(operand);
	const bool operandIsFloat = IS_FLOAT(operand);

	if (op == OP_SET_LOCAL_PLUS &&
	    (IS_CRUX_STRING(*target) || IS_CRUX_STRING(operand))) {
		// += is not defined for strings
		return false;
	}

	if (!((currentIsInt || currentIsFloat) &&
	      (operandIsInt || operandIsFloat))) {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Operands must be numbers.");
		return false;
	}

	Value result;

	// both integers
	if (currentIsInt && operandIsInt) {
		const int32_t a = AS_INT(*target);
		const int32_t b = AS_INT(operand);
		int64_t temp;

		switch (op) {
		case OP_SET_LOCAL_PLUS:
		case OP_SET_UPVALUE_PLUS:
		case OP_SET_GLOBAL_PLUS:
			temp = (int64_t)a + (int64_t)b;
			result = (temp >= INT32_MIN && temp <= INT32_MAX)
					 ? INT_VAL((int32_t)temp)
					 : FLOAT_VAL((double)temp);
			break;
		case OP_SET_LOCAL_MINUS:
		case OP_SET_UPVALUE_MINUS:
		case OP_SET_GLOBAL_MINUS:
			temp = (int64_t)a - (int64_t)b;
			result = (temp >= INT32_MIN && temp <= INT32_MAX)
					 ? INT_VAL((int32_t)temp)
					 : FLOAT_VAL((double)temp);
			break;
		case OP_SET_LOCAL_STAR:
		case OP_SET_UPVALUE_STAR:
		case OP_SET_GLOBAL_STAR:
			temp = (int64_t)a * (int64_t)b;
			result = (temp >= INT32_MIN && temp <= INT32_MAX)
					 ? INT_VAL((int32_t)temp)
					 : FLOAT_VAL((double)temp);
			break;
		case OP_SET_LOCAL_SLASH:
		case OP_SET_UPVALUE_SLASH:
		case OP_SET_GLOBAL_SLASH:
			if (b == 0) {
				runtime_panic(currentModuleRecord, false, MATH,
					      "Division by zero.");
				return false;
			}
			result = FLOAT_VAL((double)a / (double)b);
			break;
		case OP_SET_LOCAL_INT_DIVIDE:
		case OP_SET_UPVALUE_INT_DIVIDE:
		case OP_SET_GLOBAL_INT_DIVIDE:
			if (b == 0) {
				runtime_panic(currentModuleRecord, false, MATH,
					      "Integer division by zero.");
				return false;
			}
			result = (a == INT32_MIN && b == -1)
					 ? FLOAT_VAL(-(double)INT32_MIN)
					 : INT_VAL(a / b);
			break;
		case OP_SET_LOCAL_MODULUS:
		case OP_SET_UPVALUE_MODULUS:
		case OP_SET_GLOBAL_MODULUS:
			if (b == 0) {
				runtime_panic(currentModuleRecord, false, MATH,
					      "Modulo by zero.");
				return false;
			}
			result = (a == INT32_MIN && b == -1) ? INT_VAL(0)
							     : INT_VAL(a % b);
			break;
		default:
			return false;
		}
	} else {
		// Either one of the operands is a float
		const double a = currentIsFloat ? AS_FLOAT(*target)
						: (double)AS_INT(*target);
		const double b = operandIsFloat ? AS_FLOAT(operand)
						: (double)AS_INT(operand);

		switch (op) {
		case OP_SET_LOCAL_PLUS:
		case OP_SET_UPVALUE_PLUS:
		case OP_SET_GLOBAL_PLUS:
			result = FLOAT_VAL(a + b);
			break;
		case OP_SET_LOCAL_MINUS:
		case OP_SET_UPVALUE_MINUS:
		case OP_SET_GLOBAL_MINUS:
			result = FLOAT_VAL(a - b);
			break;
		case OP_SET_LOCAL_STAR:
		case OP_SET_UPVALUE_STAR:
		case OP_SET_GLOBAL_STAR:
			result = FLOAT_VAL(a * b);
			break;
		case OP_SET_LOCAL_SLASH:
		case OP_SET_UPVALUE_SLASH:
		case OP_SET_GLOBAL_SLASH:
			if (b == 0.0) {
				runtime_panic(currentModuleRecord, false, MATH,
					      "Division by zero.");
				return false;
			}
			result = FLOAT_VAL(a / b);
			break;
		default:
			runtime_panic(
				currentModuleRecord, false, TYPE,
				"Integer operations require integer operands.");
			return false;
		}
	}

	*target = result;
	return true;
}
