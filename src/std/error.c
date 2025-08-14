#include "error.h"

#include "../memory.h"

ObjectResult *error_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	const Value message = args[0];
	ObjectString *errorMessage = to_string(vm, message);
	ObjectError *error = new_error(vm, errorMessage, RUNTIME, false);
	return new_ok_result(vm, OBJECT_VAL(error));
}

ObjectResult *panic_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	const Value value = args[0];

	if (IS_CRUX_ERROR(value)) {
		ObjectError *error = AS_CRUX_ERROR(value);
		error->is_panic = true;
		return new_error_result(vm, error);
	}
	ObjectString *errorMessage = to_string(vm, value);
	ObjectError *error = new_error(vm, errorMessage, RUNTIME, true);
	return new_error_result(vm, error);
}

ObjectResult *assert_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args)
{
	if (!IS_BOOL(args[0])) {
		ObjectError *error =
			new_error(vm,
				 copy_string(vm,
					    "Failed to assert: <condition> "
					    "must be of type 'bool'.",
					    53),
				 TYPE, true);
		return new_error_result(vm, error);
	}
	if (!IS_CRUX_STRING(args[1])) {
		ObjectError *error =
			new_error(vm,
				 copy_string(vm,
					    "Failed to assert: <message> must "
					    "be of type 'string'.",
					    53),
				 TYPE, true);
		return new_error_result(vm, error);
	}

	const bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_CRUX_STRING(args[1]);

	if (result == false) {
		ObjectError *error = new_error(vm, message, ASSERT, true);
		return new_error_result(vm, error);
	}

	return new_ok_result(vm, NIL_VAL);
}

Value error_message_method(VM *vm __attribute__((unused)),
			   int arg_count __attribute__((unused)),
			   const Value *args)
{
	const ObjectError *error = AS_CRUX_ERROR(args[0]);
	return OBJECT_VAL(error->message);
}

ObjectResult *error_type_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	const ObjectError *error = AS_CRUX_ERROR(args[0]);

	switch (error->type) {
	case SYNTAX: {
		ObjectString *type = copy_string(vm, "<syntax error>", 14);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case MATH: {
		ObjectString *type = copy_string(vm, "<math error>", 12);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case BOUNDS: {
		ObjectString *type = copy_string(vm, "<bounds error>", 14);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case RUNTIME: {
		ObjectString *type = copy_string(vm, "<runtime error>", 14);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case TYPE: {
		ObjectString *type = copy_string(vm, "<type error>", 12);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case LOOP_EXTENT: {
		ObjectString *type = copy_string(vm, "<loop extent error>", 19);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case LIMIT: {
		ObjectString *type = copy_string(vm, "<limit error>", 13);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case BRANCH_EXTENT: {
		ObjectString *type = copy_string(vm, "<branch extent error>",
						21);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case CLOSURE_EXTENT: {
		ObjectString *type = copy_string(vm, "<closure extent error>",
						22);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case LOCAL_EXTENT: {
		ObjectString *type = copy_string(vm, "<local extent error>", 20);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case ARGUMENT_EXTENT: {
		ObjectString *type = copy_string(vm, "<argument extent error>",
						23);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case NAME: {
		ObjectString *type = copy_string(vm, "<name error>", 12);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case COLLECTION_EXTENT: {
		ObjectString *type = copy_string(vm, "<collection extent error>",
						25);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case VARIABLE_EXTENT: {
		ObjectString *type = copy_string(vm, "<variable extent error>",
						23);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case RETURN_EXTENT: {
		ObjectString *type = copy_string(vm, "<return extent error>",
						21);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case ARGUMENT_MISMATCH: {
		ObjectString *type = copy_string(vm, "<argument mismatch error>",
						22);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case STACK_OVERFLOW: {
		ObjectString *type = copy_string(vm, "<stack overflow error>",
						22);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case COLLECTION_GET: {
		ObjectString *type = copy_string(vm, "<collection get error>",
						22);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case COLLECTION_SET: {
		ObjectString *type = copy_string(vm, "<collection set error>",
						22);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case UNPACK_MISMATCH: {
		ObjectString *type = copy_string(vm, "<unpack mismatch error>",
						23);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case MEMORY: {
		ObjectString *type = copy_string(vm, "<memory error>", 14);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case VALUE: {
		ObjectString *type = copy_string(vm, "<value error>", 13);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case ASSERT: {
		ObjectString *type = copy_string(vm, "<assert error>", 14);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	case IMPORT_EXTENT: {
		ObjectString *type = copy_string(vm, "<import extent error>",
						21);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	case IO: {
		ObjectString *type = copy_string(vm, "<io error>", 10);
		return new_ok_result(vm, OBJECT_VAL(type));
	}

	default: {
		ObjectString *type = copy_string(vm, "<crux error>", 12);
		return new_ok_result(vm, OBJECT_VAL(type));
	}
	}
}

ObjectResult *err_function(VM *vm, int arg_count __attribute__((unused)),
			   const Value *args)
{
	if (IS_CRUX_OBJECT(args[0]) && IS_CRUX_ERROR(args[0])) {
		return new_error_result(vm, AS_CRUX_ERROR(args[0]));
	}

	ObjectString *message = to_string(vm, args[0]);
	ObjectError *error = new_error(vm, message, RUNTIME, false);
	return new_error_result(vm, error);
}

ObjectResult *ok_function(VM *vm, int arg_count __attribute__((unused)),
			  const Value *args)
{
	return new_ok_result(vm, args[0]);
}

// arg0 - Result
Value unwrap_function(VM *vm __attribute__((unused)),
		      int arg_count __attribute__((unused)), const Value *args)
{
	const ObjectResult *result = AS_CRUX_RESULT(args[0]);
	if (result->is_ok) {
		return result->as.value;
	}
	return OBJECT_VAL(result->as.error);
}
