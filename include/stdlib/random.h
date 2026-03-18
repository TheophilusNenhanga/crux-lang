#ifndef RANDOM_H
#define RANDOM_H

#include "object.h"
#include "vm.h"

Value random_seed_method(VM *vm, const Value *args);
Value random_int_method(VM *vm, const Value *args);
Value random_float_method(VM *vm, const Value *args);
Value random_bool_method(VM *vm, const Value *args);
Value random_choice_method(VM *vm, const Value *args);

Value random_next_method(VM *vm, const Value *args);
Value random_init_function(VM *vm, const Value *args);
#endif // RANDOM_H
