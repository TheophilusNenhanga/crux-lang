#include "tables.h"

ObjectResult* tableValuesMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_CRUX_TABLE(args[0]);
	ObjectArray *values = newArray(vm, table->size);

	if (values == NULL) {
		return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate enough memory for <values> array.", 52), MEMORY, false));
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

	return newOkResult(vm, OBJECT_VAL(values));
}

ObjectResult* tableKeysMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_CRUX_TABLE(args[0]);

	ObjectArray *keys = newArray(vm, table->size);

	if (keys == NULL) {
		return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate enough memory for <keys> array.", 50), MEMORY, false));
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

	return newOkResult(vm, OBJECT_VAL(keys));
}

ObjectResult* tablePairsMethod(VM *vm, int argCount, Value *args) {
	ObjectTable *table = AS_CRUX_TABLE(args[0]);

	ObjectArray *pairs = newArray(vm, table->size);

	if (pairs == NULL) {
		return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate enough memory for <pairs> array.", 51), MEMORY, false));
	}

	uint16_t lastInsert = 0;

	for (uint16_t i = 0; i < table->capacity; i++) {
		ObjectTableEntry entry = table->entries[i];
		if (entry.isOccupied) {
			ObjectArray *pair = newArray(vm, 2);

			if (pair == NULL) {
				return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate enough memory for pair array.", 48), MEMORY, false));
			}

			pair->array[0] = entry.key;
			pair->array[1] = entry.value;
			pair->size = 2;

			pairs->array[lastInsert] = OBJECT_VAL(pair);
			lastInsert++;
		}
	}

	pairs->size = lastInsert;

	return newOkResult(vm, OBJECT_VAL(pairs));
}
