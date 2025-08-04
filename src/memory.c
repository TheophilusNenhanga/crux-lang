#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "value.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(VM *vm, void *pointer, const size_t oldSize,
                 const size_t newSize) {
  vm->bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage(vm);
#endif
    if (vm->bytesAllocated > vm->nextGC) {
      collectGarbage(vm);
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL) {
    fprintf(stderr, "Fatal error: Failed to reallocate %zu bytes.\n Exiting!",
            newSize);
    exit(1);
  }

  return result;
}

void markObject(VM *vm, Object *object) {
  if (object == NULL)
    return;
  if (object->isMarked)
    return;
  object->isMarked = true;

  if (vm->grayCapacity < vm->grayCount + 1) {
    vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
    vm->grayStack =
        (Object **)realloc(vm->grayStack, vm->grayCapacity * sizeof(Object *));
  }
  if (vm->grayStack == NULL) {
    exit(1);
  }
  vm->grayStack[vm->grayCount++] = object;
}

void markValue(VM *vm, const Value value) {
  if (IS_CRUX_OBJECT(value)) {
    markObject(vm, AS_CRUX_OBJECT(value));
  }
}

/**
 * @brief Marks all objects within a ValueArray as reachable.
 *
 * Iterates through the `values` array of the `ValueArray` and calls `markValue`
 * for each element, marking any contained objects.
 *
 * @param vm The virtual machine.
 * @param array The ValueArray whose elements should be marked.
 */
void markArray(VM *vm, const ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(vm, array->values[i]);
  }
}

/**
 * @brief Marks all objects within an ObjectArray as reachable.
 *
 * Iterates through the `array` of the `ObjectArray` and calls `markValue`
 * for each element, marking any contained objects.
 *
 * @param vm The virtual machine.
 * @param values the values of the array
 * @param size The size of the array
 */
void markObjectArray(VM *vm, const Value *values, const uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    markValue(vm, values[i]);
  }
}

/**
 * @brief Marks all objects within an ObjectTable as reachable.
 *
 * Iterates through the entries of the `ObjectTable` and calls `markValue`
 * for both the key and the value of each occupied entry, marking any contained
 * objects.
 *
 * @param vm The virtual machine.
 * @param entries The entries in the table
 * @param capacity The capacity of the table
 */
void markObjectTable(VM *vm, const ObjectTableEntry *entries,
                     const uint32_t capacity) {
  for (uint32_t i = 0; i < capacity; i++) {
    if (entries[i].isOccupied) {
      markValue(vm, entries[i].value);
      markValue(vm, entries[i].key);
    }
  }
}

static void markObjectStruct(VM *vm, const ObjectStruct *structure) {
  markObject(vm, (Object *)structure->name);
  markTable(vm, &structure->fields);
}

static void markStructInstance(VM *vm, const ObjectStructInstance *instance) {
  for (int i = 0; i < instance->fieldCount; i++) {
    markValue(vm, instance->fields[i]);
  }
  markObjectStruct(vm, instance->structType);
}

/**
 * @brief Blackens an object, marking all objects it references.
 *
 * This function moves an object from the gray set to the black set in the
 * garbage collection mark phase. It examines the object's type and recursively
 * marks all objects referenced by it, ensuring transitive reachability.
 *
 * @param vm The virtual machine.
 * @param object The object to blacken.
 */
static void blackenObject(VM *vm, Object *object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void *)object);
  printValue(OBJECT_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
  case OBJECT_CLOSURE: {
    const ObjectClosure *closure = (ObjectClosure *)object;
    markObject(vm, (Object *)closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject(vm, (Object *)closure->upvalues[i]);
    }
    break;
  }

  case OBJECT_FUNCTION: {
    const ObjectFunction *function = (ObjectFunction *)object;
    markObject(vm, (Object *)function->name);
    markObject(vm, (Object *)function->moduleRecord);
    markArray(vm, &function->chunk.constants);
    break;
  }

  case OBJECT_UPVALUE: {
    markValue(vm, ((ObjectUpvalue *)object)->closed);
    break;
  }

  case OBJECT_STATIC_ARRAY: {
    const ObjectStaticArray *staticArray = (ObjectStaticArray *)object;
    markObjectArray(vm, staticArray->values, staticArray->size);
    break;
  }
  case OBJECT_ARRAY: {
    const ObjectArray *array = (ObjectArray *)object;
    markObjectArray(vm, array->values, array->size);
    break;
  }

  case OBJECT_STATIC_TABLE: {
    const ObjectStaticTable *table = (ObjectStaticTable *)object;
    markObjectTable(vm, table->entries, table->size);
    break;
  }
  case OBJECT_TABLE: {
    const ObjectTable *table = (ObjectTable *)object;
    markObjectTable(vm, table->entries, table->capacity);
    break;
  }

  case OBJECT_ERROR: {
    const ObjectError *error = (ObjectError *)object;
    markObject(vm, (Object *)error->message);
    break;
  }

  case OBJECT_NATIVE_FUNCTION: {
    const ObjectNativeFunction *native = (ObjectNativeFunction *)object;
    markObject(vm, (Object *)native->name);
    break;
  }

  case OBJECT_NATIVE_METHOD: {
    const ObjectNativeMethod *native = (ObjectNativeMethod *)object;
    markObject(vm, (Object *)native->name);
    break;
  }

  case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
    const ObjectNativeInfallibleFunction *native =
        (ObjectNativeInfallibleFunction *)object;
    markObject(vm, (Object *)native->name);
    break;
  }

  case OBJECT_NATIVE_INFALLIBLE_METHOD: {
    const ObjectNativeInfallibleMethod *native =
        (ObjectNativeInfallibleMethod *)object;
    markObject(vm, (Object *)native->name);
    break;
  }

  case OBJECT_RESULT: {
    const ObjectResult *result = (ObjectResult *)object;
    if (result->isOk) {
      markValue(vm, result->as.value);
    } else {
      markObject(vm, (Object *)result->as.error);
    }
    break;
  }

  case OBJECT_RANDOM: {
    break;
  }

  case OBJECT_FILE: {
    const ObjectFile *file = (ObjectFile *)object;
    markObject(vm, (Object *)file->path);
    markObject(vm, (Object *)file->mode);
    break;
  }

  case OBJECT_MODULE_RECORD: {
    const ObjectModuleRecord *module = (ObjectModuleRecord *)object;
    markObject(vm, (Object *)module->path);
    markTable(vm, &module->globals);
    markTable(vm, &module->publics);
    markObject(vm, (Object *)module->moduleClosure);
    markObject(vm, (Object *)module->enclosingModule); // Can be NULL

    for (const Value *slot = module->stack; slot < module->stackTop; slot++) {
      markValue(vm, *slot);
    }
    for (int i = 0; i < module->frameCount; i++) {
      markObject(vm, (Object *)module->frames[i].closure);
    }
    for (ObjectUpvalue *upvalue = module->openUpvalues; upvalue != NULL;
         upvalue = upvalue->next) {
      markObject(vm, (Object *)upvalue);
    }
    break;
  }

  case OBJECT_STRUCT: {
    const ObjectStruct *structure = (ObjectStruct *)object;
    markObjectStruct(vm, structure);
    break;
  }

  case OBJECT_STRUCT_INSTANCE: {
    const ObjectStructInstance *instance = (ObjectStructInstance *)object;
    markStructInstance(vm, instance);
    break;
  }

  case OBJECT_VEC2:
  case OBJECT_VEC3: {
    break;
  }

  case OBJECT_STRING: {
    // Strings are primitives in terms of GC reachability in this implementation
    break;
  }
  }
}

/**
 * @brief Frees the memory associated with an object based on its type.
 *
 * This function frees the memory allocated for the given `object`. It handles
 * different object types and their specific memory management needs, such as
 * freeing character arrays for strings, chunks for functions, and upvalue
 * arrays for closures.
 *
 * @param vm The virtual machine.
 * @param object The object to free.
 */
static void freeObject(VM *vm, Object *object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void *)object, object->type);
#endif
  switch (object->type) {
  case OBJECT_STRING: {
    const ObjectString *string = (ObjectString *)object;
    FREE_ARRAY(vm, char, string->chars, string->length + 1);
    FREE(vm, ObjectString, object);
    break;
  }
  case OBJECT_FUNCTION: {
    ObjectFunction *function = (ObjectFunction *)object;
    freeChunk(vm, &function->chunk);
    FREE(vm, ObjectFunction, object);
    break;
  }
  case OBJECT_NATIVE_FUNCTION: {
    FREE(vm, ObjectNativeFunction, object);
    break;
  }
  case OBJECT_NATIVE_METHOD: {
    FREE(vm, ObjectNativeMethod, object);
    break;
  }
  case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
    FREE(vm, ObjectNativeInfallibleFunction, object);
    break;
  }
  case OBJECT_NATIVE_INFALLIBLE_METHOD: {
    FREE(vm, ObjectNativeInfallibleMethod, object);
    break;
  }
  case OBJECT_CLOSURE: {
    const ObjectClosure *closure = (ObjectClosure *)object;
    FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues, closure->upvalueCount);
    FREE(vm, ObjectClosure, object);
    break;
  }
  case OBJECT_UPVALUE: {
    FREE(vm, ObjectUpvalue, object);
    break;
  }

  case OBJECT_STATIC_ARRAY: {
    const ObjectStaticArray *staticArray = (ObjectStaticArray *)object;
    FREE_ARRAY(vm, Value, staticArray->values, staticArray->size);
    FREE(vm, ObjectArray, object);
    break;
  }

  case OBJECT_STATIC_TABLE: {
    ObjectStaticTable *staticTable = (ObjectStaticTable *)object;
    freeObjectStaticTable(vm, staticTable);
    FREE(vm, ObjectStaticTable, object);
    break;
  }

  case OBJECT_ARRAY: {
    const ObjectArray *array = (ObjectArray *)object;
    FREE_ARRAY(vm, Value, array->values, array->capacity);
    FREE(vm, ObjectArray, object);
    break;
  }

  case OBJECT_TABLE: {
    ObjectTable *table = (ObjectTable *)object;
    freeObjectTable(vm, table);
    FREE(vm, ObjectTable, object);
    break;
  }

  case OBJECT_ERROR: {
    FREE(vm, ObjectError, object);
    break;
  }

  case OBJECT_RESULT: {
    FREE(vm, ObjectResult, object);
    break;
  }

  case OBJECT_RANDOM: {
    FREE(vm, ObjectRandom, object);
    break;
  }

  case OBJECT_FILE: {
    const ObjectFile *file = (ObjectFile *)object;
    fclose(file->file);
    FREE(vm, ObjectFile, object);
    break;
  }

  case OBJECT_MODULE_RECORD: {
    ObjectModuleRecord *moduleRecord = (ObjectModuleRecord *)object;
    freeObjectModuleRecord(vm, moduleRecord);
    break;
  }

  case OBJECT_STRUCT: {
    ObjectStruct *structure = (ObjectStruct *)object;
    freeTable(vm, &structure->fields);
    FREE(vm, ObjectStruct, object);
    break;
  }

  case OBJECT_STRUCT_INSTANCE: {
    const ObjectStructInstance *instance = (ObjectStructInstance *)object;
    FREE_ARRAY(vm, Value, instance->fields, instance->fieldCount);
    FREE(vm, ObjectStructInstance, object);
    break;
  }

  case OBJECT_VEC2: {
    FREE(vm, ObjectVec2, object);
    break;
  }

  case OBJECT_VEC3: {
    FREE(vm, ObjectVec3, object);
    break;
  }
  }
}

void markModuleRoots(VM *vm, ObjectModuleRecord *moduleRecord) {
  if (moduleRecord->enclosingModule != NULL) {
    markModuleRoots(vm, moduleRecord->enclosingModule);
  }

  markObject(vm, (Object *)moduleRecord->path);
  markTable(vm, &moduleRecord->globals);
  markTable(vm, &moduleRecord->publics);
  markObject(vm, (Object *)moduleRecord->moduleClosure);
  markObject(vm, (Object *)moduleRecord->enclosingModule);

  for (const Value *slot = moduleRecord->stack; slot < moduleRecord->stackTop;
       slot++) {
    markValue(vm, *slot);
  }

  for (int i = 0; i < moduleRecord->frameCount; i++) {
    markObject(vm, (Object *)moduleRecord->frames[i].closure);
  }

  for (ObjectUpvalue *upvalue = moduleRecord->openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject(vm, (Object *)upvalue);
  }

  markObject(vm, (Object *)moduleRecord);
}

void markStructInstanceStack(VM *vm, const StructInstanceStack *stack) {
  for (uint32_t i = 0; i < stack->count; i++) {
    markStructInstance(vm, stack->structs[i]);
  }
}

/**
 * @brief Marks all root objects reachable by the VM.
 *
 * This function marks objects that are considered roots for garbage collection.
 * These roots are directly accessible by the VM and include the stack, call
 * frames, open upvalues, global variables, compiler roots and the init string.
 *
 * @param vm The virtual machine.
 */
void markRoots(VM *vm) {
  if (vm->currentModuleRecord) {
    markModuleRoots(vm, vm->currentModuleRecord);
  }

  for (uint32_t i = 0; i < vm->importStack.count; i++) {
    markObject(vm, (Object *)vm->importStack.paths[i]);
  }

  for (int i = 0; i < vm->nativeModules.count; i++) {
    markTable(vm, vm->nativeModules.modules[i].names);
  }

  markStructInstanceStack(vm, &vm->structInstanceStack);

  markTable(vm, &vm->randomType);
  markTable(vm, &vm->stringType);
  markTable(vm, &vm->arrayType);
  markTable(vm, &vm->errorType);
  markTable(vm, &vm->fileType);
  markTable(vm, &vm->resultType);
  markTable(vm, &vm->vec2Type);
  markTable(vm, &vm->vec3Type);

  markTable(vm, &vm->moduleCache);

  markCompilerRoots(vm);

  markValue(vm, vm->matchHandler.matchBind);
  markValue(vm, vm->matchHandler.matchTarget);
}

/**
 * @brief Traces references from gray objects, blackening them.
 *
 * This function processes the gray stack, taking gray objects and blackening
 * them by calling `blackenObject`. This process continues until the gray stack
 * is empty, ensuring that all reachable objects and their references are
 * marked.
 *
 * @param vm The virtual machine.
 */
static void traceReferences(VM *vm) {
  while (vm->grayCount > 0) {
    Object *object = vm->grayStack[--vm->grayCount];
    blackenObject(vm, object);
  }
}

/**
 * @brief Sweeps unmarked objects, freeing their memory.
 *
 * This function performs the sweep phase of garbage collection. It iterates
 * through the VM's object list, frees any objects that are not marked (i.e.,
 * unreachable), and removes them from the linked list of objects.
 *
 * @param vm The virtual machine.
 */
static void sweep(VM *vm) {
  Object *previous = NULL;
  Object *object = vm->objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Object *unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm->objects = object;
      }
      freeObject(vm, unreached);
    }
  }
}

void collectGarbage(VM *vm) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm->bytesAllocated;
#endif

  markRoots(vm);
  traceReferences(vm);
  tableRemoveWhite(&vm->strings); // Clean up string table
  sweep(vm);
  vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("    collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif
}

void freeObjects(VM *vm) {
  Object *object = vm->objects;
  while (object != NULL) {
    Object *next = object->next;
    freeObject(vm, object);
    object = next;
  }
  free(vm->grayStack);
}
