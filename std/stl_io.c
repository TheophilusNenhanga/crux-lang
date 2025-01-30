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

static bool isMode(char **desiredMode, const char *givenMode, int length) {
	for (int i = 0; i < length; i++) {
		if (strcmp(desiredMode[i], givenMode) == 0) {
			return true;
		}
	}
	return false;
}

NativeReturn _openFile(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_STRING(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Parameter <path> must be of type 'string'.", 42), TYPE, STELLA));
		return nativeReturn;
	}

	if (!IS_STRING(args[1])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Parameter <mode> must be of type 'string'.", 42), TYPE, STELLA));
		return nativeReturn;
	}

	char *resolvedPath = resolvePath(vm->module->path->chars, AS_CSTRING(args[0]));
	if (resolvedPath == NULL) {
		free(resolvedPath);
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Failed to resolve path.", 23), IO, STELLA));
		return nativeReturn;
	}

	FILE *file = fopen(resolvedPath, AS_CSTRING(args[1]));
	if (file == NULL) {
		free(resolvedPath);
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Failed to open file.", 20), IO, STELLA));
		return nativeReturn;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	ObjectFile *objectFile = newFile(vm, takeString(vm, resolvedPath, strlen(resolvedPath)), file, AS_STRING(args[1]));
	if (objectFile == NULL) {
		free(resolvedPath);
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Failed to create file object.", 29), RUNTIME, STELLA));
		return nativeReturn;
	}

	objectFile->size = fileSize;

	free(resolvedPath);
	nativeReturn.values[0] = OBJECT_VAL(objectFile);
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _closeFile(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);
	if (!IS_FILE(args[0])) {
		nativeReturn.values[0] = BOOL_VAL(false);
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Parameter <file> must be of type 'file'.", 40), TYPE, STELLA));
		return nativeReturn;
	}
	ObjectFile *objectFile = AS_FILE(args[0]);
	int res = fclose(objectFile->handle);

	if (res != 0) {
		nativeReturn.values[0] = BOOL_VAL(false);
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Failed to close file.", 21), IO, STELLA));
		return nativeReturn;
	}
	objectFile->isOpen = false;

	// the gc will free this later.
	nativeReturn.values[0] = BOOL_VAL(true);
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _readOne(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!IS_FILE(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "First parameter must be a 'file'.", 33), TYPE, STELLA));
		return nativeReturn;
	}
	ObjectFile *objectFile = AS_FILE(args[0]);

	if (!objectFile->isOpen) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Cannot read from a file that has been closed.", 45), IO, STELLA));
		return nativeReturn;
	}

	if (objectFile->handle == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Corrupted file given.", 31), IO, STELLA));
		return nativeReturn;
	}

	char* readingModes[] = {"r", "rb", "r+", "w+", "wb+", "a+", "ab+"};
	if (!isMode(readingModes, objectFile->mode->chars, 7)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "File was not opened for reading.", 32), IO, STELLA));
		return nativeReturn;
	}

	int ch = fgetc(objectFile->handle);

	if (ch == EOF) {
		nativeReturn.values[0] = OBJECT_VAL(takeString(vm, "", 0));
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}

	char str[2] = {(char) ch, '\0'};
	nativeReturn.values[0] = OBJECT_VAL(takeString(vm, str, 1));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
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

NativeReturn _writeln(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 2);

    if (!IS_FILE(args[0]) || !IS_STRING(args[1])) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "First parameter must be a 'file', second must be a 'string'.", 60),
            TYPE, STELLA));
        return nativeReturn;
    }

    ObjectFile *objectFile = AS_FILE(args[0]);
    const char *content = AS_CSTRING(args[1]);

    if (!objectFile->isOpen) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Cannot write to a file that has been closed.", 44),
            IO, STELLA));
        return nativeReturn;
    }

    if (objectFile->handle == NULL) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Corrupted file given.", 21),
            IO, STELLA));
        return nativeReturn;
    }

    char* writingModes[] = {"w", "wb", "w+", "wb+", "a", "ab", "a+", "ab+"};
    if (!isMode(writingModes, objectFile->mode->chars, 8)) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "File was not opened for writing.", 32),
            IO, STELLA));
        return nativeReturn;
    }

    if (fprintf(objectFile->handle, "%s\n", content) < 0) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Error writing to file.", 22),
            IO, STELLA));
        return nativeReturn;
    }

    nativeReturn.values[0] = BOOL_VAL(true);
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

NativeReturn _writeOne(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 2);

    if (!IS_FILE(args[0]) || !IS_STRING(args[1])) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "First parameter must be a 'file', second must be a 'string'.", 60),
            TYPE, STELLA));
        return nativeReturn;
    }

    ObjectFile *objectFile = AS_FILE(args[0]);
    const char *str = AS_CSTRING(args[1]);

    if (!objectFile->isOpen) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Cannot write to a file that has been closed.", 43),
            IO, STELLA));
        return nativeReturn;
    }

    if (objectFile->handle == NULL) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Corrupted file given.", 21),
            IO, STELLA));
        return nativeReturn;
    }

    if (strlen(str) != 1) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Second argument must be a single character.", 43),
            VALUE, STELLA));
        return nativeReturn;
    }

    char* writingModes[] = {"w", "wb", "w+", "wb+", "a", "ab", "a+", "ab+"};
    if (!isMode(writingModes, objectFile->mode->chars, 8)) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "File was not opened for writing.", 32),
            IO, STELLA));
        return nativeReturn;
    }

    if (fputc(str[0], objectFile->handle) == EOF) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Error writing to file.", 22),
            IO, STELLA));
        return nativeReturn;
    }

    nativeReturn.values[0] = BOOL_VAL(true);
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

NativeReturn _readln(VM *vm, int argCount, Value *args) {
    NativeReturn nativeReturn = makeNativeReturn(vm, 2);

    if (!IS_FILE(args[0])) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "First parameter must be a 'file'.", 33),
            TYPE, STELLA));
        return nativeReturn;
    }

    ObjectFile *objectFile = AS_FILE(args[0]);

    if (!objectFile->isOpen) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Cannot read from a file that has been closed.", 45),
            IO, STELLA));
        return nativeReturn;
    }

    if (objectFile->handle == NULL) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Corrupted file given.", 21),
            IO, STELLA));
        return nativeReturn;
    }

    char* readingModes[] = {"r", "rb", "r+", "w+", "wb+", "a+", "ab+"};
    if (!isMode(readingModes, objectFile->mode->chars, 7)) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "File was not opened for reading.", 32),
            IO, STELLA));
        return nativeReturn;
    }

    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), objectFile->handle) == NULL) {
        if (feof(objectFile->handle)) {
            nativeReturn.values[0] = NIL_VAL;
            nativeReturn.values[1] = NIL_VAL;
            return nativeReturn;
        }
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm,
            takeString(vm, "Error reading from file.", 24),
            IO, STELLA));
        return nativeReturn;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        len--;
    }

    nativeReturn.values[0] = OBJECT_VAL(takeString(vm, buffer, len));
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
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
				OBJECT_VAL(newError(vm, takeString(vm, "Channel and content must be strings.", 34), TYPE, STELLA));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	const char *content = AS_CSTRING(args[1]);

	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Invalid channel specified.", 24), VALUE, STELLA));
		return nativeReturn;
	}

	if (fprintf(stream, "%s", content) < 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error writing to stream.", 23), IO, STELLA));
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
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stdin.", 24), IO, STELLA));
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
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stdin.", 24), IO, STELLA));
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
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Channel must be a string.", 24), TYPE, STELLA));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Invalid channel specified.", 24), VALUE, STELLA));
		return nativeReturn;
	}

	int ch = fgetc(stream);
	if (ch == EOF) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stream.", 24), IO, STELLA));
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
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Channel must be a string.", 24), TYPE, STELLA));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Invalid channel specified.", 24), VALUE, STELLA));
		return nativeReturn;
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stream.", 24), IO, STELLA));
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
				OBJECT_VAL(newError(vm, takeString(vm, "Number of characters must be a number.", 36), TYPE, STELLA));
		return nativeReturn;
	}

	int n = (int) AS_NUMBER(args[0]);
	if (n <= 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Number of characters must be positive.", 36), VALUE, STELLA));
		return nativeReturn;
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Failed to allocate memory for input buffer.", 41), MEMORY, STELLA));
		return nativeReturn;
	}

	size_t read = 0;
	while (read < n) {
		int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stdin.", 24), IO, STELLA));
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
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Channel must be a string.", 24), TYPE, STELLA));
		return nativeReturn;
	}

	if (!IS_NUMBER(args[1])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Number of characters must be a number.", 36), TYPE, STELLA));
		return nativeReturn;
	}

	const char *channel = AS_CSTRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Invalid channel specified.", 24), VALUE, STELLA));
		return nativeReturn;
	}

	int n = (int) AS_NUMBER(args[1]);
	if (n <= 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Number of characters must be positive.", 36), VALUE, STELLA));
		return nativeReturn;
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] =
				OBJECT_VAL(newError(vm, takeString(vm, "Failed to allocate memory for input buffer.", 41), MEMORY, STELLA));
		return nativeReturn;
	}

	size_t read = 0;
	while (read < n) {
		int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, takeString(vm, "Error reading from stream.", 24), IO, STELLA));
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

	nativeReturn.values[0] = OBJECT_VAL(takeString(vm, buffer, read));
	nativeReturn.values[1] = NIL_VAL;
	FREE_ARRAY(vm, char, buffer, n + 1);
	return nativeReturn;
}
