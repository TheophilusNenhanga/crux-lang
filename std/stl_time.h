#ifndef TIME_H
#define TIME_H

#include "../value.h"
#include "../object.h"

NativeReturn currentTimeSeconds(int argCount, Value *args);
NativeReturn currentTimeMillis(int argCount, Value *args);

#endif // TIME_H
