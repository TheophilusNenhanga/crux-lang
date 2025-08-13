#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../memory.h"
#include "string.h"

static void buildPrefixTable(const char *pattern, const uint32_t patternLength,
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

ObjectResult *stringFirstMethod(VM *vm, int argCount __attribute__((unused)),
				const Value *args)
{
	const Value value = args[0];
	const ObjectString *string = AS_CRUX_STRING(value);

	if (string->length == 0) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"'string' must have at least "
						"one character to "
						"get the first character.",
						69),
				     VALUE, false));
	}

	return newOkResult(vm,
			   OBJECT_VAL(copyString(vm, &string->chars[0], 1)));
}

ObjectResult *stringLastMethod(VM *vm, int argCount __attribute__((unused)),
			       const Value *args)
{
	const Value value = args[0];
	const ObjectString *string = AS_CRUX_STRING(value);
	if (string->length == 0) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"'string' must have at least "
						"one character to "
						"get the last character.",
						68),
				     VALUE, false));
	}
	return newOkResult(vm,
			   OBJECT_VAL(copyString(
				   vm, &string->chars[string->length - 1], 1)));
}

ObjectResult *stringGetMethod(VM *vm, int argCount __attribute__((unused)),
			      const Value *args)
{
	const Value value = args[0];
	const Value index = args[1];
	if (!IS_INT(index)) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "<index> must be of type 'number'.",
					    33),
				 TYPE, false));
	}
	const ObjectString *string = AS_CRUX_STRING(value);
	if (AS_INT(index) < 0 || (uint32_t)AS_INT(index) >= string->length) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(
					 vm,
					 "<index> must be a non negative "
					 "number that is "
					 "less than the length of the string.",
					 81),
				 BOUNDS, false));
	}

	if (string->length == 0) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "'string' must have at least one "
					    "character to get a character.",
					    61),
				 VALUE, false));
	}

	return newOkResult(vm,
			   OBJECT_VAL(copyString(
				   vm, &string->chars[(uint32_t)AS_INT(index)],
				   1)));
}

ObjectResult *stringUpperMethod(VM *vm, int argCount __attribute__((unused)),
				const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return newOkResult(vm, OBJECT_VAL(copyString(vm, "", 0)));
	}

	char *buffer = ALLOCATE(vm, char, string->length + 1);

	if (buffer == NULL) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm, "Memory allocation failed.",
						26),
				     MEMORY, false));
	}

	for (uint32_t i = 0; i < string->length; i++) {
		if (islower(string->chars[i])) {
			buffer[i] = toupper(string->chars[i]);
		} else {
			buffer[i] = string->chars[i];
		}
	}
	buffer[string->length] = '\0';
	return newOkResult(vm,
			   OBJECT_VAL(takeString(vm, buffer, string->length)));
}

ObjectResult *stringLowerMethod(VM *vm, int argCount __attribute__((unused)),
				const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return newOkResult(vm, OBJECT_VAL(copyString(vm, "", 0)));
	}

	char *buffer = ALLOCATE(vm, char, string->length + 1);

	for (uint32_t i = 0; i < string->length; i++) {
		if (isupper(string->chars[i])) {
			buffer[i] = tolower(string->chars[i]);
		} else {
			buffer[i] = string->chars[i];
		}
	}
	buffer[string->length] = '\0';
	return newOkResult(vm,
			   OBJECT_VAL(takeString(vm, buffer, string->length)));
}

ObjectResult *stringStripMethod(VM *vm, int argCount __attribute__((unused)),
				const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return newOkResult(vm, OBJECT_VAL(copyString(vm, "", 0)));
	}

	uint32_t start = 0;
	while (start < string->length && isspace(string->chars[start])) {
		start++;
	}

	uint32_t end = string->length;
	while (end > start && isspace(string->chars[end - 1])) {
		end--;
	}

	return newOkResult(vm, OBJECT_VAL(copyString(vm, string->chars + start,
						     end - start)));
}

ObjectResult *stringSubstringMethod(VM *vm,
				    int argCount __attribute__((unused)),
				    const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (!IS_INT(args[1])) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(
					 vm,
					 "<start> index must be of type 'int'.",
					 36),
				 VALUE, false));
	}
	if (!IS_INT(args[2])) {
		return newErrorResult(
			vm,
			newError(
				vm,
				copyString(vm,
					   "<end> index must be of type 'int'.",
					   34),
				VALUE, false));
	}

	const int rawStartIndex = AS_INT(args[1]);
	const int rawEndIndex = AS_INT(args[2]);

	if (rawStartIndex < 0) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "<start> index cannot be negative.",
					    32),
				 BOUNDS, false));
	}
	if (rawEndIndex < 0) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "<end> index cannot be negative.",
					    30),
				 BOUNDS, false));
	}

	const uint32_t startIndex = (uint32_t)rawStartIndex;
	const uint32_t endIndex = (uint32_t)rawEndIndex;

	if (startIndex > string->length || endIndex > string->length ||
	    startIndex > endIndex) {
		return newErrorResult(
			vm,
			newError(vm, copyString(vm, "Index out of bounds.", 20),
				 BOUNDS, false));
	}

	const char *substring = string->chars + startIndex;
	ObjectString *newSubstring = copyString(vm, substring,
						endIndex - startIndex);

	return newOkResult(vm, OBJECT_VAL(newSubstring));
}

ObjectResult *stringSplitMethod(VM *vm, int argCount __attribute__((unused)),
				const Value *args)
{
	ObjectModuleRecord *moduleRecord = vm->currentModuleRecord;
	if (!IS_CRUX_STRING(args[1])) {
		return newErrorResult(
			vm,
			newError(
				vm,
				copyString(
					vm,
					"<delimiter> must be of type 'string'.",
					37),
				TYPE, false));
	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	const ObjectString *delimiter = AS_CRUX_STRING(args[1]);

	if (delimiter->length == 0) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm, "<delimiter> cannot be empty.",
					    28),
				 TYPE, false));
	}

	if (string->length == 0) {
		ObjectArray *resultArray = newArray(vm, 1, moduleRecord);
		arrayAddBack(vm, resultArray,
			     OBJECT_VAL(copyString(vm, "", 0)));
		return newOkResult(vm, OBJECT_VAL(resultArray));
	}

	if (delimiter->length > string->length) {
		ObjectArray *resultArray = newArray(vm, 1, moduleRecord);
		arrayAdd(vm, resultArray, OBJECT_VAL(string), 0);
		return newOkResult(vm, OBJECT_VAL(resultArray));
	}

	const uint32_t stringLength = string->length;
	const uint32_t delimiterLength = delimiter->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, delimiterLength);
	if (prefixTable == NULL) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm, "Memory allocation failed.",
						25),
				     MEMORY, false));
	}

	buildPrefixTable(delimiter->chars, delimiterLength, prefixTable);

	// initial guess of splits size
	ObjectArray *resultArray = newArray(
		vm, stringLength / (delimiterLength + 1) + 1, moduleRecord);

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
			const uint32_t substringLength = i - lastSplitIndex -
							 delimiterLength + 1;
			ObjectString *substring =
				copyString(vm, string->chars + lastSplitIndex,
					   substringLength);
			arrayAdd(vm, resultArray, OBJECT_VAL(substring),
				 lastAddIndex);
			lastAddIndex++;

			lastSplitIndex = i + 1;

			j = 0;
		}
	}

	if (lastSplitIndex < stringLength) {
		const uint32_t substringLength = stringLength - lastSplitIndex;
		ObjectString *substring = copyString(
			vm, string->chars + lastSplitIndex, substringLength);
		arrayAdd(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
	}

	FREE_ARRAY(vm, uint32_t, prefixTable, delimiterLength);

	return newOkResult(vm, OBJECT_VAL(resultArray));
}

// KMP string-matching algorithm
ObjectResult *stringContainsMethod(VM *vm, int argCount __attribute__((unused)),
				   const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);

	if (!IS_CRUX_STRING(args[1])) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Argument 'goal' must be of "
						"type 'string'.",
						41),
				     TYPE, false));
	}
	const ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (goal->length == 0) {
		return newOkResult(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return newOkResult(vm, BOOL_VAL(false));
	}

	if (goal->length > string->length) {
		return newOkResult(vm, BOOL_VAL(false));
	}

	const uint32_t stringLength = string->length;
	const uint32_t goalLength = goal->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);

	if (prefixTable == NULL) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Failed to allocate memory for "
						"string.contains().",
						48),
				     MEMORY, false));
	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

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
			return newOkResult(vm, BOOL_VAL(true));
		}
	}

	FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
	return newOkResult(vm, BOOL_VAL(false));
}

ObjectResult *stringReplaceMethod(VM *vm, int argCount __attribute__((unused)),
				  const Value *args)
{
	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1]) ||
	    !IS_CRUX_STRING(args[2])) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "All arguments must be strings.",
					    30),
				 TYPE, false));
	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	const ObjectString *goal = AS_CRUX_STRING(args[1]);
	const ObjectString *replacement = AS_CRUX_STRING(args[2]);

	if (string->length == 0 || goal->length == 0) {
		return newErrorResult(
			vm,
			newError(vm,
				 string->length == 0
					 ? copyString(vm,
						      "Source string must have "
						      "at least one character.",
						      47)
					 : copyString(vm,
						      "<target> substring must "
						      "have at least one "
						      "character.",
						      52),
				 VALUE, false));
	}

	if (goal->length > string->length) {
		return newOkResult(vm, OBJECT_VAL(string));
	}

	const uint32_t stringLength = string->length;
	const uint32_t goalLength = goal->length;
	const uint32_t replacementLength = replacement->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);
	if (prefixTable == NULL) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm, "Memory allocation failed.",
						25),
				     MEMORY, false));
	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

	uint32_t matchCount = 0;
	uint32_t *matchIndices = ALLOCATE(vm, uint32_t, stringLength);
	if (matchIndices == NULL) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm, "Memory allocation failed.",
						25),
				     MEMORY, false));
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
		return newOkResult(vm, OBJECT_VAL(string));
	}

	const int64_t lengthDiff = (int64_t)replacementLength -
				   (int64_t)goalLength;
	const int64_t newLength_64 = (int64_t)stringLength +
				     lengthDiff * matchCount;

	if (newLength_64 < 0 || newLength_64 > UINT32_MAX) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Resulting string length "
						"exceeds maximum.",
						40),
				     VALUE, false));
	}
	const uint32_t newStringLength = (uint32_t)newLength_64;

	char *newStringChars = ALLOCATE(vm, char, newStringLength + 1);
	if (newStringChars == NULL) {
		FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
		FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm, "Memory allocation failed.",
						25),
				     MEMORY, false));
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

	ObjectString *newString = takeString(vm, newStringChars,
					     newStringLength);
	FREE_ARRAY(vm, uint32_t, prefixTable, goalLength);
	FREE_ARRAY(vm, uint32_t, matchIndices, stringLength);

	return newOkResult(vm, OBJECT_VAL(newString));
}

ObjectResult *stringStartsWithMethod(VM *vm,
				     int argCount __attribute__((unused)),
				     const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"First argument <char> must be "
						"of type 'string'.",
						47),
				     TYPE, false));
	}

	const ObjectString *prefix = AS_CRUX_STRING(
		args[1]); // Renamed 'goal' to 'prefix'

	if (prefix->length == 0) {
		return newOkResult(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return newOkResult(vm,
				   BOOL_VAL(false)); // prefix is non-empty here
	}

	if (prefix->length > string->length) {
		return newOkResult(vm, BOOL_VAL(false));
	}

	if (memcmp(string->chars, prefix->chars, prefix->length) == 0) {
		return newOkResult(vm, BOOL_VAL(true));
	}
	return newOkResult(vm, BOOL_VAL(false));
}

ObjectResult *stringEndsWithMethod(VM *vm, int argCount __attribute__((unused)),
				   const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"First argument must be of "
						"type 'string'.",
						40),
				     TYPE, false));
	}
	const ObjectString *suffix = AS_CRUX_STRING(args[1]);

	if (suffix->length == 0) {
		return newOkResult(vm, BOOL_VAL(true));
	}

	if (string->length == 0) {
		return newOkResult(vm, BOOL_VAL(false));
	}
	if (suffix->length > string->length) {
		return newOkResult(vm, BOOL_VAL(false));
	}

	const uint32_t stringStart = string->length - suffix->length;

	if (memcmp(string->chars + stringStart, suffix->chars,
		   suffix->length) == 0) {
		return newOkResult(vm, BOOL_VAL(true));
	}
	return newOkResult(vm, BOOL_VAL(false));
}

Value stringIsAlNumMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsAlphaMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsDigitMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsLowerMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsUpperMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsSpaceMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
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

Value stringIsEmptyMethod(VM *vm __attribute__((unused)),
			  int argCount __attribute__((unused)),
			  const Value *args)
{
	const ObjectString *string = AS_CRUX_STRING(args[0]);
	return BOOL_VAL(string->length == 0);
}
