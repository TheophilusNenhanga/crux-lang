#include "object.h"
#include "value.h"

int32_t range_len(const ObjectRange *range);
bool validate_range_values(int32_t start, int32_t step, int32_t end, const char **error_message);

Value new_range_function(VM *vm, const Value *args);

Value contains_range_method(VM *vm, const Value *args);
Value to_array_range_method(VM *vm, const Value *args);
Value start_range_method(VM *vm, const Value *args);
Value end_range_method(VM *vm, const Value *args);
Value step_range_method(VM *vm, const Value *args);
Value is_empty_range_method(VM *vm, const Value *args);
Value reversed_range_method(VM *vm, const Value *args);
