#include "object.h"
#include "value.h"

Value new_set_function(VM *vm, const Value *args);
Value add_set_method(VM *vm, const Value *args);
Value remove_set_method(VM *vm, const Value *args);
Value discard_set_method(VM *vm, const Value *args);
Value union_set_method(VM *vm, const Value *args);
Value intersection_set_method(VM *vm, const Value *args);
Value difference_set_method(VM *vm, const Value *args);
Value sym_difference_set_method(VM *vm, const Value *args);
Value is_subset_set_method(VM *vm, const Value *args);
Value is_superset_set_method(VM *vm, const Value *args);
Value is_disjoint_set_method(VM *vm, const Value *args);

Value contains_set_method(VM *vm, const Value *args);
Value is_empty_set_method(VM *vm, const Value *args);
Value to_array_set_method(VM *vm, const Value *args);
Value clone_set_method(VM *vm, const Value *args);
