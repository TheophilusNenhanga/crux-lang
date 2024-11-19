#ifndef ERROR_H
#define ERROR_H

#include "../value.h"

Value errorNative(int argCount, Value *args);
Value panicNative(int argCount, Value *args);

#endif //ERROR_H
