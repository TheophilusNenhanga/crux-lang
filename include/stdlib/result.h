#ifndef CRUX_STDLIB_RESULT_H
#define CRUX_STDLIB_RESULT_H

#include "value.h"

Value result_unwrap_method(VM *vm, const Value *args);
Value result_is_ok_method(VM *vm, const Value *args);
Value result_is_err_method(VM *vm, const Value *args);
Value result_unwrap_or_method(VM *vm, const Value *args);

#endif // CRUX_STDLIB_RESULT_H
