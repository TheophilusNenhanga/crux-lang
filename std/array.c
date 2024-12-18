#include <stdlib.h>

#include "array.h"
#include "../memory.h"



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
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("Cannot remove a value from an empty array.", 43), INDEX_OUT_OF_BOUNDS, STELLA));
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
	
	if (!IS_NUMBER(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("<index> must be of type 'number'.", 34), TYPE, STELLA));
		return returnValue;
	}

	Value toInsert = args[1];
	uint64_t insertAt = (uint64_t) AS_NUMBER(args[2]);

	if (insertAt < 0 || insertAt > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("<index> is out of bounds.", 26), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	if (ensureCapacity(array, array->size + 1)) {
		for (uint64_t i = array->size; i > insertAt; i--) {
			array->array[i] = array->array[i - 1];
		}

		array->array[insertAt] = toInsert;
		array->size++;
	}
	else {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Failed to allocate enough memory for new array.", 48), MEMORY, STELLA));
		return returnValue;
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayRemoveAtMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_NUMBER(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("<index> must be of type 'number'.", 34), TYPE, STELLA));
		return returnValue;
	}

	uint64_t removeAt = (uint64_t) AS_NUMBER(args[1]);

	if (removeAt < 0 || removeAt >= array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("<index> is out of bounds.", 26), INDEX_OUT_OF_BOUNDS, STELLA));
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

	return returnValue;
}

NativeReturn arrayConcatMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_ARRAY(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("<target> must be of type 'array'.", 34), TYPE, STELLA));
		return returnValue;
	}

	ObjectArray* targetArray = AS_ARRAY(args[1]);

	uint64_t combinedSize = targetArray->size + array->size;
	if (combinedSize > MAX_ARRAY_SIZE) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("Size of resultant array out of bounds.", 39), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	ObjectArray *resultArray = newArray(combinedSize);
	
    for (uint64_t i = 0; i < combinedSize; i++) {
		resultArray->array[i] = (i < array->size) ? array->array[i] : targetArray->array[i - array->size];
	}

	resultArray->size = combinedSize;

	returnValue.values[0] = OBJECT_VAL(resultArray);
	returnValue.values[1] = NIL_VAL;

	return returnValue;
}

NativeReturn arraySliceMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	if (!IS_NUMBER(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("<start_index> must be of type 'number'.", 40), TYPE, STELLA));
		return returnValue;
	}

	if (!IS_NUMBER(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("<end_index> must be of type 'number'.", 38), TYPE, STELLA));
		return returnValue;
	}

	uint64_t startIndex = (uint64_t) AS_NUMBER(args[1]);
	uint64_t endIndex = (uint64_t) AS_NUMBER(args[2]);

	if (startIndex > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("<start_index> out of bounds.", 29), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	if (endIndex > array->size) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("<end_index> out of bounds.", 27), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	if (endIndex < startIndex) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("indexes out of bounds.", 23), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	size_t sliceSize = endIndex - startIndex;
	ObjectArray *slicedArray = newArray(sliceSize);

	for (size_t i = 0; i < sliceSize; i++) {
		slicedArray->array[i] = array->array[startIndex + i];
		slicedArray->size += 1;
	}

	returnValue.values[0] = OBJECT_VAL(slicedArray);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn arrayReverseMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	Value *values = ALLOCATE(Value, array->size);

	if (values == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("Failed to allocate memory when reversing array.", 48), MEMORY, STELLA));
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

NativeReturn arrayIndexOfMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
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
	returnValue.values[1] = OBJECT_VAL(newError(takeString("Value could not be found in the array.", 39), VALUE, STELLA));
	return returnValue;
}

NativeReturn arrayContainsMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
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

NativeReturn arrayClearMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectArray *array = AS_ARRAY(args[0]);

	for (uint64_t i = 0; i < array->size; i++) {
		array->array[i] = NIL_VAL;
	}
	array->size = 0;

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}
