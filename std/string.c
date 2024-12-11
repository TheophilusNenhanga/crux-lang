#include "string.h"

#include <stdlib.h>

#include "../memory.h"

#define ALPHABET_SIZE 256


static bool isUpper(char c) { return (c >= 'A' && c <= 'Z'); }
static bool isLower(char c) { return (c >= 'a' && c <= 'z'); }
static bool isDigit(char c) { return (c >= '0' && c <= '9'); }
static bool isSpace(char c) { return (c == ' ' || c == '\n' || c == '\t'); }

static char toUpper(char c) { return c + ('A' - 'a'); }

static char toLower(char c) { return c + ('a' - 'A'); }

NativeReturn stringFirstMethod(int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_STRING(value);
	NativeReturn returnValue = makeNativeReturn(2);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("'string' must have at least one character to get the first character.", 69), VALUE, STELLA));
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(&string->chars[0], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringLastMethod(int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_STRING(value);
	NativeReturn returnValue = makeNativeReturn(2);
	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("'string' must have at least one character to get the last character.", 68), VALUE, STELLA));
		return returnValue;
	}
	returnValue.values[0] = OBJECT_VAL(copyString(&string->chars[string->length - 1], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringGetMethod(int argCount, Value *args) {
	Value value = args[0];
	Value index = args[1];
	NativeReturn returnValue = makeNativeReturn(2);
	if (!IS_NUMBER(index)) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("index must be of type 'number'.", 31), TYPE, STELLA));
		return returnValue;
	}
	ObjectString *string = AS_STRING(value);
	if (AS_NUMBER(index) < 0 || AS_NUMBER(index) > string->length) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
				takeString("Index must be a non negative number that is less than the length of the string.", 79), INDEX_OUT_OF_BOUNDS, STELLA));
		return returnValue;
	}

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] =
				OBJECT_VAL(newError(takeString("'string' must have at least one character to get a character.", 61), VALUE, STELLA));
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(&string->chars[(uint64_t) AS_NUMBER(index)], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringUpperMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);
	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Cannot make empty string uppercase.", 35), VALUE, STELLA));
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

NativeReturn stringLowerMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Cannot make empty string lowercase.", 35), VALUE, STELLA));

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

NativeReturn stringStripMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Cannot strip whitespace from an empty string.", 45), VALUE, STELLA));
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
		returnValue.values[0] = OBJECT_VAL(copyString("", 0));
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(string->chars + start, end - start));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringSubstringMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	return returnValue;
}

NativeReturn stringReplaceMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	return returnValue;
}

NativeReturn stringSplitMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	return returnValue;
}

void prepareBadCharacterHeuristic(ObjectString *goal, uint64_t* badCharacter) {
	for (int i = 0; i <ALPHABET_SIZE; i++) {
		badCharacter[i] = goal->length;
	}

	for (uint64_t i = 0; i < goal->length; i++) {
		badCharacter[(unsigned char) goal->chars[i]] = goal->length - 1 - i;
	}
}

// Two-way string-matching algorithm
NativeReturn stringContainsMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);

	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("First argument must be of type 'string'.", 40), TYPE, STELLA));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
			string->length == 0
				? takeString("String must have at least one character.", 40)
				: takeString("<goal> string must have at least one character.", 47),
			VALUE,
			STELLA
		));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = BOOL_VAL(false);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	uint64_t* badCharacter = ALLOCATE(uint64_t, ALPHABET_SIZE * sizeof(uint64_t));

	if (badCharacter == NULL) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("Failed to allocate memory for string.contains().", 48), MEMORY, STELLA));
		return returnValue;
	}

	prepareBadCharacterHeuristic(goal, badCharacter);

	uint64_t i = 0;
	while (i<= string->length - goal->length) {
		uint64_t j = goal->length - 1;
		while (j<goal->length &&string->chars[i+j] == goal->chars[j]) {
			if (j==0) {
				free(badCharacter);
				returnValue.values[0] = BOOL_VAL(true);
				returnValue.values[1] = NIL_VAL;
				return returnValue;
			}
			j--;
		}
		uint64_t shift = badCharacter[(unsigned char)string->chars[i + goal->length - 1]];
		i += (shift == goal->length) ? 1 : shift;
	}

	free(badCharacter);

	returnValue.values[0] = BOOL_VAL(false);
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}

NativeReturn stringStartsWithMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);
	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("First argument must be of type 'string'.", 40), TYPE, STELLA));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
			string->length == 0
				? takeString("String must have at least one character.", 40)
				: takeString("<goal> string must have at least one character.", 47),
			VALUE,
			STELLA
		));
		return returnValue;
	}

	if (goal->length > string->length) {
		returnValue.values[0] = BOOL_VAL(false);
		returnValue.values[1] = NIL_VAL;
		return returnValue;
	}

	for (uint64_t i= 0; i < goal->length; i++) {
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

NativeReturn stringEndsWithMethod(int argCount, Value *args) {
	NativeReturn returnValue = makeNativeReturn(2);
	ObjectString *string = AS_STRING(args[0]);
	if (!IS_STRING(args[1])) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(takeString("First argument must be of type 'string'.", 40), TYPE, STELLA));
		return returnValue;
	}
	ObjectString *goal = AS_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError(
			string->length == 0
				? takeString("String must have at least one character.", 40)
				: takeString("<goal> string must have at least one character.", 47),
			VALUE,
			STELLA
		));
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
