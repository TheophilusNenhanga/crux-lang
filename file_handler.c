#include "file_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *resolvePath(const char *path) {
	if (path == NULL) {
		return NULL;
	}
#ifdef _WIN32
	char *resolvedPath = (char *) malloc(MAX_PATH_LENGTH);
	if (_fullpath(resolvedPath, path, MAX_PATH_LENGTH) == NULL) {
		free(resolvedPath);
		return NULL;
	}
#else
	char *resolvedPath[MAX_PATH_LENGTH];
	if (realpath(path, resolvedPath) == NULL) {
		return strdup(path);
	}
#endif
	char *newPath = strdup(resolvedPath);
	free(resolvedPath);
	return newPath;
}

FileResult readFile(const char *path) {
	FileResult result = {NULL, NULL};
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		size_t errorLen = strlen(path) + 32;
		result.error = (char *) malloc(errorLen);
		if (result.error != NULL) {
			snprintf(result.error, errorLen, "Could not open file \"%s\"", path);
		}
		return result;
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	result.content = (char *) malloc(fileSize + 1);
	if (result.content == NULL) {
		result.error = strdup("Not enough memory to read file");
		fclose(file);
		return result;
	}

	size_t bytesRead = fread(result.content, 1, fileSize, file);
	if (bytesRead < fileSize) {
		free(result.content);
		result.content = NULL;
		result.error = strdup("Could not read file completely");
		fclose(file);
		return result;
	}

	result.content[bytesRead] = '\0';
	fclose(file);
	return result;
}

void freeFileResult(FileResult result) {
	free(result.content);
	free(result.error);
}
