#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"

NativeReturn arrayPushMethod(int argCount, Value *args);         // [1,2].push( 3) -> [1,2,3]
NativeReturn arrayPopMethod(int argCount, Value *args);          // [1,2,3].pop([1,2,3]) -> 3, [1,2]
NativeReturn arrayInsertMethod(int argCount, Value *args);       // [1,3].insert(1, 2) -> [1,2,3]
NativeReturn arrayRemoveAtMethod(int argCount, Value *args);     // [1,2,3].remove_at(1) -> [1,3]
NativeReturn arrayConcatMethod(int argCount, Value *args);       // [1,2].concat([3,4]) -> [1,2,3,4]
NativeReturn arraySliceMethod(int argCount, Value *args);        // [1,2,3].slice(1, 2) -> [2]
NativeReturn arrayReverseMethod(int argCount, Value *args);      // [1,2,3].reverse([1,2,3]) -> [3,2,1]
NativeReturn arrayIndexOfMethod(int argCount, Value *args);      // [1,2,3].index_of(2) -> 1
NativeReturn arrayContainsMethod(int argCount, Value *args);     // [1,2,3].contains(2) -> true
NativeReturn arrayClearMethod(int argCount, Value *args);        // [1,2,3].clear([1,2,3]) -> []



#endif //ARRAY_H
