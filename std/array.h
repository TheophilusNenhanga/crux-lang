#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"

NativeReturn arrayPushNative(int argCount, Value *args);         // [1,2].push( 3) -> [1,2,3]
NativeReturn arrayPopNative(int argCount, Value *args);          // [1,2,3].pop([1,2,3]) -> 3, [1,2]
NativeReturn arrayInsertNative(int argCount, Value *args);       // [1,3].insert(1, 2) -> [1,2,3]
NativeReturn arrayRemoveAtNative(int argCount, Value *args);     // [1,2,3].remove_at(1) -> [1,3]
NativeReturn arrayConcatNative(int argCount, Value *args);       // [1,2].concat([3,4]) -> [1,2,3,4]
NativeReturn arraySliceNative(int argCount, Value *args);        // [1,2,3].slice(1, 2) -> [2]
NativeReturn arrayReverseNative(int argCount, Value *args);      // [1,2,3].reverse([1,2,3]) -> [3,2,1]
NativeReturn arrayIndexOfNative(int argCount, Value *args);      // [1,2,3].index_of(2) -> 1
NativeReturn arrayContainsNative(int argCount, Value *args);     // [1,2,3].contains(2) -> true
NativeReturn arrayClearNative(int argCount, Value *args);        // [1,2,3].clear([1,2,3]) -> []

#endif //ARRAY_H
