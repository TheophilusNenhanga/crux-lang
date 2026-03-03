#ifndef CRUX_LANG_TYPE_SYSTEM_H
#define CRUX_LANG_TYPE_SYSTEM_H

#include "garbage_collector.h"
#include "object.h"
#include "value.h"

#define TYPE_ARENA_CAPACITY (256 * 1024)

typedef struct TypeRecord TypeRecord;

typedef struct TypeEntry {
	ObjectString *key;
	TypeRecord *value;
} TypeEntry;

typedef struct {
	TypeEntry *entries;
	int count;
	int capacity;
} TypeTable;

bool runtime_types_compatible(TypeMask expected, Value actual);
void type_mask_name(TypeMask mask, char *buf, int buf_size);
TypeMask get_type_mask(Value value);

void init_type_table(TypeTable *table);
void free_type_table(TypeTable *table);
bool type_table_set(TypeTable *table, ObjectString *key, TypeRecord *value);
bool type_table_get(const TypeTable *table, const ObjectString *key,
		    TypeRecord **value);
bool type_table_delete(const TypeTable *table, const ObjectString *key);
void type_table_add_all(const TypeTable *from, TypeTable *to);

typedef struct {
	uint8_t data[TYPE_ARENA_CAPACITY];
	size_t used;
} TypeArena;

struct TypeRecord {
	TypeMask base_type;
	union {
		struct {
			TypeRecord *element_type;
		} array_type;
		struct {
			TypeRecord *key_type; // must be Hashable
			TypeRecord *value_type;
		} table_type;
		struct {
			TypeRecord *ok_type;
		} result_type;
		struct {
			TypeTable *field_types;
			int field_count;
			ObjectStruct *definition; // has the field names
		} struct_type;
		struct {
			// values within a vector are always doubles so Int |
			// Float doesn't matter
			int dimensions;
		} vector_type;
		struct {
			TypeRecord *element_type;
		} tuple_type;
		struct {
			// values with a matrix are always doubles so Int |
			// Float doesn't matter
			int rows;
			int cols;
		} matrix_type;
		struct {
			TypeRecord **arg_types;
			int arg_count;
			TypeRecord *return_type;
		} function_type;
		struct {
			TypeRecord *element_type;
		} set_type;
		struct {
			TypeRecord **element_types;
			ObjectString **element_names;
			int element_count;
		} union_type;
		struct {
			TypeTable *element_types;
			int element_count;
		} shape_type;
	} as;
};

TypeRecord *new_type_rec(TypeArena *arena, TypeMask base_type);

TypeRecord *new_array_type_rec(TypeArena *arena, TypeRecord *element_type);
TypeRecord *new_table_type_rec(TypeArena *arena, TypeRecord *key_type,
			       TypeRecord *value_type);
TypeRecord *new_result_type_rec(TypeArena *arena, TypeRecord *ok_type);
TypeRecord *new_struct_type_rec(TypeArena *arena, ObjectStruct *definition,
				TypeTable *field_types, int field_count);
TypeRecord *new_vector_type_rec(TypeArena *arena, int dimensions);
TypeRecord *new_tuple_type_rec(TypeArena *arena, TypeRecord *element_type);
TypeRecord *new_matrix_type_rec(TypeArena *arena, int rows, int cols);
TypeRecord *new_function_type_rec(TypeArena *arena, TypeRecord **arg_types,
				  int arg_count, TypeRecord *return_type);
TypeRecord *new_set_type_rec(TypeArena *arena, TypeRecord *element_type);
TypeRecord *new_shape_type_rec(TypeArena *arena, TypeTable *element_types,
			       int element_count);
TypeRecord *new_union_type_rec(TypeArena *arena, TypeRecord **element_types,
			       ObjectString **element_names, int element_count);

bool types_equal(TypeRecord *a, TypeRecord *b);
bool types_compatible(TypeRecord *a, TypeRecord *b);

void type_arena_reset(TypeArena *arena);
TypeRecord *copy_type_rec_to_arena(TypeArena *dest, TypeRecord *src);

#endif // CRUX_LANG_TYPE_SYSTEM_H
