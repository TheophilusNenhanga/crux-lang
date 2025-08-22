#include "error.h"

#include "../memory.h"
#include "../panic.h"

ObjectResult *error_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	ObjectModuleRecord* module_record = vm->current_module_record;
	const Value message = args[0];
	ObjectString *errorMessage = to_string(vm, message);
	push(module_record, OBJECT_VAL(errorMessage));
	ObjectError *error = new_error(vm, errorMessage, RUNTIME, false);
	push(module_record, OBJECT_VAL(error));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(error));
	pop(module_record);
	pop(module_record);
	return res;
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
	push(vm->current_module_record, OBJECT_VAL(errorMessage));
	ObjectError *error = new_error(vm, errorMessage, RUNTIME, true);
	push(vm->current_module_record, OBJECT_VAL(error));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(error));
	pop(vm->current_module_record);
	pop(vm->current_module_record);
	return res;
}

ObjectResult *assert_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args)
{
	ObjectModuleRecord* module_record = vm->current_module_record;
	if (!IS_BOOL(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to assert: <condition> must be of type 'bool'.", TYPE);
	}
	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to assert: <message> must be of type 'string'.", TYPE);
	}

	const bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_CRUX_STRING(args[1]);
	push(module_record, OBJECT_VAL(message));

	if (result == false) {
		ObjectError *error = new_error(vm, message, ASSERT, true);
		push(vm->current_module_record, OBJECT_VAL(error));
		ObjectResult* res =  new_error_result(vm, error);
		pop(vm->current_module_record);
		pop(vm->current_module_record);
		return res;
	}

	ObjectResult* res = new_ok_result(vm, NIL_VAL);
	pop(vm->current_module_record);
	pop(vm->current_module_record);
	return res;
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
	ObjectModuleRecord* module_record = vm->current_module_record;

	switch (error->type) {
	case SYNTAX: {
		ObjectString *type = copy_string(vm, "<syntax error>", 14);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case MATH: {
		ObjectString *type = copy_string(vm, "<math error>", 12);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case BOUNDS: {
		ObjectString *type = copy_string(vm, "<bounds error>", 14);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case RUNTIME: {
		ObjectString *type = copy_string(vm, "<runtime error>", 14);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case TYPE: {
		ObjectString *type = copy_string(vm, "<type error>", 12);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case LOOP_EXTENT: {
		ObjectString *type = copy_string(vm, "<loop extent error>", 19);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case LIMIT: {
		ObjectString *type = copy_string(vm, "<limit error>", 13);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case BRANCH_EXTENT: {
		ObjectString *type = copy_string(vm, "<branch extent error>",
						21);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case CLOSURE_EXTENT: {
		ObjectString *type = copy_string(vm, "<closure extent error>",
						22);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case LOCAL_EXTENT: {
		ObjectString *type = copy_string(vm, "<local extent error>", 20);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case ARGUMENT_EXTENT: {
		ObjectString *type = copy_string(vm, "<argument extent error>",
						23);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case NAME: {
		ObjectString *type = copy_string(vm, "<name error>", 12);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case COLLECTION_EXTENT: {
		ObjectString *type = copy_string(vm, "<collection extent error>",
						25);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case VARIABLE_EXTENT: {
		ObjectString *type = copy_string(vm, "<variable extent error>",
						23);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case RETURN_EXTENT: {
		ObjectString *type = copy_string(vm, "<return extent error>",
						21);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case ARGUMENT_MISMATCH: {
		ObjectString *type = copy_string(vm, "<argument mismatch error>",
						22);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case STACK_OVERFLOW: {
		ObjectString *type = copy_string(vm, "<stack overflow error>",
						22);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case COLLECTION_GET: {
		ObjectString *type = copy_string(vm, "<collection get error>",
						22);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case COLLECTION_SET: {
		ObjectString *type = copy_string(vm, "<collection set error>",
						22);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case UNPACK_MISMATCH: {
		ObjectString *type = copy_string(vm, "<unpack mismatch error>",
						23);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case MEMORY: {
		ObjectString *type = copy_string(vm, "<memory error>", 14);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case VALUE: {
		ObjectString *type = copy_string(vm, "<value error>", 13);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case ASSERT: {
		ObjectString *type = copy_string(vm, "<assert error>", 14);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	case IMPORT_EXTENT: {
		ObjectString *type = copy_string(vm, "<import extent error>",
						21);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}
	case IO: {
		ObjectString *type = copy_string(vm, "<io error>", 10);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
	}

	default: {
		ObjectString *type = copy_string(vm, "<crux error>", 12);
		push(module_record, OBJECT_VAL(type));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(type));
		pop(module_record);
		return res;
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
	push(vm->current_module_record, OBJECT_VAL(message));
	ObjectError *error = new_error(vm, message, RUNTIME, false);
	push(vm->current_module_record, OBJECT_VAL(error));
	ObjectResult* res = new_error_result(vm, error);
	pop(vm->current_module_record);
	pop(vm->current_module_record);
	return res;
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
