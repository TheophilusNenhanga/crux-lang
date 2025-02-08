#include "tables.h"

NativeReturn tableValuesMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_TABLE(args[0]);
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	ObjectArray *values = newArray(vm, table->size);

	if (values == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Failed to allocate enough memory for <values> array.", 52), MEMORY, false));
		return returnValue;
	}

	uint16_t lastInsert = 0;

	for (uint16_t i = 0; i < table->capacity; i++) {
		ObjectTableEntry entry = table->entries[i];
		if (entry.isOccupied) {
			values->array[lastInsert] = entry.value;
			lastInsert++;
		}
	}

	values->size = lastInsert;

	returnValue.values[0] = OBJECT_VAL(values);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn tableKeysMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_TABLE(args[0]);
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	ObjectArray *keys = newArray(vm, table->size);

	if (keys == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Failed to allocate enough memory for <keys> array.", 50), MEMORY, false));
		return returnValue;
	}

	uint16_t lastInsert = 0;

	for (uint16_t i = 0; i < table->capacity; i++) {
		ObjectTableEntry entry = table->entries[i];
		if (entry.isOccupied) {
			keys->array[lastInsert] = entry.key;
			lastInsert++;
		}
	}

	keys->size = lastInsert;

	returnValue.values[0] = OBJECT_VAL(keys);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn tablePairsMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_TABLE(args[0]);
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	ObjectArray *pairs = newArray(vm, table->size);

	if (pairs == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Failed to allocate enough memory for <pairs> array.", 51), MEMORY, false));
		return returnValue;
	}

	uint16_t lastInsert = 0;

	for (uint16_t i = 0; i < table->capacity; i++) {
		ObjectTableEntry entry = table->entries[i];
		if (entry.isOccupied) {
			ObjectArray *pair = newArray(vm, 2);

			if (pair == NULL) {
				returnValue.values[0] = NIL_VAL;
				returnValue.values[1] = OBJECT_VAL(
						newError(vm, copyString(vm, "Failed to allocate enough memory for pair array.", 48), MEMORY, false));
				return returnValue;
			}

			pair->array[0] = entry.key;
			pair->array[1] = entry.value;
			pair->size = 2;

			pairs->array[lastInsert] = OBJECT_VAL(pair);
			lastInsert++;
		}
	}

	pairs->size = lastInsert;

	returnValue.values[0] = OBJECT_VAL(pairs);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}
