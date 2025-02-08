#include <stdlib.h>

#include "../memory.h"
#include "string.h"

static bool isUpper(char c) { return (c >= 'A' && c <= 'Z'); }
static bool isLower(char c) { return (c >= 'a' && c <= 'z'); }
static bool isSpace(char c) { return (c == ' ' || c == '\n' || c == '\t'); }

static char toUpper(char c) { return c + ('A' - 'a'); }

static char toLower(char c) { return c + ('a' - 'A'); }

void buildPrefixTable(const char *pattern, uint64_t patternLength, uint64_t *prefixTable) {
	uint64_t j = 0;
	prefixTable[0] = 0;

	for (int i = 1; i < patternLength; i++) {
		while (j > 0 && pattern[i] != pattern[j]) {
			j = prefixTable[j - 1];
		}
		if (pattern[i] == pattern[j]) {
			j++;
		}
		prefixTable[i] = j;
	}
}

NativeReturn stringFirstMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_STRING(value);
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "'string' must have at least one character to get the first character.", 69), VALUE,
								 false));
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(vm, &string->chars[0], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringLastMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_STRING(value);
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
				vm, copyString(vm, "'string' must have at least one character to get the last character.", 68), VALUE, false));
		return returnValue;
	}
	returnValue.values[0] = OBJECT_VAL(copyString(vm, &string->chars[string->length - 1], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringGetMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	Value index = args[1];
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	if (!IS_NUMBER(index)) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));
		return returnValue;
	}
	ObjectString *string = AS_STRING(value);
	if (AS_NUMBER(index) < 0 || AS_NUMBER(index) > (double) string->length) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
				vm, copyString(vm, "<index> must be a non negative number that is less than the length of the string.", 81),
				INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
				vm, copyString(vm, "'string' must have at least one character to get a character.", 61), VALUE, false));
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(vm, &string->chars[(uint64_t) AS_NUMBER(index)], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringUpperMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);
	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Cannot make empty string uppercase.", 35), VALUE, false));
		return returnValue;
	}

	for (uint64_t i = 0; i < string->length; i++) {
		if (isLower(string->chars[i])) {
			string->chars[i] = toUpper(string->chars[i]);
		}
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringLowerMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Cannot make empty string lowercase.", 35), VALUE, false));

		return returnValue;
	}

	for (uint64_t i = 0; i < string->length; i++) {
		if (isUpper(string->chars[i])) {
			string->chars[i] = toLower(string->chars[i]);
		}
	}

	returnValue.values[0] = NIL_VAL;
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringStripMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Cannot strip whitespace from an empty string.", 45), VALUE, false));
		return returnValue;
	}

	uint64_t start = 0;
	while (start < string->length && isSpace(string->chars[start])) {
		start++;
	}

	uint64_t end = string->length;
	while (end > start && isSpace(string->chars[end - 1])) {
		end--;
	}

	if (start == end) {
		returnValue.values[0] = OBJECT_VAL(copyString(vm, "", 0));
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(vm, string->chars + start, end - start));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringSubstringMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Cannot get substring of empty string.", 37), VALUE, false));
		return returnValue;
	}

	if (!IS_NUMBER(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Cannot get substring of empty string.", 37), VALUE, false));
		return returnValue;
	}
	if (!IS_NUMBER(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "<end> index should be of type 'number'.", 39), VALUE, false));
		return returnValue;
	}

	uint64_t startIndex = AS_NUMBER(args[1]);
	uint64_t endIndex = AS_NUMBER(args[2]);
	if (startIndex > string->length || endIndex > string->length || startIndex > endIndex) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Index out of bounds.", 20), INDEX_OUT_OF_BOUNDS, false));
		return returnValue;
	}

	char *substring = string->chars + startIndex;
	ObjectString *newSubstring = copyString(vm, substring, endIndex - startIndex);

	returnValue.values[0] = OBJECT_VAL(newSubstring);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringSplitMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Both arguments must be strings.", 31), TYPE, false));
		return returnValue;
	}

	ObjectString *string = AS_STRING(args[0]);
	ObjectString *delimiter = AS_STRING(args[1]);

	if (string->length == 0 || delimiter->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm,
														string->length == 0 ? copyString(vm, "Source string cannot be empty.", 30)
																								: copyString(vm, "Delimiter cannot be empty.", 26),
														VALUE, false));
		return returnValue;
	}

	if (delimiter->length > string->length) {
		ObjectArray *resultArray = newArray(vm, 1);
		arrayAdd(vm, resultArray, OBJECT_VAL(string), 0);
		returnValue.values[0] = OBJECT_VAL(resultArray);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t stringLength = string->length;
	uint64_t delimiterLength = delimiter->length;

	uint64_t *prefixTable = ALLOCATE(vm, uint64_t, delimiterLength * sizeof(uint64_t));
	if (prefixTable == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
		return returnValue;
	}

	buildPrefixTable(delimiter->chars, delimiterLength, prefixTable);

	// initial guess of splits size
	ObjectArray *resultArray = newArray(vm, stringLength / (delimiterLength + 1) + 1);

	uint64_t lastSplitIndex = 0;
	uint64_t j = 0;
	uint64_t lastAddIndex = 0;

	for (uint64_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != delimiter->chars[j]) {
			j = prefixTable[j - 1];
		}

		if (string->chars[i] == delimiter->chars[j]) {
			j++;
		}

		if (j == delimiterLength) {
			uint64_t substringLength = i - lastSplitIndex - delimiterLength + 1;

			char *substringChars = ALLOCATE(vm, char, substringLength + 1);
			if (substringChars == NULL) {
				free(prefixTable);
				returnValue.values[0] = NIL_VAL;
				returnValue.values[1] =
						OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
				return returnValue;
			}

			memcpy(substringChars, string->chars + lastSplitIndex, substringLength);
			substringChars[substringLength] = '\0';

			ObjectString *substring = copyString(vm, substringChars, substringLength);
			arrayAdd(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
			lastAddIndex++;

			lastSplitIndex = i + 1;

			j = 0;
		}
	}

	if (lastSplitIndex < stringLength) {
		uint64_t substringLength = stringLength - lastSplitIndex;
		char *substringChars = ALLOCATE(vm, char, substringLength + 1);

		if (substringChars == NULL) {
			free(prefixTable);
			returnValue.values[0] = NIL_VAL;
			returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
			return returnValue;
		}

		memcpy(substringChars, string->chars + lastSplitIndex, substringLength);
		substringChars[substringLength] = '\0';

		ObjectString *substring = copyString(vm, substringChars, substringLength);
		arrayAdd(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
	}

	free(prefixTable);

	returnValue.values[0] = OBJECT_VAL(resultArray);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

// KMP string-matching algorithm
NativeReturn stringContainsMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);

	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Argument 'goal' must be of type 'string'.", 41), TYPE, false));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = BOOL_VAL(false);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t stringLength = string->length;
	uint64_t goalLength = goal->length;

	uint64_t *prefixTable = ALLOCATE(vm, uint64_t, goalLength * sizeof(uint64_t));

	if (prefixTable == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm, copyString(vm, "Failed to allocate memory for string.contains().", 48), MEMORY, false));
		return returnValue;
	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

	uint64_t j = 0;
	for (uint64_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != goal->chars[j]) {
			j = prefixTable[j - 1];
		}
		if (string->chars[i] == goal->chars[j]) {
			j++;
		}
		if (j == goalLength) {
			free(prefixTable);
			returnValue.values[0] = BOOL_VAL(true);
			returnValue.values[1] = NIL_VAL;
			return returnValue;
		}
	}

	free(prefixTable);
	returnValue.values[0] = BOOL_VAL(false);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringReplaceMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "All arguments must be strings.", 30), TYPE, false));
		return returnValue;
	}

	ObjectString *string = AS_STRING(args[0]);
	ObjectString *goal = AS_STRING(args[1]);
	ObjectString *replacement = AS_STRING(args[2]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(
				newError(vm,
								 string->length == 0 ? copyString(vm, "Source string must have at least one character.", 47)
																		 : copyString(vm, "<target> substring must have at least one character.", 52),
								 VALUE, false));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = OBJECT_VAL(string);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t stringLength = string->length;
	uint64_t goalLength = goal->length;
	uint64_t replacementLength = replacement->length;

	uint64_t *prefixTable = ALLOCATE(vm, uint64_t, goalLength * sizeof(uint64_t));
	if (prefixTable == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
		return returnValue;
	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

	uint64_t matchCount = 0;
	uint64_t *matchIndices = ALLOCATE(vm, uint64_t, stringLength * sizeof(uint64_t));
	if (matchIndices == NULL) {
		free(prefixTable);
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
		return returnValue;
	}

	uint64_t j = 0;
	for (uint64_t i = 0; i < stringLength; i++) {
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
		free(prefixTable);
		free(matchIndices);
		returnValue.values[0] = OBJECT_VAL(string);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t newStringLength = stringLength + (replacementLength - goalLength) * matchCount;

	char *newStringChars = ALLOCATE(vm, char, newStringLength + 1);
	if (newStringChars == NULL) {
		free(prefixTable);
		free(matchIndices);
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
		return returnValue;
	}

	uint64_t writeIndex = 0;
	uint64_t lastCopyIndex = 0;
	for (uint64_t i = 0; i < matchCount; i++) {
		uint64_t copyLength = matchIndices[i] - lastCopyIndex;
		memcpy(newStringChars + writeIndex, string->chars + lastCopyIndex, copyLength);
		writeIndex += copyLength;

		memcpy(newStringChars + writeIndex, replacement->chars, replacementLength);
		writeIndex += replacementLength;

		lastCopyIndex = matchIndices[i] + goalLength;
	}

	uint64_t remainingLength = stringLength - lastCopyIndex;
	memcpy(newStringChars + writeIndex, string->chars + lastCopyIndex, remainingLength);
	writeIndex += remainingLength;

	newStringChars[newStringLength] = '\0';

	ObjectString *newString = copyString(vm, newStringChars, newStringLength);

	free(prefixTable);
	free(matchIndices);

	returnValue.values[0] = OBJECT_VAL(newString);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringStartsWithMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);
	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "First argument <char> must be of type 'string'.", 47), TYPE, false));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = BOOL_VAL(false);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	for (uint64_t i = 0; i < goal->length; i++) {
		if (string->chars[i] != goal->chars[i]) {
			returnValue.values[0] = BOOL_VAL(false);
			returnValue.values[1] = NIL_VAL;
			return returnValue;
		}
	}


	returnValue.values[0] = BOOL_VAL(true);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringEndsWithMethod(VM *vm, int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(vm, 2);
	ObjectString *string = AS_STRING(args[0]);
	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "First argument must be of type 'string'.", 40), TYPE, false));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = BOOL_VAL(false);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t stringStart = string->length - goal->length;
	for (uint64_t i = 0; i < goal->length; i++) {
		if (string->chars[stringStart + i] != goal->chars[i]) {
			returnValue.values[0] = BOOL_VAL(false);
			returnValue.values[1] = NIL_VAL;
			return returnValue;
		}
	}

	returnValue.values[0] = BOOL_VAL(true);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}
