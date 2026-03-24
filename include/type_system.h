#ifndef CRUX_LANG_TYPE_SYSTEM_H
#define CRUX_LANG_TYPE_SYSTEM_H

#include "object.h"
#include "value.h"
#include "vm.h"

bool runtime_types_compatible(TypeMask expected, Value actual);
void type_mask_name(TypeMask mask, char *buf, int buf_size);
void type_record_name(const ObjectTypeRecord *rec, char *buf, int buf_size);
TypeMask get_type_mask(Value value);

bool type_table_set(ObjectTypeTable *table, ObjectString *key, ObjectTypeRecord *value);
bool type_table_get(const ObjectTypeTable *table, const ObjectString *key, ObjectTypeRecord **value);
bool type_table_delete(const ObjectTypeTable *table, const ObjectString *key);
void type_table_add_all(const ObjectTypeTable *from, ObjectTypeTable *to);

ObjectTypeRecord *new_type_rec(VM *vm, TypeMask base_type);

ObjectTypeRecord *new_array_type_rec(VM *vm, ObjectTypeRecord *element_type);
ObjectTypeRecord *new_table_type_rec(VM *vm, ObjectTypeRecord *key_type, ObjectTypeRecord *value_type);
ObjectTypeRecord *new_result_type_rec(VM *vm, ObjectTypeRecord *ok_type);
ObjectTypeRecord *new_struct_type_rec(VM *vm, ObjectStruct *definition, ObjectTypeTable *field_types, int field_count);
ObjectTypeRecord *new_vector_type_rec(VM *vm, int dimensions);
ObjectTypeRecord *new_tuple_type_rec(VM *vm, ObjectTypeRecord **element_types, int element_count);
ObjectTypeRecord *new_matrix_type_rec(VM *vm, int rows, int cols);
ObjectTypeRecord *new_function_type_rec(VM *vm, ObjectTypeRecord **arg_types, int arg_count,
										ObjectTypeRecord *return_type);
ObjectTypeRecord *new_set_type_rec(VM *vm, ObjectTypeRecord *element_type);
ObjectTypeRecord *new_shape_type_rec(VM *vm, ObjectTypeTable *element_types, int element_count);
ObjectTypeRecord *new_union_type_rec(VM *vm, ObjectTypeRecord **element_types, ObjectString **element_names,
									 int element_count);

bool types_equal(ObjectTypeRecord *a, ObjectTypeRecord *b);
bool types_compatible(ObjectTypeRecord *a, ObjectTypeRecord *b);

ObjectTypeRecord *type_from_string(VM *vm, const ObjectTypeTable *type_table, const char *str);
ObjectTypeRecord *strip_type(VM *vm, ObjectTypeRecord *union_type, ObjectTypeRecord *to_remove);
bool is_numeric_type(ObjectTypeRecord *type);

#endif // CRUX_LANG_TYPE_SYSTEM_H
