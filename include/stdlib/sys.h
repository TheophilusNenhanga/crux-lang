#ifndef SYS_H
#define SYS_H

#include "../object.h"

Value args_function(VM *vm, const Value *args);
Value platform_function(VM *vm, const Value *args);
Value arch_function(VM *vm, const Value *args);
Value pid_function(VM *vm, const Value *args);
Value get_env_function(VM *vm, const Value *args);
Value sleep_function(VM *vm, const Value *args);
Value exit_function(VM *vm, const Value *args);

#endif // SYS_H
