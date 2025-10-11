#ifndef RANDOM_H
#define RANDOM_H

#include "../object.h"
#include "../vm/vm.h"

ObjectResult *random_seed_method(VM *vm, int arg_count, const Value *args);
ObjectResult *random_int_method(VM *vm, int arg_count, const Value *args);
ObjectResult *random_double_method(VM *vm, int arg_count, const Value *args);
ObjectResult *random_bool_method(VM *vm, int arg_count, const Value *args);
ObjectResult *random_choice_method(VM *vm, int arg_count, const Value *args);

Value random_next_method(VM *vm, int arg_count, const Value *args);
Value random_init_function(VM *vm, int arg_count, const Value *args);
#endif // RANDOM_H
