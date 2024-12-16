#include "array.h"


NativeReturn arrayPushMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);
	
	Value toAdd = args[1];

	if (!arrayAdd(array, toAdd, array->size)) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Failed to add to array.", 24), RUNTIME, STELLA));
		return returnValue;
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayPopMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (array->size == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Cannot remove a value from an empty array.", 43), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	Value popped = array->array[array->size - 1];
	array->array[array->size - 1] = NIL_VAL;
	array->size--;

	returnValue.values[0] = popped;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayInsertMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayRemoveAtMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayConcatMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arraySliceMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayReverseMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayIndexOfMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayContainsMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}

NativeReturn arrayClearMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	return returnValue;
}
