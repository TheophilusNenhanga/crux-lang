#include "object.h"
#include "value.h"

Value new_buffer_function(VM *vm, const Value *args);

Value write_byte_buffer_method(VM *vm, const Value *args);
Value write_int16_le_buffer_method(VM *vm, const Value *args);
Value write_int32_le_buffer_method(VM *vm, const Value *args);
Value write_float32_le_buffer_method(VM *vm, const Value *args);
Value write_float64_le_buffer_method(VM *vm, const Value *args);
Value write_int16_be_buffer_method(VM *vm, const Value *args);
Value write_int32_be_buffer_method(VM *vm, const Value *args);
Value write_float32_be_buffer_method(VM *vm, const Value *args);
Value write_float64_be_buffer_method(VM *vm, const Value *args);

Value write_string_buffer_method(VM *vm, const Value *args);
Value write_buffer_buffer_method(VM *vm, const Value *args);

Value read_byte_buffer_method(VM *vm, const Value *args);
Value read_string_buffer_method(VM *vm, const Value *args);
Value read_line_buffer_method(VM *vm, const Value *args);
Value read_all_buffer_method(VM *vm, const Value *args);

Value read_int16_le_buffer_method(VM *vm, const Value *args);
Value read_int32_le_buffer_method(VM *vm, const Value *args);
Value read_float32_le_buffer_method(VM *vm, const Value *args);
Value read_float64_le_buffer_method(VM *vm, const Value *args);
Value read_int16_be_buffer_method(VM *vm, const Value *args);
Value read_int32_be_buffer_method(VM *vm, const Value *args);
Value read_float32_be_buffer_method(VM *vm, const Value *args);
Value read_float64_be_buffer_method(VM *vm, const Value *args);

Value capacity_buffer_method(VM *vm, const Value *args);
Value is_empty_buffer_method(VM *vm, const Value *args);
Value clear_buffer_method(VM *vm, const Value *args);
Value peek_byte_buffer_method(VM *vm, const Value *args);
Value skip_bytes_buffer_method(VM *vm, const Value *args);
Value clone_buffer_method(VM *vm, const Value *args);
Value compact_buffer_method(VM *vm, const Value *args);
