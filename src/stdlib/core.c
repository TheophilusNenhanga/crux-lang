#include "stdlib/core.h"

#include <stdint.h>
#include <stdlib.h>

#include "object.h"
#include "panic.h"
#include "stdlib/range.h"
#include "utf8.h"

static Value get_length(const Value value)
{
	if (IS_CRUX_ARRAY(value)) {
		return INT_VAL(AS_CRUX_ARRAY(value)->size);
	}
	if (IS_CRUX_STRING(value)) {
		return INT_VAL(AS_CRUX_STRING(value)->code_point_length);
	}
	if (IS_CRUX_TABLE(value)) {
		return INT_VAL(AS_CRUX_TABLE(value)->size);
	}
	if (IS_CRUX_VECTOR(value)) {
		return INT_VAL(AS_CRUX_VECTOR(value)->dimensions);
	}
	if (IS_CRUX_MATRIX(value)) {
		return INT_VAL(AS_CRUX_MATRIX(value)->col_dim * AS_CRUX_MATRIX(value)->row_dim);
	}
	if (IS_CRUX_RANGE(value)) {
		const ObjectRange *range = AS_CRUX_RANGE(value);
		return INT_VAL(range_len(range));
	}
	if (IS_CRUX_BUFFER(value)) {
		const ObjectBuffer *buffer = AS_CRUX_BUFFER(value);
		return INT_VAL((int32_t)buffer->write_pos - buffer->read_pos);
	}
	if (IS_CRUX_TUPLE(value)) {
		const ObjectTuple *tuple = AS_CRUX_TUPLE(value);
		return INT_VAL(tuple->size);
	}
	if (IS_CRUX_SET(value)) {
		const ObjectSet *set = AS_CRUX_SET(value);
		return INT_VAL(set->entries->size);
	}
	return INT_VAL(-1);
}

/**
 * Returns the length of a value (works with Array, String, Table, Vector,
 * Matrix) arg0 -> value: Any Returns Int
 */
Value length_function(VM *vm, const Value *args)
{
	(void)vm;
	const Value value = args[0];
	const Value length = get_length(value);
	return length;
}

static Value cast_array(VM *vm, const Value *args, bool *success)
{
	const Value value = args[0];

	if (IS_CRUX_ARRAY(value)) {
		return value;
	}

	if (IS_CRUX_STRING(value)) {
		const ObjectString *string = AS_CRUX_STRING(value);
		ObjectArray *array = new_array(vm, string->code_point_length);
		push(vm->current_module_record, OBJECT_VAL(array));

		const utf8_int8_t *cursor = string->chars;

		for (uint32_t i = 0; i < string->code_point_length; i++) {
			size_t char_bytes = utf8codepointcalcsize(cursor);
			ObjectString *char_str = copy_string(vm, cursor, char_bytes);

			push(vm->current_module_record, OBJECT_VAL(char_str));

			if (!array_add_back(vm, array, OBJECT_VAL(char_str))) {
				pop(vm->current_module_record); // char_str
				pop(vm->current_module_record); // array
				*success = false;
				return NIL_VAL;
			}
			pop(vm->current_module_record); // char_str
			cursor += char_bytes;
		}

		const Value result = OBJECT_VAL(array);
		pop(vm->current_module_record); // array
		return result;
	}

	if (IS_CRUX_TABLE(value)) {
		const ObjectTable *table = AS_CRUX_TABLE(value);
		ObjectArray *array = new_array(vm, table->size * 2);
		push(vm->current_module_record, OBJECT_VAL(array));

		uint32_t index = 0;
		for (uint32_t i = 0; i < table->capacity; i++) {
			if (index == table->size) {
				break;
			}
			if (table->entries[i].is_occupied) {
				if (!array_add_back(vm, array, table->entries[i].key) ||
					!array_add_back(vm, array, table->entries[i].value)) {
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

	ObjectArray *array = new_array(vm, 1);
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
		ObjectTable *table = new_object_table(vm, (int)array->size);
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
		ObjectTable *table = new_object_table(vm, (int)string->code_point_length);
		push(vm->current_module_record, OBJECT_VAL(table));

		utf8_int8_t *cursor = string->chars;
		for (uint32_t i = 0; i < string->code_point_length; i++) {
			uint32_t char_bytes = utf8codepointcalcsize(cursor);
			ObjectString *char_str = copy_string(vm, cursor, char_bytes);
			push(vm->current_module_record, OBJECT_VAL(char_str));
			object_table_set(vm, table, INT_VAL(i), OBJECT_VAL(char_str));
			pop(vm->current_module_record); // char_str
			cursor += char_bytes;
		}

		const Value result = OBJECT_VAL(table);
		pop(vm->current_module_record); // table
		return result;
	}

	ObjectTable *table = new_object_table(vm, 1);
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
		*success = true;
		return arg;
	}

	if (IS_FLOAT(arg)) {
		*success = true;
		return INT_VAL((uint32_t)AS_FLOAT(arg));
	}

	if (IS_CRUX_STRING(arg)) {
		const char *str = AS_C_STRING(arg);
		char *end;
		const double num = strtod(str, &end);
		// can check for overflow or underflow with errno
		if (end != str) {
			*success = true;
			return INT_VAL((uint32_t)num);
		}
	}

	if (IS_BOOL(arg)) {
		*success = true;
		return INT_VAL(AS_BOOL(arg) ? 1 : 0);
	}

	if (IS_NIL(arg)) {
		*success = true;
		return INT_VAL(0);
	}

	*success = false;
	return NIL_VAL;
}

static Value cast_float(VM *vm, const Value *args, bool *success)
{
	(void)vm;
	const Value value = args[0];

	if (IS_FLOAT(value)) {
		*success = true;
		return value;
	}

	if (IS_INT(value)) {
		const double v = TO_DOUBLE(value);
		*success = true;
		return FLOAT_VAL(v);
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

/**
 * Converts a value to an integer
 * arg0 -> value: Any
 * Returns Result<Int>
 */
Value int_function(VM *vm, const Value *args)
{
	bool success = false;
	const Value argument = args[0];
	const Value value = cast_int(vm, argument, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to convert value to integer.", RUNTIME);
	}
	return OBJECT_VAL(new_ok_result(vm, value));
}

/**
 * Converts a value to a float
 * arg0 -> value: Any
 * Returns Result<Float>
 */
Value float_function(VM *vm, const Value *args)
{
	bool success = true;
	const Value value = cast_float(vm, args, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to convert value to float.", RUNTIME);
	}
	return OBJECT_VAL(new_ok_result(vm, value));
}

/**
 * Converts a value to a string
 * arg0 -> value: Any
 * Returns String
 */
Value string_function(VM *vm, const Value *args)
{
	const Value value = args[0];
	ObjectString *str = to_string(vm, value);
	return OBJECT_VAL(str);
}

/**
 * Converts a value to an array
 * arg0 -> value: Any
 * Returns Result<Array>
 */
Value array_function(VM *vm, const Value *args)
{
	bool success = true;
	const Value array = cast_array(vm, args, &success);
	if (!success) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to convert value to array.", RUNTIME);
	}
	return OBJECT_VAL(new_ok_result(vm, array));
}

/**
 * Converts a value to a table
 * arg0 -> value: Any
 * Returns Result<Table>
 */
Value table_function(VM *vm, const Value *args)
{
	const Value table = cast_table(vm, args);
	return OBJECT_VAL(table);
}

/**
 * Formats a string using placeholders like {key} replaced with values from a table
 * arg0 -> format_string: String
 * arg1 -> values: Table
 * Returns Result<Nil>
 */
Value format_function(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectTable *table = AS_CRUX_TABLE(args[1]);

	typedef struct {
		uint32_t byte_start;
		uint32_t byte_end;
		Value value;
	} FormatToken;

	uint32_t tokens_capacity = 8;
	uint32_t token_count = 0;
	FormatToken *tokens = malloc(sizeof(FormatToken) * tokens_capacity);

	if (!tokens) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed", VALUE);
	}

	bool in_token = false;
	uint32_t token_start_offset = 0;

	uint32_t byte_i = 0;
	while (byte_i < str->byte_length) {
		utf8_int8_t c = str->chars[byte_i];

		if (c == '{') {
			if (in_token) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(vm, "Format token cannot have { within it", VALUE);
			}
			in_token = true;
			token_start_offset = byte_i;
		} else if (c == '}') {
			if (!in_token) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(vm, "Unexpected } without matching {", VALUE);
			}

			if (token_count >= tokens_capacity) {
				tokens_capacity *= 2;
				FormatToken *new_tokens = realloc(tokens, sizeof(FormatToken) * tokens_capacity);
				if (!new_tokens) {
					free(tokens);
					return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed", VALUE);
				}
				tokens = new_tokens;
			}

			const uint32_t key_byte_length = byte_i - token_start_offset - 1;
			ObjectString *key = copy_string(vm, (const char *)(str->chars + token_start_offset + 1), key_byte_length);

			push(vm->current_module_record, OBJECT_VAL(key));

			Value val;
			bool found = object_table_get(table->entries, table->size, table->capacity, OBJECT_VAL(key), &val);

			if (!found) {
				free(tokens);
				return MAKE_GC_SAFE_ERROR(vm, "Format token name does not exist in table", VALUE);
			}

			tokens[token_count].byte_start = token_start_offset;
			tokens[token_count].byte_end = byte_i;
			tokens[token_count].value = val;
			token_count++;

			in_token = false;
		}
		byte_i++;
	}

	if (in_token) {
		free(tokens);
		return MAKE_GC_SAFE_ERROR(vm, "Unterminated format token.", VALUE);
	}

	uint32_t current_token_idx = 0;
	uint32_t cursor_byte = 0;

	while (cursor_byte < str->byte_length) {
		if (current_token_idx < token_count && cursor_byte == tokens[current_token_idx].byte_start) {
			print_value(tokens[current_token_idx].value, false);
			cursor_byte = tokens[current_token_idx].byte_end + 1;
			current_token_idx++;
		} else {
			size_t char_bytes = utf8codepointcalcsize(str->chars + cursor_byte);
			printf("%.*s", (int)char_bytes, str->chars + cursor_byte);
			cursor_byte += (uint32_t)char_bytes;
		}
	}

	for (uint32_t i = 0; i < token_count; i++) {
		pop(vm->current_module_record);
	}

	free(tokens);
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}
