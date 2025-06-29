#include "core.h"

#include <stdio.h>
#include <stdlib.h>

#include "../memory.h"
#include "../object.h"

static Value getLength(const Value value) {
  if (IS_CRUX_ARRAY(value)) {
    return INT_VAL(AS_CRUX_ARRAY(value)->size);
  }
  if (IS_CRUX_STRING(value)) {
    return INT_VAL(AS_CRUX_STRING(value)->length);
  }
  if (IS_CRUX_TABLE(value)) {
    return INT_VAL(AS_CRUX_TABLE(value)->size);
  }
  return NIL_VAL;
}

ObjectResult *lengthFunction(VM *vm, int argCount, const Value *args) {
  const Value value = args[0];
  const Value length = getLength(value);
  if (IS_NIL(length)) {
    return newErrorResult(
        vm, newError(vm,
                     copyString(vm,
                                "Expected either a collection type ('string', "
                                "'array', 'table').",
                                64),
                     TYPE, false));
  }
  return newOkResult(vm, length);
}

Value lengthFunction_(VM *vm, int argCount, const Value *args) {
  const Value value = args[0];
  return getLength(value);
}

static Value castArray(VM *vm, const Value *args, bool *success) {
  const Value value = args[0];

  if (IS_CRUX_ARRAY(value)) {
    return value;
  }

  if (IS_CRUX_STRING(value)) {
    const ObjectString *string = AS_CRUX_STRING(value);
    ObjectArray *array = newArray(vm, string->length);
    for (int i = 0; i < string->length; i++) {
      if (!arrayAddBack(vm, array,
                        OBJECT_VAL(copyString(vm, &string->chars[i], 1)))) {
        *success = false;
        return NIL_VAL;
      }
    }
    return OBJECT_VAL(array);
  }

  if (IS_CRUX_TABLE(value)) {
    const ObjectTable *table = AS_CRUX_TABLE(value);
    ObjectArray *array = newArray(vm, table->size * 2);
    int index = 0;
    for (int i = 0; i < table->capacity; i++) {
      if (index == table->size) {
        break;
      }
      if (table->entries[i].isOccupied) {
        if (!arrayAddBack(vm, array, table->entries[i].key) ||
            !arrayAddBack(vm, array, table->entries[i].value)) {
          *success = false;
          return NIL_VAL;
        }
        index++;
      }
    }
    return OBJECT_VAL(array);
  }
  ObjectArray *array = newArray(vm, 1);
  arrayAdd(vm, array, value, 0);
  return OBJECT_VAL(array);
}

static Value castTable(VM *vm, const Value *args) {
  const Value value = args[0];

  if (IS_CRUX_TABLE(value)) {
    return value;
  }

  if (IS_CRUX_ARRAY(value)) {
    const ObjectArray *array = AS_CRUX_ARRAY(value);
    ObjectTable *table = newTable(vm, (int)array->size);
    for (int i = 0; i < array->size; i++) {
      const Value k = INT_VAL(i);
      const Value v = array->array[i];
      objectTableSet(vm, table, k, v);
    }
    return OBJECT_VAL(table);
  }

  if (IS_CRUX_STRING(value)) {
    const ObjectString *string = AS_CRUX_STRING(value);
    ObjectTable *table = newTable(vm, string->length);
    for (int i = 0; i < string->length; i++) {
      objectTableSet(vm, table, INT_VAL(i),
                     OBJECT_VAL(copyString(vm, &string->chars[i], 1)));
    }
    return OBJECT_VAL(table);
  }

  ObjectTable *table = newTable(vm, 1);
  objectTableSet(vm, table, INT_VAL(0), value);
  return OBJECT_VAL(table);
}

static Value castInt(VM *vm, const Value *args, bool *success) {
  const Value value = args[0];

  if (IS_INT(value)) {
    return value;
  }

  if (IS_CRUX_STRING(value)) {
    const char *str = AS_C_STRING(value);
    char *end;
    const double num = strtod(str, &end);

    // can check for overflow or underflow with errno

    if (end != str) {
      return INT_VAL((uint32_t)num);
    }
  }

  if (IS_BOOL(value)) {
    return INT_VAL(AS_BOOL(value) ? 1 : 0);
  }

  if (IS_NIL(value)) {
    return INT_VAL(0);
  }

  *success = false;
  return NIL_VAL;
}

static Value castFloat(VM *vm, const Value *args, bool *success) {
  const Value value = args[0];

  if (IS_INT(value)) {
    return value;
  }

  if (IS_CRUX_STRING(value)) {
    const char *str = AS_C_STRING(value);
    char *end;
    const double num = strtod(str, &end);

    if (end != str) {
      return FLOAT_VAL(num);
    }
  }

  if (IS_BOOL(value)) {
    return FLOAT_VAL(AS_BOOL(value) ? 1 : 0);
  }

  if (IS_NIL(value)) {
    return FLOAT_VAL(0);
  }

  *success = false;
  return NIL_VAL;
}

ObjectResult *intFunction(VM *vm, int argCount, const Value *args) {
  bool success = true;
  const Value value = castInt(vm, args, &success);
  if (!success) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot convert value to number.", 30),
                     TYPE, false));
  }
  return newOkResult(vm, value);
}

ObjectResult *floatFunction(VM *vm, int argCount, const Value *args) {
  bool success = true;
  const Value value = castFloat(vm, args, &success);
  if (!success) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot convert value to number.", 30),
                     TYPE, false));
  }
  return newOkResult(vm, value);
}

ObjectResult *stringFunction(VM *vm, int argCount, const Value *args) {
  const Value value = args[0];
  return newOkResult(vm, OBJECT_VAL(toString(vm, value)));
}

ObjectResult *arrayFunction(VM *vm, int argCount, const Value *args) {
  bool success = true;
  const Value array = castArray(vm, args, &success);
  if (!success) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Failed to convert value to array.", 33),
                 RUNTIME, false));
  }
  return newOkResult(vm, OBJECT_VAL(array));
}

ObjectResult *tableFunction(VM *vm, int argCount, const Value *args) {
  return newOkResult(vm, castTable(vm, args));
}

Value intFunction_(VM *vm, int argCount, const Value *args) {
  bool success = true;
  return castInt(vm, args, &success);
}

Value floatFunction_(VM *vm, int argCount, const Value *args) {
  bool success = true;
  return castFloat(vm, args, &success);
}

Value stringFunction_(VM *vm, int argCount, const Value *args) {
  return OBJECT_VAL(toString(vm, args[0]));
}

Value arrayFunction_(VM *vm, int argCount, const Value *args) {
  bool success = true;
  const Value array = castArray(vm, args, &success);
  if (!success) {
    return NIL_VAL;
  }
  return OBJECT_VAL(array);
}

Value tableFunction_(VM *vm, int argCount, const Value *args) {
  return castTable(vm, args);
}
