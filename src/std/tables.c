#include "tables.h"

ObjectResult *tableValuesMethod(VM *vm, int argCount, const Value *args) {
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);
  ObjectArray *values = newArray(vm, table->size);

  if (values == NULL) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm, "Failed to allocate enough memory for <values> array.", 52),
            MEMORY, false));
  }

  uint16_t lastInsert = 0;

  for (uint16_t i = 0; i < table->capacity; i++) {
    const ObjectTableEntry entry = table->entries[i];
    if (entry.isOccupied) {
      values->array[lastInsert] = entry.value;
      lastInsert++;
    }
  }

  values->size = lastInsert;

  return newOkResult(vm, OBJECT_VAL(values));
}

ObjectResult *tableKeysMethod(VM *vm, int argCount, const Value *args) {
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);

  ObjectArray *keys = newArray(vm, table->size);

  if (keys == NULL) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "Failed to allocate enough memory for <keys> array.",
                       50),
            MEMORY, false));
  }

  uint16_t lastInsert = 0;

  for (uint16_t i = 0; i < table->capacity; i++) {
    const ObjectTableEntry entry = table->entries[i];
    if (entry.isOccupied) {
      keys->array[lastInsert] = entry.key;
      lastInsert++;
    }
  }

  keys->size = lastInsert;

  return newOkResult(vm, OBJECT_VAL(keys));
}

ObjectResult *tablePairsMethod(VM *vm, int argCount, const Value *args) {
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);

  ObjectArray *pairs = newArray(vm, table->size);

  if (pairs == NULL) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm, "Failed to allocate enough memory for <pairs> array.", 51),
            MEMORY, false));
  }

  uint16_t lastInsert = 0;

  for (uint16_t i = 0; i < table->capacity; i++) {
    const ObjectTableEntry entry = table->entries[i];
    if (entry.isOccupied) {
      ObjectArray *pair = newArray(vm, 2);

      if (pair == NULL) {
        return newErrorResult(
            vm,
            newError(
                vm,
                copyString(
                    vm, "Failed to allocate enough memory for pair array.", 48),
                MEMORY, false));
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

ObjectResult *tableRemoveMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - table
  // arg1 - key
  ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  if (IS_CRUX_HASHABLE(key)) {
    const bool result = objectTableRemove(table, key);
    if (!result) {
      return newErrorResult(
          vm,
          newError(vm,
                   copyString(
                       vm, "Failed to remove key: value pair from table.", 44),
                   VALUE, false));
    }
    return newOkResult(vm, NIL_VAL);
  }
  return newErrorResult(
      vm,
      newError(vm, copyString(vm, "Unhashable type given as table key.", 35),
               TYPE, false));
}

ObjectResult *tableGetMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - table
  // arg1 - key
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  if (IS_CRUX_HASHABLE(key)) {
    Value value;
    const bool result = objectTableGet(table, key, &value);
    if (!result) {
      return newErrorResult(
          vm,
          newError(vm, copyString(vm, "Failed to get value from table.", 31),
                   VALUE, false));
    }
    return newOkResult(vm, value);
  }
  return newErrorResult(
      vm,
      newError(vm, copyString(vm, "Unhashable type given as table key.", 35),
               TYPE, false));
}

Value tableHasKeyMethod(VM *vm, int argCount, const Value *args) {
  // args[0] - table
  // args[1] - key
  ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  if (IS_CRUX_HASHABLE(key)) {
    const bool result = objectTableContainsKey(table, key);
    return BOOL_VAL(result);
  }
  return BOOL_VAL(false);
}

Value tableGetOrElseMethod(VM *vm, int argCount, const Value *args) {
  // args[0] - table
  // args[1] - key
  // args[2] - default value
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  const Value defaultValue = args[2];
  if (IS_CRUX_HASHABLE(key)) {
    Value value;
    const bool result = objectTableGet(table, key, &value);
    if (!result) {
      return defaultValue;
    }
    return value;
  }
  return defaultValue;
}
