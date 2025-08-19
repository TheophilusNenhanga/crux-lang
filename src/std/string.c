#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../memory.h"
#include "string.h"
#include "../panic.h"

static void build_prefix_table(const char *pattern,
			       const uint32_t patternLength,
			       uint32_t *prefixTable)
{
	uint32_t j = 0;
	prefixTable[0] = 0;

	for (uint32_t i = 1; i < patternLength; i++) {
		while (j > 0 && pattern[i] != pattern[j]) {
			j = prefixTable[j - 1];
		}
		if (pattern[i] == pattern[j]) {
			j++;
		}
		prefixTable[i] = j;
	}
}

ObjectResult *string_first_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const Value value = args[0];
	const ObjectString *string = AS_CRUX_STRING(value);

	if (string->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "'string' must have at least one character to get the first character.", VALUE);
	}

	ObjectString* str = copy_string(vm, &string->chars[0], 1);
	push(vm->current_module_record, OBJECT_VAL(str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_last_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	const Value value = args[0];
	const ObjectString *string = AS_CRUX_STRING(value);
	if (string->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "'string' must have at least one character to get the last character.", VALUE);
	}

	ObjectString* str = copy_string(vm, &string->chars[string->length - 1], 1);
	push(vm->current_module_record, OBJECT_VAL(str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(str));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *string_get_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	const Value value = args[0];
	const Value index = args[1];
	if (!IS_INT(index)) {
		return MAKE_GC_SAFE_ERROR(vm, "<index> must be of type 'number'.", TYPE);
	}
	const ObjectString *string = AS_CRUX_STRING(value);
	if (AS_INT(index) < 0 || (uint32_t)AS_INT(index) >= string->length) {
		return MAKE_GC_SAFE_ERROR(vm, "<index> must be a non negative number that is less than the length of the string.", BOUNDS);
	}

	if (string->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "'string' must have at least one character to get a character.", VALUE);
	}

	ObjectString* str = copy_string(vm, &string->chars[(uint32_t)AS_INT(index)], 1);
	push(vm->current_module_record, OBJECT_VAL(str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_upper_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		ObjectString* empty_str = copy_string(vm, "", 0);
		push(vm->current_module_record, OBJECT_VAL(empty_str));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(empty_str));
		pop(vm->current_module_record);
		return res;
	}

	char *buffer = ALLOCATE(vm, char, string->length + 1);

	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	for (uint32_t i = 0; i < string->length; i++) {
		if (islower(string->chars[i])) {
			buffer[i] = toupper(string->chars[i]);
		} else {
			buffer[i] = string->chars[i];
		}
	}
	buffer[string->length] = '\0';

	ObjectString* result_str = take_string(vm, buffer, string->length);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_lower_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		ObjectString* empty_str = copy_string(vm, "", 0);
		push(vm->current_module_record, OBJECT_VAL(empty_str));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(empty_str));
		pop(vm->current_module_record);
		return res;
	}

	char *buffer = ALLOCATE(vm, char, string->length + 1);

	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	for (uint32_t i = 0; i < string->length; i++) {
		if (isupper(string->chars[i])) {
			buffer[i] = tolower(string->chars[i]);
		} else {
			buffer[i] = string->chars[i];
		}
	}
	buffer[string->length] = '\0';

	ObjectString* result_str = take_string(vm, buffer, string->length);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_strip_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		ObjectString* empty_str = copy_string(vm, "", 0);
		push(vm->current_module_record, OBJECT_VAL(empty_str));
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(empty_str));
		pop(vm->current_module_record);
		return res;
	}

	uint32_t start = 0;
	while (start < string->length && isspace(string->chars[start])) {
		start++;
	}

	uint32_t end = string->length;
	while (end > start && isspace(string->chars[end - 1])) {
		end--;
	}

	ObjectString* result_str = copy_string(vm, string->chars + start, end - start);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_substring_method(VM *vm,
				      int arg_count __attribute__((unused)),
				      const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<start> index must be of type 'int'.", VALUE);
	}
	if (!IS_INT(args[2])) {
		return MAKE_GC_SAFE_ERROR(vm, "<end> index must be of type 'int'.", VALUE);
	}

	const int rawStartIndex = AS_INT(args[1]);
	const int rawEndIndex = AS_INT(args[2]);

	if (rawStartIndex < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<start> index cannot be negative.", BOUNDS);
	}
	if (rawEndIndex < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<end> index cannot be negative.", BOUNDS);
	}

	const uint32_t startIndex = (uint32_t)rawStartIndex;
	const uint32_t endIndex = (uint32_t)rawEndIndex;

	if (startIndex > string->length || endIndex > string->length ||
	    startIndex > endIndex) {
		return MAKE_GC_SAFE_ERROR(vm, "Index out of bounds.", BOUNDS);
	}

	const char *substring = string->chars + startIndex;
	ObjectString *newSubstring = copy_string(vm, substring, endIndex - startIndex);
	push(vm->current_module_record, OBJECT_VAL(newSubstring));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(newSubstring));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *string_split_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectModuleRecord *moduleRecord = vm->current_module_record;
	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<delimiter> must be of type 'string'.", TYPE);
	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	const ObjectString *delimiter = AS_CRUX_STRING(args[1]);

	if (delimiter->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<delimiter> cannot be empty.", TYPE);
	}

	if (string->length == 0) {
		ObjectArray *resultArray = new_array(vm, 1, moduleRecord);
		push(vm->current_module_record, OBJECT_VAL(resultArray));

		ObjectString* empty_str = copy_string(vm, "", 0);
		push(vm->current_module_record, OBJECT_VAL(empty_str));
		array_add_back(vm, resultArray, OBJECT_VAL(empty_str));
		pop(vm->current_module_record); // pop empty_str

		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(resultArray));
		pop(vm->current_module_record); // pop resultArray
		return res;
	}

	if (delimiter->length > string->length) {
		ObjectArray *resultArray = new_array(vm, 1, moduleRecord);
		push(vm->current_module_record, OBJECT_VAL(resultArray));
		array_add(vm, resultArray, OBJECT_VAL(string), 0);
		ObjectResult* res = new_ok_result(vm, OBJECT_VAL(resultArray));
		pop(vm->current_module_record);
		return res;
	}

	const uint32_t stringLength = string->length;
	const uint32_t delimiterLength = delimiter->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, delimiterLength);
	if (prefixTable == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	build_prefix_table(delimiter->chars, delimiterLength, prefixTable);

	// initial guess of splits size
	ObjectArray *resultArray = new_array(vm, stringLength / (delimiterLength + 1) + 1, moduleRecord);
	push(vm->current_module_record, OBJECT_VAL(resultArray));

	uint32_t lastSplitIndex = 0;
	uint32_t j = 0;
	uint32_t lastAddIndex = 0;

	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != delimiter->chars[j]) {
			j = prefixTable[j - 1];
		}

		if (string->chars[i] == delimiter->chars[j]) {
			j++;
		}

		if (j == delimiterLength) {
			const uint32_t substringLength = i - lastSplitIndex - delimiterLength + 1;
			ObjectString *substring = copy_string(vm, string->chars + lastSplitIndex, substringLength);
			push(vm->current_module_record, OBJECT_VAL(substring));
			array_add(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
			pop(vm->current_module_record);
			lastAddIndex++;

			lastSplitIndex = i + 1;
			j = 0;
		}
	}

	if (lastSplitIndex < stringLength) {
		const uint32_t substringLength = stringLength - lastSplitIndex;
		ObjectString *substring = copy_string(vm, string->chars + lastSplitIndex, substringLength);
		push(vm->current_module_record, OBJECT_VAL(substring));
		array_add(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
		pop(vm->current_module_record);
	}

	FREE_ARRAY(vm, uint32_t, prefixTable, delimiterLength);

	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(resultArray));
	pop(vm->current_module_record);
	return res;
}

// KMP string-matching algorithm
ObjectResult *string_contains_method(VM *vm,
				     int arg_count __attribute__((unused)),
				     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Argument 'goal' must be of type 'string'.", TYPE);
	}
	const ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (goal->length == 0) {
		return new_ok_result(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	if (goal->length > string->length) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	const uint32_t stringLength = string->length;
	const uint32_t goalLength = goal->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);

	if (prefixTable == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate memory for string.contains().", MEMORY);
	}

	build_prefix_table(goal->chars, goalLength, prefixTable);

	uint32_t j = 0;
	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != goal->chars[j]) {
			j = prefixTable[j - 1];
		}
		if (string->chars[i] == goal->chars[j]) {
			j++;
		}
		if (j == goalLength) {
			FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
			return new_ok_result(vm, BOOL_VAL(true));
		}
	}

	FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
	return new_ok_result(vm, BOOL_VAL(false));
}

ObjectResult *string_replace_method(VM *vm,
				    int arg_count __attribute__((unused)),
				    const Value *args)
{
	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1]) ||
	    !IS_CRUX_STRING(args[2])) {
		return MAKE_GC_SAFE_ERROR(vm, "All arguments must be strings.", TYPE);
	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	const ObjectString *goal = AS_CRUX_STRING(args[1]);
	const ObjectString *replacement = AS_CRUX_STRING(args[2]);

	if (string->length == 0 || goal->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm,
			string->length == 0
				? "Source string must have at least one character."
				: "<target> substring must have at least one character.",
			VALUE);
	}

	if (goal->length > string->length) {
		return new_ok_result(vm, OBJECT_VAL(string));
	}

	const uint32_t stringLength = string->length;
	const uint32_t goalLength = goal->length;
	const uint32_t replacementLength = replacement->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);
	if (prefixTable == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	build_prefix_table(goal->chars, goalLength, prefixTable);

	uint32_t matchCount = 0;
	uint32_t *matchIndices = ALLOCATE(vm, uint32_t, stringLength);
	if (matchIndices == NULL) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	uint32_t j = 0;
	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != goal->chars[j]) {
			j = prefixTable[j - 1];
		}
		if (string->chars[i] == goal->chars[j]) {
			j++;
		}
		if (j == goalLength) {
			matchIndices[matchCount++] = i - goalLength + 1;
			j = 0;
		}
	}

	if (matchCount == 0) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);
		return new_ok_result(vm, OBJECT_VAL(string));
	}

	const int64_t lengthDiff = (int64_t)replacementLength - (int64_t)goalLength;
	const int64_t newLength_64 = (int64_t)stringLength + lengthDiff * matchCount;

	if (newLength_64 < 0 || newLength_64 > UINT32_MAX) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);
		return MAKE_GC_SAFE_ERROR(vm, "Resulting string length exceeds maximum.", VALUE);
	}
	const uint32_t newStringLength = (uint32_t)newLength_64;

	char *newStringChars = ALLOCATE(vm, char, newStringLength + 1);
	if (newStringChars == NULL) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed.", MEMORY);
	}

	uint32_t writeIndex = 0;
	uint32_t lastCopyIndex = 0;
	for (uint32_t i = 0; i < matchCount; i++) {
		const uint32_t copyLength = matchIndices[i] - lastCopyIndex;
		memcpy(newStringChars + writeIndex,
		       string->chars + lastCopyIndex, copyLength);
		writeIndex += copyLength;

		memcpy(newStringChars + writeIndex, replacement->chars,
		       replacementLength);
		writeIndex += replacementLength;

		lastCopyIndex = matchIndices[i] + goalLength;
	}

	const uint32_t remainingLength = stringLength - lastCopyIndex;
	memcpy(newStringChars + writeIndex, string->chars + lastCopyIndex,
	       remainingLength);
	writeIndex += remainingLength;

	newStringChars[newStringLength] = '\0';

	ObjectString *newString = take_string(vm, newStringChars, newStringLength);
	push(vm->current_module_record, OBJECT_VAL(newString));
	ObjectResult* res = new_ok_result(vm, OBJECT_VAL(newString));
	pop(vm->current_module_record);

	FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
	FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);

	return res;
}

ObjectResult *string_starts_with_method(VM *vm,
					int arg_count __attribute__((unused)),
					const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "First argument <char> must be of type 'string'.", TYPE);
	}

	const ObjectString *prefix = AS_CRUX_STRING(args[1]);

	if (prefix->length == 0) {
		return new_ok_result(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	if (prefix->length > string->length) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	if (memcmp(string->chars, prefix->chars, prefix->length) == 0) {
		return new_ok_result(vm, BOOL_VAL(true));
	}
	return new_ok_result(vm, BOOL_VAL(false));
}

ObjectResult *string_ends_with_method(VM *vm,
				      int arg_count __attribute__((unused)),
				      const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "First argument must be of type 'string'.", TYPE);
	}
	const ObjectString *suffix = AS_CRUX_STRING(args[1]);

	if (suffix->length == 0) {
		return new_ok_result(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return new_ok_result(vm, BOOL_VAL(false));
	}
	if (suffix->length > string->length) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	const uint32_t stringStart = string->length - suffix->length;

	if (memcmp(string->chars + stringStart, suffix->chars,
		   suffix->length) == 0) {
		return new_ok_result(vm, BOOL_VAL(true));
	}
	return new_ok_result(vm, BOOL_VAL(false));
}

Value string_is_al_num_method(VM *vm __attribute__((unused)),
			      int arg_count __attribute__((unused)),
			      const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!isalnum(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_alpha_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!isalpha(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_digit_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!isdigit(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_lower_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!islower(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_upper_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!isupper(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_space_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	for (uint32_t i = 0; i < string->length; i++) {
		if (!isspace(string->chars[i])) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

Value string_is_empty_method(VM *vm __attribute__((unused)),
			     int arg_count __attribute__((unused)),
			     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	return BOOL_VAL(string->length == 0);
}