#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"


ObjectResult *arrayPushMethod(VM *vm, int argCount,
                              Value *args); // [1,2].push( 3) -> [1,2,3]
ObjectResult *arrayPopMethod(VM *vm, int argCount,
                             Value *args); // [1,2,3].pop([1,2,3]) -> 3, [1,2]
ObjectResult *arrayInsertMethod(VM *vm, int argCount,
                                Value *args); // [1,3].insert(1, 2) -> [1,2,3]
ObjectResult *arrayRemoveAtMethod(VM *vm, int argCount,
                                  Value *args); // [1,2,3].remove_at(1) -> [1,3]
ObjectResult *
arrayConcatMethod(VM *vm, int argCount,
                  Value *args); // [1,2].concat([3,4]) -> [1,2,3,4]
ObjectResult *arraySliceMethod(VM *vm, int argCount,
                               Value *args); // [1,2,3].slice(1, 2) -> [2]
ObjectResult *
arrayReverseMethod(VM *vm, int argCount,
                   Value *args); // [1,2,3].reverse([1,2,3]) -> [3,2,1]
ObjectResult *arrayIndexOfMethod(VM *vm, int argCount,
                                 Value *args); // [1,2,3].index_of(2) -> 1
Value arrayContainsMethod(VM *vm, int argCount,
                          Value *args); // [1,2,3].contains(2) -> true
Value arrayClearMethod(VM *vm, int argCount,
                       Value *args); // [1,2,3].clear([1,2,3]) -> []
Value arrayEqualsMethod(VM *vm, int argCount,
                        Value *args); // [1,2,3].equals([1,2,3]) -> true

ObjectResult* arrayMapMethod(VM *vm, int argCount,
                             Value *args); // [1,2,3].map(fn (x){ return x * 2;}) -> [2,4,6]

ObjectResult* arrayFilterMethod(VM *vm, int argCount,
                                Value *args); // [1,2,3].filter(fn (x){ return x % 2 == 0;}) -> [2]

ObjectResult* arrayReduceMethod(VM *vm, int argCount,
                                Value *args); // [1,2,3].reduce(fn (acc, x){ return acc + x;}, 0) -> 6

ObjectResult* arraySortMethod(VM *vm, int argCount,
                      Value *args); // [1,2,3].sort() -> [1,2,3]

ObjectResult* arrayJoinMethod(VM *vm, int argCount, Value *args); // [1, 2, 3].join("") -> "123"

#endif // ARRAY_H
