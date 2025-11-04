#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "value.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 1.5

void *allocate_object_with_gc(VM *vm, const size_t size)
{
	vm->bytes_allocated += size;
	if (vm->bytes_allocated > vm->next_gc) {
		collect_garbage(vm);
	}
	void *result = malloc(size);
	if (result == NULL) {
		fprintf(stderr,
			"Fatal error - Out of Memory: Failed to reallocate %zu "
			"bytes.\n "
			"Exiting!",
			size);
		exit(1);
	}
	return result;
}

void *allocate_object_without_gc(const size_t size)
{
	void *result = malloc(size);
	if (result == NULL) {
		fprintf(stderr,
			"Fatal error - Out of Memory: Failed to allocate %zu "
			"bytes.\n"
			"Exiting!",
			size);
		exit(1);
	}
	return result;
}

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
			"Fatal error - Out of Memory: Failed to reallocate %zu "
			"bytes.\n "
			"Exiting!",
			newSize);
		exit(1);
	}

	return result;
}

void mark_object(VM *vm, CruxObject *object)
{
	if (object == NULL)
		return;

	PoolObject *pool_object = &vm->object_pool->objects[object->pool_index];

	if (pool_object == NULL)
		return;
	if (IS_MARKED(pool_object))
		return;
	SET_MARKED(pool_object, true);

	if (vm->gray_capacity < vm->gray_count + 1) {
		vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
		vm->gray_stack = (CruxObject **)
			realloc(vm->gray_stack,
				vm->gray_capacity * sizeof(CruxObject *));
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
	mark_object(vm, (CruxObject *)structure->name);
	mark_table(vm, &structure->fields);
	mark_object(vm, (CruxObject *)structure);
}

static void mark_struct_instance(VM *vm, ObjectStructInstance *instance)
{
	for (int i = 0; i < instance->field_count; i++) {
		mark_value(vm, instance->fields[i]);
	}
	mark_object_struct(vm, instance->struct_type);
	mark_object(vm, (CruxObject *)instance);
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
typedef void (*BlackenFunction)(VM *vm, CruxObject *object);
typedef void (*FreeFunction)(VM *vm, CruxObject *object);

static void blacken_closure(VM *vm, CruxObject *object);
static void blacken_function(VM *vm, CruxObject *object);
static void blacken_upvalue(VM *vm, CruxObject *object);
static void blacken_array(VM *vm, CruxObject *object);
static void blacken_table(VM *vm, CruxObject *object);
static void blacken_error(VM *vm, CruxObject *object);
static void blacken_native_function(VM *vm, CruxObject *object);
static void blacken_native_method(VM *vm, CruxObject *object);
static void blacken_native_infallible_function(VM *vm, CruxObject *object);
static void blacken_native_infallible_method(VM *vm, CruxObject *object);
static void blacken_result(VM *vm, CruxObject *object);
static void blacken_random(VM *vm, CruxObject *object);
static void blacken_file(VM *vm, CruxObject *object);
static void blacken_module_record(VM *vm, CruxObject *object);
static void blacken_struct(VM *vm, CruxObject *object);
static void blacken_struct_instance(VM *vm, CruxObject *object);
static void blacken_vector(VM *vm, CruxObject *object);
static void blacken_string(VM *vm, CruxObject *object);

static const BlackenFunction blacken_dispatch[] = {
	[OBJECT_STRING] = blacken_string,
	[OBJECT_FUNCTION] = blacken_function,
	[OBJECT_NATIVE_FUNCTION] = blacken_native_function,
	[OBJECT_NATIVE_METHOD] = blacken_native_method,
	[OBJECT_CLOSURE] = blacken_closure,
	[OBJECT_UPVALUE] = blacken_upvalue,
	[OBJECT_ARRAY] = blacken_array,
	[OBJECT_TABLE] = blacken_table,
	[OBJECT_ERROR] = blacken_error,
	[OBJECT_RESULT] = blacken_result,
	[OBJECT_NATIVE_INFALLIBLE_FUNCTION] =
		blacken_native_infallible_function,
	[OBJECT_NATIVE_INFALLIBLE_METHOD] = blacken_native_infallible_method,
	[OBJECT_RANDOM] = blacken_random,
	[OBJECT_FILE] = blacken_file,
	[OBJECT_MODULE_RECORD] = blacken_module_record,
	[OBJECT_STRUCT] = blacken_struct,
	[OBJECT_STRUCT_INSTANCE] = blacken_struct_instance,
	[OBJECT_VECTOR] = blacken_vector,
};

static void blacken_object(VM *vm, CruxObject *object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *)object);
	print_value(OBJECT_VAL(object), false);
	printf("\n");
#endif

	const ObjectType type = OBJECT_TYPE(OBJECT_VAL(object));
	if (type < (ObjectType)(sizeof(blacken_dispatch) /
				sizeof(blacken_dispatch[0]))) {
		blacken_dispatch[type](vm, object);
	}
}

static void blacken_closure(VM *vm, CruxObject *object)
{
	const ObjectClosure *closure = (ObjectClosure *)object;
	mark_object(vm, (CruxObject *)closure->function);
	for (int i = 0; i < closure->upvalue_count; i++) {
		mark_object(vm, (CruxObject *)closure->upvalues[i]);
	}
}

static void blacken_function(VM *vm, CruxObject *object)
{
	const ObjectFunction *function = (ObjectFunction *)object;
	mark_object(vm, (CruxObject *)function->name);
	mark_object(vm, (CruxObject *)function->module_record);
	mark_array(vm, &function->chunk.constants);
}

static void blacken_upvalue(VM *vm, CruxObject *object)
{
	mark_value(vm, ((ObjectUpvalue *)object)->closed);
}


static void blacken_array(VM *vm, CruxObject *object)
{
	const ObjectArray *array = (ObjectArray *)object;
	mark_object_array(vm, array->values, array->size);
}

static void blacken_table(VM *vm, CruxObject *object)
{
	const ObjectTable *table = (ObjectTable *)object;
	mark_object_table(vm, table->entries, table->capacity);
}

static void blacken_error(VM *vm, CruxObject *object)
{
	const ObjectError *error = (ObjectError *)object;
	mark_object(vm, (CruxObject *)error->message);
}

static void blacken_native_function(VM *vm, CruxObject *object)
{
	const ObjectNativeFunction *native = (ObjectNativeFunction *)object;
	mark_object(vm, (CruxObject *)native->name);
}

static void blacken_native_method(VM *vm, CruxObject *object)
{
	const ObjectNativeMethod *native = (ObjectNativeMethod *)object;
	mark_object(vm, (CruxObject *)native->name);
}

static void blacken_native_infallible_function(VM *vm, CruxObject *object)
{
	const ObjectNativeInfallibleFunction *native =
		(ObjectNativeInfallibleFunction *)object;
	mark_object(vm, (CruxObject *)native->name);
}

static void blacken_native_infallible_method(VM *vm, CruxObject *object)
{
	const ObjectNativeInfallibleMethod *native =
		(ObjectNativeInfallibleMethod *)object;
	mark_object(vm, (CruxObject *)native->name);
}

static void blacken_result(VM *vm, CruxObject *object)
{
	const ObjectResult *result = (ObjectResult *)object;
	if (result->is_ok) {
		mark_value(vm, result->as.value);
	} else {
		mark_object(vm, (CruxObject *)result->as.error);
	}
}

static void blacken_random(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_file(VM *vm, CruxObject *object)
{
	const ObjectFile *file = (ObjectFile *)object;
	mark_object(vm, (CruxObject *)file->path);
	mark_object(vm, (CruxObject *)file->mode);
}

static void blacken_module_record(VM *vm, CruxObject *object)
{
	const ObjectModuleRecord *module = (ObjectModuleRecord *)object;
	mark_object(vm, (CruxObject *)module->path);
	mark_table(vm, &module->globals);
	mark_table(vm, &module->publics);
	mark_object(vm, (CruxObject *)module->module_closure);
	mark_object(vm, (CruxObject *)module->enclosing_module); // Can be NULL

	for (const Value *slot = module->stack; slot < module->stack_top;
	     slot++) {
		mark_value(vm, *slot);
	}
	for (int i = 0; i < module->frame_count; i++) {
		mark_object(vm, (CruxObject *)module->frames[i].closure);
	}
	for (ObjectUpvalue *upvalue = module->open_upvalues; upvalue != NULL;
	     upvalue = upvalue->next) {
		mark_object(vm, (CruxObject *)upvalue);
	}
}

static void blacken_struct(VM *vm, CruxObject *object)
{
	ObjectStruct *structure = (ObjectStruct *)object;
	mark_object_struct(vm, structure);
}

static void blacken_struct_instance(VM *vm, CruxObject *object)
{
	ObjectStructInstance *instance = (ObjectStructInstance *)object;
	mark_struct_instance(vm, instance);
}

static void blacken_vector(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_string(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
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

static void free_object_string(VM *vm, CruxObject *object);
static void free_object_function(VM *vm, CruxObject *object);
static void free_object_native_function(VM *vm, CruxObject *object);
static void free_object_native_method(VM *vm, CruxObject *object);
static void free_object_native_infallible_function(VM *vm, CruxObject *object);
static void free_object_native_infallible_method(VM *vm, CruxObject *object);
static void free_object_closure(VM *vm, CruxObject *object);
static void free_object_upvalue(VM *vm, CruxObject *object);
static void free_object_array(VM *vm, CruxObject *object);
static void free_object_table_wrapper(VM *vm, CruxObject *object);
static void free_object_error(VM *vm, CruxObject *object);
static void free_object_result(VM *vm, CruxObject *object);
static void free_object_random(VM *vm, CruxObject *object);
static void free_object_file(VM *vm, CruxObject *object);
static void free_object_module_record_wrapper(VM *vm, CruxObject *object);
static void free_object_struct(VM *vm, CruxObject *object);
static void free_object_struct_instance(VM *vm, CruxObject *object);
static void free_object_vector(VM *vm, CruxObject *object);

static const FreeFunction free_dispatch[] = {
	[OBJECT_STRING] = free_object_string,
	[OBJECT_FUNCTION] = free_object_function,
	[OBJECT_NATIVE_FUNCTION] = free_object_native_function,
	[OBJECT_NATIVE_METHOD] = free_object_native_method,
	[OBJECT_CLOSURE] = free_object_closure,
	[OBJECT_UPVALUE] = free_object_upvalue,
	[OBJECT_ARRAY] = free_object_array,
	[OBJECT_TABLE] = free_object_table_wrapper,
	[OBJECT_ERROR] = free_object_error,
	[OBJECT_RESULT] = free_object_result,
	[OBJECT_NATIVE_INFALLIBLE_FUNCTION] =
		free_object_native_infallible_function,
	[OBJECT_NATIVE_INFALLIBLE_METHOD] =
		free_object_native_infallible_method,
	[OBJECT_RANDOM] = free_object_random,
	[OBJECT_FILE] = free_object_file,
	[OBJECT_MODULE_RECORD] = free_object_module_record_wrapper,
	[OBJECT_STRUCT] = free_object_struct,
	[OBJECT_STRUCT_INSTANCE] = free_object_struct_instance,
	[OBJECT_VECTOR] = free_object_vector
};

static void free_object(VM *vm, CruxObject *object)
{
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void *)object,
	       OBJECT_TYPE(OBJECT_VAL(object)));
#endif
	if (object == NULL) {
		return;
	}

	const uint32_t index = object->pool_index;
	ObjectPool *pool = vm->object_pool;

	if (index >= pool->capacity) {
		fprintf(stderr, "Error: Invalid pool index in free_object\n");
		return;
	}

	const ObjectType type = object->type;
	if (type <
	    (ObjectType)(sizeof(free_dispatch) / sizeof(free_dispatch[0]))) {
		free_dispatch[type](vm, object);
	}

	PoolObject* pool_object = &pool->objects[index];

	SET_DATA(pool_object, NULL);
	SET_MARKED(pool_object, false);

	pool->free_list[pool->free_top++] = index;
	pool->count--;
}

static void free_object_string(VM *vm, CruxObject *object)
{
	const ObjectString *string = (ObjectString *)object;
	FREE_ARRAY(vm, char, string->chars, string->length + 1);
	FREE(vm, ObjectString, object);
}

static void free_object_function(VM *vm, CruxObject *object)
{
	ObjectFunction *function = (ObjectFunction *)object;
	free_chunk(vm, &function->chunk);
	FREE(vm, ObjectFunction, object);
}

static void free_object_native_function(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectNativeFunction, object);
}

static void free_object_native_method(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectNativeMethod, object);
}

static void free_object_native_infallible_function(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectNativeInfallibleFunction, object);
}

static void free_object_native_infallible_method(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectNativeInfallibleMethod, object);
}

static void free_object_closure(VM *vm, CruxObject *object)
{
	const ObjectClosure *closure = (ObjectClosure *)object;
	FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues,
		   closure->upvalue_count);
	FREE(vm, ObjectClosure, object);
}

static void free_object_upvalue(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectUpvalue, object);
}


static void free_object_array(VM *vm, CruxObject *object)
{
	const ObjectArray *array = (ObjectArray *)object;
	FREE_ARRAY(vm, Value, array->values, array->capacity);
	FREE(vm, ObjectArray, object);
}

static void free_object_table_wrapper(VM *vm, CruxObject *object)
{
	ObjectTable *table = (ObjectTable *)object;
	free_object_table(vm, table);
	FREE(vm, ObjectTable, object);
}

static void free_object_error(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectError, object);
}

static void free_object_result(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectResult, object);
}

static void free_object_random(VM *vm, CruxObject *object)
{
	FREE(vm, ObjectRandom, object);
}

static void free_object_file(VM *vm, CruxObject *object)
{
	const ObjectFile *file = (ObjectFile *)object;
	if (file->file != NULL) {
		fclose(file->file);
	}
	FREE(vm, ObjectFile, object);
}

static void free_object_module_record_wrapper(VM *vm, CruxObject *object)
{
	ObjectModuleRecord *moduleRecord = (ObjectModuleRecord *)object;
	// First free the internal data
	free_object_module_record(vm, moduleRecord);
	// Then free the object itself
	FREE(vm, ObjectModuleRecord, object);
}

static void free_object_struct(VM *vm, CruxObject *object)
{
	ObjectStruct *structure = (ObjectStruct *)object;
	free_table(vm, &structure->fields);
	FREE(vm, ObjectStruct, object);
}

static void free_object_struct_instance(VM *vm, CruxObject *object)
{
	const ObjectStructInstance *instance = (ObjectStructInstance *)object;
	FREE_ARRAY(vm, Value, instance->fields, instance->field_count);
	FREE(vm, ObjectStructInstance, object);
}

static void free_object_vector(VM *vm, CruxObject *object)
{
	const ObjectVector *vector = (ObjectVector *)object;
	if (vector->dimensions > 4) {
		FREE(vm, double, vector->as.h_components);
	}
	FREE(vm, ObjectVector, object);
}

void mark_module_roots(VM *vm, ObjectModuleRecord *moduleRecord)
{
	if (moduleRecord->enclosing_module != NULL) {
		mark_module_roots(vm, moduleRecord->enclosing_module);
	}

	mark_object(vm, (CruxObject *)moduleRecord->path);
	mark_table(vm, &moduleRecord->globals);
	mark_table(vm, &moduleRecord->publics);
	mark_object(vm, (CruxObject *)moduleRecord->module_closure);
	mark_object(vm, (CruxObject *)moduleRecord->enclosing_module);

	for (const Value *slot = moduleRecord->stack;
	     slot < moduleRecord->stack_top; slot++) {
		mark_value(vm, *slot);
	}

	for (int i = 0; i < moduleRecord->frame_count; i++) {
		mark_object(vm, (CruxObject *)moduleRecord->frames[i].closure);
	}

	for (ObjectUpvalue *upvalue = moduleRecord->open_upvalues;
	     upvalue != NULL; upvalue = upvalue->next) {
		mark_object(vm, (CruxObject *)upvalue);
	}

	mark_object(vm, (CruxObject *)moduleRecord);
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
	if (vm->current_module_record) {
		mark_module_roots(vm, vm->current_module_record);
	}

	for (uint32_t i = 0; i < vm->import_stack.count; i++) {
		mark_object(vm, (CruxObject *)vm->import_stack.paths[i]);
	}

	if (vm->native_modules.modules != NULL) {
		for (int i = 0; i < vm->native_modules.count; i++) {
			mark_table(vm, vm->native_modules.modules[i].names);
			mark_object(vm,
				    (CruxObject *)vm->native_modules.modules[i]
					    .name);
		}
	}

	mark_table(vm, &vm->module_cache);

	mark_table(vm, &vm->random_type);
	mark_table(vm, &vm->string_type);
	mark_table(vm, &vm->array_type);
	mark_table(vm, &vm->table_type);
	mark_table(vm, &vm->error_type);
	mark_table(vm, &vm->file_type);
	mark_table(vm, &vm->result_type);
	mark_table(vm, &vm->vector_type);

	mark_struct_instance_stack(vm, &vm->struct_instance_stack);

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
		CruxObject *object = vm->gray_stack[--vm->gray_count];
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
	const ObjectPool *pool = vm->object_pool;
	for (size_t i = 0; i < pool->capacity; i++) {
		PoolObject *pool_object = &pool->objects[i];

		if (GET_DATA(pool_object) == NULL)
			continue;

		if (IS_MARKED(pool_object)) {
			SET_MARKED(pool_object, false);
		} else {
			free_object(vm, GET_DATA(pool_object));
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
	table_remove_white(vm, &vm->strings); // Clean up string table
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
	const ObjectPool *pool = vm->object_pool;
	for (uint32_t i = 0; i < pool->capacity; i++) {
		const PoolObject *pool_object = &pool->objects[i];
		if (GET_DATA(pool_object) != NULL) {
			free_object(vm, GET_DATA(pool_object));
		}
	}
	free(vm->gray_stack);
}
