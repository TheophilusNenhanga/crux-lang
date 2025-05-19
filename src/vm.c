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
    uint32_t oldCapacity = stack->capacity;
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

static bool stringEquals(ObjectString *a, ObjectString *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->chars, b->chars, a->length) == 0;
}

bool isInImportStack(VM *vm, ObjectString *path) {
  ImportStack *stack = &vm->importStack;
  for (int i = 0; i < stack->count; i++) {
    if (stack->paths[i] == path || stringEquals(stack->paths[i], path)) {
      return true;
    }
  }
  return false;
}

VM *newVM(int argc, const char **argv) {
  VM *vm = malloc(sizeof(VM));
  if (vm == NULL) {
    fprintf(stderr, "Fatal Error: Could not allocate memory for VM\n");
    exit(1);
  }
  initVM(vm, argc, argv);
  return vm;
}

void resetStack(ObjectModuleRecord* moduleRecord) {
  moduleRecord->stackTop = moduleRecord->stack;
  moduleRecord->frameCount = 0;
  moduleRecord->openUpvalues = NULL;
}

void push(VM *vm, Value value) {
  *vm->currentModuleRecord->stackTop = value;
  vm->currentModuleRecord->stackTop++;
}

Value pop(VM *vm) {
  vm->currentModuleRecord->stackTop--;
  return *vm->currentModuleRecord->stackTop;
}

static inline void popTwo(VM *vm) {
  pop(vm);
  pop(vm);
}

static inline void popPush(VM *vm, Value value) {
  pop(vm);
  push(vm, value);
}

/**
 * Returns a value from the stack without removing it.
 * @param vm The virtual machine
 * @param distance How far from the top of the stack to look (0 is the top)
 * @return The value at the specified distance from the top
 */
static inline Value peek(VM *vm, int distance) {
  return vm->currentModuleRecord->stackTop[-1 - distance];
}

/**
 * Calls a function closure with the given arguments.
 * @param vm The virtual machine
 * @param closure The function closure to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
static bool call(VM *vm, ObjectClosure *closure, int argCount) {
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
static bool callValue(VM *vm, Value callee, int argCount) {
  ObjectModuleRecord *moduleRecord = vm->currentModuleRecord;
  if (IS_CRUX_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
    case OBJECT_CLOSURE:
      return call(vm, AS_CRUX_CLOSURE(callee), argCount);
    case OBJECT_NATIVE_METHOD: {
      ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      ObjectResult *result =
          native->function(vm, argCount, moduleRecord->stackTop - argCount);

      moduleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(vm, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      push(vm, OBJECT_VAL(result));

      return true;
    }
    case OBJECT_NATIVE_FUNCTION: {
      ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      ObjectResult *result =
          native->function(vm, argCount, moduleRecord->stackTop - argCount);
      moduleRecord->stackTop -= argCount + 1;

      if (!result->isOk) {
        if (result->as.error->isPanic) {
          runtimePanic(vm, result->as.error->type,
                       result->as.error->message->chars);
          return false;
        }
      }

      push(vm, OBJECT_VAL(result));
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
      ObjectNativeInfallibleFunction *native =
          AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      Value result = native->function(vm, argCount, moduleRecord->stackTop - argCount);
      moduleRecord->stackTop -= argCount + 1;

      push(vm, result);
      return true;
    }
    case OBJECT_NATIVE_INFALLIBLE_METHOD: {
      ObjectNativeInfallibleMethod *native =
          AS_CRUX_NATIVE_INFALLIBLE_METHOD(callee);
      if (argCount != native->arity) {
        runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d",
                     native->arity, argCount);
        return false;
      }

      Value result = native->function(vm, argCount, moduleRecord->stackTop - argCount);
      moduleRecord->stackTop -= argCount + 1;
      push(vm, result);
      return true;
    }
    case OBJECT_CLASS: {
      ObjectClass *klass = AS_CRUX_CLASS(callee);
      moduleRecord->stackTop[-argCount - 1] = OBJECT_VAL(newInstance(vm, klass));
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
      ObjectBoundMethod *bound = AS_CRUX_BOUND_METHOD(callee);
      moduleRecord->stackTop[-argCount - 1] = bound->receiver;
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
static bool invokeFromClass(VM *vm, ObjectClass *klass, ObjectString *name,
                            int argCount) {
  Value method;
  if (tableGet(&klass->methods, name, &method)) {
    return call(vm, AS_CRUX_CLOSURE(method), argCount);
  }
  runtimePanic(vm, NAME, "Undefined property '%s'.", name->chars);
  return false;
}

static bool handleInvoke(VM *vm, int argCount, Value receiver, Value original,
                         Value value) {
  // Save original stack order
  vm->currentModuleRecord->stackTop[-argCount - 1] = value;
  vm->currentModuleRecord->stackTop[-argCount] = receiver;

  if (!callValue(vm, value, argCount)) {
    return false;
  }

  // restore the caller and put the result in the right place
  Value result = pop(vm);
  push(vm, original);
  push(vm, result);
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
  Value receiver = peek(vm, argCount);
  Value original = peek(vm, argCount + 1); // Store the original caller

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

    runtimePanic(vm, TYPE, "Only instances have methods.");
    return false;
  }

  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    // Save original stack order
    vm->currentModuleRecord->stackTop[-argCount - 1] = value;

    if (!callValue(vm, value, argCount)) {
      return false;
    }

    // After the call, restore the original caller and put the result in the
    // right place
    Value result = pop(vm);
    push(vm, original);
    push(vm, result);
    return true;
  }

  // For class methods, we need special handling
  if (invokeFromClass(vm, instance->klass, name, argCount)) {
    // After the call, the result is already on the stack
    Value result = pop(vm);
    push(vm, original);
    push(vm, result);
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
static bool bindMethod(VM *vm, ObjectClass *klass, ObjectString *name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimePanic(vm, NAME, "Undefined property '%s'", name->chars);
    return false;
  }

  ObjectBoundMethod *bound =
      newBoundMethod(vm, peek(vm, 0), AS_CRUX_CLOSURE(method));
  pop(vm);
  push(vm, OBJECT_VAL(bound));
  return true;
}

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
static ObjectUpvalue *captureUpvalue(VM *vm, Value *local) {
  ObjectUpvalue *prevUpvalue = NULL;
  ObjectUpvalue *upvalue = vm->currentModuleRecord->openUpvalues;

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
    vm->currentModuleRecord->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

/**
 * Closes all upvalues up to a certain stack position.
 * @param vm The virtual machine
 * @param last Pointer to the last variable to close
 */
static void closeUpvalues(VM *vm, Value *last) {
  while (vm->currentModuleRecord->openUpvalues != NULL && vm->currentModuleRecord->openUpvalues->location >= last) {
    ObjectUpvalue *upvalue = vm->currentModuleRecord->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->currentModuleRecord->openUpvalues = upvalue->next;
  }
}

/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
static void defineMethod(VM *vm, ObjectString *name) {
  Value method = peek(vm, 0);
  ObjectClass *klass = AS_CRUX_CLASS(peek(vm, 1));
  if (tableSet(vm, &klass->methods, name, method)) {
    pop(vm);
  }
}

/**
 * Determines if a value is falsy (nil or false).
 * @param value The value to check
 * @return true if the value is falsy, false otherwise
 */
static inline bool isFalsy(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/**
 * Concatenates two values as strings.
 * @param vm The virtual machine
 * @return true if concatenation succeeds, false otherwise
 */
static bool concatenate(VM *vm) {
  Value b = peek(vm, 0);
  Value a = peek(vm, 1);

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

  uint64_t length = stringA->length + stringB->length;
  char *chars = ALLOCATE(vm, char, length + 1);

  if (chars == NULL) {
    runtimePanic(vm, MEMORY, "Could not allocate memory for concatenation.");
    return false;
  }

  memcpy(chars, stringA->chars, stringA->length);
  memcpy(chars + stringA->length, stringB->chars, stringB->length);
  chars[length] = '\0';

  ObjectString *result = takeString(vm, chars, length);

  popTwo(vm);
  push(vm, OBJECT_VAL(result));
  return true;
}

void initVM(VM *vm, int argc, const char **argv) {
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

  for (int i = 0; i < vm->nativeModules.count; i++) {
    NativeModule module = vm->nativeModules.modules[i];
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
static bool binaryOperation(VM *vm, OpCode operation) {
  Value b = peek(vm, 0);
  Value a = peek(vm, 1);

  bool aIsInt = IS_INT(a);
  bool bIsInt = IS_INT(b);
  bool aIsFloat = IS_FLOAT(a);
  bool bIsFloat = IS_FLOAT(b);

  if (!((aIsInt || aIsFloat) && (bIsInt || bIsFloat))) {
    if (!(aIsInt || aIsFloat)) {
      runtimePanic(vm, TYPE, typeErrorMessage(vm, a, "'int' or 'float'"));
    } else {
      runtimePanic(vm, TYPE, typeErrorMessage(vm, b, "'int' or 'float'"));
    }
    return false;
  }

  if (aIsInt && bIsInt) {
    int32_t intA = AS_INT(a);
    int32_t intB = AS_INT(b);

    switch (operation) {
    case OP_ADD: {
      int64_t result = (int64_t)intA + (int64_t)intB;
      popTwo(vm);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        push(vm, INT_VAL((int32_t)result));
      } else {
        push(vm, FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_SUBTRACT: {
      int64_t result = (int64_t)intA - (int64_t)intB;
      popTwo(vm);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        push(vm, INT_VAL((int32_t)result));
      } else {
        push(vm, FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_MULTIPLY: {
      int64_t result = (int64_t)intA * (int64_t)intB;
      popTwo(vm);
      if (result >= INT32_MIN && result <= INT32_MAX) {
        push(vm, INT_VAL((int32_t)result));
      } else {
        push(vm, FLOAT_VAL((double)result)); // Promote on overflow
      }
      break;
    }
    case OP_DIVIDE: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero.");
        return false;
      }
      popTwo(vm);
      push(vm, FLOAT_VAL((double)intA / (double)intB));
      break;
    }
    case OP_INT_DIVIDE: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Integer division by zero.");
        return false;
      }
      // Edge case: INT32_MIN / -1 overflows int32_t
      if (intA == INT32_MIN && intB == -1) {
        popTwo(vm);
        push(vm, FLOAT_VAL(-(double)INT32_MIN)); // Promote
      } else {
        popTwo(vm);
        push(vm, INT_VAL(intA / intB));
      }
      break;
    }
    case OP_MODULUS: {
      if (intB == 0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Modulo by zero.");
        return false;
      }

      if (intA == INT32_MIN && intB == -1) {
        popTwo(vm);
        push(vm, INT_VAL(0));
      } else {
        popTwo(vm);
        push(vm, INT_VAL(intA % intB));
      }
      break;
    }
    case OP_LEFT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(vm, RUNTIME, "Invalid shift amount (%d) for <<.", intB);
        return false;
      }
      popTwo(vm);
      push(vm, INT_VAL(intA << intB));
      break;
    }
    case OP_RIGHT_SHIFT: {
      if (intB < 0 || intB >= 32) {
        runtimePanic(vm, RUNTIME, "Invalid shift amount (%d) for >>.", intB);
        return false;
      }
      popTwo(vm);
      push(vm, INT_VAL(intA >> intB));
      break;
    }
    case OP_POWER: {
      // Promote int^int to float
      popTwo(vm);
      push(vm, FLOAT_VAL(pow((double)intA, (double)intB)));
      break;
    }
    case OP_LESS:
      popTwo(vm);
      push(vm, BOOL_VAL(intA < intB));
      break;
    case OP_LESS_EQUAL:
      popTwo(vm);
      push(vm, BOOL_VAL(intA <= intB));
      break;
    case OP_GREATER:
      popTwo(vm);
      push(vm, BOOL_VAL(intA > intB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(vm);
      push(vm, BOOL_VAL(intA >= intB));
      break;

    default:
      runtimePanic(vm, RUNTIME, "Unknown binary operation %d for int, int.",
                   operation);
      return false;
    }
  } else {
    double doubleA = aIsFloat ? AS_FLOAT(a) : (double)AS_INT(a);
    double doubleB = bIsFloat ? AS_FLOAT(b) : (double)AS_INT(b);

    switch (operation) {
    case OP_ADD:
      popTwo(vm);
      push(vm, FLOAT_VAL(doubleA + doubleB));
      break;
    case OP_SUBTRACT:
      popTwo(vm);
      push(vm, FLOAT_VAL(doubleA - doubleB));
      break;
    case OP_MULTIPLY:
      popTwo(vm);
      push(vm, FLOAT_VAL(doubleA * doubleB));
      break;
    case OP_DIVIDE: {
      if (doubleB == 0.0) {
        runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero.");
        return false;
      }
      popTwo(vm);
      push(vm, FLOAT_VAL(doubleA / doubleB));
      break;
    }
    case OP_POWER: {
      popTwo(vm);
      push(vm, FLOAT_VAL(pow(doubleA, doubleB)));
      break;
    }

    case OP_LESS:
      popTwo(vm);
      push(vm, BOOL_VAL(doubleA < doubleB));
      break;
    case OP_LESS_EQUAL:
      popTwo(vm);
      push(vm, BOOL_VAL(doubleA <= doubleB));
      break;
    case OP_GREATER:
      popTwo(vm);
      push(vm, BOOL_VAL(doubleA > doubleB));
      break;
    case OP_GREATER_EQUAL:
      popTwo(vm);
      push(vm, BOOL_VAL(doubleA >= doubleB));
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
                                        OpCode opcode, char *operation) {
  Value currentValue;
  if (!tableGet(&vm->currentModuleRecord->globals, name, &currentValue)) {
    runtimePanic(vm, NAME, "Undefined variable '%s' for compound assignment.",
                 name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  Value operandValue = peek(vm, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

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
    int32_t icurrent = AS_INT(currentValue);
    int32_t ioperand = AS_INT(operandValue);

    switch (opcode) {
    case OP_SET_GLOBAL_PLUS: {
      // +=
      int64_t result = (int64_t)icurrent + (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
      break;
    }
    case OP_SET_GLOBAL_MINUS: {
      int64_t result = (int64_t)icurrent - (int64_t)ioperand;
      if (result >= INT32_MIN && result <= INT32_MAX) {
        resultValue = INT_VAL((int32_t)result);
      } else {
        resultValue = FLOAT_VAL((double)result);
      }
      break;
    }
    case OP_SET_GLOBAL_STAR: {
      int64_t result = (int64_t)icurrent * (int64_t)ioperand;
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
    double dcurrent =
        currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
    double doperand =
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

  if (!tableSet(vm, &vm->currentModuleRecord->globals, name, resultValue)) {
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
static bool checkPreviousInstruction(CallFrame *frame, int instructionsAgo,
                                     OpCode instruction) {
  uint8_t *current = frame->ip;
  if (current - instructionsAgo < frame->closure->function->chunk.code) {
    return false;
  }
  return *(current - (instructionsAgo + 2)) ==
         instruction; // +2 to account for offset
}

/**
 * Executes bytecode in the virtual machine.
 * @param vm The virtual machine
 * @return The interpretation result
 */
static InterpretResult run(VM *vm) {
  ObjectModuleRecord *moduleRecord = vm->currentModuleRecord;
  CallFrame* frame = &moduleRecord->frames[moduleRecord->frameCount - 1];

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
  Value result = pop(vm);
  closeUpvalues(vm, frame->slots);
  moduleRecord->frameCount--;
  if (moduleRecord->frameCount == 0) {
    pop(vm);
    return INTERPRET_OK;
  }
  moduleRecord->stackTop = frame->slots;
  push(vm, result);
  frame = &moduleRecord->frames[moduleRecord->frameCount - 1];
  DISPATCH();
}

OP_CONSTANT: {
  Value constant = READ_CONSTANT();
  push(vm, constant);
  DISPATCH();
}

OP_NIL: {
  push(vm, NIL_VAL);
  DISPATCH();
}

OP_TRUE: {
  push(vm, BOOL_VAL(true));
  DISPATCH();
}

OP_FALSE: {
  push(vm, BOOL_VAL(false));
  DISPATCH();
}

OP_NEGATE: {
  Value operand = peek(vm, 0);
  if (IS_INT(operand)) {
    int32_t iVal = AS_INT(operand);
    if (iVal == INT32_MIN) {
      popPush(vm, FLOAT_VAL(-(double)INT32_MIN));
    } else {
      popPush(vm, INT_VAL(-iVal));
    }
  } else if (IS_FLOAT(operand)) {
    popPush(vm, FLOAT_VAL(-AS_FLOAT(operand)));
  } else {
    pop(vm);
    runtimePanic(vm, TYPE, typeErrorMessage(vm, operand, "int' | 'float"));
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_EQUAL: {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesEqual(a, b)));
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
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(!valuesEqual(a, b)));
  DISPATCH();
}

OP_ADD: {
  if (IS_CRUX_STRING(peek(vm, 0)) || IS_CRUX_STRING(peek(vm, 1))) {
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
  push(vm, BOOL_VAL(isFalsy(pop(vm))));
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
  pop(vm);
  DISPATCH();
}

OP_DEFINE_GLOBAL: {
  ObjectString *name = READ_STRING();
  bool isPublic = false;
  if (checkPreviousInstruction(frame, 3, OP_PUB)) {
    isPublic = true;
  }
  if (tableSet(vm, &moduleRecord->globals, name, peek(vm, 0))) {
    if (isPublic) {
      tableSet(vm, &moduleRecord->publics, name, peek(vm, 0));
    }
    pop(vm);
DISPATCH();
  }
  runtimePanic(vm, NAME, "Cannot define '%s' because it is already defined.",
               name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL: {
  ObjectString *name = READ_STRING();
  Value value;
  if (tableGet(&vm->currentModuleRecord->globals, name, &value)) {
    push(vm, value);
DISPATCH();
  }
  runtimePanic(vm, NAME, "Undefined variable '%s'.", name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL: {
  ObjectString *name = READ_STRING();
  if (tableSet(vm, &vm->currentModuleRecord->globals, name, peek(vm, 0))) {
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
  push(vm, frame->slots[slot]);
  DISPATCH();
}

OP_SET_LOCAL: {
  uint8_t slot = READ_BYTE();
  frame->slots[slot] = peek(vm, 0);
  DISPATCH();
}

OP_JUMP_IF_FALSE: {
  uint16_t offset = READ_SHORT();
  if (isFalsy(peek(vm, 0))) {
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
  if (!callValue(vm, peek(vm, argCount), argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &moduleRecord->frames[moduleRecord->frameCount - 1];
  DISPATCH();
}

OP_CLOSURE: {
  ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
  ObjectClosure *closure = newClosure(vm, function);
  push(vm, OBJECT_VAL(closure));

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
  push(vm, *frame->closure->upvalues[slot]->location);
  DISPATCH();
}

OP_SET_UPVALUE: {
  uint8_t slot = READ_BYTE();
  *frame->closure->upvalues[slot]->location = peek(vm, 0);
  DISPATCH();
}

OP_CLOSE_UPVALUE: {
  closeUpvalues(vm, moduleRecord->stackTop - 1);
  pop(vm);
  DISPATCH();
}

OP_CLASS: {
  push(vm, OBJECT_VAL(newClass(vm, READ_STRING())));
  DISPATCH();
}

OP_GET_PROPERTY: {
  Value receiver = peek(vm, 0);
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
    pop(vm);
    push(vm, value);
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
  Value receiver = peek(vm, 1);
  if (!IS_CRUX_INSTANCE(receiver)) {
    ObjectString *name = READ_STRING();
    runtimePanic(vm, TYPE, "Cannot set property '%s' on non-instance value. %s",
                 name->chars, typeErrorMessage(vm, receiver, "instance"));
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectInstance *instance = AS_CRUX_INSTANCE(receiver);
  ObjectString *name = READ_STRING();

  if (tableSet(vm, &instance->fields, name, peek(vm, 0))) {
    Value value = pop(vm);
    popPush(vm, value);
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
  frame = &moduleRecord->frames[moduleRecord->frameCount - 1];
  DISPATCH();
}

OP_METHOD: {
  defineMethod(vm, READ_STRING());
  DISPATCH();
}

OP_INHERIT: {
  Value superClass = peek(vm, 1);

  if (!IS_CRUX_CLASS(superClass)) {
    runtimePanic(vm, TYPE, "Cannot inherit from non class object.");
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectClass *subClass = AS_CRUX_CLASS(peek(vm, 0));
  tableAddAll(vm, &AS_CRUX_CLASS(superClass)->methods, &subClass->methods);
  pop(vm);
  DISPATCH();
}

OP_GET_SUPER: {
  ObjectString *name = READ_STRING();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(vm));

  if (!bindMethod(vm, superClass, name)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

OP_SUPER_INVOKE: {
  ObjectString *method = READ_STRING();
  int argCount = READ_BYTE();
  ObjectClass *superClass = AS_CRUX_CLASS(pop(vm));
  if (!invokeFromClass(vm, superClass, method, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &moduleRecord->frames[moduleRecord->frameCount - 1];
  DISPATCH();
}

OP_ARRAY: {
  uint16_t elementCount = READ_SHORT();
  ObjectArray *array = newArray(vm, elementCount);
  for (int i = elementCount - 1; i >= 0; i--) {
    arrayAdd(vm, array, pop(vm), i);
  }
  push(vm, OBJECT_VAL(array));
  DISPATCH();
}

OP_GET_COLLECTION: {
  Value indexValue = pop(vm);
  if (!IS_CRUX_OBJECT(peek(vm, 0))) {
    runtimePanic(vm, TYPE, "Cannot get from a non-collection type.");
    return INTERPRET_RUNTIME_ERROR;
  }
  switch (AS_CRUX_OBJECT(peek(vm, 0))->type) {
  case OBJECT_TABLE: {
    if (IS_CRUX_STRING(indexValue) || IS_INT(indexValue)) {
      ObjectTable *table = AS_CRUX_TABLE(peek(vm, 0));
      Value value;
      if (!objectTableGet(table, indexValue, &value)) {
        runtimePanic(vm, COLLECTION_GET, "Failed to get value from table");
        return INTERPRET_RUNTIME_ERROR;
      }
      popPush(vm, value);
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
    ObjectArray *array = AS_CRUX_ARRAY(peek(vm, 0));
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
        vm,
        value); // pop the array off the stack // push the value onto the stack
DISPATCH();
  }
  case OBJECT_STRING: {
    if (!IS_INT(indexValue)) {
      runtimePanic(vm, TYPE, "Index must be of type 'int'.");
      return INTERPRET_RUNTIME_ERROR;
    }
    int index = AS_INT(indexValue);
    ObjectString *string = AS_CRUX_STRING(peek(vm, 0));
    ObjectString *ch;
    if (index < 0 || index >= string->length) {
      runtimePanic(vm, INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
      return INTERPRET_RUNTIME_ERROR;
    }
    // Only single character indexing
    ch = copyString(vm, string->chars + index, 1);
    popPush(vm, OBJECT_VAL(ch));
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
  Value value = pop(vm);
  Value indexValue = peek(vm, 0);

  if (IS_CRUX_TABLE(peek(vm, 1))) {
    ObjectTable *table = AS_CRUX_TABLE(peek(vm, 1));
    if (IS_INT(indexValue) || IS_CRUX_STRING(indexValue)) {
      if (!objectTableSet(vm, table, indexValue, value)) {
        runtimePanic(vm, COLLECTION_GET, "Failed to set value in table");
        return INTERPRET_RUNTIME_ERROR;
      }
    } else {
      runtimePanic(vm, TYPE, "Key cannot be hashed.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (IS_CRUX_ARRAY(peek(vm, 1))) {
    ObjectArray *array = AS_CRUX_ARRAY(peek(vm, 1));
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
  popTwo(vm); // indexValue and collection
  push(vm, indexValue);
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
  Value operandValue = peek(vm, 0); // Right-hand side

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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (IS_CRUX_STRING(currentValue) || IS_CRUX_STRING(operandValue)) {
    push(vm, currentValue);
    if (!concatenate(vm)) {
      pop(vm);

      return INTERPRET_RUNTIME_ERROR;
    }

    frame->slots[slot] = peek(vm, 0);
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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

  bool currentIsInt = IS_INT(currentValue);
  bool currentIsFloat = IS_FLOAT(currentValue);
  bool operandIsInt = IS_INT(operandValue);
  bool operandIsFloat = IS_FLOAT(operandValue);

  if (IS_CRUX_STRING(currentValue) || IS_CRUX_STRING(operandValue)) {
    push(vm, currentValue);
    if (!concatenate(vm)) {
      pop(vm);
      return INTERPRET_RUNTIME_ERROR;
    }
    *location = peek(vm, 0);
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
  Value operandValue = peek(vm, 0);

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
    Value value = pop(vm);
    Value key = pop(vm);
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
  push(vm, OBJECT_VAL(table));
  DISPATCH();
}

OP_ANON_FUNCTION: {
  ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
  ObjectClosure *closure = newClosure(vm, function);
  push(vm, OBJECT_VAL(closure));
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
  Value target = peek(vm, 0);
  vm->matchHandler.matchTarget = target;
  vm->matchHandler.isMatchTarget = true;
  DISPATCH();
}

OP_MATCH_JUMP: {
  uint16_t offset = READ_SHORT();
  Value pattern = pop(vm);
  Value target = peek(vm, 0);
  if (!valuesEqual(pattern, target)) {
    frame->ip += offset;
  }
  DISPATCH();
}

OP_MATCH_END: {
  if (vm->matchHandler.isMatchBind) {
    push(vm, vm->matchHandler.matchBind);
  }
  vm->matchHandler.matchTarget = NIL_VAL;
  vm->matchHandler.matchBind = NIL_VAL;
  vm->matchHandler.isMatchBind = false;
  vm->matchHandler.isMatchTarget = false;
  DISPATCH();
}

OP_RESULT_MATCH_OK: {
  uint16_t offset = READ_SHORT();
  Value target = peek(vm, 0);
  if (!IS_CRUX_RESULT(target) || !AS_CRUX_RESULT(target)->isOk) {
    frame->ip += offset;
  } else {
    Value value = AS_CRUX_RESULT(target)->as.value;
    popPush(vm, value);
  }
  DISPATCH();
}

OP_RESULT_MATCH_ERR: {
  uint16_t offset = READ_SHORT();
  Value target = peek(vm, 0);
  if (!IS_CRUX_RESULT(target) || AS_CRUX_RESULT(target)->isOk) {
    frame->ip += offset;
  } else {
    Value error = OBJECT_VAL(AS_CRUX_RESULT(target)->as.error);
    popPush(vm, error);
  }
  DISPATCH();
}

OP_RESULT_BIND: {
  uint8_t slot = READ_BYTE();
  Value bind = peek(vm, 0);
  vm->matchHandler.matchBind = bind;
  vm->matchHandler.isMatchBind = true;
  frame->slots[slot] = bind;
  DISPATCH();
}

OP_GIVE: {
  Value result = pop(vm);
  popPush(vm, result);
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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

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
  Value operandValue = peek(vm, 0);

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
    push(vm, OBJECT_VAL(value));
    bool setSuccess =
        tableSet(vm, &vm->currentModuleRecord->globals, aliases[i], value);

    if (!setSuccess) {
      runtimePanic(vm, IMPORT, "Failed to import '%s' from '%s'.",
                   names[i]->chars, moduleName->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
    pop(vm);
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
    push(vm, cachedModule);
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
  module->frameCapacity = FRAMES_MAX;

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
    push(vm, OBJECT_VAL(module));
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjectFunction *function = compile(vm, file.content);
  freeFileResult(file);

  if (function == NULL) {
    module->state = STATE_ERROR;
    runtimePanic(vm, RUNTIME, "Failed to compile '%s'.", resolvedPath->chars);
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(vm, OBJECT_VAL(module));
    return INTERPRET_COMPILE_ERROR;
  }
  push(vm, OBJECT_VAL(function));
  ObjectClosure *closure = newClosure(vm, function);
  pop(vm);
  push(vm, OBJECT_VAL(closure));

  module->moduleClosure = closure;

  tableSet(vm, &vm->moduleCache, resolvedPath, OBJECT_VAL(module));

  if (!call(vm, closure, 0)) {
    module->state = STATE_ERROR;
    runtimePanic(vm, RUNTIME, "Failed to call module.");
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(vm, OBJECT_VAL(module));
    return INTERPRET_RUNTIME_ERROR;
  }

  InterpretResult result = run(vm);
  if (result != INTERPRET_OK) {
    module->state = STATE_ERROR;
    popImportStack(vm);
    vm->currentModuleRecord = previousModuleRecord;
    push(vm, OBJECT_VAL(module));
    return result;
  }

  module->state = STATE_LOADED;
  
  popImportStack(vm);
  vm->currentModuleRecord = previousModuleRecord;
  push(vm, OBJECT_VAL(module));

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
  if (!IS_CRUX_MODULE_RECORD(peek(vm, 1))) {
    runtimePanic(vm, RUNTIME, "Module record creation could not be completed.");
    return INTERPRET_RUNTIME_ERROR;
  }
  pop(vm);
  Value moduleValue = pop(vm);
  ObjectModuleRecord* importedModule = AS_CRUX_MODULE_RECORD(moduleValue);

  // copy names
  for (uint8_t i = 0; i < nameCount; i++) {
    ObjectString* name = names[i];
    ObjectString* alias = aliases[i];

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
  for (Value *slot = moduleRecord->stack; slot < moduleRecord->stackTop; slot++) {
    printf("[");
    printValue(*slot);
    printf("]");
  }
  printf("\n");

  disassembleInstruction(
      &frame->closure->function->chunk,
      (int)(frame->ip - frame->closure->function->chunk.code));

  DISPATCH();
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
  if (function == NULL) {
    vm->currentModuleRecord->state = STATE_ERROR;
    return INTERPRET_COMPILE_ERROR;
  }

  push(vm, OBJECT_VAL(function));
  ObjectClosure *closure = newClosure(vm, function);
  vm->currentModuleRecord->moduleClosure = closure;
  pop(vm);
  push(vm, OBJECT_VAL(closure));
  call(vm, closure, 0);

  InterpretResult result = run(vm);

  return result;
}
