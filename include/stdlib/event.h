#ifndef CRUX_STDLIB_EVENT_H
#define CRUX_STDLIB_EVENT_H

#include "vm.h"

Value key_char_function(VM *vm, const Value *args);
Value key_enter_function(VM *vm, const Value *args);
Value key_tab_function(VM *vm, const Value *args);
Value key_backspace_function(VM *vm, const Value *args);
Value key_delete_function(VM *vm, const Value *args);
Value key_escape_function(VM *vm, const Value *args);
Value key_up_function(VM *vm, const Value *args);
Value key_down_function(VM *vm, const Value *args);
Value key_left_function(VM *vm, const Value *args);
Value key_right_function(VM *vm, const Value *args);
Value key_home_function(VM *vm, const Value *args);
Value key_end_function(VM *vm, const Value *args);
Value key_page_up_function(VM *vm, const Value *args);
Value key_page_down_function(VM *vm, const Value *args);
Value key_f_function(VM *vm, const Value *args);
Value key_ctrl_function(VM *vm, const Value *args);
Value key_alt_function(VM *vm, const Value *args);

Value key_is_char_method(VM *vm, const Value *args);
Value key_is_arrow_method(VM *vm, const Value *args);
Value key_is_function_method(VM *vm, const Value *args);
Value key_is_modifier_method(VM *vm, const Value *args);
Value key_char_method(VM *vm, const Value *args);
Value key_name_method(VM *vm, const Value *args);
Value key_equals_method(VM *vm, const Value *args);

Value event_new_function(VM *vm, const Value *args);

Value event_type_method(VM *vm, const Value *args);
Value event_source_method(VM *vm, const Value *args);
Value event_data_method(VM *vm, const Value *args);
Value event_timestamp_method(VM *vm, const Value *args);
Value event_is_key_method(VM *vm, const Value *args);
Value event_is_keydown_method(VM *vm, const Value *args);
Value event_is_keyup_method(VM *vm, const Value *args);
Value event_is_mouse_method(VM *vm, const Value *args);
Value event_is_resize_method(VM *vm, const Value *args);
Value event_key_method(VM *vm, const Value *args);

#endif
