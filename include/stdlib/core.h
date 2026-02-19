#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "object.h"
#include "value.h"

Value length_function(VM *vm, int arg_count, const Value *args);
Value int_function(VM *vm, int arg_count, const Value *args);
Value float_function(VM *vm, int arg_count, const Value *args);
Value string_function(VM *vm, int arg_count, const Value *args);
Value array_function(VM *vm, int arg_count, const Value *args);
Value table_function(VM *vm, int arg_count, const Value *args);

Value length_function_(VM *vm, int arg_count, const Value *args);
Value int_function_(VM *vm, int arg_count, const Value *args);
Value float_function_(VM *vm, int arg_count, const Value *args);
Value string_function_(VM *vm, int arg_count, const Value *args);
Value array_function_(VM *vm, int arg_count, const Value *args);
Value table_function_(VM *vm, int arg_count, const Value *args);

Value format_function(VM *vm, int arg_count, const Value *args);

#endif
