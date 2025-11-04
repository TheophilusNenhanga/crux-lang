#include <stdlib.h>

#include "stdlib/core.h"
#include "object.h"
#include "panic.h"

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
	return NIL_VAL;
}

ObjectResult *length_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	const Value value = args[0];
	const Value length = get_length(value);
	if (IS_NIL(length)) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Expected either a collection type "
					  "('string', 'array', 'table').",
					  TYPE);
	}
	return new_ok_result(vm, length);
}

Value length_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)vm;
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
					       vm->current_module_record);
		push(vm->current_module_record, OBJECT_VAL(array));

		for (uint32_t i = 0; i < string->length; i++) {
			ObjectString *char_str = copy_string(vm,
							     &string->chars[i],
							     1);
			push(vm->current_module_record, OBJECT_VAL(char_str));
			if (!array_add_back(vm, array, OBJECT_VAL(char_str))) {
				pop(vm->current_module_record); // char_str
				pop(vm->current_module_record); // array
				*success = false;
				return NIL_VAL;
			}
			pop(vm->current_module_record); // char_str
		}

		const Value result = OBJECT_VAL(array);
		pop(vm->current_module_record); // array
		return result;
	}

	if (IS_CRUX_TABLE(value)) {
		const ObjectTable *table = AS_CRUX_TABLE(value);
		ObjectArray *array = new_array(vm, table->size * 2,
					       vm->current_module_record);
		push(vm->current_module_record, OBJECT_VAL(array));

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
					pop(vm->current_module_record); // array
					*success = false;
					return NIL_VAL;
				}
				index++;
			}
		}

		const Value result = OBJECT_VAL(array);
		pop(vm->current_module_record); // array
		return result;
	}

	ObjectArray *array = new_array(vm, 1, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(array));
	array_add(vm, array, value, 0);
	const Value result = OBJECT_VAL(array);
	pop(vm->current_module_record); // array
	return result;
}

static Value cast_table(VM *vm, const Value *args)
{
	ObjectModuleRecord *moduleRecord = vm->current_module_record;
	const Value value = args[0];

	if (IS_CRUX_TABLE(value)) {
		return value;
	}

	if (IS_CRUX_ARRAY(value)) {
		const ObjectArray *array = AS_CRUX_ARRAY(value);
		ObjectTable *table = new_table(vm, (int)array->size,
					       moduleRecord);
		push(vm->current_module_record, OBJECT_VAL(table));

		for (uint32_t i = 0; i < array->size; i++) {
			const Value k = INT_VAL(i);
			const Value v = array->values[i];
			object_table_set(vm, table, k, v);
		}

		const Value result = OBJECT_VAL(table);
		pop(vm->current_module_record); // table
		return result;
	}

	if (IS_CRUX_STRING(value)) {
		const ObjectString *string = AS_CRUX_STRING(value);
		ObjectTable *table = new_table(vm, (int)string->length,
					       moduleRecord);
		push(vm->current_module_record, OBJECT_VAL(table));

		for (uint32_t i = 0; i < string->length; i++) {
			ObjectString *char_str = copy_string(vm,
							     &string->chars[i],
							     1);
			push(vm->current_module_record, OBJECT_VAL(char_str));
			object_table_set(vm, table, INT_VAL(i),
					 OBJECT_VAL(char_str));
			pop(vm->current_module_record); // char_str
		}

		const Value result = OBJECT_VAL(table);
		pop(vm->current_module_record); // table
		return result;
	}

	ObjectTable *table = new_table(vm, 1, moduleRecord);
	push(vm->current_module_record, OBJECT_VAL(table));
	object_table_set(vm, table, INT_VAL(0), value);
	const Value result = OBJECT_VAL(table);
	pop(vm->current_module_record); // table
	return result;
}

static Value cast_int(VM *vm, const Value arg, bool *success)
{
	(void)vm;
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

static Value cast_float(VM *vm, const Value *args, bool *success)
{
	(void)vm;
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

ObjectResult *int_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	const Value argument = args[0];
	const Value value = cast_int(vm, argument, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot convert value to number.",
					  TYPE);
	}
	return new_ok_result(vm, value);
}

ObjectResult *float_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	const Value value = cast_float(vm, args, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot convert value to number.",
					  TYPE);
	}
	return new_ok_result(vm, value);
}

ObjectResult *string_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	const Value value = args[0];
	ObjectString *str = to_string(vm, value);
	push(vm->current_module_record, OBJECT_VAL(str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(str));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *array_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	const Value array = cast_array(vm, args, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Failed to convert value to array.",
					  RUNTIME);
	}
	return new_ok_result(vm, array);
}

ObjectResult *table_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	const Value table = cast_table(vm, args);
	return new_ok_result(vm, table);
}

Value int_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	const Value argument = args[0];
	return cast_int(vm, argument, &success);
}

Value float_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	return cast_float(vm, args, &success);
}

Value string_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	return OBJECT_VAL(to_string(vm, args[0]));
}

Value array_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	bool success = true;
	const Value array = cast_array(vm, args, &success);
	if (!success) {
		return NIL_VAL;
	}
	return array;
}

Value table_function_(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	return cast_table(vm, args);
}

ObjectResult *format_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"argument <format_string> must be of type 'string'.",
			TYPE);
	}
	if (!IS_CRUX_TABLE(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "argument <format_table> must be of type 'table'.",
			TYPE);
	}

	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectTable *table = AS_CRUX_TABLE(args[1]);

	typedef struct {
		uint32_t start;
		uint32_t end;
		ObjectString *key;
		Value value;
	} FormatToken;

	uint32_t tokens_capacity = 8;

	FormatToken *tokens = malloc(sizeof(FormatToken) * tokens_capacity);

	if (!tokens) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed",
					  VALUE);
	}

	bool in_token = false;
	uint32_t token_count = 0;
	uint32_t token_start = 0;

	for (uint32_t i = 0; i < str->length; i++) {
		if (str->chars[i] == '{') {
			if (in_token) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(
					vm,
					"format token cannot have { within it",
					VALUE);
			}
			in_token = true;
			token_start = i;
		} else if (str->chars[i] == '}') {
			if (!in_token) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(
					vm, "Unexpected } without matching {",
					VALUE);
			}

			if (token_count >= tokens_capacity) {
				tokens_capacity *= 2;
				FormatToken *new_tokens = realloc(
					tokens,
					sizeof(FormatToken) * tokens_capacity);
				if (!new_tokens) {
					free(tokens);
					return MAKE_GC_SAFE_ERROR(
						vm, "Memory allocation failed",
						VALUE);
				}
				tokens = new_tokens;
			}

			const uint32_t key_length = i - token_start - 1;
			ObjectString *key = copy_string(
				vm, str->chars + token_start + 1, key_length);
			push(vm->current_module_record, OBJECT_VAL(key));
			Value value;
			if (!object_table_get(table->entries, table->size,
					      table->capacity, OBJECT_VAL(key),
					      &value)) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(
					vm,
					"Format token specified name that does "
					"not exist in format table",
					VALUE);
			}

			tokens[token_count].start = token_start;
			tokens[token_count].end = i;
			tokens[token_count].key = key;
			tokens[token_count].value = value;
			token_count++;

			in_token = false;
		}
	}

	if (in_token) {
		free(tokens);
		return MAKE_GC_SAFE_ERROR(vm, "Unterminated format token.",
					  VALUE);
	}

	uint32_t current_token = 0;
	for (uint32_t i = 0; i < str->length; i++) {
		if (current_token < token_count &&
		    i == tokens[current_token].start) {
			print_value(tokens[current_token].value, false);
			i = tokens[current_token].end;
			current_token++;
		} else {
			printf("%c", str->chars[i]);
		}
	}

	for (uint32_t i = 0; i < token_count; i++) {
		pop(vm->current_module_record);
	}

	free(tokens);
	return new_ok_result(vm, NIL_VAL);
}
