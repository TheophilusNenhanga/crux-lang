#ifndef STRING_H
#define STRING_H

#include "object.h"

Value string_byte_length_method(VM *vm, const Value *args);
Value string_first_method(VM *vm, const Value *args);
Value string_last_method(VM *vm, const Value *args);
Value string_get_method(VM *vm, const Value *args);
Value string_is_upper_method(VM *vm, const Value *args);
Value string_is_lower_method(VM *vm, const Value *args);
Value string_strip_method(VM *vm, const Value *args);
Value string_substring_method(VM *vm, const Value *args);
Value string_replace_method(VM *vm, const Value *args);
Value string_split_method(VM *vm, const Value *args);
Value string_contains_method(VM *vm, const Value *args);
Value string_starts_with_method(VM *vm, const Value *args);
Value string_ends_with_method(VM *vm, const Value *args);
Value string_concat_method(VM *vm, const Value *args);

Value string_to_upper_method(VM *vm, const Value *args);
Value string_to_lower_method(VM *vm, const Value *args);

Value string_is_al_num_method(VM *vm, const Value *args);
Value string_is_alpha_method(VM *vm, const Value *args);
Value string_is_digit_method(VM *vm, const Value *args);
Value string_reverse_method(VM *vm, const Value *args);
Value string_find_method(VM *vm, const Value *args);
Value string_repeat_method(VM *vm, const Value *args);
Value string_join_method(VM *vm, const Value *args);

Value string_pad_left_method(VM *vm, const Value *args);
Value string_pad_right_method(VM *vm, const Value *args);
Value string_count_method(VM *vm, const Value *args);
Value string_is_empty_method(VM *vm, const Value *args);
Value string_is_space_method(VM *vm, const Value *args);
#endif // STRING_H
