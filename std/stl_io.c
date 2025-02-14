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

void printResult(ObjectResult* result) {
	if (result->isOk) {
			printf("Ok<");
			printValue(result->content.value);
			printf(">");
	}
	else {
		printf("Err<");
		// TODO: Make this print the error's type
		printf("%s", result->content.error->message->chars);
		printf(">");
	}
}

void valuePrint(Value value) {
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_NUMBER(value)) {
		printNumber(value);
	} else if (IS_STL_ARRAY(value)) {
		printArray(AS_STL_ARRAY(value));
	} else if (IS_STL_TABLE(value)) {
		printTable(AS_STL_TABLE(value));
	}else if (IS_STL_RESULT(value)) {
		printResult(AS_STL_RESULT(value));
	} else if (IS_STL_OBJECT(value)) {
		printObject(value);
	}
}

// Standard I/O Functions
ObjectResult* _print(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* _println(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	printf("\n");
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* _printTo(VM *vm, int argCount, Value *args) {

	if (!IS_STL_STRING(args[0]) || !IS_STL_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel and content must be strings.", 36), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	const char *content = AS_C_STRING(args[1]);

	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	if (fprintf(stream, "%s", content) < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error writing to stream.", 26), IO, false));
	}

	return stellaOk(vm, BOOL_VAL(true));
}

ObjectResult* _scan(VM *vm, int argCount, Value *args) {

	int ch = getchar();
	if (ch == EOF) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
	}

	int overflow;
	while ((overflow = getchar()) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	return stellaOk(vm, OBJECT_VAL(copyString(vm, str, 1)));
}

ObjectResult* _scanln(VM *vm, int argCount, Value *args) {

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 26), IO, false));
	}

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, buffer, len)));
}

ObjectResult* _scanFrom(VM *vm, int argCount, Value *args) {

	if (!IS_STL_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	int ch = fgetc(stream);
	if (ch == EOF) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
	}

	int overflow;
	while ((overflow = fgetc(stream)) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	return stellaOk(vm, OBJECT_VAL(copyString(vm, str, 1)));
}

ObjectResult* _scanlnFrom(VM *vm, int argCount, Value *args) {

	if (!IS_STL_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
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

	return stellaOk(vm, OBJECT_VAL(copyString(vm, buffer, len)));
}

ObjectResult* _nscan(VM *vm, int argCount, Value *args) {

	if (!IS_NUMBER(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be a number.", 38), TYPE, false));
	}

	int n = (int) AS_NUMBER(args[0]);
	if (n <= 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
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

	Value string = OBJECT_VAL(copyString(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return stellaOk(vm, string);
}

ObjectResult* _nscanFrom(VM *vm, int argCount, Value *args) {

	if (!IS_STL_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	if (!IS_NUMBER(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be a number.", 38), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	int n = (int) AS_NUMBER(args[1]);
	if (n <= 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
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

	Value string =  OBJECT_VAL(copyString(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return stellaOk(vm, string);
}
