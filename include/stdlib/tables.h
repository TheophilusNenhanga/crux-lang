#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

Value table_values_method(VM *vm, const Value *args);
Value table_keys_method(VM *vm, const Value *args);
Value table_pairs_method(VM *vm, const Value *args);
Value table_remove_method(VM *vm, const Value *args);
Value table_get_method(VM *vm, const Value *args);

Value table_has_key_method(VM *vm, const Value *args);
Value table_get_or_else_method(VM *vm, const Value *args);

#endif // TABLES_H
