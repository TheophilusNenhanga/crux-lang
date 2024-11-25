#include "error.h"

#include "../memory.h"

Value errorNative(int argCount, Value *args) {
	Value message = args[0];
	ObjectString *errorMessage = toString(message);
	ObjectError *error = newError(errorMessage, RUNTIME, USER);
	return OBJECT_VAL(error);
}

Value panicNative(int argCount, Value *args) {
	Value value = args[0];

	if (IS_ERROR(value)) {
		ObjectError *error = AS_ERROR(value);
		error->creator = PANIC;
		return OBJECT_VAL(error);
	}
	ObjectString *errorMessage = toString(value);
	ObjectError *error = newError(errorMessage, RUNTIME, PANIC);
	return OBJECT_VAL(error);
}
