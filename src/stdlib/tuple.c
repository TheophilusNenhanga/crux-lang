#include "stdlib/tuple.h"
#include "object.h"
#include "panic.h"

/**
 * Creates a new tuple from an array.
 * arg0 -> array
 * returns -> Tuple
 */
Value new_tuple_function(VM *vm, const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectTuple *tuple = new_tuple(vm, array->size);
	for (uint32_t i = 0; i < array->size; i++) {
		tuple->elements[i] = array->values[i];
	}
	tuple->size = array->size;
	return OBJECT_VAL(tuple);
}

/**
 * Checks if a tuple is empty.
 * arg0 -> tuple
 * returns -> Bool
 */
Value is_empty_tuple_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	return BOOL_VAL(tuple->size == 0);
}

/**
 * Checks if a tuple contains a value.
 * arg0 -> tuple
 * arg1 -> value
 * returns -> Bool
 */
Value contains_tuple_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	Value value = args[1];
	for (uint32_t i = 0; i < tuple->size; i++) {
		if (values_equal(tuple->elements[i], value)) {
			return BOOL_VAL(true);
		}
	}
	return BOOL_VAL(false);
}

/**
 * Converts a tuple to an array.
 * arg0 -> tuple
 * returns -> Array
 */
Value to_array_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	ObjectArray *array = new_array(vm, tuple->size);
	for (uint32_t i = 0; i < tuple->size; i++) {
		array->values[i] = tuple->elements[i];
	}
	array->size = tuple->size;
	return OBJECT_VAL(array);
}

/**
 * Gets the first element of a tuple.
 * arg0 -> tuple
 * returns -> Value
 */
Value first_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	if (tuple->size == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot get first element of empty tuple", BOUNDS);
	}
	ObjectResult *result = new_ok_result(vm, tuple->elements[0]);
	return OBJECT_VAL(result);
}

/**
 * Gets the last element of a tuple.
 * arg0 -> tuple
 * returns -> Value
 */
Value last_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	if (tuple->size == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot get last element of empty tuple", BOUNDS);
	}
	ObjectResult *result = new_ok_result(vm, tuple->elements[tuple->size - 1]);
	return OBJECT_VAL(result);
}

/**
 * Checks if two tuples are equal.
 * arg0 -> tuple1
 * arg1 -> tuple2
 * returns -> Bool
 */
Value equals_tuple_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectTuple *tuple1 = AS_CRUX_TUPLE(args[0]);
	ObjectTuple *tuple2 = AS_CRUX_TUPLE(args[1]);
	if (tuple1->size != tuple2->size) {
		return BOOL_VAL(false);
	}
	for (uint32_t i = 0; i < tuple1->size; i++) {
		if (!values_equal(tuple1->elements[i], tuple2->elements[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

/**
 * Gets an element from a tuple.
 * arg0 -> tuple
 * arg1 -> index
 * returns -> Value
 */
Value get_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	uint32_t index = AS_INT(args[1]);
	if (index >= tuple->size) {
		return MAKE_GC_SAFE_ERROR(vm, "Index out of bounds", BOUNDS);
	}
	ObjectResult *result = new_ok_result(vm, tuple->elements[index]);
	return OBJECT_VAL(result);
}

/**
 * Slices a tuple.
 * arg0 -> tuple
 * arg1 -> start
 * arg2 -> end
 * returns -> Result<Array<Value>>
 */
Value slice_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	uint32_t start = AS_INT(args[1]);
	uint32_t end = AS_INT(args[2]);
	if (start < 0 || end > tuple->size || start > end) {
		return MAKE_GC_SAFE_ERROR(vm, "Invalid slice range", BOUNDS);
	}
	ObjectArray *array = new_array(vm, end - start);
	for (uint32_t i = start; i < end; i++) {
		array->values[i - start] = tuple->elements[i];
	}
	array->size = end - start;
	push(vm->current_module_record, OBJECT_VAL(array));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(array));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Gets the index of a value in a tuple.
 * arg0 -> tuple
 * arg1 -> value
 * returns -> Int
 */
Value index_tuple_method(VM *vm, const Value *args)
{
	ObjectTuple *tuple = AS_CRUX_TUPLE(args[0]);
	Value value = args[1];
	for (uint32_t i = 0; i < tuple->size; i++) {
		if (values_equal(tuple->elements[i], value)) {
			ObjectResult *result = new_ok_result(vm, INT_VAL(i));
			return OBJECT_VAL(result);
		}
	}
	return MAKE_GC_SAFE_ERROR(vm, "Value not found", VALUE);
}
