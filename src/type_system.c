#include "type_system.h"
#include <stdlib.h>
#include <string.h>
#include "garbage_collector.h"
#include "object.h"
#include "value.h"

#define TYPE_GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

void init_type_table(TypeTable *table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void free_type_table(TypeTable *table)
{
	free(table->entries);
	init_type_table(table);
}

static bool compare_strings(const ObjectString *a, const ObjectString *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->chars, b->chars, a->length) == 0;
}

static TypeEntry *type_find_entry(TypeEntry *entries, const int capacity,
				  const ObjectString *key)
{
	uint32_t index = key->hash & (capacity - 1);
	TypeEntry *tombstone = NULL;
	for (;;) {
		TypeEntry *entry = &entries[index];

		if (entry->key == NULL) {
			if (entry->value == NULL) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL)
				tombstone = entry;
		} else if (entry->key == key ||
			   compare_strings(entry->key, key)) {
			return entry;
		}

		index = (index + 1) & (capacity - 1);
	}
}

static void type_adjust_capacity(TypeTable *table, const int capacity)
{
	TypeEntry *entries = calloc(capacity, sizeof(TypeEntry));
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NULL;
	}
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		TypeEntry *entry = &table->entries[i];
		if (entry->key == NULL)
			continue;

		TypeEntry *dest = type_find_entry(entries, capacity,
						  entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	free(table->entries);
	table->entries = entries;
	table->capacity = capacity;
}

bool type_table_set(TypeTable *table, ObjectString *key, TypeRecord *value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		const int capacity = TYPE_GROW_CAPACITY(table->capacity);
		type_adjust_capacity(table, capacity);
	}

	TypeEntry *entry = type_find_entry(table->entries, table->capacity,
					   key);
	const bool isNewKey = entry->key == NULL;

	if (isNewKey && entry->value == NULL) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool type_table_get(const TypeTable *table, const ObjectString *key,
		    TypeRecord **value)
{
	if (!table) {
		return false;
	}
	if (table->count == 0)
		return false;

	const TypeEntry *entry = type_find_entry(table->entries,
						 table->capacity, key);
	if (entry->key == NULL)
		return false;
	*value = entry->value;
	return true;
}

bool type_table_delete(const TypeTable *table, const ObjectString *key)
{
	if (table->count == 0)
		return false;

	TypeEntry *entry = type_find_entry(table->entries, table->capacity,
					   key);
	if (entry->key == NULL)
		return false;

	entry->key = NULL;
	entry->value = NULL;
	return true;
}

void type_table_add_all(const TypeTable *from, TypeTable *to)
{
	for (int i = 0; i < from->capacity; i++) {
		const TypeEntry *entry = &from->entries[i];
		if (entry->key != NULL) {
			type_table_set(to, entry->key, entry->value);
		}
	}
}

TypeMask get_type_mask(Value value)
{
	if (IS_BOOL(value))
		return BOOL_TYPE;
	if (IS_INT(value))
		return INT_TYPE;
	if (IS_FLOAT(value))
		return FLOAT_TYPE;
	if (IS_NIL(value))
		return NIL_TYPE;

	if (IS_CRUX_OBJECT(value)) {
		switch (OBJECT_TYPE(value)) {
		case OBJECT_STRING:
			return STRING_TYPE;
		case OBJECT_ARRAY:
			return ARRAY_TYPE;
		case OBJECT_TABLE:
			return TABLE_TYPE;
		case OBJECT_ERROR:
			return ERROR_TYPE;
		case OBJECT_RESULT:
			return RESULT_TYPE;
		case OBJECT_RANDOM:
			return RANDOM_TYPE;
		case OBJECT_FILE:
			return FILE_TYPE;
		case OBJECT_STRUCT:
		case OBJECT_STRUCT_INSTANCE:
			return STRUCT_TYPE;
		case OBJECT_VECTOR:
			return VECTOR_TYPE;
		case OBJECT_COMPLEX:
			return COMPLEX_TYPE;
		case OBJECT_MATRIX:
			return MATRIX_TYPE;
		case OBJECT_MODULE_RECORD:
			return MODULE_TYPE;
		case OBJECT_UPVALUE: {
			ObjectUpvalue *upvalue = AS_CRUX_UPVALUE(value);
			return get_type_mask(upvalue->closed);
		}
		case OBJECT_CLOSURE:
		case OBJECT_NATIVE_CALLABLE:
		case OBJECT_FUNCTION:
			return FUNCTION_TYPE;
		case OBJECT_SET:
			return SET_TYPE;
		case OBJECT_TUPLE:
			return TUPLE_TYPE;
		case OBJECT_BUFFER:
			return BUFFER_TYPE;
		case OBJECT_RANGE:
			return RANGE_TYPE;
		default:
			return ANY_TYPE;
		}
	}
	return ANY_TYPE;
}

void type_mask_name(const TypeMask mask, char *buf, const int buf_size)
{
	if (mask == ANY_TYPE) {
		snprintf(buf, buf_size, "Any");
		return;
	}

	static const struct {
		TypeMask bit;
		const char *name;
	} entries[] = {
		{NIL_TYPE, "Nil"},	   {BOOL_TYPE, "Bool"},
		{INT_TYPE, "Int"},	   {FLOAT_TYPE, "Float"},
		{STRING_TYPE, "String"},   {ARRAY_TYPE, "Array"},
		{TABLE_TYPE, "Table"},	   {FUNCTION_TYPE, "Function"},
		{ERROR_TYPE, "Error"},	   {RESULT_TYPE, "Result"},
		{FILE_TYPE, "File"},	   {VECTOR_TYPE, "Vector"},
		{COMPLEX_TYPE, "Complex"}, {MATRIX_TYPE, "Matrix"},
		{STRUCT_TYPE, "Struct"},   {MODULE_TYPE, "Module"},
		{SET_TYPE, "Set"},	   {TUPLE_TYPE, "Tuple"},
		{BUFFER_TYPE, "Buffer"},   {RANGE_TYPE, "Range"},
	};

	int offset = 0;
	bool first = true;
	for (int i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++) {
		if (mask & entries[i].bit) {
			if (!first)
				offset += snprintf(buf + offset,
						   buf_size - offset, " | ");
			offset += snprintf(buf + offset, buf_size - offset,
					   "%s", entries[i].name);
			first = false;
		}
	}
}

bool runtime_types_compatible(const TypeMask expected, const Value actual)
{
	if (expected == ANY_TYPE)
		return true;
	const TypeMask actual_mask = get_type_mask(actual);
	return (expected & actual_mask) != 0;
}

static TypeRecord *type_arena_alloc(TypeArena *arena)
{
	// align to 8 bytes
	const size_t aligned = (arena->used + 7) & ~7;
	const size_t next = aligned + sizeof(TypeRecord);

	if (next > TYPE_ARENA_CAPACITY) {
		// compiler should panic
		return NULL;
	}

	arena->used = next;
	TypeRecord *rec = (TypeRecord *)(arena->data + aligned);
	memset(rec, 0, sizeof(TypeRecord));
	return rec;
}

void type_arena_reset(TypeArena *arena)
{
	arena->used = 0;
}

TypeRecord *new_type_rec(TypeArena *arena, TypeMask base_type)
{
	TypeRecord *rec = type_arena_alloc(arena);
	if (!rec)
		return NULL;
	rec->base_type = base_type;
	return rec;
}

TypeRecord *new_array_type_rec(TypeArena *arena, TypeRecord *element_type)
{
	TypeRecord *rec = new_type_rec(arena, ARRAY_TYPE);
	if (!rec)
		return NULL;
	rec->as.array_type.element_type = element_type;
	return rec;
}

TypeRecord *new_table_type_rec(TypeArena *arena, TypeRecord *key_type,
			       TypeRecord *value_type)
{
	TypeRecord *rec = new_type_rec(arena, TABLE_TYPE);
	if (!rec)
		return NULL;
	rec->as.table_type.key_type = key_type;
	rec->as.table_type.value_type = value_type;
	return rec;
}

TypeRecord *new_result_type_rec(TypeArena *arena, TypeRecord *ok_type)
{
	TypeRecord *rec = new_type_rec(arena, RESULT_TYPE);
	if (!rec)
		return NULL;
	rec->as.result_type.ok_type = ok_type;
	return rec;
}

TypeRecord *new_struct_type_rec(TypeArena *arena, ObjectStruct *definition,
				TypeTable *field_types, int field_count)
{
	TypeRecord *rec = new_type_rec(arena, STRUCT_TYPE);
	if (!rec)
		return NULL;
	rec->as.struct_type.definition = definition;
	rec->as.struct_type.field_types = field_types;
	rec->as.struct_type.field_count = field_count;
	return rec;
}

TypeRecord *new_vector_type_rec(TypeArena *arena, int dimensions)
{
	TypeRecord *rec = new_type_rec(arena, VECTOR_TYPE);
	if (!rec)
		return NULL;
	rec->as.vector_type.dimensions = dimensions;
	return rec;
}

TypeRecord *new_tuple_type_rec(TypeArena *arena, TypeRecord *element_type)
{
	TypeRecord *rec = new_type_rec(arena, TUPLE_TYPE);
	if (!rec)
		return NULL;
	rec->as.tuple_type.element_type = element_type;
	return rec;
}

TypeRecord *new_matrix_type_rec(TypeArena *arena, int rows, int cols)
{
	TypeRecord *rec = new_type_rec(arena, MATRIX_TYPE);
	if (!rec)
		return NULL;
	rec->as.matrix_type.rows = rows;
	rec->as.matrix_type.cols = cols;
	return rec;
}

TypeRecord *new_function_type_rec(TypeArena *arena, TypeRecord **arg_types,
				  int arg_count, TypeRecord *return_type)
{
	TypeRecord *rec = new_type_rec(arena, FUNCTION_TYPE);
	if (!rec)
		return NULL;
	rec->as.function_type.arg_types = arg_types;
	rec->as.function_type.arg_count = arg_count;
	rec->as.function_type.return_type = return_type;
	return rec;
}

TypeRecord *new_set_type_rec(TypeArena *arena, TypeRecord *element_type)
{
	TypeRecord *rec = new_type_rec(arena, SET_TYPE);
	if (!rec)
		return NULL;
	rec->as.set_type.element_type = element_type;
	return rec;
}

TypeRecord *new_shape_type_rec(TypeArena *arena, TypeTable *element_types,
			       int element_count)
{
	TypeRecord *rec = new_type_rec(arena, SHAPE_TYPE);
	if (!rec)
		return NULL;
	rec->as.shape_type.element_types = element_types;
	rec->as.shape_type.element_count = element_count;
	return rec;
}

TypeRecord *new_union_type_rec(TypeArena *arena, TypeRecord **element_types,
			       ObjectString **element_names, int element_count)
{
	TypeRecord *rec = new_type_rec(arena, UNION_TYPE);
	if (!rec)
		return NULL;
	rec->as.union_type.element_types = element_types;
	rec->as.union_type.element_names = element_names;
	rec->as.union_type.element_count = element_count;
	return rec;
}

bool types_equal(TypeRecord *a, TypeRecord *b)
{
	if (!a || !b)
		return false;
	if (a->base_type != b->base_type)
		return false;

	switch (a->base_type) {
	case ARRAY_TYPE: {
		return types_equal(a->as.array_type.element_type,
				   b->as.array_type.element_type);
	}
	case TABLE_TYPE: {
		return types_equal(a->as.table_type.key_type,
				   b->as.table_type.key_type) &&
		       types_equal(a->as.table_type.value_type,
				   b->as.table_type.value_type);
	}
	case RESULT_TYPE: {
		return types_equal(a->as.result_type.ok_type,
				   b->as.result_type.ok_type);
	}
	case TUPLE_TYPE: {
		return types_equal(a->as.tuple_type.element_type,
				   b->as.tuple_type.element_type);
	}
	case VECTOR_TYPE: {
		return a->as.vector_type.dimensions ==
		       b->as.vector_type.dimensions;
	}
	case MATRIX_TYPE: {
		return a->as.matrix_type.cols == b->as.matrix_type.cols &&
		       a->as.matrix_type.rows == b->as.matrix_type.rows;
	}

	case STRUCT_TYPE: {
		const ObjectStruct *a_struct = a->as.struct_type.definition;
		const ObjectStruct *b_struct = b->as.struct_type.definition;
		return a_struct->name ==
		       b_struct->name; // should work because of interning
	}

	case FUNCTION_TYPE: {
		if (a->as.function_type.arg_count !=
		    b->as.function_type.arg_count)
			return false;
		for (int i = 0; i < a->as.function_type.arg_count; i++) {
			if (!types_equal(a->as.function_type.arg_types[i],
					 b->as.function_type.arg_types[i]))
				return false;
		}
		return types_equal(a->as.function_type.return_type,
				   b->as.function_type.return_type);
	}
	case SET_TYPE:
		return types_equal(a->as.set_type.element_type,
				   b->as.set_type.element_type);
	case SHAPE_TYPE: {
		if (a->as.shape_type.element_count !=
		    b->as.shape_type.element_count)
			return false;
		const TypeTable *a_table = a->as.shape_type.element_types;
		const TypeTable *b_table = b->as.shape_type.element_types;
		for (int i = 0; i < a_table->capacity; i++) {
			const TypeEntry *entry = &a_table->entries[i];
			if (entry->key == NULL)
				continue;
			TypeRecord *b_value = NULL;
			if (!type_table_get(b_table, entry->key, &b_value))
				return false;
			if (!types_equal(entry->value, b_value))
				return false;
		}
		return true;
	}
	case UNION_TYPE: {
		if (a->as.union_type.element_count !=
		    b->as.union_type.element_count)
			return false;
		for (int i = 0; i < a->as.union_type.element_count; i++) {
			if (!types_equal(a->as.union_type.element_types[i],
					 b->as.union_type.element_types[i]))
				return false;
		}
		return true;
	}
	case RANGE_TYPE:
	case BUFFER_TYPE:
	case NIL_TYPE:
	case BOOL_TYPE:
	case INT_TYPE:
	case FLOAT_TYPE:
	case STRING_TYPE:
	case ERROR_TYPE:
	case RANDOM_TYPE:
	case FILE_TYPE:
	case MODULE_TYPE:
	case ANY_TYPE:
		return true;
	default:
		return false;
	}
}

bool types_compatible(TypeRecord *a, TypeRecord *b)
{
	if (!a || !b)
		return false;
	if (a->base_type == ANY_TYPE)
		return true;
	if (a->base_type == FLOAT_TYPE && b->base_type == INT_TYPE)
		return true;
	if (a->base_type != b->base_type)
		return false;

	return types_equal(a, b);
}

// Deep-copies a TypeRecord from any source arena into dest.
// For function types, the arg_types array is heap-allocated.
TypeRecord *copy_type_rec_to_arena(TypeArena *dest, TypeRecord *src)
{
	if (!src)
		return NULL;

	TypeRecord *copy = new_type_rec(dest, src->base_type);
	if (!copy)
		return NULL;

	switch (src->base_type) {
	case ARRAY_TYPE:
		copy->as.array_type.element_type = copy_type_rec_to_arena(
			dest, src->as.array_type.element_type);
		break;
	case TABLE_TYPE:
		copy->as.table_type.key_type = copy_type_rec_to_arena(
			dest, src->as.table_type.key_type);
		copy->as.table_type.value_type = copy_type_rec_to_arena(
			dest, src->as.table_type.value_type);
		break;
	case RESULT_TYPE:
		copy->as.result_type.ok_type = copy_type_rec_to_arena(
			dest, src->as.result_type.ok_type);
		break;
	case VECTOR_TYPE:
		copy->as.vector_type.dimensions =
			src->as.vector_type.dimensions;
		break;
	case TUPLE_TYPE:
		copy->as.tuple_type.element_type = copy_type_rec_to_arena(
			dest, src->as.tuple_type.element_type);
		break;
	case MATRIX_TYPE:
		copy->as.matrix_type.rows = src->as.matrix_type.rows;
		copy->as.matrix_type.cols = src->as.matrix_type.cols;
		break;
	case SET_TYPE:
		copy->as.set_type.element_type = copy_type_rec_to_arena(
			dest, src->as.set_type.element_type);
		break;
	case FUNCTION_TYPE: {
		int count = src->as.function_type.arg_count;
		TypeRecord **args = NULL;
		if (count > 0) {
			args = malloc(sizeof(TypeRecord *) * count);
			for (int i = 0; i < count; i++) {
				args[i] = copy_type_rec_to_arena(
					dest,
					src->as.function_type.arg_types[i]);
			}
		}
		copy->as.function_type.arg_types = args;
		copy->as.function_type.arg_count = count;
		copy->as.function_type.return_type = copy_type_rec_to_arena(
			dest, src->as.function_type.return_type);
		break;
	}
	// Struct, shape, union have pointers to external ObjectStruct /
	// TypeTable — those are GC-managed or long-lived so no deep copy
	// needed for their nested pointers.
	default:
		break;
	}
	return copy;
}
