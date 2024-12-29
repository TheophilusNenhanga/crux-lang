#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

NativeReturn arrayPushMethod(VM* vm, int argCount, Value *args);         // [1,2].push( 3) -> [1,2,3]
NativeReturn arrayPopMethod(VM* vm, int argCount, Value *args);          // [1,2,3].pop([1,2,3]) -> 3, [1,2]
NativeReturn arrayInsertMethod(VM* vm, int argCount, Value *args);       // [1,3].insert(1, 2) -> [1,2,3]
NativeReturn arrayRemoveAtMethod(VM* vm, int argCount, Value *args);     // [1,2,3].remove_at(1) -> [1,3]
NativeReturn arrayConcatMethod(VM* vm, int argCount, Value *args);       // [1,2].concat([3,4]) -> [1,2,3,4]
NativeReturn arraySliceMethod(VM* vm, int argCount, Value *args);        // [1,2,3].slice(1, 2) -> [2]
NativeReturn arrayReverseMethod(VM* vm, int argCount, Value *args);      // [1,2,3].reverse([1,2,3]) -> [3,2,1]
NativeReturn arrayIndexOfMethod(VM* vm, int argCount, Value *args);      // [1,2,3].index_of(2) -> 1
NativeReturn arrayContainsMethod(VM* vm, int argCount, Value *args);     // [1,2,3].contains(2) -> true
NativeReturn arrayClearMethod(VM* vm, int argCount, Value *args);        // [1,2,3].clear([1,2,3]) -> []



#endif //ARRAY_H
