#ifndef ARRAY_H
#define ARRAY_H

#include "../object.h"
#include "../value.h"

ObjectResult *array_push_method(VM *vm, int arg_count,
				const Value *args); // [1,2].push( 3) -> [1,2,3]
ObjectResult *
array_pop_method(VM *vm, int arg_count,
		 const Value *args); // [1,2,3].pop([1,2,3]) -> 3, [1,2]
ObjectResult *
array_insert_method(VM *vm, int arg_count,
		    const Value *args); // [1,3].insert(1, 2) -> [1,2,3]
ObjectResult *
array_remove_at_method(VM *vm, int arg_count,
		       const Value *args); // [1,2,3].remove_at(1) -> [1,3]
ObjectResult *
array_concat_method(VM *vm, int arg_count,
		    const Value *args); // [1,2].concat([3,4]) -> [1,2,3,4]
ObjectResult *
array_slice_method(VM *vm, int arg_count,
		   const Value *args); // [1,2,3].slice(1, 2) -> [2]
ObjectResult *
array_reverse_method(VM *vm, int arg_count,
		     const Value *args); // [1,2,3].reverse([1,2,3]) -> [3,2,1]
ObjectResult *
array_index_of_method(VM *vm, int arg_count,
		      const Value *args); // [1,2,3].index_of(2) -> 1
Value array_contains_method(VM *vm, int arg_count,
			    const Value *args); // [1,2,3].contains(2) -> true
Value array_clear_method(VM *vm, int arg_count,
			 const Value *args); // [1,2,3].clear([1,2,3]) -> []
Value arrayEqualsMethod(VM *vm, int arg_count,
			const Value *args); // [1,2,3].equals([1,2,3]) -> true

ObjectResult *array_map_method(
	VM *vm, int arg_count,
	const Value *args); // [1,2,3].map(fn (x){ return x * 2;}) -> [2,4,6]

ObjectResult *array_filter_method(
	VM *vm, int arg_count,
	const Value
		*args); // [1,2,3].filter(fn (x){ return x % 2 == 0;}) -> [2]

ObjectResult *array_reduce_method(
	VM *vm, int arg_count,
	const Value
		*args); // [1,2,3].reduce(fn (acc, x){ return acc + x;}, 0) -> 6

ObjectResult *array_sort_method(VM *vm, int arg_count,
				const Value *args); // [1,2,3].sort() -> [1,2,3]

ObjectResult *
array_join_method(VM *vm, int arg_count,
		  const Value *args); // [1, 2, 3].join("") -> "123"

#endif // ARRAY_H
