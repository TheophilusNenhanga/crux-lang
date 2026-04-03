#ifndef CRUX_STDLIB_OPTION_H
#define CRUX_STDLIB_OPTION_H

#include "value.h"

Value option_is_some_method(VM *vm, const Value *args);
Value option_is_none_method(VM *vm, const Value *args);
Value option_unwrap_method(VM *vm, const Value *args);
Value option_unwrap_or_method(VM *vm, const Value *args);

#endif // CRUX_STDLIB_OPTION_H
