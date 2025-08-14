#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "value.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 1.5

void *reallocate(VM *vm, void *pointer, const size_t oldSize,
		 const size_t newSize)
{
	vm->bytes_allocated += newSize - oldSize;
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collect_garbage(vm);
#endif
		if (vm->bytes_allocated > vm->next_gc) {
			collect_garbage(vm);
		}
	}

	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, newSize);
	if (result == NULL) {
		fprintf(stderr,
			"Fatal error: Failed to reallocate %zu bytes.\n "
			"Exiting!",
			newSize);
		exit(1);
	}

	return result;
}

void mark_object(VM *vm, Object *object)
{
	if (object == NULL)
		return;
	if (object->is_marked)
		return;
	object->is_marked = true;

	if (vm->gray_capacity < vm->gray_count + 1) {
		vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
		vm->gray_stack = (Object **)realloc(vm->gray_stack,
						   vm->gray_capacity *
							   sizeof(Object *));
	}
	if (vm->gray_stack == NULL) {
		exit(1);
	}
	vm->gray_stack[vm->gray_count++] = object;
}

void mark_value(VM *vm, const Value value)
{
	if (IS_CRUX_OBJECT(value)) {
		mark_object(vm, AS_CRUX_OBJECT(value));
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
void mark_array(VM *vm, const ValueArray *array)
{
	for (int i = 0; i < array->count; i++) {
		mark_value(vm, array->values[i]);
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
void mark_object_array(VM *vm, const Value *values, const uint32_t size)
{
	for (uint32_t i = 0; i < size; i++) {
		mark_value(vm, values[i]);
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
void mark_object_table(VM *vm, const ObjectTableEntry *entries,
		     const uint32_t capacity)
{
	for (uint32_t i = 0; i < capacity; i++) {
		if (entries[i].is_occupied) {
			mark_value(vm, entries[i].value);
			mark_value(vm, entries[i].key);
		}
	}
}

static void mark_object_struct(VM *vm, ObjectStruct *structure)
{
	mark_object(vm, (Object *)structure->name);
	mark_table(vm, &structure->fields);
	structure->object.is_marked = true;
}

static void mark_struct_instance(VM *vm, ObjectStructInstance *instance)
{
	for (int i = 0; i < instance->field_count; i++) {
		mark_value(vm, instance->fields[i]);
	}
	mark_object_struct(vm, instance->struct_type);
	instance->object.is_marked = true;
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
static void blacken_object(VM *vm, Object *object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *)object);
	print_value(OBJECT_VAL(object), false);
	printf("\n");
#endif

	switch (object->type) {
	case OBJECT_CLOSURE: {
		const ObjectClosure *closure = (ObjectClosure *)object;
		mark_object(vm, (Object *)closure->function);
		for (int i = 0; i < closure->upvalue_count; i++) {
			mark_object(vm, (Object *)closure->upvalues[i]);
		}
		break;
	}

	case OBJECT_FUNCTION: {
		const ObjectFunction *function = (ObjectFunction *)object;
		mark_object(vm, (Object *)function->name);
		mark_object(vm, (Object *)function->module_record);
		mark_array(vm, &function->chunk.constants);
		break;
	}

	case OBJECT_UPVALUE: {
		mark_value(vm, ((ObjectUpvalue *)object)->closed);
		break;
	}

	case OBJECT_STATIC_ARRAY: {
		const ObjectStaticArray *staticArray = (ObjectStaticArray *)
			object;
		mark_object_array(vm, staticArray->values, staticArray->size);
		break;
	}
	case OBJECT_ARRAY: {
		const ObjectArray *array = (ObjectArray *)object;
		mark_object_array(vm, array->values, array->size);
		break;
	}

	case OBJECT_STATIC_TABLE: {
		const ObjectStaticTable *table = (ObjectStaticTable *)object;
		mark_object_table(vm, table->entries, table->size);
		break;
	}
	case OBJECT_TABLE: {
		const ObjectTable *table = (ObjectTable *)object;
		mark_object_table(vm, table->entries, table->capacity);
		break;
	}

	case OBJECT_ERROR: {
		const ObjectError *error = (ObjectError *)object;
		mark_object(vm, (Object *)error->message);
		break;
	}

	case OBJECT_NATIVE_FUNCTION: {
		const ObjectNativeFunction *native = (ObjectNativeFunction *)
			object;
		mark_object(vm, (Object *)native->name);
		break;
	}

	case OBJECT_NATIVE_METHOD: {
		const ObjectNativeMethod *native = (ObjectNativeMethod *)object;
		mark_object(vm, (Object *)native->name);
		break;
	}

	case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
		const ObjectNativeInfallibleFunction *native =
			(ObjectNativeInfallibleFunction *)object;
		mark_object(vm, (Object *)native->name);
		break;
	}

	case OBJECT_NATIVE_INFALLIBLE_METHOD: {
		const ObjectNativeInfallibleMethod *native =
			(ObjectNativeInfallibleMethod *)object;
		mark_object(vm, (Object *)native->name);
		break;
	}

	case OBJECT_RESULT: {
		const ObjectResult *result = (ObjectResult *)object;
		if (result->is_ok) {
			mark_value(vm, result->as.value);
		} else {
			mark_object(vm, (Object *)result->as.error);
		}
		break;
	}

	case OBJECT_RANDOM: {
		break;
	}

	case OBJECT_FILE: {
		const ObjectFile *file = (ObjectFile *)object;
		mark_object(vm, (Object *)file->path);
		mark_object(vm, (Object *)file->mode);
		break;
	}

	case OBJECT_MODULE_RECORD: {
		const ObjectModuleRecord *module = (ObjectModuleRecord *)object;
		mark_object(vm, (Object *)module->path);
		mark_table(vm, &module->globals);
		mark_table(vm, &module->publics);
		mark_object(vm, (Object *)module->module_closure);
		mark_object(vm,
			   (Object *)module->enclosing_module); // Can be NULL

		for (const Value *slot = module->stack; slot < module->stack_top;
		     slot++) {
			mark_value(vm, *slot);
		}
		for (int i = 0; i < module->frame_count; i++) {
			mark_object(vm, (Object *)module->frames[i].closure);
		}
		for (ObjectUpvalue *upvalue = module->open_upvalues;
		     upvalue != NULL; upvalue = upvalue->next) {
			mark_object(vm, (Object *)upvalue);
		}
		break;
	}

	case OBJECT_STRUCT: {
		ObjectStruct *structure = (ObjectStruct *)object;
		mark_object_struct(vm, structure);
		break;
	}

	case OBJECT_STRUCT_INSTANCE: {
		ObjectStructInstance *instance = (ObjectStructInstance *)object;
		mark_struct_instance(vm, instance);
		break;
	}

	case OBJECT_VEC2:
	case OBJECT_VEC3:
	case OBJECT_STRING: {
		// Strings are primitives in terms of GC reachability in this
		// implementation
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
static void free_object(VM *vm, Object *object)
{
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
		free_chunk(vm, &function->chunk);
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
		FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues,
			   closure->upvalue_count);
		FREE(vm, ObjectClosure, object);
		break;
	}
	case OBJECT_UPVALUE: {
		FREE(vm, ObjectUpvalue, object);
		break;
	}

	case OBJECT_STATIC_ARRAY: {
		const ObjectStaticArray *staticArray = (ObjectStaticArray *)
			object;
		FREE_ARRAY(vm, Value, staticArray->values, staticArray->size);
		FREE(vm, ObjectStaticArray, object);
		break;
	}

	case OBJECT_STATIC_TABLE: {
		ObjectStaticTable *staticTable = (ObjectStaticTable *)object;
		free_object_static_table(vm, staticTable);
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
		free_object_table(vm, table);
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
		free_object_module_record(vm, moduleRecord);
		break;
	}

	case OBJECT_STRUCT: {
		ObjectStruct *structure = (ObjectStruct *)object;
		free_table(vm, &structure->fields);
		FREE(vm, ObjectStruct, object);
		break;
	}

	case OBJECT_STRUCT_INSTANCE: {
		const ObjectStructInstance *instance = (ObjectStructInstance *)
			object;
		FREE_ARRAY(vm, Value, instance->fields, instance->field_count);
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

void mark_module_roots(VM *vm, ObjectModuleRecord *moduleRecord)
{
	if (moduleRecord->enclosing_module != NULL) {
		mark_module_roots(vm, moduleRecord->enclosing_module);
	}

	mark_object(vm, (Object *)moduleRecord->path);
	mark_table(vm, &moduleRecord->globals);
	mark_table(vm, &moduleRecord->publics);
	mark_object(vm, (Object *)moduleRecord->module_closure);
	mark_object(vm, (Object *)moduleRecord->enclosing_module);

	for (const Value *slot = moduleRecord->stack;
	     slot < moduleRecord->stack_top; slot++) {
		mark_value(vm, *slot);
	}

	for (int i = 0; i < moduleRecord->frame_count; i++) {
		mark_object(vm, (Object *)moduleRecord->frames[i].closure);
	}

	for (ObjectUpvalue *upvalue = moduleRecord->open_upvalues;
	     upvalue != NULL; upvalue = upvalue->next) {
		mark_object(vm, (Object *)upvalue);
	}

	mark_object(vm, (Object *)moduleRecord);
}

void mark_struct_instance_stack(VM *vm, const StructInstanceStack *stack)
{
	if (stack->structs != NULL) {
		for (uint32_t i = 0; i < stack->count; i++) {
			mark_struct_instance(vm, stack->structs[i]);
		}
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
void mark_roots(VM *vm)
{
	if (vm->currentModuleRecord) {
		mark_module_roots(vm, vm->currentModuleRecord);
	}

	for (uint32_t i = 0; i < vm->import_stack.count; i++) {
		mark_object(vm, (Object *)vm->import_stack.paths[i]);
	}

	if (vm->native_modules.modules != NULL) {
		for (int i = 0; i < vm->native_modules.count; i++) {
			mark_table(vm, vm->native_modules.modules[i].names);
			mark_object(vm,
				   (Object *)vm->native_modules.modules[i].name);
		}
	}

	mark_struct_instance_stack(vm, &vm->struct_instance_stack);

	mark_table(vm, &vm->random_type);
	mark_table(vm, &vm->string_type);
	mark_table(vm, &vm->array_type);
	mark_table(vm, &vm->error_type);
	mark_table(vm, &vm->file_type);
	mark_table(vm, &vm->result_type);
	mark_table(vm, &vm->vec2_type);
	mark_table(vm, &vm->vec3_type);

	mark_table(vm, &vm->module_cache);

	mark_compiler_roots(vm);

	mark_value(vm, vm->match_handler.match_bind);
	mark_value(vm, vm->match_handler.match_target);
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
static void trace_references(VM *vm)
{
	while (vm->gray_count > 0) {
		Object *object = vm->gray_stack[--vm->gray_count];
		blacken_object(vm, object);
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
static void sweep(VM *vm)
{
	Object *previous = NULL;
	Object *object = vm->objects;
	while (object != NULL) {
		if (object->is_marked) {
			object->is_marked = false;
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
			free_object(vm, unreached);
		}
	}
}

void collect_garbage(VM *vm)
{
	if (vm->gc_status == PAUSED)
		return;
#ifdef DEBUG_LOG_GC
	printf("--- gc begin ---\n");
	const size_t before = vm->bytes_allocated;
#endif

	mark_roots(vm);
	trace_references(vm);
	table_remove_white(&vm->strings); // Clean up string table
	sweep(vm);
	vm->next_gc = vm->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
	printf("--- gc end ---\n");
	printf("    collected %zu bytes (from %zu to %zu) next at %zu\n",
	       before - vm->bytes_allocated, before, vm->bytes_allocated,
	       vm->next_gc);
#endif
}

void free_objects(VM *vm)
{
	Object *object = vm->objects;
	while (object != NULL) {
		Object *next = object->next;
		free_object(vm, object);
		object = next;
	}
	free(vm->gray_stack);
}
