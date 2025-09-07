#ifndef SYS_H
#define SYS_H

#include "../object.h"

ObjectResult *args_function(VM *vm, int arg_count, const Value *args);
Value platform_function(VM *vm, int arg_count, const Value *args);
Value arch_function(VM *vm, int arg_count, const Value *args);
Value pid_function(VM *vm, int arg_count, const Value *args);
ObjectResult *get_env_function(VM *vm, int arg_count, const Value *args);
ObjectResult *sleep_function(VM *vm, int arg_count, const Value *args);
Value exit_function(VM *vm, int arg_count, const Value *args);

#endif // SYS_H
