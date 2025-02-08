#include "stl_io.h"
#include <stdio.h>
#include <stdlib.h>

#include "../file_handler.h"
#include "../memory.h"
#include "../object.h"


static FILE *getChannel(const char *channel) {
	if (strcmp(channel, "stdin") == 0)
		return stdin;
	if (strcmp(channel, "stdout") == 0)
		return stdout;
	if (strcmp(channel, "stderr") == 0)
		return stderr;
	return NULL;
}

static bool isValidMode(const char *mode) {
	char* modes[] = {"r", "rb", "w", "wb", "a", "ab", "r+", "rb+", "w+", "wb+", "a+", "ab+"};
	int modeLength = 12;
	for (int i = 0; i < modeLength; i++) {
		if (strcmp(modes[i], mode) == 0) {
			return true;
		}
	}
	return false;
}

static bool isMode(char **desiredMode, const char *givenMode, int length) {
	for (int i = 0; i < length; i++) {
		if (strcmp(desiredMode[i], givenMode) == 0) {
			return true;
		}
	}
	return false;
}

void valuePrint(Value value);

void printNumber(Value value) {
	double number = AS_NUMBER(value);
	if (number == (int) number) {
		printf("%.0f", number);
	} else {
		printf("%lf", number);
	}
}

void printArray(ObjectArray *array) {
	printf("[");
	for (int i = 0; i < array->size; i++) {
		valuePrint(array->array[i]);
		if (i != array->size - 1) {
			printf(", ");
		}
	}
	printf("]");
}

void printTable(ObjectTable *table) {
	uint16_t printed = 0;
	printf("{");
	for (int i = 0; i < table->capacity; i++) {
		if (table->entries[i].isOccupied) {
			valuePrint(table->entries[i].key);
			printf(":");
			valuePrint(table->entries[i].value);
			if (printed != table->size - 1) {
				printf(", ");
			}
			printed++;
		}
	}
	printf("}");
}

void valuePrint(Value value) {
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_NUMBER(value)) {
		printNumber(value);
	} else if (IS_ARRAY(value)) {
		printArray(AS_ARRAY(value));
	} else if (IS_TABLE(value)) {
		printTable(AS_TABLE(value));
	} else if (IS_OBJECT(value)) {
		printObject(value);
	}
}

// Standard I/O Functions
NativeReturn _print(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _println(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	printf("\n");
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _printTo(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Channel and content must be strings.", 36), TYPE, false));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	const char *content = AS_CSTRING(args[1]);

	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
		return nativeReturn;
	}

	if (fprintf(stream, "%s", content) < 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error writing to stream.", 26), IO, false));
		return nativeReturn;
	}

	nativeReturn.values[0] = BOOL_VAL(true);
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _scan(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	int ch = getchar();
	if (ch == EOF) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
		return nativeReturn;
	}

	int overflow;
	while ((overflow = getchar()) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, str, 1));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _scanln(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stdin.", 26), IO, false));
		return nativeReturn;
	}

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, buffer, len));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _scanFrom(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
		return nativeReturn;
	}

	int ch = fgetc(stream);
	if (ch == EOF) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
		return nativeReturn;
	}

	int overflow;
	while ((overflow = fgetc(stream)) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, str, 1));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _scanlnFrom(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
		return nativeReturn;
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
		return nativeReturn;
	}


	size_t len = strlen(buffer);
	// discard extra characters
	if (len > 0 && buffer[len - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF);
	}

	// Removing trailing newline character
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, buffer, len));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _nscan(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_NUMBER(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Number of characters must be a number.", 38), TYPE, false));
		return nativeReturn;
	}

	int n = (int) AS_NUMBER(args[0]);
	if (n <= 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
		return nativeReturn;
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
		return nativeReturn;
	}

	size_t read = 0;
	while (read < n) {
		int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
			return nativeReturn;
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = getchar()) != '\n' && ch != EOF);
	}

	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, buffer, read));
	nativeReturn.values[1] = NIL_VAL;
	FREE_ARRAY(vm, char, buffer, n + 1);
	return nativeReturn;
}

NativeReturn _nscanFrom(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
		return nativeReturn;
	}

	if (!IS_NUMBER(args[1])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Number of characters must be a number.", 38), TYPE, false));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
		return nativeReturn;
	}

	int n = (int) AS_NUMBER(args[1]);
	if (n <= 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
		return nativeReturn;
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
		return nativeReturn;
	}

	size_t read = 0;
	while (read < n) {
		int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
			return nativeReturn;
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF);
	}

	nativeReturn.values[0] = OBJECT_VAL(copyString(vm, buffer, read));
	nativeReturn.values[1] = NIL_VAL;
	FREE_ARRAY(vm, char, buffer, n + 1);
	return nativeReturn;
}
