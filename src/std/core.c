#include "core.h"

#include <stdlib.h>

#include "../memory.h"
#include "../object.h"

static Value get_length(const Value value)
{
	if (IS_CRUX_ARRAY(value)) {
		return INT_VAL(AS_CRUX_ARRAY(value)->size);
	}
	if (IS_CRUX_STRING(value)) {
		return INT_VAL(AS_CRUX_STRING(value)->length);
	}
	if (IS_CRUX_TABLE(value)) {
		return INT_VAL(AS_CRUX_TABLE(value)->size);
	}
	if (IS_CRUX_STATIC_ARRAY(value)) {
		return INT_VAL(AS_CRUX_STATIC_ARRAY(value)->size);
	}
	if (IS_CRUX_STATIC_TABLE(value)) {
		return INT_VAL(AS_CRUX_STATIC_TABLE(value)->size);
	}
	return NIL_VAL;
}

ObjectResult *length_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args)
{
	const Value value = args[0];
	const Value length = get_length(value);
	if (IS_NIL(length)) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Expected either a collection "
						"type ('string', "
						"'array', 'table').",
						64),
				     TYPE, false));
	}
	return new_ok_result(vm, length);
}

Value length_function_(VM *vm __attribute__((unused)),
		       int arg_count __attribute__((unused)), const Value *args)
{
	const Value value = args[0];
	return get_length(value);
}

static Value cast_array(VM *vm, const Value *args, bool *success)
{
	const Value value = args[0];

	if (IS_CRUX_ARRAY(value)) {
		return value;
	}

	if (IS_CRUX_STRING(value)) {
		const ObjectString *string = AS_CRUX_STRING(value);
		ObjectArray *array = new_array(vm, string->length,
					      vm->currentModuleRecord);
		for (uint32_t i = 0; i < string->length; i++) {
			if (!array_add_back(vm, array,
					  OBJECT_VAL(copy_string(
						  vm, &string->chars[i], 1)))) {
				*success = false;
				return NIL_VAL;
			}
		}
		return OBJECT_VAL(array);
	}

	if (IS_CRUX_TABLE(value)) {
		const ObjectTable *table = AS_CRUX_TABLE(value);
		ObjectArray *array = new_array(vm, table->size * 2,
					      vm->currentModuleRecord);
		uint32_t index = 0;
		for (uint32_t i = 0; i < table->capacity; i++) {
			if (index == table->size) {
				break;
			}
			if (table->entries[i].is_occupied) {
				if (!array_add_back(vm, array,
						  table->entries[i].key) ||
				    !array_add_back(vm, array,
						  table->entries[i].value)) {
					*success = false;
					return NIL_VAL;
				}
				index++;
			}
		}
		return OBJECT_VAL(array);
	}
	ObjectArray *array = new_array(vm, 1, vm->currentModuleRecord);
	array_add(vm, array, value, 0);
	return OBJECT_VAL(array);
}

static Value cast_table(VM *vm, const Value *args)
{
	ObjectModuleRecord *moduleRecord = vm->currentModuleRecord;
	const Value value = args[0];

	if (IS_CRUX_TABLE(value)) {
		return value;
	}

	if (IS_CRUX_ARRAY(value)) {
		const ObjectArray *array = AS_CRUX_ARRAY(value);
		ObjectTable *table = new_table(vm, (int)array->size,
					      moduleRecord);
		for (uint32_t i = 0; i < array->size; i++) {
			const Value k = INT_VAL(i);
			const Value v = array->values[i];
			object_table_set(vm, table, k, v);
		}
		return OBJECT_VAL(table);
	}

	if (IS_CRUX_STRING(value)) {
		const ObjectString *string = AS_CRUX_STRING(value);
		ObjectTable *table = new_table(vm, (int)string->length,
					      moduleRecord);
		for (uint32_t i = 0; i < string->length; i++) {
			object_table_set(vm, table, INT_VAL(i),
				       OBJECT_VAL(copy_string(vm,
							     &string->chars[i],
							     1)));
		}
		return OBJECT_VAL(table);
	}

	ObjectTable *table = new_table(vm, 1, moduleRecord);
	object_table_set(vm, table, INT_VAL(0), value);
	return OBJECT_VAL(table);
}

static Value cast_int(VM *vm __attribute__((unused)), const Value arg,
		      bool *success)
{
	if (IS_INT(arg)) {
		return arg;
	}

	if (IS_FLOAT(arg)) {
		return INT_VAL((uint32_t)AS_FLOAT(arg));
	}

	if (IS_CRUX_STRING(arg)) {
		const char *str = AS_C_STRING(arg);
		char *end;
		const double num = strtod(str, &end);

		// can check for overflow or underflow with errno

		if (end != str) {
			return INT_VAL((uint32_t)num);
		}
	}

	if (IS_BOOL(arg)) {
		return INT_VAL(AS_BOOL(arg) ? 1 : 0);
	}

	if (IS_NIL(arg)) {
		return INT_VAL(0);
	}

	*success = false;
	return NIL_VAL;
}

static Value cast_float(VM *vm __attribute__((unused)), const Value *args,
			bool *success)
{
	const Value value = args[0];

	if (IS_INT(value)) {
		return value;
	}

	if (IS_CRUX_STRING(value)) {
		const char *str = AS_C_STRING(value);
		char *end;
		const double num = strtod(str, &end);

		if (end != str) {
			return FLOAT_VAL(num);
		}
	}

	if (IS_BOOL(value)) {
		return FLOAT_VAL(AS_BOOL(value) ? 1 : 0);
	}

	if (IS_NIL(value)) {
		return FLOAT_VAL(0);
	}

	*success = false;
	return NIL_VAL;
}

ObjectResult *int_function(VM *vm, int arg_count __attribute__((unused)),
			   const Value *args)
{
	bool success = true;
	const Value argument = args[0];
	const Value value = cast_int(vm, argument, &success);
	if (!success) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Cannot convert value to number.",
					    30),
				 TYPE, false));
	}
	return new_ok_result(vm, value);
}

ObjectResult *float_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	bool success = true;
	const Value value = cast_float(vm, args, &success);
	if (!success) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Cannot convert value to number.",
					    30),
				 TYPE, false));
	}
	return new_ok_result(vm, value);
}

ObjectResult *string_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args)
{
	const Value value = args[0];
	return new_ok_result(vm, OBJECT_VAL(to_string(vm, value)));
}

ObjectResult *array_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	bool success = true;
	const Value array = cast_array(vm, args, &success);
	if (!success) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Failed to convert value to array.",
					    33),
				 RUNTIME, false));
	}
	return new_ok_result(vm, OBJECT_VAL(array));
}

ObjectResult *table_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	return new_ok_result(vm, cast_table(vm, args));
}

Value int_function_(VM *vm, int arg_count __attribute__((unused)),
		    const Value *args)
{
	bool success = true;
	const Value argument = args[0];
	return cast_int(vm, argument, &success);
}

Value float_function_(VM *vm, int arg_count __attribute__((unused)),
		      const Value *args)
{
	bool success = true;
	return cast_float(vm, args, &success);
}

Value string_function_(VM *vm, int arg_count __attribute__((unused)),
		       const Value *args)
{
	return OBJECT_VAL(to_string(vm, args[0]));
}

Value array_function_(VM *vm, int arg_count __attribute__((unused)),
		      const Value *args)
{
	bool success = true;
	const Value array = cast_array(vm, args, &success);
	if (!success) {
		return NIL_VAL;
	}
	return OBJECT_VAL(array);
}

Value table_function_(VM *vm, int arg_count __attribute__((unused)),
		      const Value *args)
{
	return cast_table(vm, args);
}
