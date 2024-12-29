#ifndef TIME_H
#define TIME_H

#include "../object.h"
#include "../value.h"

NativeReturn currentTimeSeconds(VM *vm, int argCount, Value *args);
NativeReturn currentTimeMillis(VM *vm, int argCount, Value *args);

#endif // TIME_H
