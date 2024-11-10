#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

Value lengthNative(int argCount, Value *args);

Value arrayAddNative(int argCount, Value *args);

Value arrayRemoveNative(int argCount, Value *args);

#endif
