
#ifndef IO_H
#define IO_H

#include "../value.h"
#include "../object.h"

NativeReturn printNative(int argCount, Value *args);
NativeReturn printlnNative(int argCount, Value *args);

#endif // IO_H
