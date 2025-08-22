#include "vm_helpers.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../compiler.h"
#include "../memory.h"
#include "../object.h"
#include "../panic.h"
#include "../std/std.h"
#include "../table.h"
#include "../value.h"
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
	return vm->struct_instance_stack.structs[vm->struct_instance_stack.count];
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

void pop_two(ObjectModuleRecord *moduleRecord)
{
	pop(moduleRecord);
	pop(moduleRecord);
}

void pop_push(ObjectModuleRecord *moduleRecord, const Value value)
{
	pop(moduleRecord);
	push(moduleRecord, value);
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
	if (IS_CRUX_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
		case OBJECT_CLOSURE:
			return call(current_module_record,
				    AS_CRUX_CLOSURE(callee), arg_count);
		case OBJECT_NATIVE_METHOD: {
			const ObjectNativeMethod *native =
				AS_CRUX_NATIVE_METHOD(callee);
			if (arg_count != native->arity) {
				runtime_panic(current_module_record, false,
					     ARGUMENT_MISMATCH,
					     "Expected %d argument(s), got %d",
					     native->arity, arg_count);
				return false;
			}

			ObjectResult *result = native->function(
				vm, arg_count,
				current_module_record->stack_top - arg_count);

			current_module_record->stack_top -= arg_count + 1;

			if (!result->is_ok) {
				if (result->as.error->is_panic) {
					runtime_panic(current_module_record, false,
						     result->as.error->type,
						     result->as.error->message
							     ->chars);
					return false;
				}
			}

			push(current_module_record, OBJECT_VAL(result));

			return true;
		}
		case OBJECT_NATIVE_FUNCTION: {
			const ObjectNativeFunction *native =
				AS_CRUX_NATIVE_FUNCTION(callee);
			if (arg_count != native->arity) {
				runtime_panic(current_module_record, false,
					     ARGUMENT_MISMATCH,
					     "Expected %d argument(s), got %d",
					     native->arity, arg_count);
				return false;
			}

			ObjectResult *result = native->function(
				vm, arg_count,
				current_module_record->stack_top - arg_count);
			current_module_record->stack_top -= arg_count + 1;

			if (!result->is_ok) {
				if (result->as.error->is_panic) {
					runtime_panic(current_module_record, false,
						     result->as.error->type,
						     result->as.error->message
							     ->chars);
					return false;
				}
			}

			push(current_module_record, OBJECT_VAL(result));
			return true;
		}
		case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
			const ObjectNativeInfallibleFunction *native =
				AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(callee);
			if (arg_count != native->arity) {
				runtime_panic(current_module_record, false,
					     ARGUMENT_MISMATCH,
					     "Expected %d argument(s), got %d",
					     native->arity, arg_count);
				return false;
			}

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
			if (arg_count != native->arity) {
				runtime_panic(current_module_record, false,
					     ARGUMENT_MISMATCH,
					     "Expected %d argument(s), got %d",
					     native->arity, arg_count);
				return false;
			}

			const Value result = native->function(
				vm, arg_count,
				current_module_record->stack_top - arg_count);
			current_module_record->stack_top -= arg_count + 1;
			push(current_module_record, result);
			return true;
		}
		default:
			break;
		}
	}
	runtime_panic(current_module_record, false, TYPE,
		     "Only functions can be called.");
	return false;
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

	const Object *object = AS_CRUX_OBJECT(receiver);
	arg_count++; // for the value that the method will act on
	switch (OBJECT_TYPE(receiver)) {
	case OBJECT_STRING: {
		Value value;
		if (table_get(&vm->string_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_ARRAY: {
		Value value;
		if (table_get(&vm->array_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_FILE: {
		Value value;
		if (table_get(&vm->file_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_ERROR: {
		Value value;
		if (table_get(&vm->error_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_TABLE: {
		Value value;
		if (table_get(&vm->table_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_RANDOM: {
		Value value;
		if (table_get(&vm->random_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_VEC2: {
		Value value;
		if (table_get(&vm->vec2_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_VEC3: {
		Value value;
		if (table_get(&vm->vec3_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_RESULT: {
		Value value;
		if (table_get(&vm->result_type, name, &value)) {
			return handle_invoke(vm, arg_count, receiver, original,
					    value);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	case OBJECT_STRUCT_INSTANCE: {
		arg_count--;
		const ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(
			receiver);
		Value indexValue;
		if (table_get(&instance->struct_type->fields, name,
			     &indexValue)) {
			return call_value(
				vm,
				instance->fields[(uint16_t)AS_INT(indexValue)],
				arg_count);
		}
		runtime_panic(current_module_record, false, NAME,
			     "Undefined method '%s'.", name->chars);
		return false;
	}
	default: {
		runtime_panic(current_module_record, false, TYPE,
			     "Only instances have methods");
		return false;
	}
	}
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

void init_vm(VM *vm, const int argc, const char **argv)
{
	const bool isRepl = argc == 1 ? true : false;
	vm->gc_status = PAUSED;
	vm->objects = NULL;
	vm->bytes_allocated = 0;
	vm->next_gc = 1024 * 1024;
	vm->gray_count = 0;
	vm->gray_capacity = 0;
	vm->gray_stack = NULL;
	vm->struct_instance_stack.structs = NULL;

	vm->current_module_record = (ObjectModuleRecord *)malloc(
		sizeof(ObjectModuleRecord));
	if (vm->current_module_record == NULL) {
		fprintf(stderr, "Fatal Error: Could not allocate memory for "
				"module record.\nShutting Down!\n");
		exit(1);
	}
	init_module_record(vm->current_module_record, NULL, isRepl, true);

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
	free(vm);
}

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

		switch (operation) {
		case OP_ADD: {
			const int64_t result = (int64_t)intA + (int64_t)intB;
			pop_two(current_module_record);
			if (result >= INT32_MIN && result <= INT32_MAX) {
				push(current_module_record,
				     INT_VAL((int32_t)result));
			} else {
				push(current_module_record,
				     FLOAT_VAL((double)result)); // Promote on
								 // overflow
			}
			break;
		}
		case OP_SUBTRACT: {
			const int64_t result = (int64_t)intA - (int64_t)intB;
			pop_two(current_module_record);
			if (result >= INT32_MIN && result <= INT32_MAX) {
				push(current_module_record,
				     INT_VAL((int32_t)result));
			} else {
				push(current_module_record,
				     FLOAT_VAL((double)result)); // Promote on
								 // overflow
			}
			break;
		}
		case OP_MULTIPLY: {
			const int64_t result = (int64_t)intA * (int64_t)intB;
			pop_two(current_module_record);
			if (result >= INT32_MIN && result <= INT32_MAX) {
				push(current_module_record,
				     INT_VAL((int32_t)result));
			} else {
				push(current_module_record,
				     FLOAT_VAL((double)result)); // Promote on
								 // overflow
			}
			break;
		}
		case OP_DIVIDE: {
			if (intB == 0) {
				runtime_panic(current_module_record, false, MATH,
					     "Division by zero.");
				return false;
			}
			pop_two(current_module_record);
			push(current_module_record,
			     FLOAT_VAL((double)intA / (double)intB));
			break;
		}
		case OP_INT_DIVIDE: {
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
			break;
		}
		case OP_MODULUS: {
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
			break;
		}
		case OP_LEFT_SHIFT: {
			if (intB < 0 || intB >= 32) {
				runtime_panic(
					current_module_record, false, RUNTIME,
					"Invalid shift amount (%d) for <<.",
					intB);
				return false;
			}
			pop_two(current_module_record);
			push(current_module_record, INT_VAL(intA << intB));
			break;
		}
		case OP_RIGHT_SHIFT: {
			if (intB < 0 || intB >= 32) {
				runtime_panic(
					current_module_record, false, RUNTIME,
					"Invalid shift amount (%d) for >>.",
					intB);
				return false;
			}
			pop_two(current_module_record);
			push(current_module_record, INT_VAL(intA >> intB));
			break;
		}
		case OP_POWER: {
			// Promote int^int to float
			pop_two(current_module_record);
			push(current_module_record, FLOAT_VAL(pow(intA, intB)));
			break;
		}
		case OP_LESS:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(intA < intB));
			break;
		case OP_LESS_EQUAL:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(intA <= intB));
			break;
		case OP_GREATER:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(intA > intB));
			break;
		case OP_GREATER_EQUAL:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(intA >= intB));
			break;

		default:
			runtime_panic(
				current_module_record, false, RUNTIME,
				"Unknown binary operation %d for int, int.",
				operation);
			return false;
		}
	} else {
		const double doubleA = aIsFloat ? AS_FLOAT(a)
						: (double)AS_INT(a);
		const double doubleB = bIsFloat ? AS_FLOAT(b)
						: (double)AS_INT(b);

		switch (operation) {
		case OP_ADD:
			pop_two(current_module_record);
			push(current_module_record, FLOAT_VAL(doubleA + doubleB));
			break;
		case OP_SUBTRACT:
			pop_two(current_module_record);
			push(current_module_record, FLOAT_VAL(doubleA - doubleB));
			break;
		case OP_MULTIPLY:
			pop_two(current_module_record);
			push(current_module_record, FLOAT_VAL(doubleA * doubleB));
			break;
		case OP_DIVIDE: {
			if (doubleB == 0.0) {
				runtime_panic(current_module_record, false, MATH,
					     "Division by zero.");
				return false;
			}
			pop_two(current_module_record);
			push(current_module_record, FLOAT_VAL(doubleA / doubleB));
			break;
		}
		case OP_POWER: {
			pop_two(current_module_record);
			push(current_module_record,
			     FLOAT_VAL(pow(doubleA, doubleB)));
			break;
		}

		case OP_LESS:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(doubleA < doubleB));
			break;
		case OP_LESS_EQUAL:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(doubleA <= doubleB));
			break;
		case OP_GREATER:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(doubleA > doubleB));
			break;
		case OP_GREATER_EQUAL:
			pop_two(current_module_record);
			push(current_module_record, BOOL_VAL(doubleA >= doubleB));
			break;

		case OP_INT_DIVIDE:
		case OP_MODULUS:
		case OP_LEFT_SHIFT:
		case OP_RIGHT_SHIFT: {
			runtime_panic(current_module_record, false, TYPE,
				     "Operands for integer operation must both "
				     "be integers.");
			return false;
		}

		default:
			runtime_panic(
				current_module_record, false, RUNTIME,
				"Unknown binary operation %d for float/mixed.",
				operation);
			return false;
		}
	}
	return true;
}

InterpretResult global_compound_operation(VM *vm, ObjectString *name,
					const OpCode opcode, char *operation)
{
	ObjectModuleRecord *current_module_record = vm->current_module_record;
	Value currentValue;
	if (!table_get(&current_module_record->globals, name, &currentValue)) {
		runtime_panic(current_module_record, false, NAME,
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

		switch (opcode) {
		case OP_SET_GLOBAL_PLUS: {
			// +=
			const int64_t result = (int64_t)icurrent +
					       (int64_t)ioperand;
			if (result >= INT32_MIN && result <= INT32_MAX) {
				resultValue = INT_VAL((int32_t)result);
			} else {
				resultValue = FLOAT_VAL((double)result);
			}
			break;
		}
		case OP_SET_GLOBAL_MINUS: {
			const int64_t result = (int64_t)icurrent -
					       (int64_t)ioperand;
			if (result >= INT32_MIN && result <= INT32_MAX) {
				resultValue = INT_VAL((int32_t)result);
			} else {
				resultValue = FLOAT_VAL((double)result);
			}
			break;
		}
		case OP_SET_GLOBAL_STAR: {
			const int64_t result = (int64_t)icurrent *
					       (int64_t)ioperand;
			if (result >= INT32_MIN && result <= INT32_MAX) {
				resultValue = INT_VAL((int32_t)result);
			} else {
				resultValue = FLOAT_VAL((double)result);
			}
			break;
		}
		case OP_SET_GLOBAL_SLASH: {
			if (ioperand == 0) {
				runtime_panic(current_module_record, false, MATH,
					     "Division by zero in '%s %s'.",
					     name->chars, operation);
				return INTERPRET_RUNTIME_ERROR;
			}
			resultValue = FLOAT_VAL((double)icurrent /
						(double)ioperand);
			break;
		}

		case OP_SET_GLOBAL_INT_DIVIDE: {
			if (ioperand == 0) {
				runtime_panic(current_module_record, false,
					     RUNTIME,
					     "Division by zero in '%s %s'.",
					     name->chars, operation);
				return INTERPRET_RUNTIME_ERROR;
			}
			if (icurrent == INT32_MIN && ioperand == -1) {
				resultValue = FLOAT_VAL(
					-(double)INT32_MIN); // Overflow
			} else {
				resultValue = INT_VAL(icurrent / ioperand);
			}
			break;
		}

		case OP_SET_GLOBAL_MODULUS: {
			if (ioperand == 0) {
				runtime_panic(current_module_record, false,
					     RUNTIME,
					     "Division by zero in '%s %s'.",
					     name->chars, operation);
				return INTERPRET_RUNTIME_ERROR;
			}
			if (icurrent == INT32_MIN && ioperand == -1) {
				resultValue = INT_VAL(0);
			} else {
				resultValue = INT_VAL(icurrent % ioperand);
			}
			break;
		}

		default:
			runtime_panic(current_module_record, false, RUNTIME,
				     "Unsupported compound assignment opcode "
				     "%d for int/int.",
				     opcode);
			return INTERPRET_RUNTIME_ERROR;
		}
	} else {
		const double dcurrent = currentIsFloat
						? AS_FLOAT(currentValue)
						: (double)AS_INT(currentValue);
		const double doperand = operandIsFloat
						? AS_FLOAT(operandValue)
						: (double)AS_INT(operandValue);

		switch (opcode) {
		case OP_SET_GLOBAL_PLUS: {
			resultValue = FLOAT_VAL(dcurrent + doperand);
			break;
		}
		case OP_SET_GLOBAL_MINUS: {
			resultValue = FLOAT_VAL(dcurrent - doperand);
			break;
		}
		case OP_SET_GLOBAL_STAR: {
			resultValue = FLOAT_VAL(dcurrent * doperand);
			break;
		}
		case OP_SET_GLOBAL_SLASH: {
			if (doperand == 0.0) {
				runtime_panic(current_module_record, false, MATH,
					     "Division by zero in '%s %s'.",
					     name->chars, operation);
				return INTERPRET_RUNTIME_ERROR;
			}
			resultValue = FLOAT_VAL(dcurrent / doperand);
			break;
		}

		case OP_SET_GLOBAL_INT_DIVIDE:
		case OP_SET_GLOBAL_MODULUS: {
			runtime_panic(current_module_record, false, TYPE,
				     "Operands for integer compound assignment "
				     "'%s' must both be "
				     "integers.",
				     operation);
			return INTERPRET_RUNTIME_ERROR;
		}
		default:
			runtime_panic(current_module_record, false, RUNTIME,
				     "Unsupported compound assignment opcode "
				     "%d for float/mixed.",
				     opcode);
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	table_set(vm, &current_module_record->globals, name, resultValue);
	return INTERPRET_OK;
}

bool check_previous_instruction(const CallFrame *frame, const int instructions_ago,
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
				  const int arg_count, InterpretResult *result)
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

		switch (type) {
		case OBJECT_STRING:
			return OBJECT_VAL(copy_string(vm, "string", 6));

		case OBJECT_FUNCTION:
		case OBJECT_NATIVE_FUNCTION:
		case OBJECT_NATIVE_METHOD:
		case OBJECT_CLOSURE:
		case OBJECT_NATIVE_INFALLIBLE_FUNCTION:
		case OBJECT_NATIVE_INFALLIBLE_METHOD:
			return OBJECT_VAL(copy_string(vm, "function", 8));

		case OBJECT_UPVALUE: {
			const ObjectUpvalue *upvalue = AS_CRUX_UPVALUE(value);
			return typeof_value(vm, upvalue->closed);
		}

		case OBJECT_ARRAY:
			return OBJECT_VAL(copy_string(vm, "array", 5));

		case OBJECT_TABLE:
			return OBJECT_VAL(copy_string(vm, "table", 5));

		case OBJECT_ERROR:
			return OBJECT_VAL(copy_string(vm, "error", 5));

		case OBJECT_RESULT:
			return OBJECT_VAL(copy_string(vm, "result", 6));

		case OBJECT_RANDOM:
			return OBJECT_VAL(copy_string(vm, "random", 6));

		case OBJECT_FILE:
			return OBJECT_VAL(copy_string(vm, "file", 4));

		case OBJECT_MODULE_RECORD:
			return OBJECT_VAL(copy_string(vm, "module", 6));
		case OBJECT_STATIC_ARRAY:
			return OBJECT_VAL(copy_string(vm, "static array", 12));
		case OBJECT_STATIC_TABLE:
			return OBJECT_VAL(copy_string(vm, "static table", 12));
		case OBJECT_STRUCT:
			return OBJECT_VAL(copy_string(vm, "struct", 6));
		case OBJECT_STRUCT_INSTANCE:
			return OBJECT_VAL(
				copy_string(vm, "struct instance", 15));
		case OBJECT_VEC2:
			return OBJECT_VAL(copy_string(vm, "vec2", 4));
		case OBJECT_VEC3:
			return OBJECT_VAL(copy_string(vm, "vec3", 4));
		}
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
