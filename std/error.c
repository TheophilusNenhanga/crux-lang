#include "error.h"

#include "../memory.h"

ObjectResult* errorNative(VM *vm, int argCount, Value *args) {
	Value message = args[0];
	ObjectString *errorMessage = toString(vm, message);
	ObjectError *error = newError(vm, errorMessage, RUNTIME, false);
	nativeReturn.values[0] = OBJECT_VAL(error);
	return nativeReturn;
}

ObjectResult* panicNative(VM *vm, int argCount, Value *args) {
	Value value = args[0];

	if (IS_ERROR(value)) {
		ObjectError *error = AS_ERROR(value);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}
	ObjectString *errorMessage = toString(vm, value);
	ObjectError *error = newError(vm, errorMessage, RUNTIME, true);
	nativeReturn.values[0] = OBJECT_VAL(error);
	return nativeReturn;
}

ObjectResult* assertNative(VM *vm, int argCount, Value *args) {	if (!IS_BOOL(args[0])) {
		ObjectError *error =
				newError(vm, copyString(vm, "Failed to assert: <condition> must be of type 'bool'.", 53), TYPE, true);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}
	if (!IS_STRING(args[1])) {
		ObjectError *error =
				newError(vm, copyString(vm, "Failed to assert: <message> must be of type 'string'.", 53), TYPE, true);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}

	bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_STRING(args[1]);

	if (result == false) {
		ObjectError *error = newError(vm, message, ASSERT, true);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}

	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}

ObjectResult* errorMessageMethod(VM *vm, int argCount, Value *args) {
	ObjectError *error = AS_ERROR(args[0]);

	returnValue.values[0] = OBJECT_VAL(error->message);
	return returnValue;
}

ObjectResult* errorTypeMethod(VM *vm, int argCount, Value *args) {
	ObjectError *error = AS_ERROR(args[0]);

	switch (error->type) {
		case SYNTAX: {
			ObjectString *type = copyString(vm, "<syntax error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case DIVISION_BY_ZERO: {
			ObjectString *type = copyString(vm, "<zero division error>", 21);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case INDEX_OUT_OF_BOUNDS: {
			ObjectString *type = copyString(vm, "<index error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case RUNTIME: {
			ObjectString *type = copyString(vm, "<runtime error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case TYPE: {
			ObjectString *type = copyString(vm, "<type error>", 12);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LOOP_EXTENT: {
			ObjectString *type = copyString(vm, "<loop extent error>", 19);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LIMIT: {
			ObjectString *type = copyString(vm, "<limit error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case BRANCH_EXTENT: {
			ObjectString *type = copyString(vm, "<branch extent error>", 21);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case CLOSURE_EXTENT: {
			ObjectString *type = copyString(vm, "<closure extent error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LOCAL_EXTENT: {
			ObjectString *type = copyString(vm, "<local extent error>", 20);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case ARGUMENT_EXTENT: {
			ObjectString *type = copyString(vm, "<argument extent error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case NAME: {
			ObjectString *type = copyString(vm, "<name error>", 12);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case COLLECTION_EXTENT: {
			ObjectString *type = copyString(vm, "<collection extent error>", 25);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case VARIABLE_EXTENT: {
			ObjectString *type = copyString(vm, "<variable extent error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case VARIABLE_DECLARATION_MISMATCH: {
			ObjectString *type = copyString(vm, "<variable mismatch error>", 25);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case RETURN_EXTENT: {
			ObjectString *type = copyString(vm, "<return extent error>", 21);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case ARGUMENT_MISMATCH: {
			ObjectString *type = copyString(vm, "<argument mismatch error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case STACK_OVERFLOW: {
			ObjectString *type = copyString(vm, "<stack overflow error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case COLLECTION_GET: {
			ObjectString *type = copyString(vm, "<collection get error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case COLLECTION_SET: {
			ObjectString *type = copyString(vm, "<collection set error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case UNPACK_MISMATCH: {
			ObjectString *type = copyString(vm, "<unpack mismatch error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case MEMORY: {
			ObjectString *type = copyString(vm, "<memory error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case VALUE: {
			ObjectString *type = copyString(vm, "<value error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case ASSERT: {
			ObjectString *type = copyString(vm, "<assert error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case IMPORT_EXTENT: {
			ObjectString *type = copyString(vm, "<import extent error>", 21);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case IO: {
			ObjectString *type = copyString(vm, "<io error>", 10);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		default: {
			ObjectString *type = copyString(vm, "<stella error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
	}
	return returnValue;
}

ObjectResult* _err(VM *vm, int argCount, Value *args) {

	if (IS_OBJECT(args[0]) && IS_ERROR(args[0])) {
		ObjectResult *result = Err(vm, AS_ERROR(args[0]));
		returnValue.values[0] =  OBJECT_VAL(result);
		return returnValue;
	}

	ObjectString* message = toString(vm, args[0]);
	ObjectError* error = newError(vm, message, RUNTIME, false);
	ObjectResult* result = Err(vm, error);
	returnValue.values[0] = OBJECT_VAL(result);
	return returnValue;

}

ObjectResult* _ok(VM *vm, int argCount, Value *args) {
	return stellaOk(vm, args[0]);
}
