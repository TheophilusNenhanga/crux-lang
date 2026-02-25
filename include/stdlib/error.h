#ifndef ERROR_H
#define ERROR_H

#include "object.h"
#include "value.h"

Value error_function(VM *vm, const Value *args);
Value panic_function(VM *vm, const Value *args);
Value assert_function(VM *vm, const Value *args);
Value error_type_method(VM *vm, const Value *args);
Value err_function(VM *vm, const Value *args);
Value ok_function(VM *vm, const Value *args);

Value unwrap_function(VM *vm, const Value *args);
Value error_message_method(VM *vm, const Value *args);

#endif // ERROR_H
