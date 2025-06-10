#include "vm_helpers.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "../compiler.h"
#include "../memory.h"
#include "../object.h"
#include "../panic.h"
#include "../std/std.h"
#include "../table.h"
#include "../value.h"
#include "vm.h"
#include "vm_run.h"

void initImportStack(VM *vm) {
  vm->importStack.paths = NULL;
  vm->importStack.count = 0;
  vm->importStack.capacity = 0;
}

void freeImportStack(VM *vm) {
  FREE_ARRAY(vm, ObjectString *, vm->importStack.paths,
             vm->importStack.capacity);
  initImportStack(vm);
}

bool pushImportStack(VM *vm, ObjectString *path) {
  ImportStack *stack = &vm->importStack;

  if (stack->count + 1 > stack->capacity) { // resize
    const uint32_t oldCapacity = stack->capacity;
    stack->capacity = GROW_CAPACITY(oldCapacity);
    stack->paths = GROW_ARRAY(vm, ObjectString *, stack->paths, oldCapacity,
                              stack->capacity);
    if (stack->paths == NULL) {
      fprintf(stderr,
              "Fatal Error: Could not allocate memory for import stack.\n");
      stack->capacity = oldCapacity;
      return false;
    }
  }

  stack->paths[stack->count] = path;
  stack->count++;
  return true;
}

void popImportStack(VM *vm) {
  ImportStack *stack = &vm->importStack;
  if (stack->count == 0) {
    return;
  }
  stack->count--;
}

static bool stringEquals(const ObjectString *a, const ObjectString *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->chars, b->chars, a->length) == 0;
}

bool isInImportStack(const VM *vm, const ObjectString *path) {
  const ImportStack *stack = &vm->importStack;
  for (int i = 0; i < stack->count; i++) {
    if (stack->paths[i] == path || stringEquals(stack->paths[i], path)) {
      return true;
    }
  }
  return false;
}

VM *newVM(const int argc, const char **argv) {
  VM *vm = malloc(sizeof(VM));
  if (vm == NULL) {
    fprintf(stderr, "Fatal Error: Could not allocate memory for VM\n");
    exit(1);
  }
  initVM(vm, argc, argv);
  return vm;
}

void resetStack(ObjectModuleRecord *moduleRecord) {
  moduleRecord->stackTop = moduleRecord->stack;
  moduleRecord->frameCount = 0;
  moduleRecord->openUpvalues = NULL;
}

void popTwo(ObjectModuleRecord *moduleRecord) {
  POP(moduleRecord);
  POP(moduleRecord);
}

void popPush(ObjectModuleRecord *moduleRecord, const Value value) {
  POP(moduleRecord);
  PUSH(moduleRecord, value);
}

bool call(ObjectModuleRecord *moduleRecord, ObjectClosure *closure,
          const int argCount) {
  if (argCount != closure->function->arity) {
    runtimePanic(moduleRecord, false, ARGUMENT_MISMATCH,
                 "Expected %d arguments, got %d", closure->function->arity,
                 argCount);
    return false;
  }

  if (moduleRecord->frameCount >= FRAMES_MAX) {
    runtimePanic(moduleRecord, false, STACK_OVERFLOW, "Stack overflow");
    return false;
  }

  CallFrame *frame = &moduleRecord->frames[moduleRecord->frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = moduleRecord->stackTop - argCount - 1;
  return true;
}

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool callValue(VM *vm, const Value callee, const int argCount) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  if (IS_CRUX_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
    case OBJECT_CLOSURE:
      return call(currentModuleRecord, AS_CRUX_CLOSURE(callee), argCount);
    case OBJECT_NATIVE_METHOD: {
      const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(currentModuleRecord, false, ARGUMENT_MISMATCH,
                     "Expected %d argument(s), got %d", native->arity,
                     argCount);
        return false;
      }

      ObjectResult *result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);

      currentModuleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(currentModuleRecord, false, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      PUSH(currentModuleRecord, OBJECT_VAL(result));

      return true;
    }
    case OBJECT_NATIVE_FUNCTION: {
      const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(currentModuleRecord, false, ARGUMENT_MISMATCH,
                     "Expected %d argument(s), got %d", native->arity,
                     argCount);
        return false;
      }

      ObjectResult *result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(currentModuleRecord, false, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      PUSH(currentModuleRecord, OBJECT_VAL(result));
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
      const ObjectNativeInfallibleFunction *native =
          AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(currentModuleRecord, false, ARGUMENT_MISMATCH,
                     "Expected %d argument(s), got %d", native->arity,
                     argCount);
        return false;
      }

      const Value result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;

      PUSH(currentModuleRecord, result);
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_METHOD: {
      const ObjectNativeInfallibleMethod *native =
          AS_CRUX_NATIVE_INFALLIBLE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(currentModuleRecord, false, ARGUMENT_MISMATCH,
                     "Expected %d argument(s), got %d", native->arity,
                     argCount);
        return false;
      }

      const Value result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;
      PUSH(currentModuleRecord, result);
      return true;
    }
    case OBJECT_CLASS: {
      ObjectClass *klass = AS_CRUX_CLASS(callee);
      currentModuleRecord->stackTop[-argCount - 1] =
          OBJECT_VAL(newInstance(vm, klass));
      Value initializer;

      if (tableGet(&klass->methods, vm->initString, &initializer)) {
        return call(currentModuleRecord, AS_CRUX_CLOSURE(initializer),
                    argCount);
      }
      if (argCount != 0) {
        runtimePanic(currentModuleRecord, false, ARGUMENT_MISMATCH,
                     "Expected 0 arguments but got %d arguments.", argCount);
        return false;
      }
      return true;
    }
    case OBJECT_BOUND_METHOD: {
      const ObjectBoundMethod *bound = AS_CRUX_BOUND_METHOD(callee);
      currentModuleRecord->stackTop[-argCount - 1] = bound->receiver;
      return call(currentModuleRecord, bound->method, argCount);
    }
    default:
      break;
    }
  }
  runtimePanic(currentModuleRecord, false, TYPE,
               "Can only call functions and classes.");
  return false;
}

/**
 * Invokes a method from a class with the given arguments.
 * @param moduleRecord The currently executing module
 * @param klass The class containing the method
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invokeFromClass(ObjectModuleRecord *moduleRecord, const ObjectClass *klass,
                     const ObjectString *name, const int argCount) {
  Value method;
  if (tableGet(&klass->methods, name, &method)) {
    return call(moduleRecord, AS_CRUX_CLOSURE(method), argCount);
  }
  runtimePanic(moduleRecord, false, NAME, "Undefined property '%s'.",
               name->chars);
  return false;
}

bool handleInvoke(VM *vm, const int argCount, const Value receiver,
                  const Value original, const Value value) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  // Save original stack order
  currentModuleRecord->stackTop[-argCount - 1] = value;
  currentModuleRecord->stackTop[-argCount] = receiver;

  if (!callValue(vm, value, argCount)) {
    return false;
  }

  // restore the caller and put the result in the right place
  const Value result = POP(currentModuleRecord);
  PUSH(currentModuleRecord, original);
  PUSH(currentModuleRecord, result);
  return true;
}

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invoke(VM *vm, const ObjectString *name, int argCount) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value receiver = PEEK(currentModuleRecord, argCount);
  const Value original =
      PEEK(currentModuleRecord, argCount + 1); // Store the original caller

  if (!IS_CRUX_OBJECT(receiver)) {
    runtimePanic(currentModuleRecord, false, TYPE,
                 "Only instances have methods");
    return false;
  }

  const Object *object = AS_CRUX_OBJECT(receiver);
  argCount++; // for the value that the method will act on
  switch (object->type) {
  case OBJECT_STRING: {
    Value value;
    if (tableGet(&vm->stringType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_ARRAY: {
    Value value;
    if (tableGet(&vm->arrayType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_FILE: {
    Value value;
    if (tableGet(&vm->fileType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_ERROR: {
    Value value;
    if (tableGet(&vm->errorType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_TABLE: {
    Value value;
    if (tableGet(&vm->tableType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_RANDOM: {
    Value value;
    if (tableGet(&vm->randomType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_RESULT: {
    Value value;
    if (tableGet(&vm->resultType, name, &value)) {
      return handleInvoke(vm, argCount, receiver, original, value);
    }
    runtimePanic(currentModuleRecord, false, NAME, "Undefined method '%s'.",
                 name->chars);
    return false;
  }
  case OBJECT_INSTANCE: {
    argCount--;
    const ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value)) {
      // Save original stack order
      currentModuleRecord->stackTop[-argCount - 1] = value;

      if (!callValue(vm, value, argCount)) {
        return false;
      }

      // After the call, restore the original caller and put the result in the
      // right place
      const Value result = POP(currentModuleRecord);
      PUSH(currentModuleRecord, original);
      PUSH(currentModuleRecord, result);
      return true;
    }

    // For class methods, we need special handling
    if (invokeFromClass(currentModuleRecord, instance->klass, name, argCount)) {
      // After the call, the result is already on the stack
      const Value result = POP(currentModuleRecord);
      PUSH(currentModuleRecord, original);
      PUSH(currentModuleRecord, result);
      return true;
    }
  }
  default: {
    runtimePanic(currentModuleRecord, false, TYPE,
                 "Only instances have methods");
    return false;
  }
  }
}

/**
 * Binds a method from a class to an instance.
 * @param vm The virtual machine
 * @param klass The class containing the method
 * @param name The name of the method to bind
 * @return true if the binding succeeds, false otherwise
 */
bool bindMethod(VM *vm, const ObjectClass *klass, const ObjectString *name) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimePanic(currentModuleRecord, false, NAME, "Undefined property '%s'",
                 name->chars);
    return false;
  }

  ObjectBoundMethod *bound =
      newBoundMethod(vm, PEEK(currentModuleRecord, 0), AS_CRUX_CLOSURE(method));
  POP(currentModuleRecord);
  PUSH(currentModuleRecord, OBJECT_VAL(bound));
  return true;
}

ObjectUpvalue *captureUpvalue(VM *vm, Value *local) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  ObjectUpvalue *prevUpvalue = NULL;
  ObjectUpvalue *upvalue = currentModuleRecord->openUpvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjectUpvalue *createdUpvalue = newUpvalue(vm, local);

  createdUpvalue->next = upvalue;
  if (prevUpvalue == NULL) {
    currentModuleRecord->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

/**
 * Closes all upvalues up to a certain stack position.
 * @param moduleRecord the currently executing module
 * @param last Pointer to the last variable to close
 */
void closeUpvalues(ObjectModuleRecord *moduleRecord, const Value *last) {
  while (moduleRecord->openUpvalues != NULL &&
         moduleRecord->openUpvalues->location >= last) {
    ObjectUpvalue *upvalue = moduleRecord->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    moduleRecord->openUpvalues = upvalue->next;
  }
}

/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
void defineMethod(VM *vm, ObjectString *name) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value method = PEEK(currentModuleRecord, 0);
  ObjectClass *klass = AS_CRUX_CLASS(PEEK(currentModuleRecord, 1));
  if (tableSet(vm, &klass->methods, name, method)) {
    POP(currentModuleRecord);
  }
}

/**
 * Determines if a value is falsy (nil or false).
 * @param value The value to check
 * @return true if the value is falsy, false otherwise
 */
bool isFalsy(const Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
         (IS_INT(value) && AS_INT(value) == 0) ||
         (IS_FLOAT(value) && AS_FLOAT(value) == 0.0);
}

/**
 * Concatenates two values as strings.
 * @param vm The virtual machine
 * @return true if concatenation succeeds, false otherwise
 */
bool concatenate(VM *vm) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value b = PEEK(currentModuleRecord, 0);
  const Value a = PEEK(currentModuleRecord, 1);

  ObjectString *stringB;
  ObjectString *stringA;

  if (IS_CRUX_STRING(b)) {
    stringB = AS_CRUX_STRING(b);
  } else {
    stringB = toString(vm, b);
    if (stringB == NULL) {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Could not convert right operand to a string.");
      return false;
    }
  }

  if (IS_CRUX_STRING(a)) {
    stringA = AS_CRUX_STRING(a);
  } else {
    stringA = toString(vm, a);
    if (stringA == NULL) {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Could not convert left operand to a string.");
      return false;
    }
  }

  const uint64_t length = stringA->length + stringB->length;
  char *chars = ALLOCATE(vm, char, length + 1);

  if (chars == NULL) {
    runtimePanic(currentModuleRecord, false, MEMORY,
                 "Could not allocate memory for concatenation.");
    return false;
  }

  memcpy(chars, stringA->chars, stringA->length);
  memcpy(chars + stringA->length, stringB->chars, stringB->length);
  chars[length] = '\0';

  ObjectString *result = takeString(vm, chars, length);

  popTwo(currentModuleRecord);
  PUSH(currentModuleRecord, OBJECT_VAL(result));
  return true;
}

void initVM(VM *vm, const int argc, const char **argv) {
  const bool isRepl = argc == 1 ? true : false;

  vm->objects = NULL;
  vm->bytesAllocated = 0;
  vm->nextGC = 1024 * 1024;
  vm->grayCount = 0;
  vm->grayCapacity = 0;
  vm->grayStack = NULL;

  vm->currentModuleRecord = newObjectModuleRecord(vm, NULL, isRepl, true);
  resetStack(vm->currentModuleRecord);

  initTable(&vm->currentModuleRecord->globals);
  initTable(&vm->currentModuleRecord->publics);

  vm->nativeModules = (NativeModules){
      .modules = ALLOCATE(vm, NativeModule, 8), .count = 0, .capacity = 8};

  vm->matchHandler.isMatchBind = false;
  vm->matchHandler.isMatchTarget = false;

  vm->matchHandler.matchBind = NIL_VAL;
  vm->matchHandler.matchTarget = NIL_VAL;

  initTable(&vm->moduleCache);
  initImportStack(vm);

  initTable(&vm->stringType);
  initTable(&vm->arrayType);
  initTable(&vm->tableType);
  initTable(&vm->errorType);
  initTable(&vm->randomType);
  initTable(&vm->fileType);
  initTable(&vm->resultType);
  initTable(&vm->strings);

  vm->initString = NULL;
  vm->initString = copyString(vm, "init", 4);

  vm->args.argc = argc;
  vm->args.argv = argv;

  ObjectString *path;
  if (argc > 1) {
    path = copyString(vm, argv[1], strlen(argv[1]));
  } else {
#ifdef _WIN32
    path = copyString(vm, ".\\", 2);
#else
    path = copyString(vm, "./", 2);
#endif
  }

  vm->currentModuleRecord->path = path;
  tableSet(vm, &vm->moduleCache, vm->currentModuleRecord->path,
           OBJECT_VAL(vm->currentModuleRecord));

  if (!initializeStdLib(vm)) {
    runtimePanic(vm->currentModuleRecord, true, RUNTIME,
                 "Failed to initialize standard library.");
  }
}

void freeVM(VM *vm) {
  freeTable(vm, &vm->strings);
  freeTable(vm, &vm->currentModuleRecord->globals);

  freeTable(vm, &vm->stringType);
  freeTable(vm, &vm->arrayType);
  freeTable(vm, &vm->tableType);
  freeTable(vm, &vm->errorType);
  freeTable(vm, &vm->randomType);
  freeTable(vm, &vm->fileType);
  freeTable(vm, &vm->resultType);

  for (int i = 0; i < vm->nativeModules.count; i++) {
    const NativeModule module = vm->nativeModules.modules[i];
    freeTable(vm, module.names);
    FREE(vm, char, module.name);
    FREE(vm, Table, module.names);
  }
  FREE_ARRAY(vm, NativeModule, vm->nativeModules.modules,
             vm->nativeModules.capacity);

  vm->initString = NULL;

  freeTable(vm, &vm->moduleCache);

  freeImportStack(vm);
  freeObjectModuleRecord(vm, vm->currentModuleRecord);

  freeObjects(vm);
  free(vm);
}

/**
 * Performs a binary operation on the top two values of the stack.
 * @param vm The virtual machine
 * @param operation The operation code to perform
 * @return true if the operation succeeds, false otherwise
 */
bool binaryOperation(VM *vm, const OpCode operation) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value b = PEEK(currentModuleRecord, 0);
  const Value a = PEEK(currentModuleRecord, 1);

  const bool aIsInt = IS_INT(a);
  const bool bIsInt = IS_INT(b);
  const bool aIsFloat = IS_FLOAT(a);
  const bool bIsFloat = IS_FLOAT(b);

  if (!((aIsInt || aIsFloat) && (bIsInt || bIsFloat))) {
    if (!(aIsInt || aIsFloat)) {
      runtimePanic(currentModuleRecord, false, TYPE,
                   typeErrorMessage(vm, a, "'int' or 'float'"));
    } else {
      runtimePanic(currentModuleRecord, false, TYPE,
                   typeErrorMessage(vm, b, "'int' or 'float'"));
    }
    return false;
  }

  if (aIsInt && bIsInt) {
    const int32_t intA = AS_INT(a);
    const int32_t intB = AS_INT(b);

    switch (operation) {
    case OP_ADD: {
      const int64_t result = (int64_t)intA + (int64_t)intB;
      popTwo(currentModuleRecord);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        PUSH(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        PUSH(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_SUBTRACT: {
      const int64_t result = (int64_t)intA - (int64_t)intB;
      popTwo(currentModuleRecord);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        PUSH(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        PUSH(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_MULTIPLY: {
      const int64_t result = (int64_t)intA * (int64_t)intB;
      popTwo(currentModuleRecord);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        PUSH(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        PUSH(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_DIVIDE: {
      if (intB == 0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Division by zero.");
        return false;
      }
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL((double)intA / (double)intB));
      break;
    }
    case OP_INT_DIVIDE: {
      if (intB == 0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Integer division by zero.");
        return false;
      }
      // Edge case: INT32_MIN / -1 overflows int32_t
      if (intA == INT32_MIN && intB == -1) {
        popTwo(currentModuleRecord);
        PUSH(currentModuleRecord, FLOAT_VAL(-(double)INT32_MIN)); // Promote
      } else {
        popTwo(currentModuleRecord);
        PUSH(currentModuleRecord, INT_VAL(intA / intB));
      }
      break;
    }
    case OP_MODULUS: {
      if (intB == 0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Modulo by zero.");
        return false;
      }

      if (intA == INT32_MIN && intB == -1) {
        popTwo(currentModuleRecord);
        PUSH(currentModuleRecord, INT_VAL(0));
      } else {
        popTwo(currentModuleRecord);
        PUSH(currentModuleRecord, INT_VAL(intA % intB));
      }
      break;
    }
    case OP_LEFT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(currentModuleRecord, false, RUNTIME,
                     "Invalid shift amount (%d) for <<.", intB);
        return false;
      }
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, INT_VAL(intA << intB));
      break;
    }
    case OP_RIGHT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(currentModuleRecord, false, RUNTIME,
                     "Invalid shift amount (%d) for >>.", intB);
        return false;
      }
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, INT_VAL(intA >> intB));
      break;
    }
    case OP_POWER: {
      // Promote int^int to float
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(pow(intA, intB)));
      break;
    }
    case OP_LESS:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(intA < intB));
      break;
    case OP_LESS_EQUAL:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(intA <= intB));
      break;
    case OP_GREATER:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(intA > intB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(intA >= intB));
      break;

    default:
      runtimePanic(currentModuleRecord, false, RUNTIME,
                   "Unknown binary operation %d for int, int.", operation);
      return false;
    }
  } else {
    const double doubleA = aIsFloat ? AS_FLOAT(a) : (double)AS_INT(a);
    const double doubleB = bIsFloat ? AS_FLOAT(b) : (double)AS_INT(b);

    switch (operation) {
    case OP_ADD:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(doubleA + doubleB));
      break;
    case OP_SUBTRACT:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(doubleA - doubleB));
      break;
    case OP_MULTIPLY:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(doubleA * doubleB));
      break;
    case OP_DIVIDE: {
      if (doubleB == 0.0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Division by zero.");
        return false;
      }
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(doubleA / doubleB));
      break;
    }
    case OP_POWER: {
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, FLOAT_VAL(pow(doubleA, doubleB)));
      break;
    }

    case OP_LESS:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(doubleA < doubleB));
      break;
    case OP_LESS_EQUAL:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(doubleA <= doubleB));
      break;
    case OP_GREATER:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(doubleA > doubleB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(currentModuleRecord);
      PUSH(currentModuleRecord, BOOL_VAL(doubleA >= doubleB));
      break;

    case OP_INT_DIVIDE:
    case OP_MODULUS:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT: {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Operands for integer operation must both be integers.");
      return false;
    }

    default:
      runtimePanic(currentModuleRecord, false, RUNTIME,
                   "Unknown binary operation %d for float/mixed.", operation);
      return false;
    }
  }
  return true;
}

InterpretResult globalCompoundOperation(VM *vm, ObjectString *name,
                                        const OpCode opcode, char *operation) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  Value currentValue;
  if (!tableGet(&currentModuleRecord->globals, name, &currentValue)) {
    runtimePanic(currentModuleRecord, false, NAME,
                 "Undefined variable '%s' for compound assignment.",
                 name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  const Value operandValue = PEEK(currentModuleRecord, 0);

  const bool currentIsInt = IS_INT(currentValue);
  const bool currentIsFloat = IS_FLOAT(currentValue);
  const bool operandIsInt = IS_INT(operandValue);
  const bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    if (!(currentIsInt || currentIsFloat)) {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Variable '%s' is not a number for '%s' operator.",
                   name->chars, operation);
    } else {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Right-hand operand for '%s' must be an 'int' or 'float'.",
                   operation);
    }
    return INTERPRET_RUNTIME_ERROR;
  }

  Value resultValue;

  if (currentIsInt && operandIsInt) {
    const int32_t icurrent = AS_INT(currentValue);
    const int32_t ioperand = AS_INT(operandValue);

    switch (opcode) {
    case OP_SET_GLOBAL_PLUS: {
      // +=
      const int64_t result = (int64_t)icurrent + (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
      break;
    }
    case OP_SET_GLOBAL_MINUS: {
      const int64_t result = (int64_t)icurrent - (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
      break;
    }
    case OP_SET_GLOBAL_STAR: {
      const int64_t result = (int64_t)icurrent * (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
      break;
    }
    case OP_SET_GLOBAL_SLASH: {
      if (ioperand == 0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Division by zero in '%s %s'.", name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      resultValue = FLOAT_VAL((double)icurrent / (double)ioperand);
      break;
    }

    case OP_SET_GLOBAL_INT_DIVIDE: {
      if (ioperand == 0) {
        runtimePanic(currentModuleRecord, false, RUNTIME,
                     "Division by zero in '%s %s'.", name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      if (icurrent == INT32_MIN && ioperand == -1) {
        resultValue = FLOAT_VAL(-(double)INT32_MIN); // Overflow
      } else {
        resultValue = INT_VAL(icurrent / ioperand);
      }
      break;
    }

    case OP_SET_GLOBAL_MODULUS: {
      if (ioperand == 0) {
        runtimePanic(currentModuleRecord, false, RUNTIME,
                     "Division by zero in '%s %s'.", name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      if (icurrent == INT32_MIN && ioperand == -1) {
        resultValue = INT_VAL(0);
      } else {
        resultValue = INT_VAL(icurrent % ioperand);
      }
      break;
    }

    default:
      runtimePanic(currentModuleRecord, false, RUNTIME,
                   "Unsupported compound assignment opcode %d for int/int.",
                   opcode);
      return INTERPRET_RUNTIME_ERROR;
    }
  } else {
    const double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    const double doperand =
        operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);

    switch (opcode) {
    case OP_SET_GLOBAL_PLUS: {
      resultValue = FLOAT_VAL(dcurrent + doperand);
      break;
    }
    case OP_SET_GLOBAL_MINUS: {
      resultValue = FLOAT_VAL(dcurrent - doperand);
      break;
    }
    case OP_SET_GLOBAL_STAR: {
      resultValue = FLOAT_VAL(dcurrent * doperand);
      break;
    }
    case OP_SET_GLOBAL_SLASH: {
      if (doperand == 0.0) {
        runtimePanic(currentModuleRecord, false, DIVISION_BY_ZERO,
                     "Division by zero in '%s %s'.", name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      resultValue = FLOAT_VAL(dcurrent / doperand);
      break;
    }

    case OP_SET_GLOBAL_INT_DIVIDE:
    case OP_SET_GLOBAL_MODULUS: {
      runtimePanic(currentModuleRecord, false, TYPE,
                   "Operands for integer compound assignment '%s' must both be "
                   "integers.",
                   operation);
      return INTERPRET_RUNTIME_ERROR;
    }
    default:
      runtimePanic(currentModuleRecord, false, RUNTIME,
                   "Unsupported compound assignment opcode %d for float/mixed.",
                   opcode);
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  tableSet(vm, &currentModuleRecord->globals, name, resultValue);
  return INTERPRET_OK;
}

bool checkPreviousInstruction(const CallFrame *frame, const int instructionsAgo,
                              const OpCode instruction) {
  const uint8_t *current = frame->ip;
  if (current - instructionsAgo < frame->closure->function->chunk.code) {
    return false;
  }
  return *(current - (instructionsAgo + 2)) ==
         instruction; // +2 to account for offset
}

InterpretResult interpret(VM *vm, char *source) {
  ObjectFunction *function = compile(vm, source);
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  if (function == NULL) {
    currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_COMPILE_ERROR;
  }

  PUSH(currentModuleRecord, OBJECT_VAL(function));
  ObjectClosure *closure = newClosure(vm, function);
  vm->currentModuleRecord->moduleClosure = closure;
  POP(currentModuleRecord);
  PUSH(currentModuleRecord, OBJECT_VAL(closure));
  call(currentModuleRecord, closure, 0);

  const InterpretResult result = run(vm, false);

  return result;
}

/**
 *
 * @param vm
 * @param closure The closure to be executed. The caller must ensure that the
 * arguments are on the stack correctly and match the arity
 * @param argCount
 * @param result result from executing function
 * @return
 */
ObjectResult *executeUserFunction(VM *vm, ObjectClosure *closure,
                                  const int argCount, InterpretResult *result) {

  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const uint32_t currentFrameCount = currentModuleRecord->frameCount;
  ObjectResult *errorResult =
      newErrorResult(vm, newError(vm, copyString(vm, "", 0), RUNTIME, true));

  if (!call(currentModuleRecord, closure, argCount)) {
    runtimePanic(currentModuleRecord, false, RUNTIME,
                 "Failed to execute function");
    *result = INTERPRET_RUNTIME_ERROR;
    return errorResult;
  }

  *result = run(vm, true);

  vm->currentModuleRecord->frameCount = currentFrameCount;
  if (*result == INTERPRET_OK) {
    const Value executionResult = PEEK(currentModuleRecord, 0);
    return newOkResult(vm, executionResult);
  }
  return errorResult;
}
