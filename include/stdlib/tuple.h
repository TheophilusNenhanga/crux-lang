#include "value.h"
#include "object.h"

Value new_tuple_function(VM* vm, const Value* args);

Value is_empty_tuple_method(VM* vm, const Value* args);
Value contains_tuple_method(VM* vm, const Value* args);
Value to_array_tuple_method(VM* vm, const Value* args);
Value first_tuple_method(VM* vm, const Value* args);
Value last_tuple_method(VM* vm, const Value* args);
Value equals_tuple_method(VM* vm, const Value* args);
Value get_tuple_method(VM* vm, const Value* args);
Value slice_tuple_method(VM* vm, const Value* args);
Value index_tuple_method(VM* vm, const Value* args);
