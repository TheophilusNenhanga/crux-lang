#include "error.h"

#include "../memory.h"

ObjectResult* errorFunction(VM *vm, int argCount, Value *args) {
	Value message = args[0];
	ObjectString *errorMessage = toString(vm, message);
	ObjectError *error = newError(vm, errorMessage, RUNTIME, false);
	return stellaOk(vm, OBJECT_VAL(error));
}

ObjectResult* panicFunction(VM *vm, int argCount, Value *args) {
	Value value = args[0];

	if (IS_CRUX_ERROR(value)) {
		ObjectError *error = AS_CRUX_ERROR(value);
		error->isPanic = true;
		return stellaErr(vm, error);
	}
	ObjectString *errorMessage = toString(vm, value);
	ObjectError *error = newError(vm, errorMessage, RUNTIME, true);
	return stellaErr(vm, error);
}

ObjectResult* assertFunction(VM *vm, int argCount, Value *args) {	
	if (!IS_BOOL(args[0])) {
		ObjectError *error =
				newError(vm, copyString(vm, "Failed to assert: <condition> must be of type 'bool'.", 53), TYPE, true);
		return stellaErr(vm, error);
	}
	if (!IS_CRUX_STRING(args[1])) {
		ObjectError *error =
				newError(vm, copyString(vm, "Failed to assert: <message> must be of type 'string'.", 53), TYPE, true);
		return stellaErr(vm, error);
	}

	bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_CRUX_STRING(args[1]);

	if (result == false) {
		ObjectError *error = newError(vm, message, ASSERT, true);
		return stellaErr(vm, error);
	}

	return stellaOk(vm, NIL_VAL);
}

ObjectResult* errorMessageMethod(VM *vm, int argCount, Value *args) {
	ObjectError *error = AS_CRUX_ERROR(args[0]);

	return stellaOk(vm, OBJECT_VAL(error->message));
}

ObjectResult* errorTypeMethod(VM *vm, int argCount, Value *args) {
	ObjectError *error = AS_CRUX_ERROR(args[0]);

	switch (error->type) {
		case SYNTAX: {
			ObjectString *type = copyString(vm, "<syntax error>", 14);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case DIVISION_BY_ZERO: {
			ObjectString *type = copyString(vm, "<zero division error>", 21);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case INDEX_OUT_OF_BOUNDS: {
			ObjectString *type = copyString(vm, "<index error>", 13);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case RUNTIME: {
			ObjectString *type = copyString(vm, "<runtime error>", 14);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case TYPE: {
			ObjectString *type = copyString(vm, "<type error>", 12);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case LOOP_EXTENT: {
			ObjectString *type = copyString(vm, "<loop extent error>", 19);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case LIMIT: {
			ObjectString *type = copyString(vm, "<limit error>", 13);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case BRANCH_EXTENT: {
			ObjectString *type = copyString(vm, "<branch extent error>", 21);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case CLOSURE_EXTENT: {
			ObjectString *type = copyString(vm, "<closure extent error>", 22);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case LOCAL_EXTENT: {
			ObjectString *type = copyString(vm, "<local extent error>", 20);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case ARGUMENT_EXTENT: {
			ObjectString *type = copyString(vm, "<argument extent error>", 23);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case NAME: {
			ObjectString *type = copyString(vm, "<name error>", 12);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case COLLECTION_EXTENT: {
			ObjectString *type = copyString(vm, "<collection extent error>", 25);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case VARIABLE_EXTENT: {
			ObjectString *type = copyString(vm, "<variable extent error>", 23);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case VARIABLE_DECLARATION_MISMATCH: {
			ObjectString *type = copyString(vm, "<variable mismatch error>", 25);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case RETURN_EXTENT: {
			ObjectString *type = copyString(vm, "<return extent error>", 21);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case ARGUMENT_MISMATCH: {
			ObjectString *type = copyString(vm, "<argument mismatch error>", 22);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case STACK_OVERFLOW: {
			ObjectString *type = copyString(vm, "<stack overflow error>", 22);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case COLLECTION_GET: {
			ObjectString *type = copyString(vm, "<collection get error>", 22);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case COLLECTION_SET: {
			ObjectString *type = copyString(vm, "<collection set error>", 22);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case UNPACK_MISMATCH: {
			ObjectString *type = copyString(vm, "<unpack mismatch error>", 23);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case MEMORY: {
			ObjectString *type = copyString(vm, "<memory error>", 14);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case VALUE: {
			ObjectString *type = copyString(vm, "<value error>", 13);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case ASSERT: {
			ObjectString *type = copyString(vm, "<assert error>", 14);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		case IMPORT_EXTENT: {
			ObjectString *type = copyString(vm, "<import extent error>", 21);
			return stellaOk(vm, OBJECT_VAL(type));
		}
		case IO: {
			ObjectString *type = copyString(vm, "<io error>", 10);
			return stellaOk(vm, OBJECT_VAL(type));
		}

		default: {
			ObjectString *type = copyString(vm, "<stella error>", 14);
			return stellaOk(vm, OBJECT_VAL(type));
		}
	}
}

ObjectResult* errFunction(VM *vm, int argCount, Value *args) {

	if (IS_CRUX_OBJECT(args[0]) && IS_CRUX_ERROR(args[0])) {
		return stellaErr(vm, AS_CRUX_ERROR(args[0]));
	}

	ObjectString* message = toString(vm, args[0]);
	ObjectError* error = newError(vm, message, RUNTIME, false);
	return stellaErr(vm, error);
}

ObjectResult* okFunction(VM *vm, int argCount, Value *args) {
	return stellaOk(vm, args[0]);
}
