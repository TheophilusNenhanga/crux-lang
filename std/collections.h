#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../value.h"
#include "../object.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

NativeReturn lengthNative(VM* vm, int argCount, Value *args);

#endif
