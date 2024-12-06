#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../value.h"
#include "../object.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

NativeReturn lengthNative(int argCount, Value *args);

NativeReturn arrayAddNative(int argCount, Value *args);

NativeReturn arrayRemoveNative(int argCount, Value *args);

#endif
