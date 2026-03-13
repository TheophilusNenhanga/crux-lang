#include "type_system.h"
#include <stdlib.h>
#include <string.h>
#include "garbage_collector.h"
#include "object.h"
#include "value.h"

#define TYPE_GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

static bool compare_strings(const ObjectString *a, const ObjectString *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->chars, b->chars, a->length) == 0;
}

static TypeEntry *type_find_entry(TypeEntry *entries, const int capacity, const ObjectString *key)
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
		} else if (entry->key == key || compare_strings(entry->key, key)) {
			return entry;
		}

		index = (index + 1) & (capacity - 1);
	}
}

static void type_adjust_capacity(ObjectTypeTable *table, const int capacity)
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

		TypeEntry *dest = type_find_entry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	free(table->entries);
	table->entries = entries;
	table->capacity = capacity;
}

/**
 * Set a type in the type table
 * @return true if new type was set false otherwise
 */
bool type_table_set(ObjectTypeTable *table, ObjectString *key, ObjectTypeRecord *value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		const int capacity = TYPE_GROW_CAPACITY(table->capacity);
		type_adjust_capacity(table, capacity);
	}

	TypeEntry *entry = type_find_entry(table->entries, table->capacity, key);
	const bool isNewKey = entry->key == NULL;

	if (isNewKey && entry->value == NULL) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool type_table_get(const ObjectTypeTable *table, const ObjectString *key, ObjectTypeRecord **value)
{
	if (!table) {
		return false;
	}
	if (table->count == 0)
		return false;

	const TypeEntry *entry = type_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;
	*value = entry->value;
	return true;
}

bool type_table_delete(const ObjectTypeTable *table, const ObjectString *key)
{
	if (table->count == 0)
		return false;

	TypeEntry *entry = type_find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;

	entry->key = NULL;
	entry->value = NULL;
	return true;
}

void type_table_add_all(const ObjectTypeTable *from, ObjectTypeTable *to)
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
			const ObjectUpvalue *upvalue = AS_CRUX_UPVALUE(value);
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
	} entries[] = {{NIL_TYPE, "Nil"},		{BOOL_TYPE, "Bool"},	   {INT_TYPE, "Int"},
				   {FLOAT_TYPE, "Float"},	{SHAPE_TYPE, "Shape"},	   {STRING_TYPE, "String"},
				   {ARRAY_TYPE, "Array"},	{TABLE_TYPE, "Table"},	   {FUNCTION_TYPE, "Function"},
				   {ERROR_TYPE, "Error"},	{RESULT_TYPE, "Result"},   {FILE_TYPE, "File"},
				   {VECTOR_TYPE, "Vector"}, {COMPLEX_TYPE, "Complex"}, {MATRIX_TYPE, "Matrix"},
				   {STRUCT_TYPE, "Struct"}, {MODULE_TYPE, "Module"},   {SET_TYPE, "Set"},
				   {TUPLE_TYPE, "Tuple"},	{BUFFER_TYPE, "Buffer"},   {RANGE_TYPE, "Range"},
				   {UNION_TYPE, "Union"}};

	int offset = 0;
	bool first = true;
	for (int i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++) {
		if (mask & entries[i].bit) {
			if (!first)
				offset += snprintf(buf + offset, buf_size - offset, " | ");
			offset += snprintf(buf + offset, buf_size - offset, "%s", entries[i].name);
			first = false;
		}
	}
}

bool runtime_types_compatible(const TypeMask expected, const Value actual)
{
	if (expected == ANY_TYPE || expected == UNION_TYPE) // TODO: escape hatch for union types. fix later
		return true;
	const TypeMask actual_mask = get_type_mask(actual);
	return (expected & actual_mask) != 0;
}

ObjectTypeRecord *new_array_type_rec(VM *vm, ObjectTypeRecord *element_type)
{
	ObjectTypeRecord *rec = new_type_rec(vm, ARRAY_TYPE);
	rec->as.array_type.element_type = element_type;
	return rec;
}

ObjectTypeRecord *new_table_type_rec(VM *vm, ObjectTypeRecord *key_type, ObjectTypeRecord *value_type)
{
	ObjectTypeRecord *rec = new_type_rec(vm, TABLE_TYPE);
	rec->as.table_type.key_type = key_type;
	rec->as.table_type.value_type = value_type;
	return rec;
}

ObjectTypeRecord *new_result_type_rec(VM *vm, ObjectTypeRecord *ok_type)
{
	ObjectTypeRecord *rec = new_type_rec(vm, RESULT_TYPE);
	rec->as.result_type.ok_type = ok_type;
	return rec;
}

ObjectTypeRecord *new_struct_type_rec(VM *vm, ObjectStruct *definition, ObjectTypeTable *field_types, int field_count)
{
	ObjectTypeRecord *rec = new_type_rec(vm, STRUCT_TYPE);
	rec->as.struct_type.definition = definition;
	rec->as.struct_type.field_types = field_types;
	rec->as.struct_type.field_count = field_count;
	return rec;
}

ObjectTypeRecord *new_vector_type_rec(VM *vm, int dimensions)
{
	ObjectTypeRecord *rec = new_type_rec(vm, VECTOR_TYPE);
	rec->as.vector_type.dimensions = dimensions;
	return rec;
}

ObjectTypeRecord *new_tuple_type_rec(VM *vm, ObjectTypeRecord **element_types, int element_count)
{
	ObjectTypeRecord *rec = new_type_rec(vm, TUPLE_TYPE);
	rec->as.tuple_type.element_types = element_types;
	rec->as.tuple_type.element_count = element_count;
	return rec;
}

ObjectTypeRecord *new_matrix_type_rec(VM *vm, int rows, int cols)
{
	ObjectTypeRecord *rec = new_type_rec(vm, MATRIX_TYPE);
	rec->as.matrix_type.rows = rows;
	rec->as.matrix_type.cols = cols;
	return rec;
}

ObjectTypeRecord *new_function_type_rec(VM *vm, ObjectTypeRecord **arg_types, int arg_count,
										ObjectTypeRecord *return_type)
{
	ObjectTypeRecord *rec = new_type_rec(vm, FUNCTION_TYPE);
	rec->as.function_type.arg_types = arg_types;
	rec->as.function_type.arg_count = arg_count;
	rec->as.function_type.return_type = return_type;
	return rec;
}

ObjectTypeRecord *new_set_type_rec(VM *vm, ObjectTypeRecord *element_type)
{
	ObjectTypeRecord *rec = new_type_rec(vm, SET_TYPE);
	rec->as.set_type.element_type = element_type;
	return rec;
}

ObjectTypeRecord *new_shape_type_rec(VM *vm, ObjectTypeTable *element_types, int element_count)
{
	ObjectTypeRecord *rec = new_type_rec(vm, SHAPE_TYPE);
	rec->as.shape_type.element_types = element_types;
	rec->as.shape_type.element_count = element_count;
	return rec;
}

ObjectTypeRecord *new_union_type_rec(VM *vm, ObjectTypeRecord **element_types, ObjectString **element_names,
									 int element_count)
{
	ObjectTypeRecord *rec = new_type_rec(vm, UNION_TYPE);
	rec->as.union_type.element_types = element_types;
	rec->as.union_type.element_names = element_names;
	rec->as.union_type.element_count = element_count;
	return rec;
}

bool types_equal(ObjectTypeRecord *a, ObjectTypeRecord *b)
{
	if (!a || !b)
		return false;

	if (a == b)
		return true;

	if (a->base_type != b->base_type)
		return false;

	switch (a->base_type) {
	case ARRAY_TYPE:
		return types_equal(a->as.array_type.element_type, b->as.array_type.element_type);
	case TABLE_TYPE:
		return types_equal(a->as.table_type.key_type, b->as.table_type.key_type) &&
			   types_equal(a->as.table_type.value_type, b->as.table_type.value_type);
	case RESULT_TYPE:
		return types_equal(a->as.result_type.ok_type, b->as.result_type.ok_type);
	case TUPLE_TYPE: {
		if (a->as.tuple_type.element_count == -1 || b->as.tuple_type.element_count == -1)
			return true;
		if (a->as.tuple_type.element_count != b->as.tuple_type.element_count)
			return false;
		for (int i = 0; i < a->as.tuple_type.element_count; i++) {
			if (!types_equal(a->as.tuple_type.element_types[i], b->as.tuple_type.element_types[i]))
				return false;
		}
		return true;
	}
	case VECTOR_TYPE:
		return a->as.vector_type.dimensions == b->as.vector_type.dimensions;
	case MATRIX_TYPE:
		return a->as.matrix_type.cols == b->as.matrix_type.cols && a->as.matrix_type.rows == b->as.matrix_type.rows;
	case STRUCT_TYPE: {
		const ObjectStruct *a_struct = a->as.struct_type.definition;
		const ObjectStruct *b_struct = b->as.struct_type.definition;
		if (!a_struct || !b_struct)
			return a_struct == b_struct;
		return a_struct->name == b_struct->name;
	}
	case FUNCTION_TYPE: {
		if (a->as.function_type.arg_count != b->as.function_type.arg_count)
			return false;
		for (int i = 0; i < a->as.function_type.arg_count; i++) {
			if (!types_equal(a->as.function_type.arg_types[i], b->as.function_type.arg_types[i]))
				return false;
		}
		return types_equal(a->as.function_type.return_type, b->as.function_type.return_type);
	}
	case SET_TYPE:
		return types_equal(a->as.set_type.element_type, b->as.set_type.element_type);
	case SHAPE_TYPE: {
		if (a->as.shape_type.element_count != b->as.shape_type.element_count)
			return false;
		const ObjectTypeTable *a_table = a->as.shape_type.element_types;
		const ObjectTypeTable *b_table = b->as.shape_type.element_types;
		for (int i = 0; i < a_table->capacity; i++) {
			const TypeEntry *entry = &a_table->entries[i];
			if (entry->key == NULL)
				continue;
			ObjectTypeRecord *b_value = NULL;
			if (!type_table_get(b_table, entry->key, &b_value))
				return false;
			if (!types_equal(entry->value, b_value))
				return false;
		}
		return true;
	}
	case UNION_TYPE: {
		if (a->as.union_type.element_count != b->as.union_type.element_count)
			return false;
		// Order independent comparison: Int | String == String | Int
		for (int i = 0; i < a->as.union_type.element_count; i++) {
			bool found = false;
			for (int j = 0; j < b->as.union_type.element_count; j++) {
				if (types_equal(a->as.union_type.element_types[i], b->as.union_type.element_types[j])) {
					found = true;
					break;
				}
			}
			if (!found)
				return false;
		}
		return true;
	}
	default:
		return true;
	}
}

bool types_compatible(ObjectTypeRecord *expected, ObjectTypeRecord *got)
{
	if (!expected || !got)
		return false;

	if (expected == got)
		return true;

	if (expected->base_type == ANY_TYPE)
		return true;

	if (got->base_type == ANY_TYPE)
		return true;

	if (expected->base_type == UNION_TYPE) {
		if (got->base_type == UNION_TYPE) {
			// Every type in 'got' must fit into 'expected'
			for (int i = 0; i < got->as.union_type.element_count; i++) {
				if (!types_compatible(expected, got->as.union_type.element_types[i]))
					return false;
			}
			return true;
		}

		// 'got' is a single type, it must match at least one option in 'expected'
		for (int i = 0; i < expected->as.union_type.element_count; i++) {
			if (types_compatible(expected->as.union_type.element_types[i], got))
				return true;
		}
		return false;
	}

	// 2. If 'got' is a Union, but 'expected' is not
	if (got->base_type == UNION_TYPE) {
		// Every possible type in 'got' must satisfy 'expected'
		for (int i = 0; i < got->as.union_type.element_count; i++) {
			if (!types_compatible(expected, got->as.union_type.element_types[i]))
				return false;
		}
		return true;
	}

	// 3. Special case: Float accepts Int
	if (expected->base_type == FLOAT_TYPE && got->base_type == INT_TYPE)
		return true;

	// Structural typing for Shapes
	if (expected->base_type == SHAPE_TYPE && (got->base_type == STRUCT_TYPE || got->base_type == SHAPE_TYPE)) {
		const ObjectTypeTable *shape_fields = expected->as.shape_type.element_types;
		const ObjectTypeTable *got_fields = (got->base_type == STRUCT_TYPE) ? got->as.struct_type.field_types
																			: got->as.shape_type.element_types;

		for (int i = 0; i < shape_fields->capacity; i++) {
			const TypeEntry *shape_entry = &shape_fields->entries[i];
			if (shape_entry->key == NULL)
				continue;

			ObjectTypeRecord *got_field_type = NULL;
			if (!type_table_get(got_fields, shape_entry->key, &got_field_type)) {
				return false;
			}

			if (!types_compatible(shape_entry->value, got_field_type)) {
				return false;
			}
		}
		return true;
	}

	// 4. Check if 'got' is a subset of 'expected' (handles NUMERIC_TYPE bitmask aliases)
	if ((expected->base_type & got->base_type) == got->base_type) {
		// If it's a complex type, recursively check components
		switch (got->base_type) {
		case ARRAY_TYPE:
			return types_compatible(expected->as.array_type.element_type, got->as.array_type.element_type);
		case TABLE_TYPE:
			return types_compatible(expected->as.table_type.key_type, got->as.table_type.key_type) &&
				   types_compatible(expected->as.table_type.value_type, got->as.table_type.value_type);
		case RESULT_TYPE:
			return types_compatible(expected->as.result_type.ok_type, got->as.result_type.ok_type);
		case TUPLE_TYPE: {
			if (expected->as.tuple_type.element_count == -1 || got->as.tuple_type.element_count == -1)
				return true;
			if (expected->as.tuple_type.element_count != got->as.tuple_type.element_count)
				return false;
			for (int i = 0; i < expected->as.tuple_type.element_count; i++) {
				if (!types_compatible(expected->as.tuple_type.element_types[i], got->as.tuple_type.element_types[i]))
					return false;
			}
			return true;
		}
		case SET_TYPE:
			return types_equal(expected->as.set_type.element_type, got->as.set_type.element_type);
		case VECTOR_TYPE:
			return expected->as.vector_type.dimensions == -1 || got->as.vector_type.dimensions == -1 ||
				   expected->as.vector_type.dimensions == got->as.vector_type.dimensions;
		case MATRIX_TYPE:
			return (expected->as.matrix_type.cols == -1 && expected->as.matrix_type.rows == -1) ||
				   (got->as.matrix_type.cols == -1 && got->as.matrix_type.rows == -1) ||
				   (expected->as.matrix_type.cols == got->as.matrix_type.cols &&
					expected->as.matrix_type.rows == got->as.matrix_type.rows);
		case FUNCTION_TYPE:
			if (expected->as.function_type.arg_count != got->as.function_type.arg_count)
				return false;
			for (int i = 0; i < expected->as.function_type.arg_count; i++) {
				// Contravariance for function arguments
				if (!types_compatible(got->as.function_type.arg_types[i], expected->as.function_type.arg_types[i]))
					return false;
			}
			return types_compatible(expected->as.function_type.return_type, got->as.function_type.return_type);
		case STRUCT_TYPE:
			return types_equal(expected, got);
		default:
			return true;
		}
	}

	return false;
}

void type_record_name(const ObjectTypeRecord *rec, char *buf, const int buf_size)
{
	if (!rec) {
		snprintf(buf, buf_size, "Unknown");
		return;
	}

	if (rec->base_type == ANY_TYPE) {
		snprintf(buf, buf_size, "Any");
		return;
	}

	TypeMask complex_mask = ARRAY_TYPE | TABLE_TYPE | RESULT_TYPE | TUPLE_TYPE | SET_TYPE | FUNCTION_TYPE |
							STRUCT_TYPE | VECTOR_TYPE | MATRIX_TYPE | UNION_TYPE | SHAPE_TYPE;

	if (!(rec->base_type & complex_mask)) {
		type_mask_name(rec->base_type, buf, buf_size);
		return;
	}

	switch (rec->base_type) {
	case ARRAY_TYPE: {
		char inner[128];
		type_record_name(rec->as.array_type.element_type, inner, sizeof(inner));
		snprintf(buf, buf_size, "Array[%s]", inner);
		break;
	}
	case TABLE_TYPE: {
		char key[128], val[128];
		type_record_name(rec->as.table_type.key_type, key, sizeof(key));
		type_record_name(rec->as.table_type.value_type, val, sizeof(val));
		snprintf(buf, buf_size, "Table[%s, %s]", key, val);
		break;
	}
	case RESULT_TYPE: {
		char inner[128];
		type_record_name(rec->as.result_type.ok_type, inner, sizeof(inner));
		snprintf(buf, buf_size, "Result[%s]", inner);
		break;
	}
	case TUPLE_TYPE: {
		if (rec->as.tuple_type.element_count == -1) {
			snprintf(buf, buf_size, "Tuple");
		} else {
			int offset = snprintf(buf, buf_size, "Tuple[");
			for (int i = 0; i < rec->as.tuple_type.element_count; i++) {
				char inner[128];
				type_record_name(rec->as.tuple_type.element_types[i], inner, sizeof(inner));
				if (i > 0)
					offset += snprintf(buf + offset, buf_size - offset, ", ");
				offset += snprintf(buf + offset, buf_size - offset, "%s", inner);
			}
			snprintf(buf + offset, buf_size - offset, "]");
		}
		break;
	}
	case SET_TYPE: {
		char inner[128];
		type_record_name(rec->as.set_type.element_type, inner, sizeof(inner));
		snprintf(buf, buf_size, "Set[%s]", inner);
		break;
	}
	case VECTOR_TYPE:
		if (rec->as.vector_type.dimensions == -1)
			snprintf(buf, buf_size, "Vector[]");
		else
			snprintf(buf, buf_size, "Vector[%d]", rec->as.vector_type.dimensions);
		break;
	case MATRIX_TYPE:
		if (rec->as.matrix_type.rows == -1)
			snprintf(buf, buf_size, "Matrix[,]"); // Updated to print [,] explicitly as requested
		else
			snprintf(buf, buf_size, "Matrix[%d, %d]", rec->as.matrix_type.rows, rec->as.matrix_type.cols);
		break;
	case STRUCT_TYPE:
		if (rec->as.struct_type.definition)
			snprintf(buf, buf_size, "Struct %s", rec->as.struct_type.definition->name->chars);
		else
			snprintf(buf, buf_size, "Struct");
		break;
	case FUNCTION_TYPE: {
		int offset = snprintf(buf, buf_size, "fn(");
		for (int i = 0; i < rec->as.function_type.arg_count; i++) {
			char arg[128];
			type_record_name(rec->as.function_type.arg_types[i], arg, sizeof(arg));
			offset += snprintf(buf + offset, buf_size - offset, "%s%s", i == 0 ? "" : ", ", arg);
		}
		char ret[128];
		type_record_name(rec->as.function_type.return_type, ret, sizeof(ret));
		snprintf(buf + offset, buf_size - offset, ") -> %s", ret);
		break;
	}
	case UNION_TYPE: {
		int offset = 0;
		for (int i = 0; i < rec->as.union_type.element_count; i++) {
			char inner[128];
			type_record_name(rec->as.union_type.element_types[i], inner, sizeof(inner));
			// Print ' | ' between types, but not before the first one
			offset += snprintf(buf + offset, buf_size - offset, "%s%s", i == 0 ? "" : " | ", inner);
		}
		break;
	}
	default:
		type_mask_name(rec->base_type, buf, buf_size);
		break;
	}
}

ObjectTypeRecord *type_from_string(VM *vm, ObjectTypeTable *type_table, const char *str)
{
	if (strcmp(str, "Int") == 0)
		return new_type_rec(vm, INT_TYPE);
	if (strcmp(str, "Float") == 0)
		return new_type_rec(vm, FLOAT_TYPE);
	if (strcmp(str, "Bool") == 0)
		return new_type_rec(vm, BOOL_TYPE);
	if (strcmp(str, "Nil") == 0)
		return new_type_rec(vm, NIL_TYPE);
	if (strcmp(str, "String") == 0)
		return new_type_rec(vm, STRING_TYPE);
	if (strncmp(str, "Array", 5) == 0)
		return new_array_type_rec(vm, new_type_rec(vm, ANY_TYPE));
	if (strncmp(str, "Table", 5) == 0)
		return new_table_type_rec(vm, new_type_rec(vm, ANY_TYPE), new_type_rec(vm, ANY_TYPE));
	if (strncmp(str, "Result", 6) == 0)
		return new_result_type_rec(vm, new_type_rec(vm, ANY_TYPE));
	if (strncmp(str, "Function", 8) == 0)
		return new_function_type_rec(vm, NULL, 0, new_type_rec(vm, ANY_TYPE));
	if (strncmp(str, "Tuple", 5) == 0)
		return new_tuple_type_rec(vm, NULL, -1);
	if (strncmp(str, "Set", 3) == 0)
		return new_set_type_rec(vm, new_type_rec(vm, ANY_TYPE));
	if (strncmp(str, "Vector", 6) == 0)
		return new_vector_type_rec(vm, -1);
	if (strncmp(str, "Matrix", 6) == 0)
		return new_matrix_type_rec(vm, -1, -1);

	if (strncmp(str, "Struct ", 7) == 0) {
		if (type_table) {
			ObjectString *name = copy_string(vm, str + 7, strlen(str + 7));
			ObjectTypeRecord *rec = NULL;
			if (type_table_get(type_table, name, &rec)) {
				return rec;
			}
		}
		return new_type_rec(vm, STRUCT_TYPE);
	}

	return new_type_rec(vm, ANY_TYPE);
}

ObjectTypeRecord *strip_type(VM *vm, ObjectTypeRecord *union_type, ObjectTypeRecord *to_remove)
{
	if (!union_type || !to_remove)
		return union_type;

	if (union_type->base_type != UNION_TYPE) {
		if (types_compatible(to_remove, union_type)) {
			return new_type_rec(vm, ANY_TYPE);
		}
		return union_type;
	}

	int keep_count = 0;
	ObjectTypeRecord **keep_types = ALLOCATE(vm, ObjectTypeRecord *, union_type->as.union_type.element_count);

	for (int i = 0; i < union_type->as.union_type.element_count; i++) {
		ObjectTypeRecord *t = union_type->as.union_type.element_types[i];
		if (!types_compatible(to_remove, t)) {
			keep_types[keep_count++] = t;
		}
	}

	if (keep_count == 0) {
		FREE_ARRAY(vm, ObjectTypeRecord *, keep_types, union_type->as.union_type.element_count);
		return new_type_rec(vm, ANY_TYPE);
	}

	if (keep_count == 1) {
		ObjectTypeRecord *res = keep_types[0];
		FREE_ARRAY(vm, ObjectTypeRecord *, keep_types, union_type->as.union_type.element_count);
		return res;
	}

	if (keep_count < union_type->as.union_type.element_count) {
		keep_types = GROW_ARRAY(vm, ObjectTypeRecord *, keep_types, union_type->as.union_type.element_count,
								keep_count);
	}

	ObjectTypeRecord *res = new_union_type_rec(vm, keep_types, NULL, keep_count);
	return res;
}
