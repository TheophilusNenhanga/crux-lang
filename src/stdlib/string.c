#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "stdlib/string.h"
#include "utf8.h"

// Returns the byte pointer to the nth code point
static const utf8_int8_t *skip_code_points(const utf8_int8_t *chars, uint32_t n)
{
	const utf8_int8_t *cursor = chars;
	for (uint32_t i = 0; i < n; i++) {
		cursor += utf8codepointcalcsize(cursor);
	}
	return cursor;
}

// Checks if an ASCII-only space character.
static bool is_utf8_space(utf8_int32_t cp)
{
	if (cp < 128)
		return isspace((int)cp);
	return false;
}

Value string_byte_length_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	return INT_VAL(string->byte_length);
}

/**
 * Get the first character of a string
 * arg0 -> string: String
 * Return: String
 */
Value string_first_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (string->byte_length == 0)
		return OBJECT_VAL(copy_string(vm, "", 0));

	uint32_t char_bytes = (uint32_t)utf8codepointcalcsize(string->chars);
	return OBJECT_VAL(copy_string(vm, (const char *)string->chars, char_bytes));
}

/**
 * Get the last character of a string
 * arg0 -> string: String
 * Return: String
 */
Value string_last_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (string->byte_length == 0)
		return OBJECT_VAL(copy_string(vm, "", 0));

	utf8_int32_t cp;
	const utf8_int8_t *last_char = utf8rcodepoint(string->chars + string->byte_length, &cp);
	uint32_t char_bytes = (uint32_t)((string->chars + string->byte_length) - last_char);

	return OBJECT_VAL(copy_string(vm, (const char *)last_char, char_bytes));
}

/**
 * Get the character at the specified index (code point index)
 * arg1 -> index: Int
 * Return: Result<String>
 */
Value string_get_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	int32_t index = AS_INT(args[1]);

	if (index < 0 || (uint32_t)index >= string->code_point_length) {
		return MAKE_GC_SAFE_ERROR(vm, "Index out of bounds.", BOUNDS);
	}

	const utf8_int8_t *start = skip_code_points(string->chars, (uint32_t)index);
	uint32_t char_bytes = (uint32_t)utf8codepointcalcsize(start);

	ObjectString *res_str = copy_string(vm, (const char *)start, char_bytes);
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Make a string uppercase
 * arg0 -> string: String
 * Return: String
 */
Value string_to_upper_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	utf8_int8_t *buffer = ALLOCATE(vm, utf8_int8_t, string->byte_length + 1);
	memcpy(buffer, string->chars, string->byte_length);
	buffer[string->byte_length] = '\0';
	utf8upr(buffer);
	return OBJECT_VAL(take_string(vm, (char *)buffer, string->byte_length));
}

/**
 * Make a string lowercase
 * arg0 -> string: String
 * Return: String
 */
Value string_to_lower_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	utf8_int8_t *buffer = ALLOCATE(vm, utf8_int8_t, string->byte_length + 1);
	memcpy(buffer, string->chars, string->byte_length);
	buffer[string->byte_length] = '\0';
	utf8lwr(buffer);
	return OBJECT_VAL(take_string(vm, (char *)buffer, string->byte_length));
}

/**
 * Checks if a string is all uppercase (non-ASCII code points are not considered uppercase)
 * arg0 -> string: String
 * Return: Bool
 */
Value string_is_upper_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		if (cp > 128 || !isupper((int)cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

/**
 * Checks if a string is all lowercase (non-ASCII code points are not considered lowercase)
 * arg0 -> string: String
 * Return: Bool
 */
Value string_is_lower_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		if (cp > 128 || !islower((int)cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

/**
 * Removes leading and trailing whitespace
 * arg0 -> string: String
 * Return: Result<String>
 */
Value string_strip_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (string->byte_length == 0)
		return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(copy_string(vm, "", 0))));

	const utf8_int8_t *start = string->chars;
	const utf8_int8_t *end = string->chars + string->byte_length;

	// Strip from start
	utf8_int32_t cp;
	while (start < end) {
		const utf8_int8_t *next = utf8codepoint(start, &cp);
		if (!is_utf8_space(cp))
			break;
		start = next;
	}

	// Strip from end
	while (end > start) {
		const utf8_int8_t *prev = end - 1;
		while (prev > start && (*prev & 0xc0) == 0x80) {
			prev--;
		}
		utf8_int32_t code_point;
		utf8codepoint(prev, &code_point);
		if (!is_utf8_space(code_point))
			break;
		end = prev;
	}

	ObjectString *res_str = copy_string(vm, (const char *)start, (uint32_t)(end - start));
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Substring using Code Point indices
 * arg1 -> start: Int
 * arg2 -> end: Int
 * Return: Result<String>
 */
Value string_substring_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	int32_t startIdx = AS_INT(args[1]);
	int32_t endIdx = AS_INT(args[2]);

	if (startIdx < 0 || endIdx < 0 || (uint32_t)startIdx > string->code_point_length ||
		(uint32_t)endIdx > string->code_point_length || startIdx > endIdx) {
		return MAKE_GC_SAFE_ERROR(vm, "Index out of bounds.", BOUNDS);
	}

	const utf8_int8_t *start_ptr = skip_code_points(string->chars, (uint32_t)startIdx);
	const utf8_int8_t *end_ptr = start_ptr;
	for (int32_t i = 0; i < (endIdx - startIdx); i++) {
		end_ptr += utf8codepointcalcsize(end_ptr);
	}

	ObjectString *res_str = copy_string(vm, (const char *)start_ptr, (uint32_t)(end_ptr - start_ptr));
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Splits string by a delimiter
 * arg1 -> delimiter: String
 * Return: Result<Array<String>>
 */
Value string_split_method(VM *vm, const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	const ObjectString *delim = AS_CRUX_STRING(args[1]);

	if (delim->byte_length == 0)
		return MAKE_GC_SAFE_ERROR(vm, "Delimiter cannot be empty.", VALUE);

	ObjectArray *array = new_array(vm, 0);
	push(vm->current_module_record, OBJECT_VAL(array));

	const utf8_int8_t *cursor = string->chars;
	const utf8_int8_t *last_match = cursor;

	while ((cursor = (utf8_int8_t *)utf8str(cursor, delim->chars)) != NULL) {
		ObjectString *sub = copy_string(vm, (const char *)last_match, (uint32_t)(cursor - last_match));
		push(vm->current_module_record, OBJECT_VAL(sub));
		array_add_back(vm, array, OBJECT_VAL(sub));
		pop(vm->current_module_record);

		cursor += delim->byte_length;
		last_match = cursor;
	}

	ObjectString *sub = copy_string(vm, (const char *)last_match,
									(uint32_t)((string->chars + string->byte_length) - last_match));
	push(vm->current_module_record, OBJECT_VAL(sub));
	array_add_back(vm, array, OBJECT_VAL(sub));
	pop(vm->current_module_record);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(array));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Checks if a string contains a substring
 * arg1 -> substring: String
 * Return: Result<Bool>
 */
Value string_contains_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (goal->byte_length == 0)
		return BOOL_VAL(true);

	bool found = utf8str(str->chars, goal->chars) != NULL;
	return BOOL_VAL(found);
}

/**
 * Checks if a string starts with a prefix
 * arg1 -> prefix: String
 * Return: Bool
 */
Value string_starts_with_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectString *prefix = AS_CRUX_STRING(args[1]);

	if (prefix->byte_length > str->byte_length)
		return OBJECT_VAL(new_ok_result(vm, BOOL_VAL(false)));

	bool match = memcmp(str->chars, prefix->chars, prefix->byte_length) == 0;
	return OBJECT_VAL(new_ok_result(vm, BOOL_VAL(match)));
}

/**
 * Checks if a string ends with a suffix
 * arg1 -> suffix: String
 * Return: Bool
 */
Value string_ends_with_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectString *suffix = AS_CRUX_STRING(args[1]);

	if (suffix->byte_length > str->byte_length)
		return OBJECT_VAL(new_ok_result(vm, BOOL_VAL(false)));

	const utf8_int8_t *start_pos = str->chars + (str->byte_length - suffix->byte_length);
	bool match = memcmp(start_pos, suffix->chars, suffix->byte_length) == 0;
	return OBJECT_VAL(new_ok_result(vm, BOOL_VAL(match)));
}

/**
 * Is the string all alphabetic characters? (non-ASCII code points are not considered alphabetic)
 * arg0 -> string: String
 * Return: Bool
 */
Value string_is_alpha_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		// only check ASCII
		if (cp >= 128 || !isalpha((int)cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

Value string_is_digit_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		// only check ASCII
		if (cp >= 128 || !isdigit((int)cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

/**
 * Concatenates two strings
 * arg0 -> a: String
 * arg1 -> b: String
 * Returns String
 */
Value string_concat_method(VM *vm, const Value *args)
{
	const ObjectString *a = AS_CRUX_STRING(args[0]);
	const ObjectString *b = AS_CRUX_STRING(args[1]);

	uint32_t total_bytes = a->byte_length + b->byte_length;
	char *buf = malloc(total_bytes + 1);
	memcpy(buf, a->chars, a->byte_length);
	memcpy(buf + a->byte_length, b->chars, b->byte_length);
	buf[total_bytes] = '\0';

	ObjectString *res_str = take_string(vm, buf, total_bytes);
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Reveses a string (Code Point aware)
 * arg0 -> string: String
 * Returns String
 */
Value string_reverse_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	utf8_int8_t *buf = malloc(str->byte_length + 1);
	if (buf == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed", MEMORY);
	}

	const utf8_int8_t *read_cursor = str->chars + str->byte_length;
	utf8_int8_t *write_cursor = buf;
	utf8_int32_t cp;

	while (read_cursor > str->chars) {
		const utf8_int8_t *prev = utf8rcodepoint(read_cursor, &cp);
		uint32_t len = (uint32_t)(read_cursor - prev);
		memcpy(write_cursor, prev, len);
		write_cursor += len;
		read_cursor = prev;
	}
	*write_cursor = '\0';

	ObjectString *res_str = take_string(vm, (char *)buf, str->byte_length);
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);

	return OBJECT_VAL(res);
}

/**
 * Returns the code point index of the first occurrence of a substring.
 * Returns -1 if not found.
 * arg0 -> string: String
 * arg1 -> needle: String
 * Returns Int
 */
Value string_find_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *haystack = AS_CRUX_STRING(args[0]);
	const ObjectString *needle = AS_CRUX_STRING(args[1]);

	if (needle->byte_length == 0)
		return INT_VAL(0);

	const utf8_int8_t *match = utf8str(haystack->chars, needle->chars);
	if (match == NULL)
		return INT_VAL(-1);

	// Convert byte pointer difference to code point index
	uint32_t index = 0;
	const utf8_int8_t *cursor = haystack->chars;
	while (cursor < match) {
		cursor += utf8codepointcalcsize(cursor);
		index++;
	}

	return INT_VAL(index);
}

/**
 * Repeats a string n times.
 * arg0 -> string: String
 * arg1 -> count: Int
 * Returns String
 */
Value string_repeat_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	int count = AS_INT(args[1]);

	if (count <= 0)
		return OBJECT_VAL(copy_string(vm, "", 0));
	if (count == 1)
		return OBJECT_VAL(str);

	size_t new_size = (size_t)str->byte_length * count;
	char *buffer = malloc(new_size + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed", MEMORY);
	}

	for (int i = 0; i < count; i++) {
		memcpy(buffer + (i * str->byte_length), str->chars, str->byte_length);
	}
	buffer[new_size] = '\0';

	ObjectString *res_str = take_string(vm, buffer, (uint32_t)new_size);
	push(vm->current_module_record, OBJECT_VAL(res_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(res_str));
	pop(vm->current_module_record);

	return OBJECT_VAL(res);
}

/**
 * Joins an array of strings using the current string as a separator.
 * arg0 -> separator: String
 * arg1 -> elements: Array
 * Returns String
 */
Value string_join_method(VM *vm, const Value *args)
{
	const ObjectString *sep = AS_CRUX_STRING(args[0]);
	const ObjectArray *array = AS_CRUX_ARRAY(args[1]);

	if (array->size == 0)
		return OBJECT_VAL(copy_string(vm, "", 0));

	size_t total_size = 0;
	for (uint32_t i = 0; i < array->size; i++) {
		if (!IS_CRUX_STRING(array->values[i]))
			continue;
		total_size += AS_CRUX_STRING(array->values[i])->byte_length;
	}
	total_size += (size_t)sep->byte_length * (array->size - 1);

	char *buffer = malloc(total_size + 1);
	char *cursor = buffer;

	for (uint32_t i = 0; i < array->size; i++) {
		if (!IS_CRUX_STRING(array->values[i]))
			continue;
		const ObjectString *s = AS_CRUX_STRING(array->values[i]);

		memcpy(cursor, s->chars, s->byte_length);
		cursor += s->byte_length;

		if (i < array->size - 1) {
			memcpy(cursor, sep->chars, sep->byte_length);
			cursor += sep->byte_length;
		}
	}
	*cursor = '\0';

	return OBJECT_VAL(take_string(vm, buffer, (uint32_t)total_size));
}

/**
 * Pads the string to a certain length.
 * arg0 -> string: String
 * arg1 -> length: Int (Target code point length)
 * arg2 -> char: String (The character to pad with)
 * Returns String
 */
Value string_pad_left_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	uint32_t target_len = (uint32_t)AS_INT(args[1]);
	const ObjectString *pad_str = AS_CRUX_STRING(args[2]);

	if (str->code_point_length >= target_len)
		return OBJECT_VAL(str);
	if (pad_str->byte_length == 0)
		return OBJECT_VAL(str);

	uint32_t needed = target_len - str->code_point_length;
	uint32_t pad_bytes = needed * pad_str->byte_length;
	uint32_t total_bytes = pad_bytes + str->byte_length;

	char *buffer = malloc(total_bytes + 1);
	for (uint32_t i = 0; i < needed; i++) {
		memcpy(buffer + (i * pad_str->byte_length), pad_str->chars, pad_str->byte_length);
	}
	memcpy(buffer + pad_bytes, str->chars, str->byte_length);
	buffer[total_bytes] = '\0';

	return OBJECT_VAL(take_string(vm, buffer, total_bytes));
}

/**
 * Pads the string to a certain length.
 * arg0 -> string: String
 * arg1 -> length: Int (Target code point length)
 * arg2 -> char: String (The character to pad with)
 * Returns String
 */
Value string_pad_right_method(VM *vm, const Value *args)
{
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	uint32_t target_len = (uint32_t)AS_INT(args[1]);
	const ObjectString *pad_str = AS_CRUX_STRING(args[2]);

	if (str->code_point_length >= target_len)
		return OBJECT_VAL(str);
	if (pad_str->byte_length == 0)
		return OBJECT_VAL(str);

	uint32_t needed = target_len - str->code_point_length;
	uint32_t pad_bytes = needed * pad_str->byte_length;
	uint32_t total_bytes = str->byte_length + pad_bytes;

	char *buffer = malloc(total_bytes + 1);
	memcpy(buffer, str->chars, str->byte_length);
	for (uint32_t i = 0; i < needed; i++) {
		memcpy(buffer + str->byte_length + (i * pad_str->byte_length), pad_str->chars, pad_str->byte_length);
	}
	buffer[total_bytes] = '\0';

	return OBJECT_VAL(take_string(vm, buffer, total_bytes));
}

/**
 * Counts occurrences of a substring.
 * arg0 -> string: String
 * arg1 -> target: String
 * Returns Int
 */
Value string_count_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	const ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (goal->byte_length == 0)
		return INT_VAL(0);

	uint32_t count = 0;
	const utf8_int8_t *cursor = str->chars;
	while ((cursor = (const utf8_int8_t *)utf8str(cursor, goal->chars)) != NULL) {
		count++;
		cursor += goal->byte_length; // Move past current match
	}

	return INT_VAL(count);
}

/**
 * Checks if a string is empty
 * arg0 -> string: String
 * Return: Bool
 */
Value string_is_empty_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	return BOOL_VAL(str->byte_length == 0);
}

/**
 * Checks if a string is all whitespace (non-ASCII code points are not considered whitespace)
 * arg0 -> string: String
 * Return: Bool
 */
Value string_is_space_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		if (!is_utf8_space(cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

Value string_is_al_num_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->byte_length == 0)
		return BOOL_VAL(false);

	const utf8_int8_t *cursor = str->chars;
	utf8_int32_t cp;
	while (*cursor) {
		cursor = utf8codepoint(cursor, &cp);
		// only check ASCII
		if (cp >= 128 || !isalnum((int)cp))
			return BOOL_VAL(false);
	}
	return BOOL_VAL(true);
}

/**
 * Replaces all occurrences of a target substring with a replacement string.
 * arg0 -> string: String (The source)
 * arg1 -> target: String (The sequence to find)
 * arg2 -> replacement: String (The sequence to insert)
 * Returns Result<String>
 */
Value string_replace_method(VM *vm, const Value *args)
{
	const ObjectString *src = AS_CRUX_STRING(args[0]);
	const ObjectString *target = AS_CRUX_STRING(args[1]);
	const ObjectString *replacement = AS_CRUX_STRING(args[2]);

	// target is empty
	if (target->byte_length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Target substring must not be empty.", VALUE);
	}

	// Source is empty or target is longer than source
	if (src->byte_length == 0 || target->byte_length > src->byte_length) {
		ObjectResult *res = new_ok_result(vm, OBJECT_VAL(src));
		return OBJECT_VAL(res);
	}

	uint32_t match_count = 0;
	const utf8_int8_t *cursor = src->chars;
	while ((cursor = (const utf8_int8_t *)utf8str(cursor, target->chars)) != NULL) {
		match_count++;
		cursor += target->byte_length; // Move past this match
	}

	if (match_count == 0) {
		ObjectResult *res = new_ok_result(vm, OBJECT_VAL(src));
		return OBJECT_VAL(res);
	}

	size_t new_byte_len = src->byte_length + (size_t)match_count * (replacement->byte_length - target->byte_length);

	char *new_chars = malloc(new_byte_len + 1);
	if (!new_chars) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed during string replace.", MEMORY);
	}

	char *write_ptr = new_chars;
	const utf8_int8_t *read_ptr = src->chars;
	const utf8_int8_t *match_ptr = NULL;

	while ((match_ptr = (const utf8_int8_t *)utf8str(read_ptr, target->chars)) != NULL) {
		size_t before_match_len = (size_t)(match_ptr - read_ptr);
		memcpy(write_ptr, read_ptr, before_match_len);
		write_ptr += before_match_len;

		memcpy(write_ptr, replacement->chars, replacement->byte_length);
		write_ptr += replacement->byte_length;

		read_ptr = match_ptr + target->byte_length;
	}

	size_t tail_len = (size_t)((src->chars + src->byte_length) - read_ptr);
	if (tail_len > 0) {
		memcpy(write_ptr, read_ptr, tail_len);
		write_ptr += tail_len;
	}

	*write_ptr = '\0';

	ObjectString *result_string = take_string(vm, new_chars, (uint32_t)new_byte_len);

	push(vm->current_module_record, OBJECT_VAL(result_string));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_string));
	pop(vm->current_module_record);

	return OBJECT_VAL(res);
}
