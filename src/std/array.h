#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

ObjectResult* arrayPushMethod(VM *vm, int argCount, Value *args); // [1,2].push( 3) -> [1,2,3]
ObjectResult* arrayPopMethod(VM *vm, int argCount, Value *args); // [1,2,3].pop([1,2,3]) -> 3, [1,2]
ObjectResult* arrayInsertMethod(VM *vm, int argCount, Value *args); // [1,3].insert(1, 2) -> [1,2,3]
ObjectResult* arrayRemoveAtMethod(VM *vm, int argCount, Value *args); // [1,2,3].remove_at(1) -> [1,3]
ObjectResult* arrayConcatMethod(VM *vm, int argCount, Value *args); // [1,2].concat([3,4]) -> [1,2,3,4]
ObjectResult* arraySliceMethod(VM *vm, int argCount, Value *args); // [1,2,3].slice(1, 2) -> [2]
ObjectResult* arrayReverseMethod(VM *vm, int argCount, Value *args); // [1,2,3].reverse([1,2,3]) -> [3,2,1]
ObjectResult* arrayIndexOfMethod(VM *vm, int argCount, Value *args); // [1,2,3].index_of(2) -> 1
Value arrayContainsMethod(VM *vm, int argCount, Value *args); // [1,2,3].contains(2) -> true
Value arrayClearMethod(VM *vm, int argCount, Value *args); // [1,2,3].clear([1,2,3]) -> []


#endif // ARRAY_H
