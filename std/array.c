#include <stdlib.h>

#include "../memory.h"
#include "array.h"


NativeReturn arrayPushMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	Value toAdd = args[1];

	if (!arrayAdd(vm, array, toAdd, array->size)) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Failed to add to array.", 23), RUNTIME, false));
		return returnValue;
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayPopMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (array->size == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Cannot remove a value from an empty array.", 42), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	Value popped = array->array[array->size - 1];
	array->array[array->size - 1] = NIL_VAL;
	array->size--;

	returnValue.values[0] = popped;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayInsertMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_NUMBER(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));
		return returnValue;
	}

	Value toInsert = args[1];
	uint64_t insertAt = (uint64_t) AS_NUMBER(args[2]);

	if (insertAt < 0 || insertAt > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<index> is out of bounds.", 25), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	if (ensureCapacity(vm, array, array->size + 1)) {
		for (uint64_t i = array->size; i > insertAt; i--) {
			array->array[i] = array->array[i - 1];
		}

		array->array[insertAt] = toInsert;
		array->size++;
	} else {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Failed to allocate enough memory for new array.", 47), MEMORY, false));
		return returnValue;
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayRemoveAtMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_NUMBER(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));
		return returnValue;
	}

	uint64_t removeAt = (uint64_t) AS_NUMBER(args[1]);

	if (removeAt >= array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<index> is out of bounds.", 25), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	Value removedElement = array->array[removeAt];

	for (uint64_t i = removeAt; i < array->size - 1; i++) {
		array->array[i] = array->array[i + 1];
	}

	array->size--;

	returnValue.values[0] = removedElement;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayConcatMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_ARRAY(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<target> must be of type 'array'.", 33), TYPE, false));
		return returnValue;
	}

	ObjectArray *targetArray = AS_ARRAY(args[1]);

	uint64_t combinedSize = targetArray->size + array->size;
	if (combinedSize > MAX_ARRAY_SIZE) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Size of resultant array out of bounds.", 38), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	ObjectArray *resultArray = newArray(vm, combinedSize);

	for (uint64_t i = 0; i < combinedSize; i++) {
		resultArray->array[i] = (i < array->size) ? array->array[i] : targetArray->array[i - array->size];
	}

	resultArray->size = combinedSize;

	returnValue.values[0] = OBJECT_VAL(resultArray);
	returnValue.values[1] = NIL_VAL;

	return returnValue;
}

NativeReturn arraySliceMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_NUMBER(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<start_index> must be of type 'number'.", 39), TYPE, false));
		return returnValue;
	}

	if (!IS_NUMBER(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<end_index> must be of type 'number'.", 37), TYPE, false));
		return returnValue;
	}

	uint64_t startIndex = (uint64_t) AS_NUMBER(args[1]);
	uint64_t endIndex = (uint64_t) AS_NUMBER(args[2]);

	if (startIndex > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<start_index> out of bounds.", 28), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	if (endIndex > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<end_index> out of bounds.", 26), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	if (endIndex < startIndex) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "indexes out of bounds.", 22), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	size_t sliceSize = endIndex - startIndex;
	ObjectArray *slicedArray = newArray(vm, sliceSize);

	for (size_t i = 0; i < sliceSize; i++) {
		slicedArray->array[i] = array->array[startIndex + i];
		slicedArray->size += 1;
	}

	returnValue.values[0] = OBJECT_VAL(slicedArray);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayReverseMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	Value *values = ALLOCATE(vm, Value, array->size);

	if (values == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Failed to allocate memory when reversing array.", 47), MEMORY, false));
		return returnValue;
	}

	for (uint64_t i = 0; i < array->size; i++) {
		values[i] = array->array[i];
	}

	for (uint64_t i = 0; i < array->size; i++) {
		array->array[i] = values[array->size - 1 - i];
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;

	free(values);

	return returnValue;
}

NativeReturn arrayIndexOfMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);
	Value target = args[1];

	for (uint64_t i = 0; i < array->size; i++) {
		if (valuesEqual(target, array->array[i])) {
			returnValue.values[0] = NUMBER_VAL(i);
			returnValue.values[1] = NIL_VAL;
			return returnValue;
		}
	}
	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] =
			OBJECT_VAL(newError(vm, copyString(vm, "Value could not be found in the array.", 38), VALUE, false));
	return returnValue;
}

NativeReturn arrayContainsMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectArray *array = AS_ARRAY(args[0]);

	Value target = args[1];

	for (uint64_t i = 0; i < array->size; i++) {
		if (valuesEqual(target, array->array[i])) {
			returnValue.values[0] = BOOL_VAL(true);
			returnValue.values[1] = NIL_VAL;
			return returnValue;
		}
	}
	returnValue.values[0] = BOOL_VAL(false);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayClearMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 1);
	ObjectArray *array = AS_ARRAY(args[0]);

	for (uint64_t i = 0; i < array->size; i++) {
		array->array[i] = NIL_VAL;
	}
	array->size = 0;

	returnValue.values[0] = NIL_VAL;
	return returnValue;
}
