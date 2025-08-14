#ifndef VM_RUN_H
#define VM_RUN_H

#include "vm.h"

InterpretResult run(VM *vm, bool is_anonymous_frame);

#endif // VM_RUN_H
