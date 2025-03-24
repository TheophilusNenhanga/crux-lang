#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

ObjectResult* lengthNative(VM *vm, int argCount, Value *args);
Value lengthNative_(VM *vm, int argCount, Value *args);

/**
 * Returns a string representing the value's type
 */
Value typeNative(VM *vm, int argCount, Value *args);

ObjectResult* numberNative(VM *vm, int argCount, Value *args);
ObjectResult* stringNative(VM *vm, int argCount, Value *args);
ObjectResult* arrayNative(VM *vm, int argCount, Value *args);
ObjectResult* tableNative(VM *vm, int argCount, Value *args);

Value numberNative_(VM *vm, int argCount, Value *args);
Value stringNative_(VM *vm, int argCount, Value *args);
Value arrayNative_(VM *vm, int argCount, Value *args);
Value tableNative_(VM *vm, int argCount, Value *args);

#endif
