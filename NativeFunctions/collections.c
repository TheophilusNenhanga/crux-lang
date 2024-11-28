#include "collections.h"

#include "../memory.h"
#include "../object.h"


NativeReturn lengthNative(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(2);

	Value value = args[0];
	if (IS_ARRAY(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_ARRAY(value)->size);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	if (IS_STRING(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_STRING(value)->length);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	if (IS_TABLE(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_TABLE(value)->size);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	ObjectError *error = newError(takeString("Expected either collection type.", 32), TYPE, STELLA);
	nativeReturn.values[0] = NIL_VAL;
	nativeReturn.values[1] = OBJECT_VAL(error);
	return nativeReturn;
}

NativeReturn arrayAddNative(int argCount, Value *args) {
	Value value = args[0];
	Value toAdd = args[1];

	NativeReturn nativeReturn = makeNativeReturn(2);

	if (!IS_ARRAY(value)) {
		ObjectError *error = newError(takeString("Expected type 'error'.", 22), TYPE, STELLA);
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(error);
		return nativeReturn;
	}
	ObjectArray *array = AS_ARRAY(value);
	if (!arrayAdd(array, toAdd, array->size)) {
		ObjectError *error = newError(takeString("Failed to add to array.", 23), RUNTIME, STELLA);
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(error);
		return nativeReturn;
	}
	nativeReturn.values[0] = NIL_VAL;
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn arrayRemoveNative(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(2);

	Value value = args[0];
	if (IS_ARRAY(value)) {
		ObjectArray *array = AS_ARRAY(value);
		if (array->size < 1) {
			ObjectError *error = newError(takeString("Array must at least have 1 value.", 33), INDEX_OUT_OF_BOUNDS, STELLA);
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(error);
			return nativeReturn;
		}
		array->array[array->size - 1] = NIL_VAL;
		array->size--;
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	ObjectError *error = newError(takeString("Expected type 'array'.", 22), TYPE, STELLA);
	nativeReturn.values[0] = NIL_VAL;
	nativeReturn.values[1] = OBJECT_VAL(error);
	return nativeReturn;
}
