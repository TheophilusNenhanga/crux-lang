#include "stdlib/error.h"

#include "garbage_collector.h"
#include "panic.h"
#include "value.h"

/**
 * Creates an error value with a message
 * arg0 -> message: Any
 * Returns Error
 */
Value error_function(VM *vm, const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	const Value message = args[0];
	ObjectString *errorMessage = to_string(vm, message);
	push(module_record, OBJECT_VAL(errorMessage));
	ObjectError *error = new_error(vm, errorMessage, RUNTIME, false);
	pop(module_record);
	return OBJECT_VAL(error);
}

/**
 * Asserts that a condition is true, creates an error with message if false
 * arg0 -> condition: Bool
 * arg1 -> message: String
 * Returns Nil
 */
Value assert_function(VM *vm, const Value *args)
{
	const bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_CRUX_STRING(args[1]);

	if (result == false) {
		runtime_panic(vm->current_module_record, true, ASSERT,
			      message->chars);
		return NIL_VAL;
	}
	// returning different types because we panicked anyway!
	return NIL_VAL;
}

/**
 * Returns the error message from an Error
 * arg0 -> error: Error
 * Returns String
 */
Value error_message_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectError *error = AS_CRUX_ERROR(args[0]);
	return OBJECT_VAL(error->message);
}

/**
 * Returns the type of error as a string
 * arg0 -> error: Error
 * Returns String
 */
Value error_type_method(VM *vm, const Value *args)
{
	const ObjectError *error = AS_CRUX_ERROR(args[0]);

	switch (error->type) {
	case SYNTAX: {
		ObjectString *type = copy_string(vm, "<syntax error>", 14);
		return OBJECT_VAL(type);
	}

	case MATH: {
		ObjectString *type = copy_string(vm, "<math error>", 12);
		return OBJECT_VAL(type);
	}

	case BOUNDS: {
		ObjectString *type = copy_string(vm, "<bounds error>", 14);
		return OBJECT_VAL(type);
	}
	case RUNTIME: {
		ObjectString *type = copy_string(vm, "<runtime error>", 14);
		return OBJECT_VAL(type);
	}

	case TYPE: {
		ObjectString *type = copy_string(vm, "<type error>", 12);
		return OBJECT_VAL(type);
	}

	case LOOP_EXTENT: {
		ObjectString *type = copy_string(vm, "<loop extent error>", 19);
		return OBJECT_VAL(type);
	}

	case LIMIT: {
		ObjectString *type = copy_string(vm, "<limit error>", 13);
		return OBJECT_VAL(type);
	}

	case BRANCH_EXTENT: {
		ObjectString *type = copy_string(vm, "<branch extent error>",
						 21);
		return OBJECT_VAL(type);
	}
	case CLOSURE_EXTENT: {
		ObjectString *type = copy_string(vm, "<closure extent error>",
						 22);
		return OBJECT_VAL(type);
	}

	case LOCAL_EXTENT: {
		ObjectString *type = copy_string(vm, "<local extent error>",
						 20);
		return OBJECT_VAL(type);
	}
	case ARGUMENT_EXTENT: {
		ObjectString *type = copy_string(vm, "<argument extent error>",
						 23);
		return OBJECT_VAL(type);
	}

	case NAME: {
		ObjectString *type = copy_string(vm, "<name error>", 12);
		return OBJECT_VAL(type);
	}

	case COLLECTION_EXTENT: {
		ObjectString *type = copy_string(vm,
						 "<collection extent error>",
						 25);
		return OBJECT_VAL(type);
	}
	case VARIABLE_EXTENT: {
		ObjectString *type = copy_string(vm, "<variable extent error>",
						 23);
		return OBJECT_VAL(type);
	}

	case RETURN_EXTENT: {
		ObjectString *type = copy_string(vm, "<return extent error>",
						 21);
		return OBJECT_VAL(type);
	}

	case ARGUMENT_MISMATCH: {
		ObjectString *type = copy_string(vm,
						 "<argument mismatch error>",
						 22);
		return OBJECT_VAL(type);
	}

	case STACK_OVERFLOW: {
		ObjectString *type = copy_string(vm, "<stack overflow error>",
						 22);
		return OBJECT_VAL(type);
	}
	case COLLECTION_GET: {
		ObjectString *type = copy_string(vm, "<collection get error>",
						 22);
		return OBJECT_VAL(type);
	}

	case COLLECTION_SET: {
		ObjectString *type = copy_string(vm, "<collection set error>",
						 22);
		return OBJECT_VAL(type);
	}

	case MEMORY: {
		ObjectString *type = copy_string(vm, "<memory error>", 14);
		return OBJECT_VAL(type);
	}

	case VALUE: {
		ObjectString *type = copy_string(vm, "<value error>", 13);
		return OBJECT_VAL(type);
	}

	case ASSERT: {
		ObjectString *type = copy_string(vm, "<assert error>", 14);
		return OBJECT_VAL(type);
	}

	case IMPORT_EXTENT: {
		ObjectString *type = copy_string(vm, "<import extent error>",
						 21);
		return OBJECT_VAL(type);
	}
	case IMPORT: {
		ObjectString *type = copy_string(vm, "<import error>", 14);
		return OBJECT_VAL(type);
	}
	case IO: {
		ObjectString *type = copy_string(vm, "<io error>", 10);
		return OBJECT_VAL(type);
	}

	default: {
		ObjectString *type = copy_string(vm, "<crux error>", 12);
		return OBJECT_VAL(type);
	}
	}
}

/**
 * Creates an error Result with a message
 * arg0 -> message: String
 * Returns Result<Nothing>
 */
Value err_function(VM *vm, const Value *args)
{
	ObjectString *message = to_string(vm, args[0]);
	ObjectError *error = new_error(vm, message, RUNTIME, false);
	push(vm->current_module_record, OBJECT_VAL(error));
	ObjectResult *res = new_error_result(vm, error);
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Wraps a value in an Ok Result
 * arg0 -> value: Any
 * Returns Result
 */
Value ok_function(VM *vm, const Value *args)
{
	return OBJECT_VAL(new_ok_result(vm, args[0]));
}

/**
 * Unwraps a Result, returning the value if Ok or the error if Err
 * arg0 -> result: Result
 * Returns Any (value or Error)
 */
Value unwrap_function(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectResult *result = AS_CRUX_RESULT(args[0]);
	if (result->is_ok) {
		return result->as.value;
	}
	return OBJECT_VAL(result->as.error);
}
