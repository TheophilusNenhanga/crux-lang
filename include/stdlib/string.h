#ifndef STRING_H
#define STRING_H

#include "object.h"

Value string_first_method(VM *vm, int arg_count, const Value *args);
Value string_last_method(VM *vm, int arg_count, const Value *args);
Value string_get_method(VM *vm, int arg_count, const Value *args);
Value string_upper_method(VM *vm, int arg_count, const Value *args);
Value string_lower_method(VM *vm, int arg_count, const Value *args);
Value string_strip_method(VM *vm, int arg_count, const Value *args);
Value string_substring_method(VM *vm, int arg_count, const Value *args);
Value string_replace_method(VM *vm, int arg_count, const Value *args);
Value string_split_method(VM *vm, int arg_count, const Value *args);
Value string_contains_method(VM *vm, int arg_count, const Value *args);
Value string_starts_with_method(VM *vm, int arg_count,
					const Value *args);
Value string_ends_with_method(VM *vm, int arg_count, const Value *args);
Value string_concat_method(VM *vm, int arg_count, const Value *args);

Value string_is_al_num_method(VM *vm, int arg_count, const Value *args);
Value string_is_alpha_method(VM *vm, int arg_count, const Value *args);
Value string_is_digit_method(VM *vm, int arg_count, const Value *args);
Value string_is_lower_method(VM *vm, int arg_count, const Value *args);
Value string_is_upper_method(VM *vm, int arg_count, const Value *args);
Value string_is_space_method(VM *vm, int arg_count, const Value *args);
Value string_is_empty_method(VM *vm, int arg_count, const Value *args);

#endif // STRING_H
