#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "memory.h"
#include "object.h"
#include "panic.h"

/**
 * @brief Allocates a new object of the specified type.
 *
 * This is a static helper function used to allocate memory for a new object
 * and initialize its basic properties. It uses the `reallocate` function
 * for memory management, integrates with the garbage collector by linking
 * the new object into the VM's object list, and sets the object's type and
 * initial marking status.
 *
 * @param vm The virtual machine.
 * @param size The size of the object to allocate in bytes.
 * @param type The type of the object being allocated (ObjectType enum).
 *
 * @return A pointer to the newly allocated and initialized Object.
 */
static Object *allocateObject(VM *vm, size_t size, ObjectType type) {
  Object *object = reallocate(vm, NULL, 0, size);

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *)object);
  printValue(OBJECT_VAL(object));
  printf("\n");
#endif

  object->type = type;
  object->next = vm->objects;
  object->isMarked = false;
  vm->objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif

  return object;
}
/**
 * @brief Macro to allocate a specific type of object.
 *
 * This macro simplifies object allocation by wrapping the `allocateObject`
 * function. It casts the result of `allocateObject` to the desired object type,
 * reducing code duplication and improving readability.
 *
 * @param vm The virtual machine.
 * @param type The C type of the object to allocate (e.g., ObjectString).
 * @param objectType The ObjectType enum value for the object.
 *
 * @return A pointer to the newly allocated object, cast to the specified type.
 */
#define ALLOCATE_OBJECT(vm, type, objectType)                                  \
  (type *)allocateObject(vm, sizeof(type), objectType)

/**
 * @brief Calculates the next power of 2 capacity for a collection.
 *
 * This static helper function calculates the next power of 2 capacity for
 * collections like tables and arrays.
 *
 * @param n The desired minimum capacity.
 *
 * @return The next power of 2 capacity greater than or equal to `n`, or
 * `UINT16_MAX - 1` if `n` is close to the maximum.
 */
static uint32_t calculateCollectionCapacity(uint32_t n) {
  if (n >= UINT16_MAX - 1) {
    return UINT16_MAX - 1;
  }

  if (n < 8)
    return 8;
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}

/**
 * @brief Generates a hash code for a Value.
 *
 * This static helper function calculates a hash code for a given `Value`.
 * It handles different value types (strings, numbers, booleans, nil) to
 * produce a suitable hash value for use in hash tables.
 *
 * @param value The Value to hash.
 *
 * @return A 32-bit hash code for the Value.
 */
static uint32_t hashValue(const Value value) {
  if (IS_CRUX_STRING(value)) {
    return AS_CRUX_STRING(value)->hash;
  }
  if (IS_INT(value)) {
    return (uint32_t)AS_INT(value);
  }
  if (IS_FLOAT(value)) {
    const double num = AS_FLOAT(value);
    uint64_t bits;
    memcpy(&bits, &num, sizeof(bits));
    return (uint32_t)(bits ^ bits >> 32);
  }
  if (IS_BOOL(value)) {
    return AS_BOOL(value) ? 1u : 0u;
  }
  if (IS_NIL(value)) {
    return 4321u;
  }
  return 0u;
}

void printType(const Value value) {
  if (IS_INT(value)) {
    printf("'int'");
  } else if (IS_FLOAT(value)) {
    printf("'float'");
  } else if (IS_BOOL(value)) {
    printf("'bool'");
  } else if (IS_NIL(value)) {
    printf("'nil'");
  }
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("'string'");
    break;
  case OBJECT_FUNCTION:
  case OBJECT_NATIVE_FUNCTION:
  case OBJECT_NATIVE_INFALLIBLE_FUNCTION:
  case OBJECT_NATIVE_METHOD:
  case OBJECT_NATIVE_INFALLIBLE_METHOD:
  case OBJECT_CLOSURE:
  case OBJECT_BOUND_METHOD:
    printf("'function'");
    break;
  case OBJECT_UPVALUE:
    printf("'upvalue'");
    break;
  case OBJECT_CLASS:
    printf("'class'");
    break;
  case OBJECT_INSTANCE:
    printf("'instance'");
    break;
  case OBJECT_ARRAY:
    printf("'array'");
    break;
  case OBJECT_TABLE:
    printf("'table'");
    break;
  case OBJECT_ERROR:
    printf("'error'");
    break;
  case OBJECT_RESULT:
    printf("'result'");
    break;
  case OBJECT_RANDOM:
    printf("'random'");
    break;
  case OBJECT_FILE:
    printf("'file'");
    break;
  default:
    printf("'unknown'");
  }
}

ObjectBoundMethod *newBoundMethod(VM *vm, const Value receiver,
                                  ObjectClosure *method) {
  ObjectBoundMethod *bound =
      ALLOCATE_OBJECT(vm, ObjectBoundMethod, OBJECT_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjectClass *newClass(VM *vm, ObjectString *name) {
  ObjectClass *klass = ALLOCATE_OBJECT(vm, ObjectClass, OBJECT_CLASS);
  initTable(&klass->methods);
  klass->name = name;
  return klass;
}

ObjectUpvalue *newUpvalue(VM *vm, Value *slot) {
  ObjectUpvalue *upvalue = ALLOCATE_OBJECT(vm, ObjectUpvalue, OBJECT_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

ObjectClosure *newClosure(VM *vm, ObjectFunction *function) {
  ObjectUpvalue **upvalues =
      ALLOCATE(vm, ObjectUpvalue *, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjectClosure *closure = ALLOCATE_OBJECT(vm, ObjectClosure, OBJECT_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

/**
 * @brief Allocates a new string object.
 *
 * This static helper function allocates a new ObjectString and copies the given
 * character array into the object's memory. It also calculates and stores the
 * string's hash value and interns the string in the VM's string table.
 *
 * @param vm The virtual machine.
 * @param chars The character array for the string. This memory is assumed to be
 * managed externally and copied.
 * @param length The length of the string.
 * @param hash The pre-calculated hash value of the string.
 *
 * @return A pointer to the newly created and interned ObjectString.
 */
static ObjectString *allocateString(VM *vm, char *chars, const uint32_t length,
                                    const uint32_t hash) {
  ObjectString *string = ALLOCATE_OBJECT(vm, ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  // intern the string
  PUSH(vm->currentModuleRecord, OBJECT_VAL(string));
  tableSet(vm, &vm->strings, string, NIL_VAL);
  POP(vm->currentModuleRecord);
  return string;
}

/**
 * @brief Calculates the hash value of a C-style string.
 *
 * This function implements the FNV-1a hash algorithm to generate a hash
 * code for a given C-style string.
 *
 * @param key The null-terminated C-style string to hash.
 * @param length The length of the string (excluding null terminator).
 *
 * @return A 32-bit hash code for the string.
 */
static uint32_t hashString(const char *key, const size_t length) {
  static const uint32_t FNV_OFFSET_BIAS = 2166136261u;
  static const uint32_t FNV_PRIME = 16777619u;

  uint32_t hash = FNV_OFFSET_BIAS;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

ObjectString *copyString(VM *vm, const char *chars, const uint32_t length) {
  const uint32_t hash = hashString(chars, length);

  ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL)
    return interned;

  char *heapChars = ALLOCATE(vm, char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] =
      '\0'; // terminating the string because it is not terminated in the source
  return allocateString(vm, heapChars, length, hash);
}

static void printErrorType(const ErrorType type) {
  switch (type) {
  case SYNTAX:
    printf("syntax");
    break;
  case MATH:
    printf("math");
    break;
  case BOUNDS:
    printf("bounds");
    break;
  case RUNTIME:
    printf("runtime");
    break;
  case TYPE:
    printf("type");
    break;
  case LOOP_EXTENT:
    printf("loop");
    break;
  case LIMIT:
    printf("limit");
    break;
  case BRANCH_EXTENT:
    printf("branch");
    break;
  case CLOSURE_EXTENT:
    printf("closure");
    break;
  case LOCAL_EXTENT:
    printf("local");
    break;
  case ARGUMENT_EXTENT:
    printf("argument");
    break;
  case NAME:
    printf("name");
    break;
  case COLLECTION_EXTENT:
    printf("collection");
    break;
  case VARIABLE_EXTENT:
    printf("variable");
    break;
  case RETURN_EXTENT:
    printf("return");
    break;
  case ARGUMENT_MISMATCH:
    printf("argument mismatch");
    break;
  case STACK_OVERFLOW:
    printf("stack overflow");
    break;
  case COLLECTION_GET:
    printf("collection get");
    break;
  case COLLECTION_SET:
    printf("collection set");
    break;
  case UNPACK_MISMATCH:
    printf("unpack mismatch");
    break;
  case MEMORY:
    printf("memory");
    break;
  case VALUE:
    printf("value");
    break;
  case ASSERT:
    printf("assert");
    break;
  case IMPORT_EXTENT:
    printf("import");
    break;
  case IO:
    printf("io");
    break;
  case IMPORT:
    printf("import");
    break;
  }
}

/**
 * @brief Prints the name of a function object.
 *
 * This static helper function prints the name of a function object to the
 * console, used for debugging and representation. If the function is
 * anonymous (name is NULL), it prints "<script>".
 *
 * @param function The ObjectFunction to print the name of.
 */
static void printFunction(const ObjectFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

static void printArray(const Value *values, const uint32_t size) {
  printf("[");
  for (int i = 0; i < size; i++) {
    printValue(values[i], true);
    if (i != size - 1) {
      printf(", ");
    }
  }
  printf("]");
}

static void printTable(const ObjectTableEntry *entries, const uint32_t capacity,
                       const uint32_t size) {
  uint32_t printed = 0;
  printf("{");
  for (int i = 0; i < capacity; i++) {
    if (entries[i].isOccupied) {
      printValue(entries[i].key, true);
      printf(":");
      printValue(entries[i].value, true);
      if (printed != size - 1) {
        printf(", ");
      }
      printed++;
    }
  }
  printf("}");
}


static void printError(ObjectError* error) {}

static void printStructInstance(const ObjectStructInstance* instance) {
  printf("{");
  uint32_t printed = 0;
  const ObjectStruct* type = instance->structType;
  for (int i =0; i < type->fields.capacity; i++) {
    if (type->fields.entries[i].key != NULL) {
      const uint16_t index = (uint16_t) AS_INT(type->fields.entries[i].value);
      const ObjectString* fieldName = type->fields.entries[i].key;
      printf("%s: ", fieldName->chars);
      printValue(instance->fields[index], true);
      if (printed != type->fields.count - 1) {
        printf(", ");
      }
      printed++;
    }
  }
  printf("}");
}

static void printResult(const ObjectResult *result) {
  if (result->isOk) {
    printf("Ok<");
    printType(result->as.value);
    printf(">");
  } else {
    printf("Err<");
    printErrorType(result->as.error->type);
    printf(">");
  }
}

void printObject(const Value value, const bool inCollection) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_CLASS: {
    printf("'%s' <class>", AS_CRUX_CLASS(value)->name->chars);
    break;
  }
  case OBJECT_STRING: {
    if (inCollection) {
      printf("'%s'", AS_C_STRING(value));
      break;
    }
    printf("%s", AS_C_STRING(value));
    break;
  }
  case OBJECT_FUNCTION: {
    printFunction(AS_CRUX_FUNCTION(value));
    break;
  }
  case OBJECT_NATIVE_FUNCTION: {
    const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(value);
    if (native->name != NULL) {
      printf("<native fn %s>", native->name->chars);
    } else {
      printf("<native fn>");
    }
    break;
  }
  case OBJECT_NATIVE_METHOD: {
    const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
    if (native->name != NULL) {
      printf("<native method %s>", native->name->chars);
    } else {
      printf("<native method>");
    }
    break;
  }
  case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
    const ObjectNativeInfallibleFunction *native =
        AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
    if (native->name != NULL) {
      printf("<native infallible fn %s>", native->name->chars);
    } else {
      printf("<native infallible fn>");
    }
    break;
  }
  case OBJECT_CLOSURE: {
    printFunction(AS_CRUX_CLOSURE(value)->function);
    break;
  }
  case OBJECT_UPVALUE: {
    printValue(value, false);
    break;
  }
  case OBJECT_INSTANCE: {
    printf("'%s' <instance>", AS_CRUX_INSTANCE(value)->klass->name->chars);
    break;
  }
  case OBJECT_BOUND_METHOD: {
    printFunction(AS_CRUX_BOUND_METHOD(value)->method->function);
    break;
  }
  case OBJECT_ARRAY: {
    const ObjectArray *array = AS_CRUX_ARRAY(value);
    printArray(array->values, array->size);
    break;
  }
  case OBJECT_STATIC_ARRAY: {
    const ObjectStaticArray *staticArray = AS_CRUX_STATIC_ARRAY(value);
    printArray(staticArray->values, staticArray->size);
    break;
  }
  case OBJECT_TABLE: {
    const ObjectTable *table = AS_CRUX_TABLE(value);
    printTable(table->entries, table->capacity, table->size);
    break;
  }
  case OBJECT_STATIC_TABLE: {
    const ObjectStaticTable *table = AS_CRUX_STATIC_TABLE(value);
    printTable(table->entries, table->capacity, table->size);
    break;
  }
  case OBJECT_ERROR: {
    printf("<error ");
    printErrorType(AS_CRUX_ERROR(value)->type);
    printf(">");
    break;
  }
  case OBJECT_RESULT: {
    printResult(AS_CRUX_RESULT(value));
    break;
  }
  case OBJECT_RANDOM: {
    printf("<random>");
    break;
  }
  case OBJECT_FILE: {
    printf("<file>");
    break;
  }
  case OBJECT_NATIVE_INFALLIBLE_METHOD: {
    printf("<native infallible method>");
    break;
  }

  case OBJECT_MODULE_RECORD: {
    printf("<module record>");
    break;
  }

  case OBJECT_STRUCT: {
    printf("<struct type %s>", AS_CRUX_STRUCT(value)->name->chars);
    break;
  }
    case OBJECT_STRUCT_INSTANCE: {
      printStructInstance(AS_CRUX_STRUCT_INSTANCE(value));
      break;
    }
  }
}

ObjectString *takeString(VM *vm, char *chars, const uint32_t length) {
  const uint32_t hash = hashString(chars, length);

  ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL) {
    // free the string that was passed to us.
    FREE_ARRAY(vm, char, chars, length + 1);
    return interned;
  }

  return allocateString(vm, chars, length, hash);
}

ObjectString *toString(VM *vm, const Value value) {
  if (!IS_CRUX_OBJECT(value)) {
    char buffer[32];
    if (IS_FLOAT(value)) {
      const double num = AS_FLOAT(value);
      snprintf(buffer, sizeof(buffer), "%g", num);
    } else if (IS_INT(value)) {
      const int32_t num = AS_INT(value);
      snprintf(buffer, sizeof(buffer), "%d", num);
    } else if (IS_BOOL(value)) {
      strcpy(buffer, AS_BOOL(value) ? "true" : "false");
    } else if (IS_NIL(value)) {
      strcpy(buffer, "nil");
    }
    return copyString(vm, buffer, (int)strlen(buffer));
  }

  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    return AS_CRUX_STRING(value);

  case OBJECT_FUNCTION: {
    const ObjectFunction *function = AS_CRUX_FUNCTION(value);
    if (function->name == NULL) {
      return copyString(vm, "<script>", 8);
    }
    char buffer[64];
    const int length =
        snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_NATIVE_FUNCTION: {
    const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(value);
    if (native->name != NULL) {
      const char *start = "<native fn ";
      const char *end = ">";
      char *buffer = ALLOCATE(
          vm, char, strlen(start) + strlen(end) + native->name->length + 1);
      strcpy(buffer, start);
      strcat(buffer, native->name->chars);
      strcat(buffer, end);
      ObjectString *result = takeString(vm, buffer, strlen(buffer));
      FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
      return result;
    }
    return copyString(vm, "<native fn>", 11);
  }

  case OBJECT_NATIVE_METHOD: {
    const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
    if (native->name != NULL) {
      const char *start = "<native method ";
      const char *end = ">";
      char *buffer = ALLOCATE(
          vm, char, strlen(start) + strlen(end) + native->name->length + 1);
      strcpy(buffer, start);
      strcat(buffer, native->name->chars);
      strcat(buffer, end);
      ObjectString *result = takeString(vm, buffer, strlen(buffer));
      FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
      return result;
    }
    return copyString(vm, "<native method>", 15);
  }

  case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
    const ObjectNativeInfallibleFunction *native =
        AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
    if (native->name != NULL) {
      const char *start = "<native infallible fn ";
      const char *end = ">";
      char *buffer = ALLOCATE(
          vm, char, strlen(start) + strlen(end) + native->name->length + 1);
      strcpy(buffer, start);
      strcat(buffer, native->name->chars);
      strcat(buffer, end);
      ObjectString *result = takeString(vm, buffer, strlen(buffer));
      FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
      return result;
    }
    return copyString(vm, "<native infallible fn>", 21);
  }

  case OBJECT_NATIVE_INFALLIBLE_METHOD: {
    const ObjectNativeInfallibleMethod *native =
        AS_CRUX_NATIVE_INFALLIBLE_METHOD(value);
    if (native->name != NULL) {
      const char *start = "<native infallible method ";
      const char *end = ">";
      char *buffer = ALLOCATE(
          vm, char, strlen(start) + strlen(end) + native->name->length + 1);
      strcpy(buffer, start);
      strcat(buffer, native->name->chars);
      strcat(buffer, end);
      ObjectString *result = takeString(vm, buffer, strlen(buffer));
      FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
      return result;
    }
    return copyString(vm, "<native infallible method>", 25);
  }

  case OBJECT_CLOSURE: {
    const ObjectFunction *function = AS_CRUX_CLOSURE(value)->function;
    if (function->name == NULL) {
      return copyString(vm, "<script>", 8);
    }
    char buffer[256];
    const int length =
        snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_UPVALUE: {
    return copyString(vm, "<upvalue>", 9);
  }

  case OBJECT_CLASS: {
    const ObjectClass *klass = AS_CRUX_CLASS(value);
    char buffer[256];
    const int length =
        snprintf(buffer, sizeof(buffer), "%s <class>", klass->name->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_INSTANCE: {
    const ObjectInstance *instance = AS_CRUX_INSTANCE(value);
    char buffer[256];
    const int length = snprintf(buffer, sizeof(buffer), "%s <instance>",
                                instance->klass->name->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_BOUND_METHOD: {
    const ObjectBoundMethod *bound = AS_CRUX_BOUND_METHOD(value);
    char buffer[256];
    const int length = snprintf(buffer, sizeof(buffer), "<bound fn %s>",
                                bound->method->function->name->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_ARRAY: {
    const ObjectArray *array = AS_CRUX_ARRAY(value);
    size_t bufSize = 2; // [] minimum
    for (int i = 0; i < array->size; i++) {
      const ObjectString *element = toString(vm, array->values[i]);
      bufSize += element->length + 2; // element + ", "
    }

    char *buffer = ALLOCATE(vm, char, bufSize);
    char *ptr = buffer;
    *ptr++ = '[';

    for (int i = 0; i < array->size; i++) {
      if (i > 0) {
        *ptr++ = ',';
        *ptr++ = ' ';
      }
      const ObjectString *element = toString(vm, array->values[i]);
      memcpy(ptr, element->chars, element->length);
      ptr += element->length;
    }
    *ptr++ = ']';

    ObjectString *result = takeString(vm, buffer, ptr - buffer);
    return result;
  }

  case OBJECT_TABLE: {
    const ObjectTable *table = AS_CRUX_TABLE(value);
    size_t bufSize = 2; // {} minimum
    for (int i = 0; i < table->capacity; i++) {
      if (table->entries[i].isOccupied) {
        const ObjectString *k = toString(vm, table->entries[i].key);
        const ObjectString *v = toString(vm, table->entries[i].value);
        bufSize += k->length + v->length + 4; // key:value,
      }
    }

    char *buffer = ALLOCATE(vm, char, bufSize);
    char *ptr = buffer;
    *ptr++ = '{';

    bool first = true;
    for (int i = 0; i < table->capacity; i++) {
      if (table->entries[i].isOccupied) {
        if (!first) {
          *ptr++ = ',';
          *ptr++ = ' ';
        }
        first = false;

        const ObjectString *key = toString(vm, table->entries[i].key);
        const ObjectString *val = toString(vm, table->entries[i].value);

        memcpy(ptr, key->chars, key->length);
        ptr += key->length;
        *ptr++ = ':';
        memcpy(ptr, val->chars, val->length);
        ptr += val->length;
      }
    }
    *ptr++ = '}';

    ObjectString *result = takeString(vm, buffer, ptr - buffer);
    return result;
  }

  case OBJECT_ERROR: {
    const ObjectError *error = AS_CRUX_ERROR(value);
    char buffer[1024];
    const int length =
        snprintf(buffer, sizeof(buffer), "<error: %s>", error->message->chars);
    return copyString(vm, buffer, length);
  }

  case OBJECT_RESULT: {
    const ObjectResult *result = AS_CRUX_RESULT(value);
    if (result->isOk) {
      return copyString(vm, "<Ok>", 4);
    }
    return copyString(vm, "<Err>", 5);
  }

  case OBJECT_RANDOM: {
    return copyString(vm, "<random>", 8);
  }

  case OBJECT_FILE: {
    return copyString(vm, "<file>", 6);
  }

  case OBJECT_STRUCT: {
    const ObjectStruct *structObject = AS_CRUX_STRUCT(value);
    printf("<struct type %s>", structObject->name->chars);
  }

    case OBJECT_STRUCT_INSTANCE: {
    const ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(value);
    printStructInstance(instance);
  }

  default:
    return copyString(vm, "<crux object>", 13);
  }
}

ObjectFunction *newFunction(VM *vm) {
  ObjectFunction *function =
      ALLOCATE_OBJECT(vm, ObjectFunction, OBJECT_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  function->upvalueCount = 0;
  initChunk(&function->chunk);
  return function;
}

ObjectInstance *newInstance(VM *vm, ObjectClass *klass) {
  ObjectInstance *instance =
      ALLOCATE_OBJECT(vm, ObjectInstance, OBJECT_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjectNativeFunction *newNativeFunction(VM *vm, const CruxCallable function,
                                        const int arity, ObjectString *name) {
  ObjectNativeFunction *native =
      ALLOCATE_OBJECT(vm, ObjectNativeFunction, OBJECT_NATIVE_FUNCTION);
  native->function = function;
  native->arity = arity;
  native->name = name;
  return native;
}

ObjectNativeMethod *newNativeMethod(VM *vm, const CruxCallable function,
                                    const int arity, ObjectString *name) {
  ObjectNativeMethod *native =
      ALLOCATE_OBJECT(vm, ObjectNativeMethod, OBJECT_NATIVE_METHOD);
  native->function = function;
  native->arity = arity;
  native->name = name;
  return native;
}

ObjectNativeInfallibleFunction *
newNativeInfallibleFunction(VM *vm, const CruxInfallibleCallable function,
                            const int arity, ObjectString *name) {
  ObjectNativeInfallibleFunction *native = ALLOCATE_OBJECT(
      vm, ObjectNativeInfallibleFunction, OBJECT_NATIVE_INFALLIBLE_FUNCTION);
  native->function = function;
  native->arity = arity;
  native->name = name;
  return native;
}

ObjectNativeInfallibleMethod *
newNativeInfallibleMethod(VM *vm, const CruxInfallibleCallable function,
                          const int arity, ObjectString *name) {
  ObjectNativeInfallibleMethod *native = ALLOCATE_OBJECT(
      vm, ObjectNativeInfallibleMethod, OBJECT_NATIVE_INFALLIBLE_METHOD);
  native->function = function;
  native->arity = arity;
  native->name = name;
  return native;
}

ObjectTable *newTable(VM *vm, const int elementCount) {
  ObjectTable *table = ALLOCATE_OBJECT(vm, ObjectTable, OBJECT_TABLE);
  table->capacity =
      elementCount < 16 ? 16 : calculateCollectionCapacity(elementCount);
  table->size = 0;
  table->entries = ALLOCATE(vm, ObjectTableEntry, table->capacity);
  for (int i = 0; i < table->capacity; i++) {
    table->entries[i].value = NIL_VAL;
    table->entries[i].key = NIL_VAL;
    table->entries[i].isOccupied = false;
  }
  return table;
}

void freeObjectTable(VM *vm, ObjectTable *table) {
  FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
  table->entries = NULL;
  table->capacity = 0;
  table->size = 0;
}

void freeObjectStaticTable(VM *vm, ObjectStaticTable *table) {
  FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
  table->entries = NULL;
  table->capacity = 0;
  table->size = 0;
}

ObjectFile *newObjectFile(VM *vm, ObjectString *path, ObjectString *mode) {
  ObjectFile *file = ALLOCATE_OBJECT(vm, ObjectFile, OBJECT_FILE);
  file->path = path;
  file->mode = mode;
  file->file = fopen(path->chars, mode->chars);
  file->isOpen = file->file != NULL;
  file->position = 0;
  return file;
}

/**
 * @brief Finds an entry in an object table.
 *
 * This static helper function finds an entry in an ObjectTable's entry array
 * based on a given key. It uses open addressing with quadratic probing to
 * resolve collisions. It also handles tombstones (entries that were
 * previously occupied but are now deleted) to allow for rehashing after
 * deletions.
 *
 * @param entries The array of ObjectTableEntry.
 * @param capacity The capacity of the table's entry array.
 * @param key The key Value to search for.
 *
 * @return A pointer to the ObjectTableEntry for the key, or a pointer to an
 * empty entry (possibly a tombstone) if the key is not found.
 */
static ObjectTableEntry *findEntry(ObjectTableEntry *entries,
                                   const uint16_t capacity, const Value key) {
  const uint32_t hash = hashValue(key);
  uint32_t index = hash & capacity - 1;
  ObjectTableEntry *tombstone = NULL;

  while (1) {
    ObjectTableEntry *entry = &entries[index];
    if (!entry->isOccupied) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      }
      if (tombstone == NULL) {
        tombstone = entry;
      }
    } else if (valuesEqual(entry->key, key)) {
      return entry;
    }
    index = index * 5 + 1 & capacity - 1; // new probe
  }
}

/**
 * @brief Adjusts the capacity of an object table.
 *
 * This static helper function resizes an ObjectTable to a new capacity. It
 * allocates a new entry array with the new capacity, rehashes all existing
 * entries into the new array, and frees the old entry array.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to resize.
 * @param capacity The new capacity for the table.
 *
 * @return true if the capacity adjustment was successful, false otherwise
 * (e.g., memory allocation failure).
 */
static bool adjustCapacity(VM *vm, ObjectTable *table, const int capacity) {
  ObjectTableEntry *entries = ALLOCATE(vm, ObjectTableEntry, capacity);
  if (entries == NULL) {
    return false;
  }

  for (int i = 0; i < capacity; i++) {
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
    entries[i].isOccupied = false;
  }

  table->size = 0;

  for (int i = 0; i < table->capacity; i++) {
    const ObjectTableEntry *entry = &table->entries[i];
    if (!entry->isOccupied) {
      continue;
    }

    ObjectTableEntry *destination = findEntry(entries, capacity, entry->key);

    destination->key = entry->key;
    destination->value = entry->value;
    destination->isOccupied = true;
    table->size++;
  }

  FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
  return true;
}

bool objectTableSet(VM *vm, ObjectTable *table, const Value key,
                    const Value value) {
  if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
    const int capacity = GROW_CAPACITY(table->capacity);
    if (!adjustCapacity(vm, table, capacity))
      return false;
  }

  ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
  const bool isNewKey = !entry->isOccupied;

  if (isNewKey) {
    table->size++;
  }

  if (IS_CRUX_OBJECT(key))
    markValue(vm, key);
  if (IS_CRUX_OBJECT(value))
    markValue(vm, value);

  entry->key = key;
  entry->value = value;
  entry->isOccupied = true;

  return true;
}

bool objectTableRemove(ObjectTable *table, const Value key) {
  if (!table) {
    return false;
  }
  ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
  if (!entry->isOccupied) {
    return false;
  }
  entry->isOccupied = false;
  entry->key = NIL_VAL;
  entry->value = BOOL_VAL(false);
  table->size--;
  return true;
}

bool objectTableContainsKey(ObjectTable *table, const Value key) {
  if (!table)
    return false;

  const ObjectTableEntry *entry =
      findEntry(table->entries, table->capacity, key);
  return entry->isOccupied;
}

bool entriesContainsKey(ObjectTableEntry *entries, const Value key,
                        const uint32_t capacity) {
  if (!entries)
    return false;
  const ObjectTableEntry *entry = findEntry(entries, capacity, key);
  return entry->isOccupied;
}

bool objectTableGet(ObjectTableEntry *entries, const uint32_t size,
                    const uint32_t capacity, const Value key, Value *value) {
  if (size == 0) {
    return false;
  }

  const ObjectTableEntry *entry = findEntry(entries, capacity, key);
  if (!entry->isOccupied) {
    return false;
  }
  *value = entry->value;
  return true;
}

ObjectArray *newArray(VM *vm, const uint32_t elementCount) {
  ObjectArray *array = ALLOCATE_OBJECT(vm, ObjectArray, OBJECT_ARRAY);
  array->capacity = calculateCollectionCapacity(elementCount);
  array->size = 0;
  array->values = ALLOCATE(vm, Value, array->capacity);
  for (int i = 0; i < array->capacity; i++) {
    array->values[i] = NIL_VAL;
  }
  return array;
}

bool ensureCapacity(VM *vm, ObjectArray *array, const uint32_t capacityNeeded) {
  if (capacityNeeded <= array->capacity) {
    return true;
  }
  uint32_t newCapacity = array->capacity;
  while (newCapacity < capacityNeeded) {
    if (newCapacity > INT_MAX / 2) {
      return false;
    }
    newCapacity *= 2;
  }
  Value *newArray =
      GROW_ARRAY(vm, Value, array->values, array->capacity, newCapacity);
  if (newArray == NULL) {
    return false;
  }
  for (uint32_t i = array->capacity; i < newCapacity; i++) {
    newArray[i] = NIL_VAL;
  }
  array->values = newArray;
  array->capacity = newCapacity;
  return true;
}

bool arraySet(VM *vm, const ObjectArray *array, const uint32_t index,
              const Value value) {
  if (index >= array->size) {
    return false;
  }
  if (IS_CRUX_OBJECT(value)) {
    markValue(vm, value);
  }
  array->values[index] = value;
  return true;
}

bool arrayAdd(VM *vm, ObjectArray *array, const Value value,
              const uint32_t index) {
  if (!ensureCapacity(vm, array, array->size + 1)) {
    return false;
  }
  if (IS_CRUX_OBJECT(value)) {
    markValue(vm, value);
  }
  array->values[index] = value;
  array->size++;
  return true;
}

bool arrayAddBack(VM *vm, ObjectArray *array, const Value value) {
  if (!ensureCapacity(vm, array, array->size + 1)) {
    return false;
  }
  array->values[array->size] = value;
  array->size++;
  return true;
}

ObjectError *newError(VM *vm, ObjectString *message, const ErrorType type,
                      const bool isPanic) {
  ObjectError *error = ALLOCATE_OBJECT(vm, ObjectError, OBJECT_ERROR);
  error->message = message;
  error->type = type;
  error->isPanic = isPanic;
  return error;
}

ObjectResult *newOkResult(VM *vm, const Value value) {
  ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
  result->isOk = true;
  result->as.value = value;
  return result;
}

ObjectResult *newErrorResult(VM *vm, ObjectError *error) {
  ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
  result->isOk = false;
  result->as.error = error;
  return result;
}

ObjectRandom *newRandom(VM *vm) {
  ObjectRandom *random = ALLOCATE_OBJECT(vm, ObjectRandom, OBJECT_RANDOM);
#ifdef _WIN32
  random->seed =
      (uint64_t)time(NULL) ^ GetTickCount() ^ (uint64_t)GetCurrentProcessId();
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  random->seed = tv.tv_sec ^ tv.tv_usec ^ (uint64_t)getpid();
#endif
  return random;
}

ObjectModuleRecord *newObjectModuleRecord(VM *vm, ObjectString *path,
                                          const bool isRepl,
                                          const bool isMain) {
  ObjectModuleRecord *moduleRecord =
      ALLOCATE_OBJECT(vm, ObjectModuleRecord, OBJECT_MODULE_RECORD);
  moduleRecord->path = path;
  initTable(&moduleRecord->globals);
  initTable(&moduleRecord->publics);
  moduleRecord->state = STATE_LOADING;
  moduleRecord->moduleClosure = NULL;
  moduleRecord->enclosingModule = NULL;

  moduleRecord->stack = (Value *)malloc(STACK_MAX * sizeof(Value));
  moduleRecord->stackTop = moduleRecord->stack;
  moduleRecord->stackLimit = moduleRecord->stack + STACK_MAX;
  moduleRecord->openUpvalues = NULL;

  moduleRecord->frames = (CallFrame *)malloc(FRAMES_MAX * sizeof(CallFrame));
  moduleRecord->frameCount = 0;
  moduleRecord->frameCapacity = FRAMES_MAX;

  moduleRecord->isMain = isMain;
  moduleRecord->isRepl = isRepl;

  return moduleRecord;
}

/**
 * Frees ObjectModuleRecord internals
 * @param vm The VM
 * @param record The ObjectModuleRecord to free
 */
void freeObjectModuleRecord(VM *vm, ObjectModuleRecord *record) {
  free(record->frames);
  record->frames = NULL;
  free(record->stack);
  record->stack = NULL;
  freeTable(vm, &record->globals);
  freeTable(vm, &record->publics);
}

ObjectStaticArray *newStaticArray(VM *vm, const uint16_t elementCount) {
  ObjectStaticArray *array =
      ALLOCATE_OBJECT(vm, ObjectStaticArray, OBJECT_STATIC_ARRAY);
  array->size = elementCount;
  array->values = ALLOCATE(vm, Value, elementCount);
  return array;
}

ObjectStaticTable *newStaticTable(VM *vm, const uint16_t elementCount) {
  ObjectStaticTable *table =
      ALLOCATE_OBJECT(vm, ObjectStaticTable, OBJECT_STATIC_TABLE);
  table->capacity = calculateCollectionCapacity(
      (uint32_t)((1 + TABLE_MAX_LOAD) * elementCount));

  table->size = 0;
  table->entries = ALLOCATE(vm, ObjectTableEntry, table->capacity);
  for (int i = 0; i < table->capacity; i++) {
    table->entries[i].value = NIL_VAL;
    table->entries[i].key = NIL_VAL;
    table->entries[i].isOccupied = false;
  }
  return table;
}

bool objectStaticTableSet(VM *vm, ObjectStaticTable *table, const Value key,
                          const Value value) {
  ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
  const bool isNewKey = !entry->isOccupied;
  if (isNewKey) {
    table->size++;
  }

  if (IS_CRUX_OBJECT(key))
    markValue(vm, key);
  if (IS_CRUX_OBJECT(value))
    markValue(vm, value);

  entry->key = key;
  entry->value = value;
  entry->isOccupied = true;

  return true;
}

ObjectStruct *newStructType(VM *vm, ObjectString *name) {
  ObjectStruct *structObject = ALLOCATE_OBJECT(vm, ObjectStruct, OBJECT_STRUCT);
  structObject->name = name;
  initTable(&structObject->fields);
  return structObject;
}

ObjectStructInstance *newStructInstance(VM *vm, ObjectStruct *structType,
                                        uint16_t fieldCount) {
  ObjectStructInstance *structInstance =
      ALLOCATE_OBJECT(vm, ObjectStructInstance, OBJECT_STRUCT_INSTANCE);
  structInstance->structType = structType;
  structInstance->fields = ALLOCATE(vm, Value, fieldCount);
  for (int i = 0; i < fieldCount; i++) {
    structInstance->fields[i] = NIL_VAL;
  }
  return structInstance;
}
