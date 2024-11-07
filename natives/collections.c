#include "collections.h"

#include "../memory.h"
#include "../object.h"


Value lengthNative(int argCount, Value *args) {
	Value value = args[0];
	if (IS_ARRAY(value)) {
		return NUMBER_VAL(AS_ARRAY(value)->size);
	}
	if (IS_STRING(value)) {
		return NUMBER_VAL(AS_STRING(value)->length);
	}
	// TODO: Runtime error
	return NIL_VAL;
}

Value arrayAddNative(int argCount, Value *args) {
	Value value = args[0];
	Value toAdd = args[1];

	if (!IS_ARRAY(value)) {
		// TODO: RUNTIME_ERROR
		return NIL_VAL;
	}
	ObjectArray *array = AS_ARRAY(value);
	if (!arrayAdd(array, toAdd, array->size)) {
		// TODO: RUNTIME ERROR
		return NIL_VAL;
	}
	return NIL_VAL;
}

Value arrayRemoveNative(int argCount, Value *args) {
	Value value = args[0];
	if (IS_ARRAY(value)) {
		ObjectArray *array = AS_ARRAY(value);
		if (array->size < 1) {
			// TODO: Runtime Error
			return NIL_VAL;
		} else {
			array->array[array->size - 1] = NIL_VAL;
			array->size--;
			return NIL_VAL;
		}
	} else {
		// TODO: RUNTIME_ERROR
		return NIL_VAL;
	}
}
