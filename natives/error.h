#ifndef ERROR_H
#define ERROR_H

#include "../value.h"
#include "../object.h"

NativeReturn errorNative(int argCount, Value *args);
NativeReturn panicNative(int argCount, Value *args);

#endif //ERROR_H
