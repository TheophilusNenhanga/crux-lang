#ifndef STRING_H
#define STRING_H

#include "../object.h"

NativeReturn stringFirstMethod(VM *vm, int argCount, Value *args);
NativeReturn stringLastMethod(VM *vm, int argCount, Value *args);
NativeReturn stringGetMethod(VM *vm, int argCount, Value *args);
NativeReturn stringUpperMethod(VM *vm, int argCount, Value *args);
NativeReturn stringLowerMethod(VM *vm, int argCount, Value *args);
NativeReturn stringStripMethod(VM *vm, int argCount, Value *args);
NativeReturn stringSubstringMethod(VM *vm, int argCount, Value *args);
NativeReturn stringReplaceMethod(VM *vm, int argCount, Value *args);
NativeReturn stringSplitMethod(VM *vm, int argCount, Value *args);
NativeReturn stringContainsMethod(VM *vm, int argCount, Value *args);
NativeReturn stringStartsWithMethod(VM *vm, int argCount, Value *args);
NativeReturn stringEndsWithMethod(VM *vm, int argCount, Value *args);

#endif // STRING_H
