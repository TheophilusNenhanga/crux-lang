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

Value stringIsAlNumMethod(VM* vm, int argCount, Value* args);
Value stringIsAlphaMethod(VM* vm, int argCount, Value* args);
Value stringIsDigitMethod(VM* vm, int argCount, Value* args);
Value stringIsLowerMethod(VM* vm, int argCount, Value* args);
Value stringIsUpperMethod(VM* vm, int argCount, Value* args);
Value stringIsSpaceMethod(VM* vm, int argCount, Value* args);
Value stringIsEmptyMethod(VM* vm, int argCount, Value* args);

#endif // STRING_H
