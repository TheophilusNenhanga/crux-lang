#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../memory.h"
#include "array.h"

ObjectResult *arrayPushMethod(VM *vm, int argCount, const Value *args) {
  ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  const Value toAdd = args[1];

  if (!arrayAdd(vm, array, toAdd, array->size)) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Failed to add to array.", 23), RUNTIME,
                     false));
  }

  return newOkResult(vm, NIL_VAL);
}

ObjectResult *arrayPopMethod(VM *vm, int argCount, const Value *args) {
  ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (array->size == 0) {
    return newErrorResult(
        vm, newError(vm,
                     copyString(
                         vm, "Cannot remove a value from an empty array.", 42),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  const Value popped = array->array[array->size - 1];
  array->array[array->size - 1] = NIL_VAL;
  array->size--;

  return newOkResult(vm, popped);
}

ObjectResult *arrayInsertMethod(VM *vm, int argCount, const Value *args) {
  ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (!IS_INT(args[2])) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "<index> must be of type 'number'.", 33),
                 TYPE, false));
  }

  const Value toInsert = args[1];
  const uint32_t insertAt = AS_INT(args[2]);

  if (insertAt > array->size) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "<index> is out of bounds.", 25),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  if (ensureCapacity(vm, array, array->size + 1)) {
    for (uint32_t i = array->size; i > insertAt; i--) {
      array->array[i] = array->array[i - 1];
    }

    array->array[insertAt] = toInsert;
    array->size++;
  } else {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "Failed to allocate enough memory for new array.", 47),
                 MEMORY, false));
  }
  return newOkResult(vm, NIL_VAL);
}

ObjectResult *arrayRemoveAtMethod(VM *vm, int argCount, const Value *args) {
  ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (!IS_INT(args[1])) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "<index> must be of type 'number'.", 33),
                 TYPE, false));
  }

  const uint32_t removeAt = AS_INT(args[1]);

  if (removeAt >= array->size) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "<index> is out of bounds.", 25),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  const Value removedElement = array->array[removeAt];

  for (uint32_t i = removeAt; i < array->size - 1; i++) {
    array->array[i] = array->array[i + 1];
  }

  array->size--;
  return newOkResult(vm, removedElement);
}

ObjectResult *arrayConcatMethod(VM *vm, int argCount, const Value *args) {
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (!IS_CRUX_ARRAY(args[1])) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "<target> must be of type 'array'.", 33),
                 TYPE, false));
  }

  const ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

  const uint32_t combinedSize = targetArray->size + array->size;
  if (combinedSize > MAX_ARRAY_SIZE) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "Size of resultant array out of bounds.", 38),
                 INDEX_OUT_OF_BOUNDS, false));
  }

  ObjectArray *resultArray = newArray(vm, combinedSize);

  for (uint32_t i = 0; i < combinedSize; i++) {
    resultArray->array[i] = (i < array->size)
                                ? array->array[i]
                                : targetArray->array[i - array->size];
  }

  resultArray->size = combinedSize;

  return newOkResult(vm, OBJECT_VAL(resultArray));
}

ObjectResult *arraySliceMethod(VM *vm, int argCount, const Value *args) {
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (!IS_INT(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "<start_index> must be of type 'number'.", 39),
                 TYPE, false));
  }

  if (!IS_INT(args[2])) {
    return newErrorResult(
        vm, newError(
                vm, copyString(vm, "<end_index> must be of type 'number'.", 37),
                TYPE, false));
  }

  const uint32_t startIndex = IS_INT(args[1]);
  const uint32_t endIndex = IS_INT(args[2]);

  if (startIndex > array->size) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "<start_index> out of bounds.", 28),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  if (endIndex > array->size) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "<end_index> out of bounds.", 26),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  if (endIndex < startIndex) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "indexes out of bounds.", 22),
                     INDEX_OUT_OF_BOUNDS, false));
  }

  const size_t sliceSize = endIndex - startIndex;
  ObjectArray *slicedArray = newArray(vm, sliceSize);

  for (size_t i = 0; i < sliceSize; i++) {
    slicedArray->array[i] = array->array[startIndex + i];
    slicedArray->size += 1;
  }

  return newOkResult(vm, OBJECT_VAL(slicedArray));
}

ObjectResult *arrayReverseMethod(VM *vm, int argCount, const Value *args) {
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  Value *values = ALLOCATE(vm, Value, array->size);

  if (values == NULL) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "Failed to allocate memory when reversing array.", 47),
                 MEMORY, false));
  }

  for (uint32_t i = 0; i < array->size; i++) {
    values[i] = array->array[i];
  }

  for (uint32_t i = 0; i < array->size; i++) {
    array->array[i] = values[array->size - 1 - i];
  }

  free(values);

  return newOkResult(vm, NIL_VAL);
}

ObjectResult *arrayIndexOfMethod(VM *vm, int argCount, const Value *args) {
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  const Value target = args[1];

  for (uint32_t i = 0; i < array->size; i++) {
    if (valuesEqual(target, array->array[i])) {
      return newOkResult(vm, INT_VAL(i));
    }
  }
  return newErrorResult(
      vm,
      newError(vm, copyString(vm, "Value could not be found in the array.", 38),
               VALUE, false));
}

Value arrayContainsMethod(VM *vm, int argCount, const Value *args) {
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  const Value target = args[1];

  for (uint32_t i = 0; i < array->size; i++) {
    if (valuesEqual(target, array->array[i])) {
      return BOOL_VAL(true);
    }
  }
  return BOOL_VAL(false);
}

Value arrayClearMethod(VM *vm, int argCount, const Value *args) {
  ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  for (uint32_t i = 0; i < array->size; i++) {
    array->array[i] = NIL_VAL;
  }
  array->size = 0;

  return NIL_VAL;
}

Value arrayEqualsMethod(VM *vm, int argCount, const Value *args) {
  if (!IS_CRUX_ARRAY(args[1])) {
    return BOOL_VAL(false);
  }

  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  const ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

  if (array->size != targetArray->size) {
    return BOOL_VAL(false);
  }

  for (uint32_t i = 0; i < array->size; i++) {
    if (!valuesEqual(array->array[i], targetArray->array[i])) {
      return BOOL_VAL(false);
    }
  }

  return BOOL_VAL(true);
}

ObjectResult *arrayMapMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - array
  // arg1 - function
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;

  if (!IS_CRUX_CLOSURE(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "Expected value of type 'callable' for <func> argument",
                    53),
                TYPE, false));
  }

  ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

  if (closure->function->arity != 1) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "<func> must take exactly 1 argument.", 36),
                 ARGUMENT_MISMATCH, true));
  }

  ObjectArray *resultArray = newArray(vm, array->size);
  push(currentModuleRecord, OBJECT_VAL(resultArray));
  for (uint32_t i = 0; i < array->size; i++) {
    const Value arrayValue = array->array[i];
    push(currentModuleRecord, arrayValue);
    InterpretResult res;
    ObjectResult *result = executeUserFunction(vm, closure, 1, &res);

    if (res != INTERPRET_OK) {
      if (!result->isOk) {
        return result;
      }
    }

    if (result->isOk) {
      arrayAddBack(vm, resultArray, result->as.value);
    } else {
      arrayAddBack(vm, resultArray, OBJECT_VAL(result->as.error));
    }
    pop(currentModuleRecord);
  }
  pop(currentModuleRecord);
  return newOkResult(vm, OBJECT_VAL(resultArray));
}

ObjectResult *arrayFilterMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - array
  // arg1 - function
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (!IS_CRUX_CLOSURE(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "Expected value of type 'callable' for <func> argument",
                    53),
                TYPE, false));
  }

  ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

  if (closure->function->arity != 1) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "<func> must take exactly 1 argument.", 36),
                 ARGUMENT_MISMATCH, true));
  }

  ObjectArray *resultArray = newArray(vm, array->size);
  push(currentModuleRecord, OBJECT_VAL(resultArray));
  uint32_t addCount = 0;
  for (uint32_t i = 0; i < array->size; i++) {
    const Value arrayValue = array->array[i];
    push(currentModuleRecord, arrayValue);
    InterpretResult res;
    ObjectResult *result = executeUserFunction(vm, closure, 1, &res);

    if (res != INTERPRET_OK) {
      if (!result->isOk) {
        return result;
      }
    }

    if (result->isOk) {
      if (!isFalsy(result->as.value)) {
        arrayAddBack(vm, resultArray, arrayValue);
        addCount++;
      }
    }
    pop(currentModuleRecord);
  }
  pop(currentModuleRecord);
  resultArray->size = addCount;
  return newOkResult(vm, OBJECT_VAL(resultArray));
}

ObjectResult *arrayReduceMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - array
  // arg1 - function
  // arg2 - initial value
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;

  if (!IS_CRUX_CLOSURE(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "Expected value of type 'callable' for <func> argument",
                    53),
                TYPE, false));
  }

  ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);
  if (closure->function->arity != 2) {
    return newErrorResult(
        vm, newError(
                vm, copyString(vm, "<func> must take exactly 2 arguments.", 37),
                ARGUMENT_MISMATCH, true));
  }

  Value accumulator = args[2];

  for (uint32_t i = 0; i < array->size; i++) {
    const Value arrayValue = array->array[i];

    push(currentModuleRecord, arrayValue);
    push(currentModuleRecord, accumulator);

    InterpretResult res;

    ObjectResult *result = executeUserFunction(vm, closure, 2, &res);

    if (res != INTERPRET_OK) {
      if (!result->isOk) {
        pop(currentModuleRecord); // arrayValue
        pop(currentModuleRecord); // accumulator
        return result;
      }
    }

    if (result->isOk) {
      accumulator = result->as.value;
    } else {
      pop(currentModuleRecord); // arrayValue
      pop(currentModuleRecord); // accumulator
      return result;
    }

    pop(currentModuleRecord); // arrayValue
    pop(currentModuleRecord); // accumulator
  }

  return newOkResult(vm, accumulator);
}

static int compareValues(const Value a, const Value b) {
  if (IS_INT(a) && IS_INT(b)) {
    const int64_t aVal = AS_INT(a);
    const int64_t bVal = AS_INT(b);
    if (aVal < bVal)
      return -1;
    if (aVal > bVal)
      return 1;
    return 0;
  }

  if (IS_FLOAT(a) && IS_FLOAT(b)) {
    const double aVal = AS_FLOAT(a);
    const double bVal = AS_FLOAT(b);
    if (aVal < bVal)
      return -1;
    if (aVal > bVal)
      return 1;
    return 0;
  }

  if ((IS_INT(a) || IS_FLOAT(a)) && (IS_INT(b) || IS_FLOAT(b))) {
    const double aVal = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
    const double bVal = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
    if (aVal < bVal)
      return -1;
    if (aVal > bVal)
      return 1;
    return 0;
  }

  if (IS_CRUX_STRING(a) && IS_CRUX_STRING(b)) {
    const ObjectString *aStr = AS_CRUX_STRING(a);
    const ObjectString *bStr = AS_CRUX_STRING(b);
    return strcmp(aStr->chars, bStr->chars);
  }

  // If types don't match or aren't comparable
  return 0;
}

static bool areAllElementsSortable(const ObjectArray *array) {
  if (array->size == 0)
    return true;

  bool hasInt = false, hasFloat = false, hasString = false;

  for (uint32_t i = 0; i < array->size; i++) {
    const Value val = array->array[i];
    if (IS_INT(val)) {
      hasInt = true;
    } else if (IS_FLOAT(val)) {
      hasFloat = true;
    } else if (IS_CRUX_STRING(val)) {
      hasString = true;
    } else {
      return false;
    }
  }

  if (hasString && (hasInt || hasFloat)) {
    return false;
  }

  return true;
}

static int partition(Value *arr, const int low, const int high) {
  const Value pivot = arr[high];
  int i = (low - 1);

  for (int j = low; j <= high - 1; j++) {
    if (compareValues(arr[j], pivot) <= 0) {
      i++;
      const Value temp = arr[i];
      arr[i] = arr[j];
      arr[j] = temp;
    }
  }
  const Value temp = arr[i + 1];
  arr[i + 1] = arr[high];
  arr[high] = temp;
  return (i + 1);
}

static void quickSort(Value *arr, const int low, const int high) {
  if (low < high) {
    const int pi = partition(arr, low, high);
    quickSort(arr, low, pi - 1);
    quickSort(arr, pi + 1, high);
  }
}

ObjectResult *arraySortMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - array
  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

  if (array->size == 0) {
    return newOkResult(vm, args[0]);
  }

  if (!areAllElementsSortable(array)) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "Array contains unsortable or mixed incompatible types",
                    53),
                TYPE, false));
  }

  ObjectArray *sortedArray = newArray(vm, array->size);
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  push(currentModuleRecord, OBJECT_VAL(sortedArray));

  for (uint32_t i = 0; i < array->size; i++) {
    sortedArray->array[i] = array->array[i];
  }
  sortedArray->size = array->size;

  quickSort(sortedArray->array, 0, (int)sortedArray->size - 1);

  pop(currentModuleRecord);
  return newOkResult(vm, OBJECT_VAL(sortedArray));
}

ObjectResult *arrayJoinMethod(VM *vm, int argCount, const Value *args) {
  // arg0 - array
  // arg1 - separator string

  if (!IS_CRUX_STRING(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "Expected arg <separator> to be of type 'string'.", 48),
                TYPE, false));
  }

  const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
  const ObjectString *separator = AS_CRUX_STRING(args[1]);

  if (array->size == 0) {
    ObjectString *emptyResult = copyString(vm, "", 0);
    return newOkResult(vm, OBJECT_VAL(emptyResult));
  }

  // estimate: 3 chars per element + separators
  size_t bufferSize =
      array->size * 3 +
      (array->size > 1 ? (array->size - 1) * separator->length : 0);
  char *buffer = malloc(bufferSize);
  if (!buffer) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Memory allocation failed", 24),
                     RUNTIME, false));
  }

  size_t actualLength = 0;

  for (uint32_t i = 0; i < array->size; i++) {
    const ObjectString *element = toString(vm, array->array[i]);

    size_t neededSpace = element->length;
    if (i > 0) {
      neededSpace += separator->length;
    }

    if (actualLength + neededSpace > bufferSize) {
      // Grow the buffer by 50%
      const size_t newSize =
          (size_t)(((double)bufferSize * 1.5) + (double)neededSpace);
      char *newBuffer = realloc(buffer, newSize);
      if (!newBuffer) {
        free(buffer);
        return newErrorResult(
            vm, newError(vm, copyString(vm, "Memory reallocation failed", 26),
                         MEMORY, false));
      }
      buffer = newBuffer;
      bufferSize = newSize;
    }

    if (i > 0) {
      memcpy(buffer + actualLength, separator->chars, separator->length);
      actualLength += separator->length;
    }

    memcpy(buffer + actualLength, element->chars, element->length);
    actualLength += element->length;
  }

  if (actualLength >= bufferSize) {
    char *newBuffer = realloc(buffer, actualLength + 1);
    if (!newBuffer) {
      free(buffer);
      return newErrorResult(
          vm, newError(vm, copyString(vm, "Memory reallocation failed", 26),
                       MEMORY, false));
    }
    buffer = newBuffer;
  }
  buffer[actualLength] = '\0';

  ObjectString *result = takeString(vm, buffer, (uint32_t)actualLength);
  return newOkResult(vm, OBJECT_VAL(result));
}
