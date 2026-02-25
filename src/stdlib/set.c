#include "stdlib/set.h"
#include <stdint.h>
#include "common.h"
#include "object.h"
#include "panic.h"
#include "value.h"

/**
 * Creates a new set.
 * arg0 -> array: Array[Hashable]
 * returns -> Set
 */
Value new_set_function(VM *vm, const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectSet *set = new_set(vm, INITIAL_SET_CAPACITY);
	for (uint32_t i = 0; i < array->size; i++) {
		Value value = array->values[i];
		if (!IS_CRUX_HASHABLE(value)) {
			return MAKE_GC_SAFE_ERROR(
				vm, "All set elements must be hashable.", TYPE);
		}
		object_table_set(vm, set->entries, value, NIL_VAL);
	}
	return OBJECT_VAL(set);
}

/**
 * Adds a value to a set.
 * arg0 -> set: Set
 * arg1 -> value: Hashable
 * returns -> Value
 */
Value add_set_method(VM *vm, const Value *args)
{
	ObjectSet *set = AS_CRUX_SET(args[0]);
	Value value = args[1];
	if (!IS_CRUX_HASHABLE(value)) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "All set elements must be hashable.",
					  TYPE);
	}
	object_table_set(
		vm, set->entries, value,
		NIL_VAL); // TODO: ensure that this is a unique addition
	return value;
}

/**
 * Removes a value from a set.
 * arg0 -> set: Set
 * arg1 -> value: Hashable
 * returns -> Value
 */
Value remove_set_method(VM *vm, const Value *args)
{
	ObjectSet *set = AS_CRUX_SET(args[0]);
	Value value = args[1];
	if (!IS_CRUX_HASHABLE(value)) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "All set elements must be hashable.",
					  TYPE);
	}
	object_table_remove(set->entries, value);
	return value;
}

/**
 * Removes a value from a set if it exists.
 * Does not error if the value is not in the set.
 * arg0 -> set: Set
 * arg1 -> value: Hashable
 * returns -> Nil
 */
Value discard_set_method(VM *vm, const Value *args)
{
	ObjectSet *set = AS_CRUX_SET(args[0]);
	Value value = args[1];
	if (!IS_CRUX_HASHABLE(value)) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "All set elements must be hashable.",
					  TYPE);
	}
	object_table_remove(set->entries, value);
	return NIL_VAL;
}

/**
 * Returns the union of two sets.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Result<Set>
 */
Value union_set_method(VM *vm, const Value *args)
{
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);

	uint32_t new_size = set1->entries->size + set2->entries->size;
	if (new_size < set1->entries->size || new_size < set2->entries->size) {
		return MAKE_GC_SAFE_ERROR(vm, "Resultant set size is too large",
					  VALUE);
	}

	ObjectSet *result_set = new_set(vm, new_size);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			object_table_set(vm, result_set->entries,
					 set1->entries->entries[i].key,
					 NIL_VAL);
		}
	}
	for (size_t i = 0; i < set2->entries->capacity; i++) {
		if (set2->entries->entries[i].is_occupied) {
			object_table_set(vm, result_set->entries,
					 set2->entries->entries[i].key,
					 NIL_VAL);
		}
	}
	push(vm->current_module_record, OBJECT_VAL(result_set));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_set));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Returns the intersection of two sets.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Set
 */
Value intersection_set_method(VM *vm, const Value *args)
{
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	ObjectSet *result = new_set(vm, set1->entries->size);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			Value key = set1->entries->entries[i].key;
			Value v;
			if (object_table_get(set2->entries->entries,
					     set2->entries->size,
					     set2->entries->capacity, key,
					     &v)) {
				object_table_set(vm, result->entries, key,
						 NIL_VAL);
			}
		}
	}
	return OBJECT_VAL(result);
}

/**
 * Returns the difference of two sets.
 * Elements in set1 but not in set2.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Set
 */
Value difference_set_method(VM *vm, const Value *args)
{
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	ObjectSet *result = new_set(vm, set1->entries->size);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			Value key = set1->entries->entries[i].key;
			Value v;
			if (!object_table_get(set2->entries->entries,
					      set2->entries->size,
					      set2->entries->capacity, key,
					      &v)) {
				object_table_set(vm, result->entries, key,
						 NIL_VAL);
			}
		}
	}
	return OBJECT_VAL(result);
}

/**
 * Returns the symmetric difference of two sets.
 * Elements in either set but not in both.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Set
 */
Value sym_difference_set_method(VM *vm, const Value *args)
{
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	ObjectSet *result = new_set(vm, set1->entries->size);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			Value key = set1->entries->entries[i].key;
			Value v;
			if (!object_table_get(set2->entries->entries,
					      set2->entries->size,
					      set2->entries->capacity, key,
					      &v)) {
				object_table_set(vm, result->entries, key,
						 NIL_VAL);
			}
		}
	}
	for (size_t i = 0; i < set2->entries->capacity; i++) {
		if (set2->entries->entries[i].is_occupied) {
			Value key = set2->entries->entries[i].key;
			Value v;
			if (!object_table_get(set1->entries->entries,
					      set1->entries->size,
					      set1->entries->capacity, key,
					      &v)) {
				object_table_set(vm, result->entries, key,
						 NIL_VAL);
			}
		}
	}
	return OBJECT_VAL(result);
}

/**
 * Checks if set1 is a subset of set2.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Bool
 */
Value is_subset_set_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			Value key = set1->entries->entries[i].key;
			Value v;
			if (!object_table_get(set2->entries->entries,
					      set2->entries->size,
					      set2->entries->capacity, key,
					      &v)) {
				return BOOL_VAL(false);
			}
		}
	}
	return BOOL_VAL(true);
}

/**
 * Checks if set1 is a superset of set2.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Bool
 */
Value is_superset_set_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	for (size_t i = 0; i < set2->entries->capacity; i++) {
		if (set2->entries->entries[i].is_occupied) {
			Value key = set2->entries->entries[i].key;
			Value v;
			if (!object_table_get(set1->entries->entries,
					      set1->entries->size,
					      set1->entries->capacity, key,
					      &v)) {
				return BOOL_VAL(false);
			}
		}
	}
	return BOOL_VAL(true);
}

/**
 * Checks if two sets are disjoint.
 * arg0 -> set1: Set
 * arg1 -> set2: Set
 * returns -> Bool
 */
Value is_disjoint_set_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectSet *set1 = AS_CRUX_SET(args[0]);
	ObjectSet *set2 = AS_CRUX_SET(args[1]);
	for (size_t i = 0; i < set1->entries->capacity; i++) {
		if (set1->entries->entries[i].is_occupied) {
			Value key = set1->entries->entries[i].key;
			Value v;
			if (object_table_get(set2->entries->entries,
					     set2->entries->size,
					     set2->entries->capacity, key,
					     &v)) {
				return BOOL_VAL(false);
			}
		}
	}
	return BOOL_VAL(true);
}

/**
 * Checks if a set contains a value.
 * arg0 -> set: Set
 * arg1 -> value: Hashable
 * returns -> Bool
 */
Value contains_set_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectSet *set = AS_CRUX_SET(args[0]);
	Value value = args[1];
	Value v;
	return BOOL_VAL(object_table_get(set->entries->entries,
					 set->entries->size,
					 set->entries->capacity, value, &v));
}

/**
 * Checks if a set is empty.
 * arg0 -> set: Set
 * returns -> Bool
 */
Value is_empty_set_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectSet *set = AS_CRUX_SET(args[0]);
	return BOOL_VAL(set->entries->size == 0);
}

/**
 * Converts a set to an array.
 * arg0 -> set: Set
 * returns -> Array
 */
Value to_array_set_method(VM *vm, const Value *args)
{
	ObjectSet *self = AS_CRUX_SET(args[0]);
	ObjectArray *array = new_array(vm, self->entries->size);
	size_t index = 0;
	for (size_t i = 0; i < self->entries->capacity; i++) {
		if (self->entries->entries[i].is_occupied) {
			array_add_back(vm, array,
				       self->entries->entries[i].key);
		}
	}
	return OBJECT_VAL(array);
}

/**
 * Creates a shallow copy of a set.
 * arg0 -> set: Set
 * returns -> Set
 */
Value clone_set_method(VM *vm, const Value *args)
{
	ObjectSet *self = AS_CRUX_SET(args[0]);
	ObjectSet *other = new_set(vm, self->entries->size);
	for (size_t i = 0; i < self->entries->capacity; i++) {
		if (self->entries->entries[i].is_occupied) {
			object_table_set(vm, other->entries,
					 self->entries->entries[i].key,
					 NIL_VAL);
		}
	}
	return OBJECT_VAL(other);
}
