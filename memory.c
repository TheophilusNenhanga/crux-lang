#include <stdlib.h>

#include "compiler.h"
#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(VM *vm, void *pointer, size_t oldSize, size_t newSize) {
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

	void *result = realloc(pointer, newSize); // When oldSize is zero this is equivalent to malloc
	if (result == NULL)
		exit(1);
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
		vm->grayStack = (Object **) realloc(vm->grayStack, vm->grayCapacity * sizeof(Object *));
	}
	if (vm->grayStack == NULL) {
		exit(1);
	}
	vm->grayStack[vm->grayCount++] = object;
}

void markValue(VM *vm, Value value) {
	if (IS_OBJECT(value)) {
		markObject(vm, AS_OBJECT(value));
	}
}

void markArray(VM *vm, ValueArray *array) {
	for (int i = 0; i < array->count; i++) {
		markValue(vm, array->values[i]);
	}
}

void markObjectArray(VM *vm, ObjectArray *array) {
	for (int i = 0; i < array->size; i++) {
		markValue(vm, array->array[i]);
	}
}

void markObjectTable(VM *vm, ObjectTable *table) {
	for (int i = 0; i < table->capacity; i++) {
		if (table->entries[i].isOccupied) {
			markValue(vm, table->entries[i].value);
			markValue(vm, table->entries[i].key);
		}
	}
}

static void blackenObject(VM *vm, Object *object) {
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *) object);
	printValue(OBJECT_VAL(object));
	printf("\n");
#endif

	switch (object->type) {

		case OBJECT_CLOSURE: {
			ObjectClosure *closure = (ObjectClosure *) object;
			markObject(vm, (Object *) closure->function);
			for (int i = 0; i < closure->upvalueCount; i++) {
				markObject(vm, (Object *) closure->upvalues[i]);
			}
			break;
		}

		case OBJECT_FUNCTION: {
			ObjectFunction *function = (ObjectFunction *) object;
			markObject(vm, (Object *) function->name);
			markArray(vm, &function->chunk.constants);
			break;
		}

		case OBJECT_UPVALUE: {
			markValue(vm, ((ObjectUpvalue *) object)->closed);
			break;
		}

		case OBJECT_CLASS: {
			ObjectClass *klass = (ObjectClass *) object;
			markObject(vm, (Object *) klass->name);
			markTable(vm, &klass->methods);
			break;
		}

		case OBJECT_INSTANCE: {
			ObjectInstance *instance = (ObjectInstance *) object;
			markObject(vm, (Object *) instance->klass);
			markTable(vm, &instance->fields);
			break;
		}

		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod *bound = (ObjectBoundMethod *) object;
			markValue(vm, bound->receiver);
			markObject(vm, (Object *) bound->method);
			break;
		}

		case OBJECT_ARRAY: {
			ObjectArray *array = (ObjectArray *) object;
			markObjectArray(vm, array);
			break;
		}

		case OBJECT_TABLE: {
			ObjectTable *table = (ObjectTable *) object;
			markObjectTable(vm, table);
			break;
		}

		case OBJECT_ERROR: {
			ObjectError *error = (ObjectError *) object;
			markObject(vm, (Object *) error->message);
			break;
		}

		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = (ObjectNativeFunction *) object;
			markObject(vm, (Object *) native->name);
			break;
		}

		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = (ObjectNativeMethod *) object;
			markObject(vm, (Object *) native->name);
			break;
		}

		case OBJECT_MODULE: {
			ObjectModule *module = (ObjectModule *) object;
			markObject(vm, (Object *) module->path);
			break;
		}

		case OBJECT_FILE: {
			ObjectFile *file = (ObjectFile *) object;
			markObject(vm, (Object *) file->path);
			break;
		}

		case OBJECT_STRING: {
			break;
		}

	}
}

static void freeObject(VM *vm, Object *object) {
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void *) object, object->type);
#endif
	switch (object->type) {
		case OBJECT_STRING: {
			ObjectString *string = (ObjectString *) object;
			FREE_ARRAY(vm, char, string->chars, string->length + 1);
			FREE(vm, ObjectString, object);
			break;
		}
		case OBJECT_FUNCTION: {
			ObjectFunction *function = (ObjectFunction *) object;
			freeChunk(vm, &function->chunk);
			FREE(vm, ObjectFunction, object);
			break;
		}
		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = (ObjectNativeFunction *) object;
			FREE(vm, ObjectNativeFunction, object);
			break;
		}
		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = (ObjectNativeMethod *) object;
			FREE(vm, ObjectNativeMethod, object);
			break;
		}
		case OBJECT_CLOSURE: {
			ObjectClosure *closure = (ObjectClosure *) object;
			FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues, closure->upvalueCount);
			FREE(vm, ObjectClosure, object);
			break;
		}
		case OBJECT_UPVALUE: {
			FREE(vm, ObjectUpvalue, object);
			break;
		}
		case OBJECT_CLASS: {
			ObjectClass *klass = (ObjectClass *) object;
			freeTable(vm, &klass->methods);
			FREE(vm, ObjectClass, object);
			break;
		}
		case OBJECT_INSTANCE: {
			ObjectInstance *instance = (ObjectInstance *) object;
			freeTable(vm, &instance->fields);
			FREE(vm, ObjectInstance, object);
			break;
		}
		case OBJECT_BOUND_METHOD: {
			FREE(vm, ObjectBoundMethod, object);
			break;
		}

		case OBJECT_ARRAY: {
			ObjectArray *array = (ObjectArray *) object;
			FREE_ARRAY(vm, Value, array->array, array->capacity);
			FREE(vm, ObjectArray, object);
			break;
		}

		case OBJECT_TABLE: {
			ObjectTable *table = (ObjectTable *) object;
			freeObjectTable(vm, table);
			FREE(vm, ObjectTable, object);
			break;
		}

		case OBJECT_ERROR: {
			FREE(vm, ObjectError, object);
			break;
		}

		case OBJECT_MODULE: {
			ObjectModule *module = (ObjectModule *) object;
			freeImportSet(vm, &module->importedModules);
			FREE(vm, ObjectModule, object);
			break;
		}
		case OBJECT_FILE: {
			FREE(vm, ObjectFile, object);
			break;
		}

	}
}

void markRoots(VM *vm) {
	for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
		markValue(vm, *slot);
	}

	for (int i = 0; i < vm->frameCount; i++) {
		markObject(vm, (Object *) vm->frames[i].closure);
	}

	for (ObjectUpvalue *upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		markObject(vm, (Object *) upvalue);
	}

	markTable(vm, &vm->globals);
	markCompilerRoots();
	markObject(vm, (Object *) vm->initString);
}

static void traceReferences(VM *vm) {
	while (vm->grayCount > 0) {
		Object *object = vm->grayStack[--vm->grayCount];
		blackenObject(vm, object);
	}
}

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
	tableRemoveWhite(&vm->strings);
	sweep(vm);
	vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
	printf("    collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm->bytesAllocated, before,
				 vm->bytesAllocated, vm->nextGC);
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
