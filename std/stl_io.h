#ifndef STL_IO_H
#define STL_IO_H

#include "../value.h"
#include "std.h"

NativeReturn _write(VM *vm, int argCount, Value *args);
NativeReturn _writeln(VM *vm, int argCount, Value *args);
NativeReturn _read(VM *vm, int argCount, Value *args);
NativeReturn _readln(VM *vm, int argCount, Value *args);

#endif //STL_IO_H
