#ifndef ERROR_H
#define ERROR_H

#include "../object.h"
#include "../value.h"

ObjectResult *error_function(VM *vm, int arg_count, const Value *args);
ObjectResult *panic_function(VM *vm, int arg_count, const Value *args);
ObjectResult *assert_function(VM *vm, int arg_count, const Value *args);
ObjectResult *error_type_method(VM *vm, int arg_count, const Value *args);
ObjectResult *err_function(VM *vm, int arg_count, const Value *args);
ObjectResult *ok_function(VM *vm, int arg_count, const Value *args);

Value unwrap_function(VM *vm, int arg_count, const Value *args);
Value error_message_method(VM *vm, int arg_count, const Value *args);

#endif // ERROR_H
