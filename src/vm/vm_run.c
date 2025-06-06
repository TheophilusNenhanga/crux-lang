#include <stdio.h>

#include "../file_handler.h"
#include "../std/std.h"
#include "vm.h"
#include "vm_helpers.h"
#include "vm_run.h"

#include <string.h>

#include "../debug.h"
#include "../object.h"
#include "../panic.h"

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH() goto *dispatchTable[endIndex]
#else
#define DISPATCH()                                                             \
  instruction = READ_BYTE();                                                   \
  goto *dispatchTable[instruction]
#endif

/**
 * Executes bytecode in the virtual machine.
 * @param vm The virtual machine
 * @param isAnonymousFrame Is this frame anonymous? (should this frame return
 * from run() at OP_RETURN?)
 * @return The interpretation result
 */
InterpretResult run(VM *vm, const bool isAnonymousFrame) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  CallFrame *frame =
      &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_CRUX_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT_16()                                                     \
  (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING_16() AS_CRUX_STRING(READ_CONSTANT_16())

  static void *dispatchTable[] = {&&OP_RETURN,
                                  &&OP_CONSTANT,
                                  &&OP_NIL,
                                  &&OP_TRUE,
                                  &&OP_FALSE,
                                  &&OP_NEGATE,
                                  &&OP_EQUAL,
                                  &&OP_GREATER,
                                  &&OP_LESS,
                                  &&OP_LESS_EQUAL,
                                  &&OP_GREATER_EQUAL,
                                  &&OP_NOT_EQUAL,
                                  &&OP_ADD,
                                  &&OP_NOT,
                                  &&OP_SUBTRACT,
                                  &&OP_MULTIPLY,
                                  &&OP_DIVIDE,
                                  &&OP_POP,
                                  &&OP_DEFINE_GLOBAL,
                                  &&OP_GET_GLOBAL,
                                  &&OP_SET_GLOBAL,
                                  &&OP_GET_LOCAL,
                                  &&OP_SET_LOCAL,
                                  &&OP_JUMP_IF_FALSE,
                                  &&OP_JUMP,
                                  &&OP_LOOP,
                                  &&OP_CALL,
                                  &&OP_CLOSURE,
                                  &&OP_GET_UPVALUE,
                                  &&OP_SET_UPVALUE,
                                  &&OP_CLOSE_UPVALUE,
                                  &&OP_CLASS,
                                  &&OP_GET_PROPERTY,
                                  &&OP_SET_PROPERTY,
                                  &&OP_INVOKE,
                                  &&OP_METHOD,
                                  &&OP_INHERIT,
                                  &&OP_GET_SUPER,
                                  &&OP_SUPER_INVOKE,
                                  &&OP_ARRAY,
                                  &&OP_GET_COLLECTION,
                                  &&OP_SET_COLLECTION,
                                  &&OP_MODULUS,
                                  &&OP_LEFT_SHIFT,
                                  &&OP_RIGHT_SHIFT,
                                  &&OP_SET_LOCAL_SLASH,
                                  &&OP_SET_LOCAL_STAR,
                                  &&OP_SET_LOCAL_PLUS,
                                  &&OP_SET_LOCAL_MINUS,
                                  &&OP_SET_UPVALUE_SLASH,
                                  &&OP_SET_UPVALUE_STAR,
                                  &&OP_SET_UPVALUE_PLUS,
                                  &&OP_SET_UPVALUE_MINUS,
                                  &&OP_SET_GLOBAL_SLASH,
                                  &&OP_SET_GLOBAL_STAR,
                                  &&OP_SET_GLOBAL_PLUS,
                                  &&OP_SET_GLOBAL_MINUS,
                                  &&OP_TABLE,
                                  &&OP_ANON_FUNCTION,
                                  &&OP_PUB,
                                  &&OP_MATCH,
                                  &&OP_MATCH_JUMP,
                                  &&OP_MATCH_END,
                                  &&OP_RESULT_MATCH_OK,
                                  &&OP_RESULT_MATCH_ERR,
                                  &&OP_RESULT_BIND,
                                  &&OP_GIVE,
                                  &&OP_INT_DIVIDE,
                                  &&OP_POWER,
                                  &&OP_SET_GLOBAL_INT_DIVIDE,
                                  &&OP_SET_GLOBAL_MODULUS,
                                  &&OP_SET_LOCAL_INT_DIVIDE,
                                  &&OP_SET_LOCAL_MODULUS,
                                  &&OP_SET_UPVALUE_INT_DIVIDE,
                                  &&OP_SET_UPVALUE_MODULUS,
                                  &&OP_USE_NATIVE,
                                  &&OP_USE_MODULE,
                                  &&OP_FINISH_USE,
                                  &&OP_CONSTANT_16,
                                  &&OP_DEFINE_GLOBAL_16,
                                  &&OP_GET_GLOBAL_16,
                                  &&OP_SET_GLOBAL_16,
                                  &&OP_GET_PROPERTY_16,
                                  &&OP_SET_PROPERTY_16,
                                  &&OP_INVOKE_16,
                                  &&OP_SUPER_INVOKE_16,
                                  &&OP_GET_SUPER_16,
                                  &&OP_CLASS_16,
                                  &&OP_METHOD_16,
                                  &&end};

  uint8_t instruction;
#ifdef DEBUG_TRACE_EXECUTION
  static uint8_t endIndex =
      sizeof(dispatchTable) / sizeof(dispatchTable[0]) - 1;
#endif
  DISPATCH();
OP_RETURN: {
  Value result = pop(currentModuleRecord);
  closeUpvalues(currentModuleRecord, frame->slots);
  currentModuleRecord->frameCount--;
  if (currentModuleRecord->frameCount == 0) {
    pop(currentModuleRecord);
    return INTERPRET_OK;
  }
  currentModuleRecord->stackTop = frame->slots;
  push(currentModuleRecord, result);
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];

  if (isAnonymousFrame)
    return INTERPRET_OK;
  DISPATCH();
}

OP_CONSTANT: {
  Value constant = READ_CONSTANT();
  push(currentModuleRecord, constant);
  DISPATCH();
}

OP_NIL: {
  push(currentModuleRecord, NIL_VAL);
  DISPATCH();
}

OP_TRUE: {
  push(currentModuleRecord, BOOL_VAL(true));
  DISPATCH();
}

OP_FALSE: {
  push(currentModuleRecord, BOOL_VAL(false));
  DISPATCH();
}

OP_NEGATE: {
  Value operand = peek(currentModuleRecord, 0);
  if (IS_INT(operand)) {
    int32_t iVal = AS_INT(operand);
    if (iVal == INT32_MIN) {
      popPush(currentModuleRecord, FLOAT_VAL(-(double)INT32_MIN));
    } else {
      popPush(currentModuleRecord, INT_VAL(-iVal));
    }
  } else if (IS_FLOAT(operand)) {
    popPush(currentModuleRecord, FLOAT_VAL(-AS_FLOAT(operand)));
  } else {
    pop(currentModuleRecord);
    runtimePanic(vm, TYPE, typeErrorMessage(vm, operand, "int' | 'float"));
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_EQUAL: {
  Value b = pop(currentModuleRecord);
  Value a = pop(currentModuleRecord);
  push(currentModuleRecord, BOOL_VAL(valuesEqual(a, b)));
  DISPATCH();
}

OP_GREATER: {
  if (!binaryOperation(vm, OP_GREATER)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_LESS: {
  if (!binaryOperation(vm, OP_LESS)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_LESS_EQUAL: {
  if (!binaryOperation(vm, OP_LESS_EQUAL)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_GREATER_EQUAL: {
  if (!binaryOperation(vm, OP_GREATER_EQUAL)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_NOT_EQUAL: {
  Value b = pop(currentModuleRecord);
  Value a = pop(currentModuleRecord);
  push(currentModuleRecord, BOOL_VAL(!valuesEqual(a, b)));
  DISPATCH();
}

OP_ADD: {
  if (IS_CRUX_STRING(peek(currentModuleRecord, 0)) ||
      IS_CRUX_STRING(peek(currentModuleRecord, 1))) {
    if (!concatenate(vm)) {
      return INTERPRET_RUNTIME_ERROR;
    }
    DISPATCH();
  }
  if (!binaryOperation(vm, OP_ADD)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_NOT: {
  push(currentModuleRecord, BOOL_VAL(isFalsy(pop(currentModuleRecord))));
  DISPATCH();
}

OP_SUBTRACT: {
  if (!binaryOperation(vm, OP_SUBTRACT)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_MULTIPLY: {
  if (!binaryOperation(vm, OP_MULTIPLY)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_DIVIDE: {
  if (!binaryOperation(vm, OP_DIVIDE)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_POP: {
  pop(currentModuleRecord);
  DISPATCH();
}

OP_DEFINE_GLOBAL: {
  ObjectString *name = READ_STRING();
  bool isPublic = false;
  if (checkPreviousInstruction(frame, 3, OP_PUB)) {
    isPublic = true;
  }
  if (tableSet(vm, &currentModuleRecord->globals, name,
               peek(currentModuleRecord, 0))) {
    if (isPublic) {
      tableSet(vm, &currentModuleRecord->publics, name,
               peek(currentModuleRecord, 0));
    }
    pop(currentModuleRecord);
    DISPATCH();
  }
  runtimePanic(vm, NAME, "Cannot define '%s' because it is already defined.",
               name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL: {
  ObjectString *name = READ_STRING();
  Value value;
  if (tableGet(&currentModuleRecord->globals, name, &value)) {
    push(currentModuleRecord, value);
    DISPATCH();
  }
  runtimePanic(vm, NAME, "Undefined variable '%s'.", name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL: {
  ObjectString *name = READ_STRING();
  if (tableSet(vm, &currentModuleRecord->globals, name,
               peek(currentModuleRecord, 0))) {
    runtimePanic(vm, NAME,
                 "Cannot give variable '%s' a value because it has not been "
                 "defined\nDid you forget 'let'?",
                 name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_GET_LOCAL: {
  uint8_t slot = READ_BYTE();
  push(currentModuleRecord, frame->slots[slot]);
  DISPATCH();
}

OP_SET_LOCAL: {
  uint8_t slot = READ_BYTE();
  frame->slots[slot] = peek(currentModuleRecord, 0);
  DISPATCH();
}

OP_JUMP_IF_FALSE: {
  uint16_t offset = READ_SHORT();
  if (isFalsy(peek(currentModuleRecord, 0))) {
    frame->ip += offset;
    DISPATCH();
  }
  DISPATCH();
}

OP_JUMP: {
  uint16_t offset = READ_SHORT();
  frame->ip += offset;
  DISPATCH();
}

OP_LOOP: {
  uint16_t offset = READ_SHORT();
  frame->ip -= offset;
  DISPATCH();
}

OP_CALL: {
  int argCount = READ_BYTE();
  if (!callValue(vm, peek(currentModuleRecord, argCount), argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];
  DISPATCH();
}

OP_CLOSURE: {
  ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
  ObjectClosure *closure = newClosure(vm, function);
  push(currentModuleRecord, OBJECT_VAL(closure));

  for (int i = 0; i < closure->upvalueCount; i++) {
    uint8_t isLocal = READ_BYTE();
    uint8_t index = READ_BYTE();

    if (isLocal) {
      closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
    } else {
      closure->upvalues[i] = frame->closure->upvalues[index];
    }
  }
  DISPATCH();
}

OP_GET_UPVALUE: {
  uint8_t slot = READ_BYTE();
  push(currentModuleRecord, *frame->closure->upvalues[slot]->location);
  DISPATCH();
}

OP_SET_UPVALUE: {
  uint8_t slot = READ_BYTE();
  *frame->closure->upvalues[slot]->location = peek(currentModuleRecord, 0);
  DISPATCH();
}

OP_CLOSE_UPVALUE: {
  closeUpvalues(currentModuleRecord, currentModuleRecord->stackTop - 1);
  pop(currentModuleRecord);
  DISPATCH();
}

OP_CLASS: {
  push(currentModuleRecord, OBJECT_VAL(newClass(vm, READ_STRING())));
  DISPATCH();
}

OP_GET_PROPERTY: {
  Value receiver = peek(currentModuleRecord, 0);
  if (!IS_CRUX_INSTANCE(receiver)) {
    ObjectString *name = READ_STRING();
    runtimePanic(vm, TYPE,
                 "Cannot access property '%s' on non-instance value. %s",
                 name->chars, typeErrorMessage(vm, receiver, "instance"));
    return INTERPRET_RUNTIME_ERROR;
  }
  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);
  ObjectString *name = READ_STRING();

  Value value;
  bool fieldFound = false;
  if (tableGet(&instance->fields, name, &value)) {
    pop(currentModuleRecord);
    push(currentModuleRecord, value);
    fieldFound = true;
    DISPATCH();
  }

  if (!fieldFound) {
    if (!bindMethod(vm, instance->klass, name)) {
      runtimePanic(vm, RUNTIME, "Failed to bind method '%s'", name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  DISPATCH();
}

OP_SET_PROPERTY: {
  Value receiver = peek(currentModuleRecord, 1);
  if (!IS_CRUX_INSTANCE(receiver)) {
    ObjectString *name = READ_STRING();
    runtimePanic(vm, TYPE, "Cannot set property '%s' on non-instance value. %s",
                 name->chars, typeErrorMessage(vm, receiver, "instance"));
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);
  ObjectString *name = READ_STRING();

  tableSet(vm, &instance->fields, name, peek(currentModuleRecord, 0));
  Value value = pop(currentModuleRecord);
  popPush(currentModuleRecord, value);
  DISPATCH();
}

OP_INVOKE: {
  ObjectString *methodName = READ_STRING();
  int argCount = READ_BYTE();
  if (!invoke(vm, methodName, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];
  DISPATCH();
}

OP_METHOD: {
  defineMethod(vm, READ_STRING());
  DISPATCH();
}

OP_INHERIT: {
  Value superClass = peek(currentModuleRecord, 1);

  if (!IS_CRUX_CLASS(superClass)) {
    runtimePanic(vm, TYPE, "Cannot inherit from non class object.");
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectClass *subClass = AS_CRUX_CLASS(peek(currentModuleRecord, 0));
  tableAddAll(vm, &AS_CRUX_CLASS(superClass)->methods, &subClass->methods);
  pop(currentModuleRecord);
  DISPATCH();
}

OP_GET_SUPER: {
  ObjectString *name = READ_STRING();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(currentModuleRecord));

  if (!bindMethod(vm, superClass, name)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SUPER_INVOKE: {
  ObjectString *method = READ_STRING();
  int argCount = READ_BYTE();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(currentModuleRecord));
  if (!invokeFromClass(vm, superClass, method, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];
  DISPATCH();
}

OP_ARRAY: {
  uint16_t elementCount = READ_SHORT();
  ObjectArray *array = newArray(vm, elementCount);
  for (int i = elementCount - 1; i >= 0; i--) {
    arrayAdd(vm, array, pop(currentModuleRecord), i);
  }
  push(currentModuleRecord, OBJECT_VAL(array));
  DISPATCH();
}

OP_GET_COLLECTION: {
  Value indexValue = pop(currentModuleRecord);
  if (!IS_CRUX_OBJECT(peek(currentModuleRecord, 0))) {
    runtimePanic(vm, TYPE, "Cannot get from a non-collection type.");
    return INTERPRET_RUNTIME_ERROR;
  }
  switch (AS_CRUX_OBJECT(peek(currentModuleRecord, 0))->type) {
  case OBJECT_TABLE: {
    if (IS_CRUX_STRING(indexValue) || IS_INT(indexValue)) {
      ObjectTable *table = AS_CRUX_TABLE(peek(currentModuleRecord, 0));
      Value value;
      if (!objectTableGet(table, indexValue, &value)) {
        runtimePanic(vm, COLLECTION_GET, "Failed to get value from table");
        return INTERPRET_RUNTIME_ERROR;
      }
      popPush(currentModuleRecord, value);
    } else {
      runtimePanic(vm, TYPE, "Key cannot be hashed.", READ_STRING());
      return INTERPRET_RUNTIME_ERROR;
    }
    DISPATCH();
  }
  case OBJECT_ARRAY: {
    if (!IS_INT(indexValue)) {
      runtimePanic(vm, TYPE, "Index must be of type 'int'.");
      return INTERPRET_RUNTIME_ERROR;
    }
    int index = AS_INT(indexValue);
    ObjectArray *array = AS_CRUX_ARRAY(peek(currentModuleRecord, 0));
    Value value;
    if (index < 0 || index >= array->size) {
      runtimePanic(vm, INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
      return INTERPRET_RUNTIME_ERROR;
    }
    if (!arrayGet(array, index, &value)) {
      runtimePanic(vm, COLLECTION_GET, "Failed to get value from array");
      return INTERPRET_RUNTIME_ERROR;
    }
    popPush(
        currentModuleRecord,
        value); // pop the array off the stack // push the value onto the stack
    DISPATCH();
  }
  case OBJECT_STRING: {
    if (!IS_INT(indexValue)) {
      runtimePanic(vm, TYPE, "Index must be of type 'int'.");
      return INTERPRET_RUNTIME_ERROR;
    }
    int index = AS_INT(indexValue);
    ObjectString *string = AS_CRUX_STRING(peek(currentModuleRecord, 0));
    ObjectString *ch;
    if (index < 0 || index >= string->length) {
      runtimePanic(vm, INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
      return INTERPRET_RUNTIME_ERROR;
    }
    // Only single character indexing
    ch = copyString(vm, string->chars + index, 1);
    popPush(currentModuleRecord, OBJECT_VAL(ch));
    DISPATCH();
  }
  default: {
    runtimePanic(vm, TYPE, "Cannot get from a non-collection type.");
    return INTERPRET_RUNTIME_ERROR;
  }
  }
  DISPATCH();
}

OP_SET_COLLECTION: {
  Value value = pop(currentModuleRecord);
  Value indexValue = peek(currentModuleRecord, 0);

  if (IS_CRUX_TABLE(peek(currentModuleRecord, 1))) {
    ObjectTable *table = AS_CRUX_TABLE(peek(currentModuleRecord, 1));
    if (IS_INT(indexValue) || IS_CRUX_STRING(indexValue)) {
      if (!objectTableSet(vm, table, indexValue, value)) {
        runtimePanic(vm, COLLECTION_GET, "Failed to set value in table");
        return INTERPRET_RUNTIME_ERROR;
      }
    } else {
      runtimePanic(vm, TYPE, "Key cannot be hashed.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (IS_CRUX_ARRAY(peek(currentModuleRecord, 1))) {
    ObjectArray *array = AS_CRUX_ARRAY(peek(currentModuleRecord, 1));
    int index = AS_INT(indexValue);
    if (!arraySet(vm, array, index, value)) {
      runtimePanic(vm, INDEX_OUT_OF_BOUNDS,
                   "Cannot set a value in an empty array.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else {
    runtimePanic(vm, TYPE, "Value is not a mutable collection type.");
    return INTERPRET_RUNTIME_ERROR;
  }
  popTwo(currentModuleRecord); // indexValue and collection
  push(currentModuleRecord, indexValue);
  DISPATCH();
}

OP_MODULUS: {
  if (!binaryOperation(vm, OP_MODULUS)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_LEFT_SHIFT: {
  if (!binaryOperation(vm, OP_LEFT_SHIFT)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_RIGHT_SHIFT: {
  if (!binaryOperation(vm, OP_RIGHT_SHIFT)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_LOCAL_SLASH: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0); // Right-hand side

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '/=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  double dcurrent =
      currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
  double doperand =
      operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);

  if (doperand == 0.0) {
    runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero in '/=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  resultValue = FLOAT_VAL(dcurrent / doperand);
  frame->slots[slot] = resultValue;
  DISPATCH();
}

OP_SET_LOCAL_STAR: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '*=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  if (currentIsInt && operandIsInt) {
    int32_t icurrent = AS_INT(currentValue);
    int32_t ioperand = AS_INT(operandValue);
    int64_t result = (int64_t)icurrent * (int64_t)ioperand;
    if (result >= INT32_MIN && result <= INT32_MAX) {
      resultValue = INT_VAL((int32_t)result);
    } else {
      resultValue = FLOAT_VAL((double)result); // Promote
    }
  } else {
    double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    double doperand =
        operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);
    resultValue = FLOAT_VAL(dcurrent * doperand);
  }

  frame->slots[slot] = resultValue;
  DISPATCH();
}

OP_SET_LOCAL_PLUS: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (IS_CRUX_STRING(currentValue) || IS_CRUX_STRING(operandValue)) {
    push(currentModuleRecord, currentValue);
    if (!concatenate(vm)) {
      pop(currentModuleRecord);

      return INTERPRET_RUNTIME_ERROR;
    }

    frame->slots[slot] = peek(currentModuleRecord, 0);
  } else if ((currentIsInt || currentIsFloat) &&
             (operandIsInt || operandIsFloat)) {
    Value resultValue;
    if (currentIsInt && operandIsInt) {
      int32_t icurrent = AS_INT(currentValue);
      int32_t ioperand = AS_INT(operandValue);
      int64_t result = (int64_t)icurrent + (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result); // Promote
      }
    } else {
      double dcurrent = currentIsFloat ? AS_FLOAT(currentValue)
                                       : (double)AS_INT(currentValue);
      double doperand = operandIsFloat ? AS_FLOAT(operandValue)
                                       : (double)AS_INT(operandValue);
      resultValue = FLOAT_VAL(dcurrent + doperand);
    }
    frame->slots[slot] = resultValue;
  } else {
    runtimePanic(
        vm, TYPE,
        "Operands for '+=' must be of type 'float' | 'int' | 'string'.");
    return INTERPRET_RUNTIME_ERROR;
  }

  DISPATCH();
}

OP_SET_LOCAL_MINUS: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '-=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  if (currentIsInt && operandIsInt) {
    int32_t icurrent = AS_INT(currentValue);
    int32_t ioperand = AS_INT(operandValue);
    int64_t result = (int64_t)icurrent - (int64_t)ioperand;
    if (result >= INT32_MIN && result <= INT32_MAX) {
      resultValue = INT_VAL((int32_t)result);
    } else {
      resultValue = FLOAT_VAL((double)result);
    }
  } else {
    double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    double doperand =
        operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);
    resultValue = FLOAT_VAL(dcurrent - doperand);
  }

  frame->slots[slot] = resultValue;
  DISPATCH();
}

OP_SET_UPVALUE_SLASH: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '/=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  double dcurrent =
      currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
  double doperand =
      operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);

  if (doperand == 0.0) {
    runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero in '/=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  resultValue = FLOAT_VAL(dcurrent / doperand);
  *location = resultValue;
  DISPATCH();
}

OP_SET_UPVALUE_STAR: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '*=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  if (currentIsInt && operandIsInt) {
    int32_t icurrent = AS_INT(currentValue);
    int32_t ioperand = AS_INT(operandValue);
    int64_t result = (int64_t)icurrent * (int64_t)ioperand;
    if (result >= INT32_MIN && result <= INT32_MAX) {
      resultValue = INT_VAL((int32_t)result);
    } else {
      resultValue = FLOAT_VAL((double)result);
    }
  } else {
    double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    double doperand =
        operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);
    resultValue = FLOAT_VAL(dcurrent * doperand);
  }

  *location = resultValue;
  DISPATCH();
}

OP_SET_UPVALUE_PLUS: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (IS_CRUX_STRING(currentValue) || IS_CRUX_STRING(operandValue)) {
    push(currentModuleRecord, currentValue);
    if (!concatenate(vm)) {
      pop(currentModuleRecord);
      return INTERPRET_RUNTIME_ERROR;
    }
    *location = peek(currentModuleRecord, 0);
  } else if ((currentIsInt || currentIsFloat) &&
             (operandIsInt || operandIsFloat)) {
    Value resultValue;
    if (currentIsInt && operandIsInt) {
      int32_t icurrent = AS_INT(currentValue);
      int32_t ioperand = AS_INT(operandValue);
      int64_t result = (int64_t)icurrent + (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
    } else {
      double dcurrent = currentIsFloat ? AS_FLOAT(currentValue)
                                       : (double)AS_INT(currentValue);
      double doperand = operandIsFloat ? AS_FLOAT(operandValue)
                                       : (double)AS_INT(operandValue);
      resultValue = FLOAT_VAL(dcurrent + doperand);
    }
    *location = resultValue;
  } else {
    runtimePanic(vm, TYPE, "Operands for '+=' must be numbers or strings.");
    return INTERPRET_RUNTIME_ERROR;
  }

  DISPATCH();
}

OP_SET_UPVALUE_MINUS: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    runtimePanic(vm, TYPE, "Operands for '-=' must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  if (currentIsInt && operandIsInt) {
    int32_t icurrent = AS_INT(currentValue);
    int32_t ioperand = AS_INT(operandValue);
    int64_t result = (int64_t)icurrent - (int64_t)ioperand;
    if (result >= INT32_MIN && result <= INT32_MAX) {
      resultValue = INT_VAL((int32_t)result);
    } else {
      resultValue = FLOAT_VAL((double)result);
    }
  } else {
    double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    double doperand =
        operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);
    resultValue = FLOAT_VAL(dcurrent - doperand);
  }

  *location = resultValue;
  DISPATCH();
}

OP_SET_GLOBAL_SLASH: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_SLASH, "/=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_GLOBAL_STAR: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_STAR, "*=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_GLOBAL_PLUS: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_PLUS, "+=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_GLOBAL_MINUS: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_MINUS, "-=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_TABLE: {
  uint16_t elementCount = READ_SHORT();
  ObjectTable *table = newTable(vm, elementCount);
  for (int i = elementCount - 1; i >= 0; i--) {
    Value value = pop(currentModuleRecord);
    Value key = pop(currentModuleRecord);
    if (IS_INT(key) || IS_CRUX_STRING(key) || IS_FLOAT(key)) {
      if (!objectTableSet(vm, table, key, value)) {
        runtimePanic(vm, COLLECTION_SET, "Failed to set value in table");
        return INTERPRET_RUNTIME_ERROR;
      }
    } else {
      runtimePanic(vm, TYPE, "Key cannot be hashed.", READ_STRING());
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  push(currentModuleRecord, OBJECT_VAL(table));
  DISPATCH();
}

OP_ANON_FUNCTION: {
  ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
  function->moduleRecord = currentModuleRecord;
  ObjectClosure *closure = newClosure(vm, function);
  push(currentModuleRecord, OBJECT_VAL(closure));
  for (int i = 0; i < closure->upvalueCount; i++) {
    uint8_t isLocal = READ_BYTE();
    uint8_t index = READ_BYTE();

    if (isLocal) {
      closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
    } else {
      closure->upvalues[i] = frame->closure->upvalues[index];
    }
  }
  DISPATCH();
}

OP_PUB: { DISPATCH(); }

OP_MATCH: {
  Value target = peek(currentModuleRecord, 0);
  vm->matchHandler.matchTarget = target;
  vm->matchHandler.isMatchTarget = true;
  DISPATCH();
}

OP_MATCH_JUMP: {
  uint16_t offset = READ_SHORT();
  Value pattern = pop(currentModuleRecord);
  Value target = peek(currentModuleRecord, 0);
  if (!valuesEqual(pattern, target)) {
    frame->ip += offset;
  }
  DISPATCH();
}

OP_MATCH_END: {
  if (vm->matchHandler.isMatchBind) {
    push(currentModuleRecord, vm->matchHandler.matchBind);
  }
  vm->matchHandler.matchTarget = NIL_VAL;
  vm->matchHandler.matchBind = NIL_VAL;
  vm->matchHandler.isMatchBind = false;
  vm->matchHandler.isMatchTarget = false;
  DISPATCH();
}

OP_RESULT_MATCH_OK: {
  uint16_t offset = READ_SHORT();
  Value target = peek(currentModuleRecord, 0);
  if (!IS_CRUX_RESULT(target) || !AS_CRUX_RESULT(target)->isOk) {
    frame->ip += offset;
  } else {
    Value value = AS_CRUX_RESULT(target)->as.value;
    popPush(currentModuleRecord, value);
  }
  DISPATCH();
}

OP_RESULT_MATCH_ERR: {
  uint16_t offset = READ_SHORT();
  Value target = peek(currentModuleRecord, 0);
  if (!IS_CRUX_RESULT(target) || AS_CRUX_RESULT(target)->isOk) {
    frame->ip += offset;
  } else {
    Value error = OBJECT_VAL(AS_CRUX_RESULT(target)->as.error);
    popPush(currentModuleRecord, error);
  }
  DISPATCH();
}

OP_RESULT_BIND: {
  uint8_t slot = READ_BYTE();
  Value bind = peek(currentModuleRecord, 0);
  vm->matchHandler.matchBind = bind;
  vm->matchHandler.isMatchBind = true;
  frame->slots[slot] = bind;
  DISPATCH();
}

OP_GIVE: {
  Value result = pop(currentModuleRecord);
  popPush(currentModuleRecord, result);
  DISPATCH();
}

OP_INT_DIVIDE: {
  if (!binaryOperation(vm, OP_INT_DIVIDE)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_POWER: {
  if (!binaryOperation(vm, OP_POWER)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_GLOBAL_INT_DIVIDE: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_INT_DIVIDE, "\\=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_GLOBAL_MODULUS: {
  ObjectString *name = READ_STRING();
  if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_MODULUS, "%=") ==
      INTERPRET_RUNTIME_ERROR) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SET_LOCAL_INT_DIVIDE: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0);

  if (!IS_INT(currentValue) || !IS_INT(operandValue)) {
    runtimePanic(vm, TYPE, "Operands for '//=' must both be integers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  int32_t icurrent = AS_INT(currentValue);
  int32_t ioperand = AS_INT(operandValue);

  if (ioperand == 0) {
    runtimePanic(vm, DIVISION_BY_ZERO,
                 "Integer division by zero in '//=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;
  if (icurrent == INT32_MIN && ioperand == -1) {
    resultValue = FLOAT_VAL(-(double)INT32_MIN); // Promote
  } else {
    resultValue = INT_VAL(icurrent / ioperand);
  }

  frame->slots[slot] = resultValue;
  DISPATCH();
}
OP_SET_LOCAL_MODULUS: {
  uint8_t slot = READ_BYTE();
  Value currentValue = frame->slots[slot];
  Value operandValue = peek(currentModuleRecord, 0);

  if (!IS_INT(currentValue) || !IS_INT(operandValue)) {
    runtimePanic(vm, TYPE, "Operands for '%=' must both be integers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  int32_t icurrent = AS_INT(currentValue);
  int32_t ioperand = AS_INT(operandValue);

  if (ioperand == 0) {
    runtimePanic(vm, DIVISION_BY_ZERO, "Modulo by zero in '%=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;
  if (icurrent == INT32_MIN && ioperand == -1) {
    resultValue = INT_VAL(0);
  } else {
    resultValue = INT_VAL(icurrent % ioperand);
  }

  frame->slots[slot] = resultValue;
  DISPATCH();
}

OP_SET_UPVALUE_INT_DIVIDE: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  if (!IS_INT(currentValue) || !IS_INT(operandValue)) {
    runtimePanic(vm, TYPE, "Operands for '//=' must both be integers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  int32_t icurrent = AS_INT(currentValue);
  int32_t ioperand = AS_INT(operandValue);

  if (ioperand == 0) {
    runtimePanic(vm, DIVISION_BY_ZERO,
                 "Integer division by zero in '//=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;
  if (icurrent == INT32_MIN && ioperand == -1) {
    resultValue = FLOAT_VAL(-(double)INT32_MIN); // Promote
  } else {
    resultValue = INT_VAL(icurrent / ioperand);
  }

  *location = resultValue;
  DISPATCH();
}

OP_SET_UPVALUE_MODULUS: {
  uint8_t slot = READ_BYTE();
  Value *location = frame->closure->upvalues[slot]->location;
  Value currentValue = *location;
  Value operandValue = peek(currentModuleRecord, 0);

  if (!IS_INT(currentValue) || !IS_INT(operandValue)) {
    runtimePanic(vm, TYPE, "Operands for '%=' must both be of type 'int'.");
    return INTERPRET_RUNTIME_ERROR;
  }

  int32_t icurrent = AS_INT(currentValue);
  int32_t ioperand = AS_INT(operandValue);

  if (ioperand == 0) {
    runtimePanic(vm, DIVISION_BY_ZERO, "Modulo by zero in '%=' assignment.");
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;
  if (ioperand == -1 && icurrent == INT32_MIN) {
    resultValue = INT_VAL(0);
  } else {
    resultValue = INT_VAL(icurrent % ioperand);
  }

  *location = resultValue;
  DISPATCH();
}

OP_USE_NATIVE: {
  uint8_t nameCount = READ_BYTE();
  ObjectString *names[UINT8_MAX];
  ObjectString *aliases[UINT8_MAX];

  for (uint8_t i = 0; i < nameCount; i++) {
    names[i] = READ_STRING();
  }
  for (uint8_t i = 0; i < nameCount; i++) {
    aliases[i] = READ_STRING();
  }

  ObjectString *moduleName = READ_STRING();
  int moduleIndex = -1;
  for (int i = 0; i < vm->nativeModules.count; i++) {
    if (memcmp(moduleName->chars, vm->nativeModules.modules[i].name,
               moduleName->length) == 0) {
      moduleIndex = i;
      break;
    }
  }
  if (moduleIndex == -1) {
    runtimePanic(vm, IMPORT, "Module '%s' not found.", moduleName->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  Table *moduleTable = vm->nativeModules.modules[moduleIndex].names;
  for (int i = 0; i < nameCount; i++) {
    Value value;
    bool getSuccess = tableGet(moduleTable, names[i], &value);
    if (!getSuccess) {
      runtimePanic(vm, IMPORT, "Failed to import '%s' from '%s'.",
                   names[i]->chars, moduleName->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
    push(currentModuleRecord, OBJECT_VAL(value));
    bool setSuccess =
        tableSet(vm, &vm->currentModuleRecord->globals, aliases[i], value);

    if (!setSuccess) {
      runtimePanic(vm, IMPORT, "Failed to import '%s' from '%s'.",
                   names[i]->chars, moduleName->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
    pop(currentModuleRecord);
  }

  DISPATCH();
}

OP_USE_MODULE: {
  ObjectString *moduleName = READ_STRING();

  if (isInImportStack(vm, moduleName)) {
    runtimePanic(vm, IMPORT, "Circular dependency detected when importing: %s",
                 moduleName->chars);
    vm->currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_RUNTIME_ERROR;
  }

  char *resolvedPathChars =
      resolvePath(vm->currentModuleRecord->path->chars, moduleName->chars);
  if (resolvedPathChars == NULL) {
    runtimePanic(vm, IMPORT, "Failed to resolve import path");
    vm->currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_RUNTIME_ERROR;
  }
  ObjectString *resolvedPath = takeString(
      vm, resolvedPathChars, strlen(resolvedPathChars)); // VM takes ownership

  Value cachedModule;
  if (tableGet(&vm->moduleCache, resolvedPath, &cachedModule)) {
    push(currentModuleRecord, cachedModule);
    DISPATCH();
  }

  if (vm->importCount + 1 > IMPORT_MAX) {
    runtimePanic(vm, IMPORT, "Import limit reached");
    return INTERPRET_RUNTIME_ERROR;
  }
  vm->importCount++;

  FileResult file = readFile(resolvedPath->chars);
  if (file.error != NULL) {
    runtimePanic(vm, IO, file.error);
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectModuleRecord *module = newObjectModuleRecord(vm, resolvedPath);
  module->enclosingModule = vm->currentModuleRecord;
  resetStack(module);
  if (module->frames == NULL) {
    runtimePanic(vm, MEMORY,
                 "Failed to allocate memory for new module from \"%s\".",
                 resolvedPath->chars);
    vm->currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_RUNTIME_ERROR;
  }
  pushImportStack(vm, resolvedPath);

  ObjectModuleRecord *previousModuleRecord = vm->currentModuleRecord;
  vm->currentModuleRecord = module;

  initTable(&vm->currentModuleRecord->globals);
  initTable(&vm->currentModuleRecord->publics);

  if (!initializeStdLib(vm)) {
    runtimePanic(vm, IO, "Failed to initialize stdlib for module:\"%s\".",
                 module->path->chars);
    module->state = STATE_ERROR;
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(currentModuleRecord, OBJECT_VAL(module));
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectFunction *function = compile(vm, file.content);
  freeFileResult(file);

  if (function == NULL) {
    module->state = STATE_ERROR;
    runtimePanic(vm, RUNTIME, "Failed to compile '%s'.", resolvedPath->chars);
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(currentModuleRecord, OBJECT_VAL(module));
    return INTERPRET_COMPILE_ERROR;
  }
  push(currentModuleRecord, OBJECT_VAL(function));
  ObjectClosure *closure = newClosure(vm, function);
  pop(currentModuleRecord);
  push(currentModuleRecord, OBJECT_VAL(closure));

  module->moduleClosure = closure;

  tableSet(vm, &vm->moduleCache, resolvedPath, OBJECT_VAL(module));

  if (!call(vm, closure, 0)) {
    module->state = STATE_ERROR;
    runtimePanic(vm, RUNTIME, "Failed to call module.");
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(currentModuleRecord, OBJECT_VAL(module));
    return INTERPRET_RUNTIME_ERROR;
  }

  InterpretResult result = run(vm, false);
  if (result != INTERPRET_OK) {
    module->state = STATE_ERROR;
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(currentModuleRecord, OBJECT_VAL(module));
    return result;
  }

  module->state = STATE_LOADED;

  popImportStack(vm);
  vm->currentModuleRecord = previousModuleRecord;
  push(currentModuleRecord, OBJECT_VAL(module));

  DISPATCH();
}

OP_FINISH_USE: {
  uint8_t nameCount = READ_BYTE();
  ObjectString *names[UINT8_MAX];
  ObjectString *aliases[UINT8_MAX];

  for (int i = 0; i < nameCount; i++) {
    names[i] = READ_STRING();
  }
  for (int i = 0; i < nameCount; i++) {
    aliases[i] = READ_STRING();
  }
  if (!IS_CRUX_MODULE_RECORD(peek(currentModuleRecord, 0))) {
    runtimePanic(vm, RUNTIME, "Module record creation could not be completed.");
    return INTERPRET_RUNTIME_ERROR;
  }
  Value moduleValue = pop(currentModuleRecord);
  ObjectModuleRecord *importedModule = AS_CRUX_MODULE_RECORD(moduleValue);

  if (importedModule->state == STATE_ERROR) {
    runtimePanic(vm, IMPORT, "Failed to import module from %s",
                 importedModule->path->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  // copy names
  for (uint8_t i = 0; i < nameCount; i++) {
    ObjectString *name = names[i];
    ObjectString *alias = aliases[i];

    Value value;
    if (!tableGet(&importedModule->publics, name, &value)) {
      runtimePanic(vm, IMPORT, "'%s' is not an exported name.", name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }

    if (!tableSet(vm, &vm->currentModuleRecord->globals, alias, value)) {
      runtimePanic(vm, IMPORT,
                   "Failed to import '%s'. This name may already be in use in "
                   "this scope.",
                   name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  vm->importCount--;
  DISPATCH();
}

OP_CONSTANT_16: {
  Value constant = READ_CONSTANT_16();
  push(currentModuleRecord, constant);
  DISPATCH();
}

OP_DEFINE_GLOBAL_16: {
  ObjectString *name = READ_STRING_16();
  bool isPublic = false;
  if (checkPreviousInstruction(frame, 3, OP_PUB)) {
    isPublic = true;
  }
  if (tableSet(vm, &currentModuleRecord->globals, name,
               peek(currentModuleRecord, 0))) {
    if (isPublic) {
      tableSet(vm, &currentModuleRecord->publics, name,
               peek(currentModuleRecord, 0));
    }
    pop(currentModuleRecord);
    DISPATCH();
  }
  runtimePanic(vm, NAME, "Cannot define '%s' because it is already defined.",
               name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL_16: {
  ObjectString *name = READ_STRING_16();
  Value value;
  if (tableGet(&currentModuleRecord->globals, name, &value)) {
    push(currentModuleRecord, value);
    DISPATCH();
  }
  runtimePanic(vm, NAME, "Undefined variable '%s'.", name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL_16: {
  ObjectString *name = READ_STRING_16();
  if (tableSet(vm, &currentModuleRecord->globals, name,
               peek(currentModuleRecord, 0))) {
    runtimePanic(vm, NAME,
                 "Cannot give variable '%s' a value because it has not been "
                 "defined\nDid you forget 'let'?",
                 name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_GET_PROPERTY_16: {
  Value receiver = peek(currentModuleRecord, 0);
  if (!IS_CRUX_INSTANCE(receiver)) {
    ObjectString *name = READ_STRING_16();
    runtimePanic(vm, TYPE,
                 "Cannot access property '%s' on non-instance value. %s",
                 name->chars, typeErrorMessage(vm, receiver, "instance"));
    return INTERPRET_RUNTIME_ERROR;
  }
  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);
  ObjectString *name = READ_STRING_16();

  Value value;
  bool fieldFound = false;
  if (tableGet(&instance->fields, name, &value)) {
    pop(currentModuleRecord);
    push(currentModuleRecord, value);
    fieldFound = true;
    DISPATCH();
  }

  if (!fieldFound) {
    if (!bindMethod(vm, instance->klass, name)) {
      runtimePanic(vm, RUNTIME, "Failed to bind method '%s'", name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  DISPATCH();
}

OP_SET_PROPERTY_16: {
  Value receiver = peek(currentModuleRecord, 1);
  if (!IS_CRUX_INSTANCE(receiver)) {
    ObjectString *name = READ_STRING_16();
    runtimePanic(vm, TYPE, "Cannot set property '%s' on non-instance value. %s",
                 name->chars, typeErrorMessage(vm, receiver, "instance"));
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);
  ObjectString *name = READ_STRING_16();

  tableSet(vm, &instance->fields, name, peek(currentModuleRecord, 0));
  Value value = pop(currentModuleRecord);
  popPush(currentModuleRecord, value);
  DISPATCH();
}

OP_INVOKE_16: {
  ObjectString *methodName = READ_STRING_16();
  int argCount = READ_BYTE();
  if (!invoke(vm, methodName, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];
  DISPATCH();
}

OP_SUPER_INVOKE_16: {
  ObjectString *method = READ_STRING_16();
  int argCount = READ_BYTE();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(currentModuleRecord));
  if (!invokeFromClass(vm, superClass, method, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];
  DISPATCH();
}

OP_GET_SUPER_16: {
  ObjectString *name = READ_STRING_16();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(currentModuleRecord));

  if (!bindMethod(vm, superClass, name)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_CLASS_16: {
  push(currentModuleRecord, OBJECT_VAL(newClass(vm, READ_STRING_16())));
  DISPATCH();
}

OP_METHOD_16: {
  defineMethod(vm, READ_STRING_16());
  DISPATCH();
}

end: {
  printf("        ");
  for (Value *slot = currentModuleRecord->stack;
       slot < currentModuleRecord->stackTop; slot++) {
    printf("[");
    printValue(*slot);
    printf("]");
  }
  printf("\n");

  disassembleInstruction(
      &frame->closure->function->chunk,
      (int)(frame->ip - frame->closure->function->chunk.code));

  instruction = READ_BYTE();
  goto *dispatchTable[instruction];
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT_16
#undef READ_STRING_16
}
