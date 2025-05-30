#include "vm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "file_handler.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "std/std.h"
#include "table.h"
#include "value.h"

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH() goto *dispatchTable[endIndex]
#else
#define DISPATCH()                                                             \
  instruction = READ_BYTE();                                                   \
  goto *dispatchTable[instruction]
#endif

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

void push(ObjectModuleRecord *moduleRecord, const Value value) {
  *moduleRecord->stackTop = value;
  moduleRecord->stackTop++;
}

Value pop(ObjectModuleRecord *moduleRecord) {
  moduleRecord->stackTop--;
  return *moduleRecord->stackTop;
}

static inline void popTwo(ObjectModuleRecord *moduleRecord) {
  pop(moduleRecord);
  pop(moduleRecord);
}

static inline void popPush(ObjectModuleRecord *moduleRecord,
                           const Value value) {
  pop(moduleRecord);
  push(moduleRecord, value);
}

/**
 * Returns a value from the stack without removing it.
 * @param moduleRecord The currently executing module
 * @param distance How far from the top of the stack to look (0 is the top)
 * @return The value at the specified distance from the top
 */
static inline Value peek(const ObjectModuleRecord *moduleRecord,
                         const int distance) {
  return moduleRecord->stackTop[-1 - distance];
}

/**
 * Calls a function closure with the given arguments.
 * @param vm The virtual machine
 * @param closure The function closure to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
static bool call(const VM *vm, ObjectClosure *closure, const int argCount) {
  if (argCount != closure->function->arity) {
    runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d arguments, got %d",
                 closure->function->arity, argCount);
    return false;
  }

  if (vm->currentModuleRecord->frameCount >= FRAMES_MAX) {
    runtimePanic(vm, STACK_OVERFLOW, "Stack overflow");
    return false;
  }

  ObjectModuleRecord *moduleRecord = vm->currentModuleRecord;

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
static bool callValue(VM *vm, const Value callee, const int argCount) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  if (IS_CRUX_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
    case OBJECT_CLOSURE:
      return call(vm, AS_CRUX_CLOSURE(callee), argCount);
    case OBJECT_NATIVE_METHOD: {
      const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      ObjectResult *result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);

      currentModuleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(vm, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      push(currentModuleRecord, OBJECT_VAL(result));

      return true;
    }
    case OBJECT_NATIVE_FUNCTION: {
      const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      ObjectResult *result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(vm, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      push(currentModuleRecord, OBJECT_VAL(result));
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
      const ObjectNativeInfallibleFunction *native =
          AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      const Value result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;

      push(currentModuleRecord, result);
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_METHOD: {
      const ObjectNativeInfallibleMethod *native =
          AS_CRUX_NATIVE_INFALLIBLE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      const Value result = native->function(
          vm, argCount, currentModuleRecord->stackTop - argCount);
      currentModuleRecord->stackTop -= argCount + 1;
      push(currentModuleRecord, result);
      return true;
    }
    case OBJECT_CLASS: {
      ObjectClass *klass = AS_CRUX_CLASS(callee);
      currentModuleRecord->stackTop[-argCount - 1] =
          OBJECT_VAL(newInstance(vm, klass));
      Value initializer;

      if (tableGet(&klass->methods, vm->initString, &initializer)) {
        return call(vm, AS_CRUX_CLOSURE(initializer), argCount);
      }
      if (argCount != 0) {
        runtimePanic(vm, ARGUMENT_MISMATCH,
                     "Expected 0 arguments but got %d arguments.", argCount);
        return false;
      }
      return true;
    }
    case OBJECT_BOUND_METHOD: {
      const ObjectBoundMethod *bound = AS_CRUX_BOUND_METHOD(callee);
      currentModuleRecord->stackTop[-argCount - 1] = bound->receiver;
      return call(vm, bound->method, argCount);
    }
    default:
      break;
    }
  }
  runtimePanic(vm, TYPE, "Can only call functions and classes.");
  return false;
}

/**
 * Invokes a method from a class with the given arguments.
 * @param vm The virtual machine
 * @param klass The class containing the method
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
static bool invokeFromClass(const VM *vm, const ObjectClass *klass,
                            ObjectString *name, const int argCount) {
  Value method;
  if (tableGet(&klass->methods, name, &method)) {
    return call(vm, AS_CRUX_CLOSURE(method), argCount);
  }
  runtimePanic(vm, NAME, "Undefined property '%s'.", name->chars);
  return false;
}

static bool handleInvoke(VM *vm, const int argCount, const Value receiver,
                         const Value original, const Value value) {
  // Save original stack order
  vm->currentModuleRecord->stackTop[-argCount - 1] = value;
  vm->currentModuleRecord->stackTop[-argCount] = receiver;

  if (!callValue(vm, value, argCount)) {
    return false;
  }

  // restore the caller and put the result in the right place
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value result = pop(currentModuleRecord);
  push(currentModuleRecord, original);
  push(currentModuleRecord, result);
  return true;
}

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
static bool invoke(VM *vm, ObjectString *name, int argCount) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value receiver = peek(currentModuleRecord, argCount);
  const Value original =
      peek(currentModuleRecord, argCount + 1); // Store the original caller

  if (!IS_CRUX_INSTANCE(receiver)) {
    argCount++; // for the value that the method will act upon
    if (IS_CRUX_STRING(receiver)) {
      Value value;
      if (tableGet(&vm->stringType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_ARRAY(receiver)) {
      Value value;
      if (tableGet(&vm->arrayType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_ERROR(receiver)) {
      Value value;
      if (tableGet(&vm->errorType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_TABLE(receiver)) {
      Value value;
      if (tableGet(&vm->tableType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_RANDOM(receiver)) {
      Value value;
      if (tableGet(&vm->randomType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_FILE(receiver)) {
      Value value;
      if (tableGet(&vm->fileType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    if (IS_CRUX_RESULT(receiver)) {
      Value value;
      if (tableGet(&vm->resultType, name, &value)) {
        return handleInvoke(vm, argCount, receiver, original, value);
      }
      runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
      return false;
    }

    runtimePanic(vm, TYPE, "Only instances have methods.");
    return false;
  }

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
    const Value result = pop(currentModuleRecord);
    push(currentModuleRecord, original);
    push(currentModuleRecord, result);
    return true;
  }

  // For class methods, we need special handling
  if (invokeFromClass(vm, instance->klass, name, argCount)) {
    // After the call, the result is already on the stack
    const Value result = pop(currentModuleRecord);
    push(currentModuleRecord, original);
    push(currentModuleRecord, result);
    return true;
  }

  return false;
}

/**
 * Binds a method from a class to an instance.
 * @param vm The virtual machine
 * @param klass The class containing the method
 * @param name The name of the method to bind
 * @return true if the binding succeeds, false otherwise
 */
static bool bindMethod(VM *vm, const ObjectClass *klass, ObjectString *name) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimePanic(vm, NAME, "Undefined property '%s'", name->chars);
    return false;
  }

  ObjectBoundMethod *bound =
      newBoundMethod(vm, peek(currentModuleRecord, 0), AS_CRUX_CLOSURE(method));
  pop(currentModuleRecord);
  push(currentModuleRecord, OBJECT_VAL(bound));
  return true;
}

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
static ObjectUpvalue *captureUpvalue(VM *vm, Value *local) {
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
static void closeUpvalues(ObjectModuleRecord *moduleRecord, const Value *last) {
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
static void defineMethod(VM *vm, ObjectString *name) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value method = peek(currentModuleRecord, 0);
  ObjectClass *klass = AS_CRUX_CLASS(peek(currentModuleRecord, 1));
  if (tableSet(vm, &klass->methods, name, method)) {
    pop(currentModuleRecord);
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
static bool concatenate(VM *vm) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value b = peek(currentModuleRecord, 0);
  const Value a = peek(currentModuleRecord, 1);

  ObjectString *stringB;
  ObjectString *stringA;

  if (IS_CRUX_STRING(b)) {
    stringB = AS_CRUX_STRING(b);
  } else {
    stringB = toString(vm, b);
    if (stringB == NULL) {
      runtimePanic(vm, TYPE, "Could not convert right operand to a string.");
      return false;
    }
  }

  if (IS_CRUX_STRING(a)) {
    stringA = AS_CRUX_STRING(a);
  } else {
    stringA = toString(vm, a);
    if (stringA == NULL) {
      runtimePanic(vm, TYPE, "Could not convert left operand to a string.");
      return false;
    }
  }

  const uint64_t length = stringA->length + stringB->length;
  char *chars = ALLOCATE(vm, char, length + 1);

  if (chars == NULL) {
    runtimePanic(vm, MEMORY, "Could not allocate memory for concatenation.");
    return false;
  }

  memcpy(chars, stringA->chars, stringA->length);
  memcpy(chars + stringA->length, stringB->chars, stringB->length);
  chars[length] = '\0';

  ObjectString *result = takeString(vm, chars, length);

  popTwo(currentModuleRecord);
  push(currentModuleRecord, OBJECT_VAL(result));
  return true;
}

void initVM(VM *vm, const int argc, const char **argv) {
  vm->objects = NULL;
  vm->bytesAllocated = 0;
  vm->nextGC = 1024 * 1024;
  vm->grayCount = 0;
  vm->grayCapacity = 0;
  vm->grayStack = NULL;

  vm->currentModuleRecord = newObjectModuleRecord(vm, NULL);
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

  if (!initializeStdLib(vm)) {
    runtimePanic(vm, RUNTIME, "Failed to initialize standard library.");
    exit(1);
  }

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
static bool binaryOperation(VM *vm, const OpCode operation) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const Value b = peek(currentModuleRecord, 0);
  const Value a = peek(currentModuleRecord, 1);

  const bool aIsInt = IS_INT(a);
  const bool bIsInt = IS_INT(b);
  const bool aIsFloat = IS_FLOAT(a);
  const bool bIsFloat = IS_FLOAT(b);

  if (!((aIsInt || aIsFloat) && (bIsInt || bIsFloat))) {
    if (!(aIsInt || aIsFloat)) {
      runtimePanic(vm, TYPE, typeErrorMessage(vm, a, "'int' or 'float'"));
    } else {
      runtimePanic(vm, TYPE, typeErrorMessage(vm, b, "'int' or 'float'"));
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
        push(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        push(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_SUBTRACT: {
      const int64_t result = (int64_t)intA - (int64_t)intB;
      popTwo(currentModuleRecord);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        push(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        push(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_MULTIPLY: {
      const int64_t result = (int64_t)intA * (int64_t)intB;
      popTwo(currentModuleRecord);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        push(currentModuleRecord, INT_VAL((int32_t)result));
      } else {
        push(currentModuleRecord,
             FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_DIVIDE: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero.");
        return false;
      }
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL((double)intA / (double)intB));
      break;
    }
    case OP_INT_DIVIDE: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Integer division by zero.");
        return false;
      }
      // Edge case: INT32_MIN / -1 overflows int32_t
      if (intA == INT32_MIN && intB == -1) {
        popTwo(currentModuleRecord);
        push(currentModuleRecord, FLOAT_VAL(-(double)INT32_MIN)); // Promote
      } else {
        popTwo(currentModuleRecord);
        push(currentModuleRecord, INT_VAL(intA / intB));
      }
      break;
    }
    case OP_MODULUS: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Modulo by zero.");
        return false;
      }

      if (intA == INT32_MIN && intB == -1) {
        popTwo(currentModuleRecord);
        push(currentModuleRecord, INT_VAL(0));
      } else {
        popTwo(currentModuleRecord);
        push(currentModuleRecord, INT_VAL(intA % intB));
      }
      break;
    }
    case OP_LEFT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(vm, RUNTIME, "Invalid shift amount (%d) for <<.", intB);
        return false;
      }
      popTwo(currentModuleRecord);
      push(currentModuleRecord, INT_VAL(intA << intB));
      break;
    }
    case OP_RIGHT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(vm, RUNTIME, "Invalid shift amount (%d) for >>.", intB);
        return false;
      }
      popTwo(currentModuleRecord);
      push(currentModuleRecord, INT_VAL(intA >> intB));
      break;
    }
    case OP_POWER: {
      // Promote int^int to float
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(pow((double)intA, (double)intB)));
      break;
    }
    case OP_LESS:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(intA < intB));
      break;
    case OP_LESS_EQUAL:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(intA <= intB));
      break;
    case OP_GREATER:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(intA > intB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(intA >= intB));
      break;

    default:
      runtimePanic(vm, RUNTIME, "Unknown binary operation %d for int, int.",
                   operation);
      return false;
    }
  } else {
    const double doubleA = aIsFloat ? AS_FLOAT(a) : (double)AS_INT(a);
    const double doubleB = bIsFloat ? AS_FLOAT(b) : (double)AS_INT(b);

    switch (operation) {
    case OP_ADD:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(doubleA + doubleB));
      break;
    case OP_SUBTRACT:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(doubleA - doubleB));
      break;
    case OP_MULTIPLY:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(doubleA * doubleB));
      break;
    case OP_DIVIDE: {
      if (doubleB == 0.0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero.");
        return false;
      }
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(doubleA / doubleB));
      break;
    }
    case OP_POWER: {
      popTwo(currentModuleRecord);
      push(currentModuleRecord, FLOAT_VAL(pow(doubleA, doubleB)));
      break;
    }

    case OP_LESS:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(doubleA < doubleB));
      break;
    case OP_LESS_EQUAL:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(doubleA <= doubleB));
      break;
    case OP_GREATER:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(doubleA > doubleB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(currentModuleRecord);
      push(currentModuleRecord, BOOL_VAL(doubleA >= doubleB));
      break;

    case OP_INT_DIVIDE:
    case OP_MODULUS:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT: {
      runtimePanic(vm, TYPE,
                   "Operands for integer operation must both be integers.");
      return false;
    }

    default:
      runtimePanic(vm, RUNTIME, "Unknown binary operation %d for float/mixed.",
                   operation);
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
    runtimePanic(vm, NAME, "Undefined variable '%s' for compound assignment.",
                 name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  const Value operandValue = peek(currentModuleRecord, 0);

  const bool currentIsInt = IS_INT(currentValue);
  const bool currentIsFloat = IS_FLOAT(currentValue);
  const bool operandIsInt = IS_INT(operandValue);
  const bool operandIsFloat = IS_FLOAT(operandValue);

  if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
    if (!(currentIsInt || currentIsFloat)) {
      runtimePanic(vm, TYPE, "Variable '%s' is not a number for '%s' operator.",
                   name->chars, operation);
    } else {
      runtimePanic(vm, TYPE,
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
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero in '%s %s'.",
                     name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      resultValue = FLOAT_VAL((double)icurrent / (double)ioperand);
      break;
    }

    case OP_SET_GLOBAL_INT_DIVIDE: {
      if (ioperand == 0) {
        runtimePanic(vm, RUNTIME, "Division by zero in '%s %s'.", name->chars,
                     operation);
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
        runtimePanic(vm, RUNTIME, "Division by zero in '%s %s'.", name->chars,
                     operation);
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
      runtimePanic(vm, RUNTIME,
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
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero in '%s %s'.",
                     name->chars, operation);
        return INTERPRET_RUNTIME_ERROR;
      }
      resultValue = FLOAT_VAL(dcurrent / doperand);
      break;
    }

    case OP_SET_GLOBAL_INT_DIVIDE:
    case OP_SET_GLOBAL_MODULUS: {
      runtimePanic(vm, TYPE,
                   "Operands for integer compound assignment '%s' must both be "
                   "integers.",
                   operation);
      return INTERPRET_RUNTIME_ERROR;
    }
    default:
      runtimePanic(vm, RUNTIME,
                   "Unsupported compound assignment opcode %d for float/mixed.",
                   opcode);
      return INTERPRET_RUNTIME_ERROR;
    }
  }

  if (!tableSet(vm, &currentModuleRecord->globals, name, resultValue)) {
    runtimePanic(
        vm, RUNTIME,
        "Failed to set global variable '%s' after compound assignment.",
        name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  return INTERPRET_OK;
}

/**
 * Checks if a previous instruction matches the expected opcode.
 * @param frame The current call frame
 * @param instructionsAgo How many instructions to look back
 * @param instruction The opcode to check for
 * @return true if the previous instruction matches, false otherwise
 */
static bool checkPreviousInstruction(const CallFrame *frame,
                                     const int instructionsAgo,
                                     const OpCode instruction) {
  const uint8_t *current = frame->ip;
  if (current - instructionsAgo < frame->closure->function->chunk.code) {
    return false;
  }
  return *(current - (instructionsAgo + 2)) ==
         instruction; // +2 to account for offset
}

/**
 * Executes bytecode in the virtual machine.
 * @param vm The virtual machine
 * @param isAnonymousFrame Is this frame anonymous? (should this frame return
 * from run at OP_RETURN)
 * @return The interpretation result
 */
static InterpretResult run(VM *vm, const bool isAnonymousFrame) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  CallFrame *frame =
      &currentModuleRecord->frames[currentModuleRecord->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_CRUX_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

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

  if (tableSet(vm, &instance->fields, name, peek(currentModuleRecord, 0))) {
    Value value = pop(currentModuleRecord);
    popPush(currentModuleRecord, value);
    DISPATCH();
  }
  runtimePanic(vm, NAME, "Cannot set undefined property '%s'.", name->chars);
  return INTERPRET_RUNTIME_ERROR;
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
      runtimePanic(vm, IMPORT, "Failed to import '%s'.", name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
  }
  vm->importCount--;
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
}

InterpretResult interpret(VM *vm, char *source) {
  ObjectFunction *function = compile(vm, source);
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  if (function == NULL) {
    currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_COMPILE_ERROR;
  }

  push(currentModuleRecord, OBJECT_VAL(function));
  ObjectClosure *closure = newClosure(vm, function);
  vm->currentModuleRecord->moduleClosure = closure;
  pop(currentModuleRecord);
  push(currentModuleRecord, OBJECT_VAL(closure));
  call(vm, closure, 0);

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

  const ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  const uint32_t currentFrameCount = currentModuleRecord->frameCount;
  ObjectResult *errorResult =
      newErrorResult(vm, newError(vm, copyString(vm, "", 0), RUNTIME, true));

  if (!call(vm, closure, argCount)) {
    runtimePanic(vm, RUNTIME, "Failed to execute function");
    *result = INTERPRET_RUNTIME_ERROR;
    return errorResult;
  }

  *result = run(vm, true);

  vm->currentModuleRecord->frameCount = currentFrameCount;
  if (*result == INTERPRET_OK) {
    const Value executionResult = peek(currentModuleRecord, 0);
    return newOkResult(vm, executionResult);
  }
  return errorResult;
}
