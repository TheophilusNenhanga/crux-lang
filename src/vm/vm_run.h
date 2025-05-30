#ifndef VM_RUN_H
#define VM_RUN_H

#include "vm.h"

InterpretResult run(VM *vm, const bool isAnonymousFrame);

#endif //VM_RUN_H
