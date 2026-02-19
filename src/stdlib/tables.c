#include "stdlib/tables.h"
#include "object.h"
#include "panic.h"

Value table_values_method(VM *vm, const Value *args)
{

	const ObjectTable *table = AS_CRUX_TABLE(args[0]);
	ObjectArray *values = new_array(vm, table->size);

	if (values == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Failed to allocate enough memory for <values> array.",
			MEMORY);
	}

	uint16_t lastInsert = 0;

	for (uint32_t i = 0; i < table->capacity; i++) {
		const ObjectTableEntry entry = table->entries[i];
		if (entry.is_occupied) {
			values->values[lastInsert] = entry.value;
			lastInsert++;
		}
	}

	values->size = lastInsert;

	return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(values)));
}

Value table_keys_method(VM *vm, const Value *args)
{

	const ObjectTable *table = AS_CRUX_TABLE(args[0]);

	ObjectArray *keys = new_array(vm, table->size);

	if (keys == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Failed to allocate enough memory for <keys> array.",
			MEMORY);
	}

	uint16_t lastInsert = 0;

	for (uint32_t i = 0; i < table->capacity; i++) {
		const ObjectTableEntry entry = table->entries[i];
		if (entry.is_occupied) {
			keys->values[lastInsert] = entry.key;
			lastInsert++;
		}
	}

	keys->size = lastInsert;

	return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(keys)));
}

Value table_pairs_method(VM *vm, const Value *args)
{

	ObjectModuleRecord *module_record = vm->current_module_record;
	const ObjectTable *table = AS_CRUX_TABLE(args[0]);

	ObjectArray *pairs = new_array(vm, table->size);
	push(module_record, OBJECT_VAL(pairs));

	if (pairs == NULL) {
		Value res = MAKE_GC_SAFE_ERROR(
			vm,
			"Failed to allocate enough memory for <pairs> array.",
			MEMORY);
		pop(vm->current_module_record); // pop the array
		return res;
	}

	uint16_t lastInsert = 0;

	for (uint32_t i = 0; i < table->capacity; i++) {
		const ObjectTableEntry entry = table->entries[i];
		if (entry.is_occupied) {
			ObjectArray *pair = new_array(vm, 2);
			push(module_record, OBJECT_VAL(pair));
			if (pair == NULL) {
				Value res = MAKE_GC_SAFE_ERROR(
					vm,
					"Failed to allocate enough memory for "
					"pair array",
					MEMORY);
				pop(module_record);
				return res;
			}

			pair->values[0] = entry.key;
			pair->values[1] = entry.value;
			pair->size = 2;

			pairs->values[lastInsert] = OBJECT_VAL(pair);
			lastInsert++;
			pop(module_record);
		}
	}

	pairs->size = lastInsert;

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(pairs));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

// arg0 - table
// arg1 - key
Value table_remove_method(VM *vm, const Value *args)
{

	ObjectTable *table = AS_CRUX_TABLE(args[0]);
	const Value key = args[1];
	if (IS_CRUX_HASHABLE(key)) {
		const bool result = object_table_remove(table, key);
		if (!result) {
			return MAKE_GC_SAFE_ERROR(
				vm,
				"Failed to remove key: value pair from table.",
				VALUE);
		}
		return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
	}
	return MAKE_GC_SAFE_ERROR(vm, "Unhashable type given as table key.",
				  TYPE);
}

// arg0 - table
// arg1 - key
Value table_get_method(VM *vm, const Value *args)
{

	const ObjectTable *table = AS_CRUX_TABLE(args[0]);
	const Value key = args[1];
	if (IS_CRUX_HASHABLE(key)) {
		Value value;
		const bool result = object_table_get(table->entries,
						     table->size,
						     table->capacity, key,
						     &value);
		if (!result) {
			return MAKE_GC_SAFE_ERROR(
				vm, "Failed to get value from table.", VALUE);
		}
		return OBJECT_VAL(new_ok_result(vm, value));
	}
	return MAKE_GC_SAFE_ERROR(vm, "Unhashable type given as table key.",
				  TYPE);
}

// args[0] - table
// args[1] - key
Value table_has_key_method(VM *vm, const Value *args)
{

	(void)vm;
	ObjectTable *table = AS_CRUX_TABLE(args[0]);
	const Value key = args[1];
	if (IS_CRUX_HASHABLE(key)) {
		const bool result = object_table_contains_key(table, key);
		return BOOL_VAL(result);
	}
	return BOOL_VAL(false);
}

// args[0] - table
// args[1] - key
// args[2] - default value
Value table_get_or_else_method(VM *vm, const Value *args)
{

	(void)vm;
	const ObjectTable *table = AS_CRUX_TABLE(args[0]);
	const Value key = args[1];
	const Value defaultValue = args[2];
	if (IS_CRUX_HASHABLE(key)) {
		Value value;
		const bool result = object_table_get(table->entries,
						     table->size,
						     table->capacity, key,
						     &value);
		if (!result) {
			return defaultValue;
		}
		return value;
	}
	return defaultValue;
}
