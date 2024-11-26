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
