#ifndef STRING_H
#define STRING_H

#include "../object.h"

ObjectResult* stringFirstMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringLastMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringGetMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringUpperMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringLowerMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringStripMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringSubstringMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringReplaceMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringSplitMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringContainsMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringStartsWithMethod(VM *vm, int argCount, Value *args);
ObjectResult* stringEndsWithMethod(VM *vm, int argCount, Value *args);

#endif // STRING_H
