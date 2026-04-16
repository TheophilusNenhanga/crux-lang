#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "alloc.h"
#include "common.h"
#include "compiler.h"
#include "garbage_collector.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

static uint64_t gc_now_ns(void)
{
#ifdef _WIN32
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;

	// Get the number of ticks per second
	QueryPerformanceFrequency(&frequency);
	// Get the current tick count
	QueryPerformanceCounter(&counter);

	// Convert to nanoseconds: (ticks * 1,000,000,000) / frequency
	return (counter.QuadPart * 1000000000LL) / frequency.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static size_t table_tombstone_count(const Table *table)
{
	size_t tombstones = 0;
	if (!table || !table->entries)
		return 0;

	for (int i = 0; i < table->capacity; i++) {
		const Entry *entry = &table->entries[i];
		if (entry->key == NULL && !IS_NIL(entry->value)) {
			tombstones++;
		}
	}
	return tombstones;
}

static size_t compute_next_gc_threshold(const VM *vm)
{
	const size_t growth_target = (size_t)((double)vm->bytes_allocated * vm->heap_growth_factor);
	const size_t delta_target = vm->bytes_allocated + vm->min_gc_growth_delta;

	size_t next_gc = growth_target > delta_target ? growth_target : delta_target;
	if (next_gc < vm->min_gc_heap_size) {
		next_gc = vm->min_gc_heap_size;
	}
	return next_gc;
}

void mark_object_internal(VM *vm, CruxObject *object)
{
	object->is_marked = true;

	if (vm->gray_capacity < vm->gray_count + 1) {
		vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
		CruxObject **new_objects = realloc(vm->gray_stack, vm->gray_capacity * sizeof(CruxObject *));
		if (new_objects == NULL)
			exit(1);
		vm->gray_stack = new_objects;
	}

	vm->gray_stack[vm->gray_count++] = object;

	if ((uint32_t)vm->gray_count > vm->gc_last_gray_peak)
		vm->gc_last_gray_peak = (uint32_t)vm->gray_count;
	if ((uint32_t)vm->gray_count > vm->gc_max_gray_peak)
		vm->gc_max_gray_peak = (uint32_t)vm->gray_count;
}

void mark_value(VM *vm, const Value value)
{
	if (IS_CRUX_OBJECT(value)) {
		mark_object(vm, AS_CRUX_OBJECT(value));
	}
}

void mark_array(VM *vm, const ValueArray *array)
{
	for (int i = 0; i < array->count; i++) {
		mark_value(vm, array->values[i]);
	}
}

void mark_object_array(VM *vm, const Value *values, const uint32_t size)
{
	for (uint32_t i = 0; i < size; i++) {
		mark_value(vm, values[i]);
	}
}

void mark_object_table(VM *vm, const ObjectTableEntry *entries, const uint32_t capacity)
{
	if (!entries)
		return;
	for (uint32_t i = 0; i < capacity; i++) {
		if (entries[i].is_occupied) {
			mark_value(vm, entries[i].value);
			mark_value(vm, entries[i].key);
		}
	}
}

void mark_type_table(VM *vm, ObjectTypeTable *table)
{
	if (!table)
		return;
	mark_object(vm, (CruxObject *)table);
}

void mark_type_record(VM *vm, ObjectTypeRecord *rec)
{
	if (!rec)
		return;
	mark_object(vm, (CruxObject *)rec);
}

static void mark_object_struct(VM *vm, ObjectStruct *structure)
{
	mark_object(vm, (CruxObject *)structure->name);
	mark_table(vm, &structure->fields);
	mark_table(vm, &structure->methods);
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

typedef void (*BlackenFunction)(VM *vm, CruxObject *object);
typedef void (*FreeFunction)(VM *vm, CruxObject *object);

static void blacken_closure(VM *vm, CruxObject *object);
static void blacken_function(VM *vm, CruxObject *object);
static void blacken_upvalue(VM *vm, CruxObject *object);
static void blacken_array(VM *vm, CruxObject *object);
static void blacken_table(VM *vm, CruxObject *object);
static void blacken_error(VM *vm, CruxObject *object);
static void blacken_native_callable(VM *vm, CruxObject *object);
static void blacken_result(VM *vm, CruxObject *object);
static void blacken_random(VM *vm, CruxObject *object);
static void blacken_file(VM *vm, CruxObject *object);
static void blacken_module_record(VM *vm, CruxObject *object);
static void blacken_struct(VM *vm, CruxObject *object);
static void blacken_struct_instance(VM *vm, CruxObject *object);
static void blacken_vector(VM *vm, CruxObject *object);
static void blacken_complex(VM *vm, CruxObject *object);
static void blacken_string(VM *vm, CruxObject *object);
static void blacken_range(VM *vm, CruxObject *object);
static void blacken_iterator(VM *vm, CruxObject *object);
static void blacken_set(VM *vm, CruxObject *object);
static void blacken_buffer(VM *vm, CruxObject *object);
static void blacken_tuple(VM *vm, CruxObject *object);
static void blacken_matrix(VM *vm, CruxObject *object);
static void blacken_type_record(VM *vm, CruxObject *object);
static void blacken_type_table(VM *vm, CruxObject *object);
static void blacken_option(VM *vm, CruxObject *object);

static const BlackenFunction blacken_dispatch[] = {
	[OBJECT_STRING] = blacken_string,
	[OBJECT_FUNCTION] = blacken_function,
	[OBJECT_NATIVE_CALLABLE] = blacken_native_callable,
	[OBJECT_CLOSURE] = blacken_closure,
	[OBJECT_UPVALUE] = blacken_upvalue,
	[OBJECT_ARRAY] = blacken_array,
	[OBJECT_TABLE] = blacken_table,
	[OBJECT_ERROR] = blacken_error,
	[OBJECT_RESULT] = blacken_result,
	[OBJECT_OPTION] = blacken_option,
	[OBJECT_RANDOM] = blacken_random,
	[OBJECT_FILE] = blacken_file,
	[OBJECT_MODULE_RECORD] = blacken_module_record,
	[OBJECT_STRUCT] = blacken_struct,
	[OBJECT_STRUCT_INSTANCE] = blacken_struct_instance,
	[OBJECT_VECTOR] = blacken_vector,
	[OBJECT_COMPLEX] = blacken_complex,
	[OBJECT_RANGE] = blacken_range,
	[OBJECT_ITERATOR] = blacken_iterator,
	[OBJECT_SET] = blacken_set,
	[OBJECT_BUFFER] = blacken_buffer,
	[OBJECT_TUPLE] = blacken_tuple,
	[OBJECT_MATRIX] = blacken_matrix,
	[OBJECT_TYPE_RECORD] = blacken_type_record,
	[OBJECT_TYPE_TABLE] = blacken_type_table,
};

static void blacken_object(VM *vm, CruxObject *object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *)object);
	print_value(OBJECT_VAL(object), false);
	printf("\n");
#endif

	const ObjectType type = object->type;
	if (type < (ObjectType)(sizeof(blacken_dispatch) / sizeof(blacken_dispatch[0]))) {
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

static void blacken_native_callable(VM *vm, CruxObject *object)
{
	const ObjectNativeCallable *native = (ObjectNativeCallable *)object;
	mark_object(vm, (CruxObject *)native->name);
	if (native->arg_types) {
		for (int i = 0; i < native->arity; i++) {
			mark_type_record(vm, native->arg_types[i]);
		}
	}
	mark_type_record(vm, native->return_type);
}

static void blacken_iterator(VM *vm, CruxObject *object)
{
	const ObjectIterator *iterator = (ObjectIterator *)object;
	mark_value(vm, iterator->iterable);
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

static void blacken_option(VM *vm, CruxObject *object)
{
	const ObjectOption *option = (ObjectOption *)object;
	mark_value(vm, option->value);
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
	mark_type_table(vm, module->types);
	mark_object(vm, (CruxObject *)module->module_closure);
	mark_object(vm, (CruxObject *)module->enclosing_module);

	for (const Value *slot = module->stack; slot < module->stack_top; slot++) {
		mark_value(vm, *slot);
	}
	for (int i = 0; i < module->frame_count; i++) {
		mark_object(vm, (CruxObject *)module->frames[i].closure);
	}
	for (ObjectUpvalue *upvalue = module->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
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

static void blacken_complex(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_matrix(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_string(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_range(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_set(VM *vm, CruxObject *object)
{
	ObjectSet *set = (ObjectSet *)object;
	mark_object_table(vm, set->entries->entries, set->entries->capacity);
}

static void blacken_buffer(VM *vm, CruxObject *object)
{
	(void)vm;
	(void)object;
}

static void blacken_tuple(VM *vm, CruxObject *object)
{
	const ObjectTuple *tuple = (ObjectTuple *)object;
	mark_object_array(vm, tuple->elements, tuple->size);
}

static void blacken_type_table(VM *vm, CruxObject *object)
{
	const ObjectTypeTable *table = (ObjectTypeTable *)object;
	if (!table->entries)
		return;
	for (int i = 0; i < table->capacity; i++) {
		const TypeEntry *entry = &table->entries[i];
		if (entry->key == NULL)
			continue;
		mark_object(vm, (CruxObject *)entry->key);
		mark_type_record(vm, entry->value);
	}
}

static void blacken_type_record(VM *vm, CruxObject *object)
{
	const ObjectTypeRecord *rec = (ObjectTypeRecord *)object;
	switch (rec->base_type) {
	case ARRAY_TYPE:
		mark_type_record(vm, rec->as.array_type.element_type);
		break;
	case ITERATOR_TYPE:
		mark_type_record(vm, rec->as.iterator_type.element_type);
		break;
	case TABLE_TYPE:
		mark_type_record(vm, rec->as.table_type.key_type);
		mark_type_record(vm, rec->as.table_type.value_type);
		break;
	case RESULT_TYPE:
		mark_type_record(vm, rec->as.result_type.ok_type);
		break;
	case OPTION_TYPE:
		mark_type_record(vm, rec->as.option_type.some_type);
		break;
	case STRUCT_TYPE:
		if (rec->as.struct_type.definition) {
			mark_object(vm, (CruxObject *)rec->as.struct_type.definition);
		}
		mark_type_table(vm, rec->as.struct_type.field_types);
		break;
	case FUNCTION_TYPE:
		if (rec->as.function_type.arg_types) {
			for (int i = 0; i < rec->as.function_type.arg_count; i++) {
				mark_type_record(vm, rec->as.function_type.arg_types[i]);
			}
		}
		mark_type_record(vm, rec->as.function_type.return_type);
		break;
	case SET_TYPE:
		mark_type_record(vm, rec->as.set_type.element_type);
		break;
	case TUPLE_TYPE: {
		for (int i = 0; i < rec->as.tuple_type.element_count; i++) {
			mark_type_record(vm, rec->as.tuple_type.element_types[i]);
		}
		break;
	}
	case UNION_TYPE:
		if (rec->as.union_type.element_types) {
			for (int i = 0; i < rec->as.union_type.element_count; i++) {
				mark_type_record(vm, rec->as.union_type.element_types[i]);
			}
		}
		if (rec->as.union_type.element_names) {
			for (int i = 0; i < rec->as.union_type.element_count; i++) {
				mark_object(vm, (CruxObject *)rec->as.union_type.element_names[i]);
			}
		}
		break;
	case SHAPE_TYPE:
		mark_type_table(vm, rec->as.shape_type.element_types);
		break;
	default:
		break;
	}
}

static void free_object_string(VM *vm, CruxObject *object);
static void free_object_function(VM *vm, CruxObject *object);
static void free_object_native_callable(VM *vm, CruxObject *object);
static void free_object_closure(VM *vm, CruxObject *object);
static void free_object_upvalue(VM *vm, CruxObject *object);
static void free_object_array(VM *vm, CruxObject *object);
static void free_object_table_wrapper(VM *vm, CruxObject *object);
static void free_object_error(VM *vm, CruxObject *object);
static void free_object_result(VM *vm, CruxObject *object);
static void free_object_option(VM *vm, CruxObject *object);
static void free_object_random(VM *vm, CruxObject *object);
static void free_object_file(VM *vm, CruxObject *object);
static void free_object_module_record_wrapper(VM *vm, CruxObject *object);
static void free_object_struct(VM *vm, CruxObject *object);
static void free_object_struct_instance(VM *vm, CruxObject *object);
static void free_object_vector(VM *vm, CruxObject *object);
static void free_object_complex(VM *vm, CruxObject *object);
static void free_object_set(VM *vm, CruxObject *object);
static void free_object_range(VM *vm, CruxObject *object);
static void free_object_iterator(VM *vm, CruxObject *object);
static void free_object_buffer(VM *vm, CruxObject *object);
static void free_object_tuple(VM *vm, CruxObject *object);
static void free_object_matrix(VM *vm, CruxObject *object);
static void free_object_type_record(VM *vm, CruxObject *object);
static void free_object_type_table(VM *vm, CruxObject *object);

static const FreeFunction free_dispatch[] = {
	[OBJECT_STRING] = free_object_string,
	[OBJECT_FUNCTION] = free_object_function,
	[OBJECT_NATIVE_CALLABLE] = free_object_native_callable,
	[OBJECT_CLOSURE] = free_object_closure,
	[OBJECT_UPVALUE] = free_object_upvalue,
	[OBJECT_ARRAY] = free_object_array,
	[OBJECT_TABLE] = free_object_table_wrapper,
	[OBJECT_ERROR] = free_object_error,
	[OBJECT_RESULT] = free_object_result,
	[OBJECT_OPTION] = free_object_option,
	[OBJECT_RANDOM] = free_object_random,
	[OBJECT_FILE] = free_object_file,
	[OBJECT_MODULE_RECORD] = free_object_module_record_wrapper,
	[OBJECT_STRUCT] = free_object_struct,
	[OBJECT_STRUCT_INSTANCE] = free_object_struct_instance,
	[OBJECT_VECTOR] = free_object_vector,
	[OBJECT_COMPLEX] = free_object_complex,
	[OBJECT_SET] = free_object_set,
	[OBJECT_RANGE] = free_object_range,
	[OBJECT_ITERATOR] = free_object_iterator,
	[OBJECT_BUFFER] = free_object_buffer,
	[OBJECT_TUPLE] = free_object_tuple,
	[OBJECT_MATRIX] = free_object_matrix,
	[OBJECT_TYPE_RECORD] = free_object_type_record,
	[OBJECT_TYPE_TABLE] = free_object_type_table,
};

static void free_object_string(VM *vm, CruxObject *object)
{
	const ObjectString *string = (ObjectString *)object;
	FREE_ARRAY(vm, char, string->chars, string->byte_length + 1);
	FREE_OBJECT(vm, ObjectString, object);
}

static void free_object_function(VM *vm, CruxObject *object)
{
	ObjectFunction *function = (ObjectFunction *)object;
	free_chunk(vm, &function->chunk);
	FREE_OBJECT(vm, ObjectFunction, object);
}

static void free_object_native_callable(VM *vm, CruxObject *object)
{
	ObjectNativeCallable *native = (ObjectNativeCallable *)object;
	if (native->arg_types) {
		FREE_ARRAY(vm, ObjectTypeRecord *, native->arg_types, native->arity);
	}
	FREE_OBJECT(vm, ObjectNativeCallable, object);
}

static void free_object_closure(VM *vm, CruxObject *object)
{
	const ObjectClosure *closure = (ObjectClosure *)object;
	FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues, closure->upvalue_count);
	FREE_OBJECT(vm, ObjectClosure, object);
}

static void free_object_upvalue(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectUpvalue, object);
}

static void free_object_array(VM *vm, CruxObject *object)
{
	const ObjectArray *array = (ObjectArray *)object;
	FREE_ARRAY(vm, Value, array->values, array->capacity);
	FREE_OBJECT(vm, ObjectArray, object);
}

static void free_object_table_wrapper(VM *vm, CruxObject *object)
{
	ObjectTable *table = (ObjectTable *)object;
	free_object_table(vm, table);
	FREE_OBJECT(vm, ObjectTable, object);
}

static void free_object_error(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectError, object);
}

static void free_object_result(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectResult, object);
}

static void free_object_option(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectOption, object);
}

static void free_object_random(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectRandom, object);
}

static void free_object_file(VM *vm, CruxObject *object)
{
	const ObjectFile *file = (ObjectFile *)object;
	if (file->file != NULL) {
		fclose(file->file);
	}
	FREE_OBJECT(vm, ObjectFile, object);
}

static void free_object_module_record_wrapper(VM *vm, CruxObject *object)
{
	ObjectModuleRecord *moduleRecord = (ObjectModuleRecord *)object;
	free_object_module_record(vm, moduleRecord);
	FREE_OBJECT(vm, ObjectModuleRecord, object);
}

static void free_object_struct(VM *vm, CruxObject *object)
{
	ObjectStruct *structure = (ObjectStruct *)object;
	free_table(vm, &structure->fields);
	free_table(vm, &structure->methods);
	FREE_OBJECT(vm, ObjectStruct, object);
}

static void free_object_struct_instance(VM *vm, CruxObject *object)
{
	const ObjectStructInstance *instance = (ObjectStructInstance *)object;
	FREE_ARRAY(vm, Value, instance->fields, instance->field_count);
	FREE_OBJECT(vm, ObjectStructInstance, object);
}

static void free_object_vector(VM *vm, CruxObject *object)
{
	const ObjectVector *vector = (ObjectVector *)object;
	if (vector->dimensions > 4) {
		FREE_ARRAY(vm, double, vector->as.h_components, vector->dimensions);
	}
	FREE_OBJECT(vm, ObjectVector, object);
}

static void free_object_complex(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectComplex, object);
}

static void free_object_set(VM *vm, CruxObject *object)
{
	const ObjectSet *set = (ObjectSet *)object;
	free_object_table_wrapper(vm, &set->entries->object);
	FREE_OBJECT(vm, ObjectSet, object);
}

static void free_object_range(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectRange, object);
}

static void free_object_iterator(VM *vm, CruxObject *object)
{
	FREE_OBJECT(vm, ObjectIterator, object);
}

static void free_object_buffer(VM *vm, CruxObject *object)
{
	const ObjectBuffer *buffer = (ObjectBuffer *)object;
	FREE_ARRAY(vm, uint8_t, buffer->data, buffer->capacity);
	FREE_OBJECT(vm, ObjectBuffer, object);
}

static void free_object_tuple(VM *vm, CruxObject *object)
{
	const ObjectTuple *tuple = (ObjectTuple *)object;
	FREE_ARRAY(vm, Value, tuple->elements, tuple->size);
	FREE_OBJECT(vm, ObjectTuple, object);
}

static void free_object_matrix(VM *vm, CruxObject *object)
{
	const ObjectMatrix *matrix = (ObjectMatrix *)object;
	FREE_ARRAY(vm, double, matrix->data, (uint32_t)matrix->row_dim * matrix->col_dim);
	FREE_OBJECT(vm, ObjectMatrix, object);
}

static void free_object_type_record(VM *vm, CruxObject *object)
{
	ObjectTypeRecord *rec = (ObjectTypeRecord *)object;
	if (rec->base_type == FUNCTION_TYPE) {
		if (rec->as.function_type.arg_types) {
			FREE_ARRAY(vm, ObjectTypeRecord *, rec->as.function_type.arg_types, rec->as.function_type.arg_count);
		}
	} else if (rec->base_type == UNION_TYPE) {
		if (rec->as.union_type.element_types) {
			FREE_ARRAY(vm, ObjectTypeRecord *, rec->as.union_type.element_types, rec->as.union_type.element_count);
		}
		if (rec->as.union_type.element_names) {
			FREE_ARRAY(vm, ObjectString *, rec->as.union_type.element_names, rec->as.union_type.element_count);
		}
	} else if (rec->base_type == TUPLE_TYPE) {
		if (rec->as.tuple_type.element_types && rec->as.tuple_type.element_count >= 0) {
			FREE_ARRAY(vm, ObjectTypeRecord *, rec->as.tuple_type.element_types, rec->as.tuple_type.element_count);
		}
	}
	FREE_OBJECT(vm, ObjectTypeRecord, object);
}

static void free_object_type_table(VM *vm, CruxObject *object)
{
	ObjectTypeTable *table = (ObjectTypeTable *)object;
	if (table->entries) {
		FREE_ARRAY(vm, TypeEntry, table->entries, table->capacity);
	}
	table->capacity = -1;
	table->count = -1;
	table->entries = NULL;
	FREE_OBJECT(vm, ObjectTypeTable, object);
}

void mark_module_roots(VM *vm, ObjectModuleRecord *moduleRecord)
{
	if (moduleRecord->enclosing_module != NULL) {
		mark_module_roots(vm, moduleRecord->enclosing_module);
	}

	mark_object(vm, (CruxObject *)moduleRecord->path);
	mark_table(vm, &moduleRecord->globals);
	mark_table(vm, &moduleRecord->publics);
	mark_type_table(vm, moduleRecord->types);
	mark_object(vm, (CruxObject *)moduleRecord->module_closure);
	mark_object(vm, (CruxObject *)moduleRecord->enclosing_module);

	for (const Value *slot = moduleRecord->stack; slot < moduleRecord->stack_top; slot++) {
		mark_value(vm, *slot);
	}

	for (int i = 0; i < moduleRecord->frame_count; i++) {
		mark_object(vm, (CruxObject *)moduleRecord->frames[i].closure);
	}

	for (ObjectUpvalue *upvalue = moduleRecord->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
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

void mark_roots(VM *vm)
{
	if (vm->current_module_record) {
		mark_module_roots(vm, vm->current_module_record);
	}

	for (uint32_t i = 0; i < vm->import_stack.count; i++) {
		mark_object(vm, (CruxObject *)vm->import_stack.paths[i]);
	}

	// No need to mark because they only contain immortal objects
	// if (vm->native_modules.modules != NULL) {
	// 	for (int i = 0; i < vm->native_modules.count; i++) {
	// 		mark_table(vm, vm->native_modules.modules[i].names);
	// 		mark_object(vm, (CruxObject *)vm->native_modules.modules[i].name);
	// 	}
	// }

	mark_table(vm, &vm->module_cache);
	mark_table(vm, &vm->strings);

	// No need to mark because they only contain immortal objects
	// mark_table(vm, &vm->core_fns);
	// mark_table(vm, &vm->random_type);
	// mark_table(vm, &vm->string_type);
	// mark_table(vm, &vm->array_type);
	// mark_table(vm, &vm->table_type);
	// mark_table(vm, &vm->error_type);
	// mark_table(vm, &vm->file_type);
	// mark_table(vm, &vm->result_type);
	// mark_table(vm, &vm->option_type);
	// mark_table(vm, &vm->vector_type);
	// mark_table(vm, &vm->complex_type);
	// mark_table(vm, &vm->matrix_type);
	// mark_table(vm, &vm->range_type);
	// mark_table(vm, &vm->set_type);
	// mark_table(vm, &vm->tuple_type);
	// mark_table(vm, &vm->buffer_type);

	mark_struct_instance_stack(vm, &vm->struct_instance_stack);

	if (vm->main_compiler) {
		mark_compiler_roots(vm, vm->main_compiler);
	}

	for (uint32_t i = 0; i < vm->match_handler_stack.count; i++) {
		mark_value(vm, vm->match_handler_stack.handlers[i].match_bind);
		mark_value(vm, vm->match_handler_stack.handlers[i].match_target);
	}
}

static void trace_references(VM *vm)
{
	while (vm->gray_count > 0) {
		CruxObject *object = vm->gray_stack[--vm->gray_count];
		blacken_object(vm, object);
	}
}

static void free_object(VM *vm, CruxObject *object, bool free_all)
{
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void *)object, object->type);
#endif
	if (object == NULL || (object->is_immortal && !free_all))
		return;

	if (object->type < (ObjectType)(sizeof(free_dispatch) / sizeof(free_dispatch[0]))) {
		free_dispatch[object->type](vm, object);
	}
}

static void sweep(VM *vm)
{
	CruxObject **object = &vm->objects;
	size_t slots_scanned = 0;

	while (*object != NULL) {
		slots_scanned++;
		if (!(*object)->is_marked) {
			// Unlink dead object
			CruxObject *unreached = *object;
			*object = unreached->next;

			free_object(vm, unreached, false);
			vm->object_count--;
		} else {
			// Unmark and move to next
			(*object)->is_marked = false;
			object = &(*object)->next;
		}
	}

	vm->gc_last_sweep_slots_scanned = slots_scanned;
	vm->gc_sweep_slots_scanned += slots_scanned;
}

void free_objects(VM *vm, bool free_all)
{
	CruxObject *object = vm->objects;
	while (object != NULL) {
		CruxObject *next = object->next;
		free_object(vm, object, free_all);
		object = next;
	}
	free(vm->gray_stack);
	vm->objects = NULL;
	vm->object_count = 0;
}

void collect_garbage(VM *vm)
{
	if (vm->gc_status == PAUSED)
		return;

	const uint64_t gc_start_ns = gc_now_ns();
	uint64_t phase_start_ns = gc_start_ns;
	vm->gc_last_gray_peak = 0;
	vm->gc_last_bytes_before = vm->bytes_allocated;

#ifdef DEBUG_LOG_GC
	printf("--- gc begin ---\n");
	const size_t before = vm->bytes_allocated;
#endif

	mark_roots(vm);
	const uint64_t mark_roots_end_ns = gc_now_ns();
	trace_references(vm);
	const uint64_t trace_end_ns = gc_now_ns();
	table_remove_white(vm, &vm->strings); // Clean up string table
	const uint64_t remove_white_end_ns = gc_now_ns();
	sweep(vm);
	const uint64_t sweep_end_ns = gc_now_ns();
	vm->next_gc = compute_next_gc_threshold(vm);
	vm->gc_last_bytes_after = vm->bytes_allocated;
	vm->gc_last_bytes_freed = vm->gc_last_bytes_before - vm->gc_last_bytes_after;
	vm->gc_last_next_gc = vm->next_gc;
	vm->gc_last_objects_freed = vm->gc_last_objects_before_sweep - vm->gc_last_objects_after_sweep;
	vm->gc_last_live_objects = vm->object_count;
	vm->gc_last_strings_count = vm->strings.count;
	vm->gc_last_strings_capacity = vm->strings.capacity;
	vm->gc_last_strings_tombstones = table_tombstone_count(&vm->strings);
	vm->gc_last_mark_roots_ns = mark_roots_end_ns - phase_start_ns;
	vm->gc_last_trace_ns = trace_end_ns - mark_roots_end_ns;
	vm->gc_last_remove_white_ns = remove_white_end_ns - trace_end_ns;
	vm->gc_last_sweep_ns = sweep_end_ns - remove_white_end_ns;
	vm->gc_last_total_ns = sweep_end_ns - gc_start_ns;
	vm->gc_collections++;
	vm->gc_mark_roots_ns += vm->gc_last_mark_roots_ns;
	vm->gc_trace_ns += vm->gc_last_trace_ns;
	vm->gc_remove_white_ns += vm->gc_last_remove_white_ns;
	vm->gc_sweep_ns += vm->gc_last_sweep_ns;
	vm->gc_total_ns += vm->gc_last_total_ns;

#ifdef DEBUG_LOG_GC
	printf("--- gc end ---\n");
	printf("    collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm->bytes_allocated, before,
		   vm->bytes_allocated, vm->next_gc);
#endif
}
