#include "error.h"

#include "../memory.h"

NativeReturn errorNative(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(1);

	Value message = args[0];
	ObjectString *errorMessage = toString(message);
	ObjectError *error = newError(errorMessage, RUNTIME, USER);
	nativeReturn.values[0] = OBJECT_VAL(error);
	return nativeReturn;
}

NativeReturn panicNative(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(1);

	Value value = args[0];

	if (IS_ERROR(value)) {
		ObjectError *error = AS_ERROR(value);
		error->creator = PANIC;
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}
	ObjectString *errorMessage = toString(value);
	ObjectError *error = newError(errorMessage, RUNTIME, PANIC);
	nativeReturn.values[0] = OBJECT_VAL(error);
	return nativeReturn;
}

NativeReturn assertNative(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(1);
	if (!IS_BOOL(args[0])) {
		ObjectError *error = newError(takeString("Failed to assert: <condition> must be of type 'bool'.", 36), TYPE, PANIC);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}
	if (!IS_STRING(args[1])) {
		ObjectError *error = newError(takeString("Failed to assert: <message> must be of type 'string'.", 54), TYPE, PANIC);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}

	bool result = AS_BOOL(args[0]);
	ObjectString *message = AS_STRING(args[1]);

	if (result == false) {
		ObjectError *error = newError(message, ASSERT, PANIC);
		nativeReturn.values[0] = OBJECT_VAL(error);
		return nativeReturn;
	}

	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}

NativeReturn errorMessageMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(1);
	ObjectError *error = AS_ERROR(args[0]);

	returnValue.values[0] = OBJECT_VAL(error->message);
	return returnValue;
}

NativeReturn errorCreatorMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(1);
	ObjectError *error = AS_ERROR(args[0]);

	switch (error->creator) {
		case STELLA: {
			returnValue.values[0] = OBJECT_VAL(takeString("'stella'", 9));
			break;
		}
		case USER: {
			returnValue.values[0] = OBJECT_VAL(takeString("'user'", 7));
			break;
		}
		case PANIC: {
			returnValue.values[0] = OBJECT_VAL(takeString("'panic'", 8));
			break;
		}
	}
	return returnValue;
}

NativeReturn errorTypeMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(1);
	ObjectError *error = AS_ERROR(args[0]);

	switch (error->type) {
		case SYNTAX: {
			ObjectString *type = takeString("<syntax error>", 15);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case DIVISION_BY_ZERO: {
			ObjectString *type = takeString("<zero division error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case INDEX_OUT_OF_BOUNDS: {
			ObjectString *type = takeString("<index error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case RUNTIME: {
			ObjectString *type = takeString("<runtime error>", 15);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case TYPE: {
			ObjectString *type = takeString("<type error>", 12);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LOOP_EXTENT: {
			ObjectString *type = takeString("<loop extent error>", 19);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LIMIT: {
			ObjectString *type = takeString("<limit error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case BRANCH_EXTENT: {
			ObjectString *type = takeString("<branch extent error>", 21);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case CLOSURE_EXTENT: {
			ObjectString *type = takeString("<closure extent error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case LOCAL_EXTENT: {
			ObjectString *type = takeString("<local extent error>", 20);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case ARGUMENT_EXTENT: {
			ObjectString *type = takeString("<argument extent error>", 24);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case NAME: {
			ObjectString *type = takeString("<name error>", 13);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case COLLECTION_EXTENT: {
			ObjectString *type = takeString("<collection extent error>", 26);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case VARIABLE_EXTENT: {
			ObjectString *type = takeString("<variable extent error>", 24);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case VARIABLE_DECLARATION_MISMATCH: {
			ObjectString *type = takeString("<variable mismatch error>", 25);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case RETURN_EXTENT: {
			ObjectString *type = takeString("<return extent error>", 22);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case ARGUMENT_MISMATCH: {
			ObjectString *type = takeString("<argument mismatch error>", 26);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case STACK_OVERFLOW: {
			ObjectString *type = takeString("<stack overflow error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
		case COLLECTION_GET: {
			ObjectString *type = takeString("<collection get error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case COLLECTION_SET: {
			ObjectString *type = takeString("<collection set error>", 23);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case UNPACK_MISMATCH: {
			ObjectString *type = takeString("<unpack mismatch error>", 24);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case MEMORY: {
			ObjectString *type = takeString("<memory error>", 15);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case VALUE: {
			ObjectString *type = takeString("<value error>", 14);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		case ASSERT: {
			ObjectString *type = takeString("<assert error>", 15);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}

		default: {
			ObjectString *type = takeString("<stella error>", 8);
			returnValue.values[0] = OBJECT_VAL(type);
			break;
		}
	}
	return returnValue;
}
