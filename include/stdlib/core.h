#ifndef CORE_H
#define CORE_H

#include "value.h"

Value length_function(VM *vm, const Value *args);
Value int_function(VM *vm, const Value *args);
Value float_function(VM *vm, const Value *args);
Value string_function(VM *vm, const Value *args);
Value array_function(VM *vm, const Value *args);
Value table_function(VM *vm, const Value *args);
Value format_function(VM *vm, const Value *args);

Value iter_function(VM *vm, const Value *args);
Value next_function(VM *vm, const Value *args);

#endif // CORE_H
