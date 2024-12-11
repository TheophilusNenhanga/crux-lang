#ifndef STRING_H
#define STRING_H

#include "../object.h"

NativeReturn stringFirstMethod(int argCount, Value *args);
NativeReturn stringLastMethod(int argCount, Value *args);
NativeReturn stringGetMethod(int argCount, Value *args);
NativeReturn stringUpperMethod(int argCount, Value *args);
NativeReturn stringLowerMethod(int argCount, Value *args);
NativeReturn stringStripMethod(int argCount, Value *args);
NativeReturn stringSubstringMethod(int argCount, Value *args);    // substring("hello", 1, 3) -> "el"
NativeReturn stringReplaceMethod(int argCount, Value *args);      // replace("hello", "l", "w") -> "hewwo"
NativeReturn stringSplitMethod(int argCount, Value *args);        // split("a,b,c", ",") -> ["a", "b", "c"]
NativeReturn stringContainsMethod(int argCount, Value *args);     // contains("hello", "ll") -> true
NativeReturn stringStartsWithMethod(int argCount, Value *args);
NativeReturn stringEndsWithMethod(int argCount, Value *args);

#endif //STRING_H
