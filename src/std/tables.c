#include "tables.h"

ObjectResult *tableValuesMethod(VM *vm, int argCount __attribute__((unused)),
                                const Value *args) {
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

  for (uint32_t i = 0; i < table->capacity; i++) {
    const ObjectTableEntry entry = table->entries[i];
    if (entry.isOccupied) {
      values->values[lastInsert] = entry.value;
      lastInsert++;
    }
  }

  values->size = lastInsert;

  return newOkResult(vm, OBJECT_VAL(values));
}

ObjectResult *tableKeysMethod(VM *vm, int argCount __attribute__((unused)),
                              const Value *args) {
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

  for (uint32_t i = 0; i < table->capacity; i++) {
    const ObjectTableEntry entry = table->entries[i];
    if (entry.isOccupied) {
      keys->values[lastInsert] = entry.key;
      lastInsert++;
    }
  }

  keys->size = lastInsert;

  return newOkResult(vm, OBJECT_VAL(keys));
}

ObjectResult *tablePairsMethod(VM *vm, int argCount __attribute__((unused)),
                               const Value *args) {
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

  for (uint32_t i = 0; i < table->capacity; i++) {
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

      pair->values[0] = entry.key;
      pair->values[1] = entry.value;
      pair->size = 2;

      pairs->values[lastInsert] = OBJECT_VAL(pair);
      lastInsert++;
    }
  }

  pairs->size = lastInsert;

  return newOkResult(vm, OBJECT_VAL(pairs));
}

// arg0 - table
// arg1 - key
ObjectResult *tableRemoveMethod(VM *vm, int argCount __attribute__((unused)),
                                const Value *args) {
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

// arg0 - table
// arg1 - key
ObjectResult *tableGetMethod(VM *vm, int argCount __attribute__((unused)),
                             const Value *args) {
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  if (IS_CRUX_HASHABLE(key)) {
    Value value;
    const bool result = objectTableGet(table->entries, table->size,
                                       table->capacity, key, &value);
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

// args[0] - table
// args[1] - key
Value tableHasKeyMethod(VM *vm __attribute__((unused)),
                        int argCount __attribute__((unused)),
                        const Value *args) {
  ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  if (IS_CRUX_HASHABLE(key)) {
    const bool result = objectTableContainsKey(table, key);
    return BOOL_VAL(result);
  }
  return BOOL_VAL(false);
}

// args[0] - table
// args[1] - key
// args[2] - default value
Value tableGetOrElseMethod(VM *vm __attribute__((unused)),
                           int argCount __attribute__((unused)),
                           const Value *args) {
  const ObjectTable *table = AS_CRUX_TABLE(args[0]);
  const Value key = args[1];
  const Value defaultValue = args[2];
  if (IS_CRUX_HASHABLE(key)) {
    Value value;
    const bool result = objectTableGet(table->entries, table->size,
                                       table->capacity, key, &value);
    if (!result) {
      return defaultValue;
    }
    return value;
  }
  return defaultValue;
}
