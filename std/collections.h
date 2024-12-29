#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

NativeReturn lengthNative(VM *vm, int argCount, Value *args);

#endif
