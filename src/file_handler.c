#include "file_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Extracts the directory name from a file path,
 *
 * Parses a file path and returns the directory portion.
 * Handles platform-specific path separators and edge cases.
 *
 * @param path The file path to process
 * @return Dynamically allocated string containing the directory name (caller
 * must free)
 */
static char *dirName(const char *path)
{
	if (path == NULL) {
		return NULL;
	}

	char *pathCopy = strdup(path);
	if (pathCopy == NULL) {
		return NULL;
	}

	size_t pathLen = strlen(pathCopy);
	if (pathLen == 0) {
		free(pathCopy);
		return strdup(".");
	}

	while (pathLen > 1 && (pathCopy[pathLen - 1] == '/' ||
			       pathCopy[pathLen - 1] == '\\')) {
		pathCopy[pathLen--] = '\0';
	}

	char *lastSlash = strrchr(pathCopy, '/');
#ifdef _WIN32
	char *lastBackslash = strrchr(pathCopy, '\\');
	if (lastBackslash > lastSlash) {
		lastSlash = lastBackslash;
	}
#endif
	if (lastSlash == NULL) {
		free(pathCopy);
		return strdup(".");
	}

	if (lastSlash == pathCopy) {
#ifdef _WIN32
		char *result;
		if (pathCopy[1] == ':') {
			result = strdup(pathCopy);
		} else {
			result = strdup("\\");
		}
#else
		char *result = strdup("/");
#endif
		free(pathCopy);
		return result;
	}
#ifdef _WIN32
	if (lastSlash == pathCopy + 2 && pathCopy[1] == ':') {
		char *result = malloc(4);
		if (result == NULL) {
			free(pathCopy);
			return NULL;
		}
		result[0] = pathCopy[0];
		result[1] = ':';
		result[2] = '\\';
		result[3] = '\0';
		free(pathCopy);
		return result;
	}
#endif
	*lastSlash = '\0';
	char *result = strdup(pathCopy);
	free(pathCopy);
	return result;
}

/**
 * @brief Gets the directory component from a path,
 *
 * Creates a copy of the input path and uses dirName() to extract
 * the directory component. Handles memory management by making
 * a separate copy of the result.
 *
 * @param path The file path to process
 * @return Dynamically allocated string containing the directory (caller must
 * free)
 */
static char *get_directory_from_path(const char *path)
{
	if (path == NULL) {
		return NULL;
	}

	char *pathCopy = strdup(path);
	if (pathCopy == NULL) {
		return NULL;
	}

	char *dir = dirName(pathCopy);
	if (dir == NULL) {
		free(pathCopy);
		return NULL;
	}

	char *result = strdup(dir);
	free(pathCopy);
	free(dir);
	return result;
}

/**
 * @brief Combines a base path with a relative path
 *
 * Joins two path components together, handling path separators appropriately.
 * If the relative path is absolute, returns a copy of the relative path.
 *
 * @param base The base directory path
 * @param relative The relative path to append
 * @return Dynamically allocated string containing the combined path (caller
 * must free)
 */
static char *combinePaths(const char *base, const char *relative)
{
	if (base == NULL || relative == NULL)
		return NULL;

	if (relative[0] == '/'
#ifdef _WIN32
	    || (strlen(relative) > 2 && relative[1] == ':')
#endif
	) {
		return strdup(relative);
	}

	const size_t base_len = strlen(base);
	const size_t relative_len = strlen(relative);
	const size_t total_len = base_len + 1 + relative_len +
				 1; // +1 : '/' +1 '\0'

	char *result = malloc(total_len);
	if (result == NULL)
		return NULL;

	strcpy(result, base);

	if (base_len > 0 && base[base_len - 1] != '/' &&
	    base[base_len - 1] != '\\') {
#ifdef _WIN32
		strcat(result, "\\");
#else
		strcat(result, "/");
#endif
	}

	strcat(result, relative);
	return result;
}

char *resolve_path(const char *base_path, const char *import_path)
{
	if (import_path == NULL)
		return NULL;

	if (base_path == NULL || import_path[0] == '/'
#ifdef _WIN32
	    || (strlen(import_path) > 2 && import_path[1] == ':')
#endif
	) {
#ifdef _WIN32
		char *resolved_path = malloc(MAX_PATH_LENGTH);
		if (_fullpath(resolved_path, import_path, MAX_PATH_LENGTH) ==
		    NULL) {
			free(resolved_path);
			return NULL;
		}
		return resolved_path;
#else
		char resolvedPath[MAX_PATH_LENGTH];
		if (realpath(import_path, resolvedPath) == NULL) {
			return strdup(import_path);
		}
		return strdup(resolvedPath);
#endif
	}

	char *baseDir = get_directory_from_path(base_path);
	if (baseDir == NULL)
		return NULL;

	char *combinedPath = combinePaths(baseDir, import_path);
	free(baseDir);
	if (combinedPath == NULL)
		return NULL;

#ifdef _WIN32
	char *resolvedPath = malloc(MAX_PATH_LENGTH);
	if (_fullpath(resolvedPath, combinedPath, MAX_PATH_LENGTH) == NULL) {
		free(combinedPath);
		free(resolvedPath);
		return NULL;
	}
	free(combinedPath);
	return resolvedPath;
#else
	char resolvedPath[MAX_PATH_LENGTH];
	if (realpath(combinedPath, resolvedPath) == NULL) {
		free(combinedPath);
		return strdup(combinedPath);
	}
	free(combinedPath);
	return strdup(resolvedPath);
#endif
}

FileResult read_file(const char *path)
{
	FileResult result = {NULL, NULL};
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		const size_t errorLen = strlen(path) + 32;
		result.error = (char *)malloc(errorLen);
		if (result.error != NULL) {
			snprintf(result.error, errorLen,
				 "Could not open file \"%s\"", path);
		}
		return result;
	}

	fseek(file, 0L, SEEK_END);
	const size_t fileSize = ftell(file);
	rewind(file);

	result.content = (char *)malloc(fileSize + 1);
	if (result.content == NULL) {
		result.error = strdup("Not enough memory to read file");
		fclose(file);
		return result;
	}

	const size_t bytesRead = fread(result.content, 1, fileSize, file);
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

void free_file_result(const FileResult result)
{
	free(result.content);
	free(result.error);
}
