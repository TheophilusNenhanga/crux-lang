
#ifndef IO_H
#define IO_H

#include "../object.h"
#include "../value.h"

NativeReturn printNative(VM *vm, int argCount, Value *args);
NativeReturn printlnNative(VM *vm, int argCount, Value *args);

#endif // IO_H
