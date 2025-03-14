#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

ObjectResult* lengthNative(VM *vm, int argCount, Value *args);
Value lengthNative_(VM *vm, int argCount, Value *args);

/**
 * Returns a string representing the type of a value
 */
Value typeofNative(VM *vm, int argCount, Value *args);

#endif
