#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#endif

#ifdef _WIN32
#define MAX_PATH_LENGTH MAX_PATH
#else
#define MAX_PATH_LENGTH PATH_MAX
#endif


typedef struct {
  char *content;
  char *error;
} FileResult;

/**
 * @brief Reads the entire contents of a file into memory
 *
 * Opens the specified file, reads its entire contents into a
 * dynamically allocated buffer, and returns the result.
 * If an error occurs, sets the error field in the result struct.
 *
 * @param path The path of the file to read
 * @return FileResult struct containing either the file content or an error
 * message
 */
FileResult readFile(const char *path);

/**
 * @brief Resolves a potentially relative import path to an absolute path
 *
 * Given a base path and an import path, resolves the import path to an
 * absolute path. If the import path is already absolute, resolves it directly.
 * Otherwise, combines it with the directory of the base path before resolving.
 * Handles platform-specific path resolution.
 *
 * @param basePath The base file path (typically the current file's path)
 * @param importPath The import path to resolve (may be relative or absolute)
 * @return Dynamically allocated string containing the resolved absolute path
 * (caller must free)
 */
char *resolvePath(const char *basePath, const char *importPath);

/**
 * @brief Frees memory allocated by a FileResult struct
 *
 * Releases the memory for both the content and error fields
 * of a FileResult struct.
 *
 * @param result The FileResult struct to free
 */
void freeFileResult(FileResult result);

#endif // FILE_HANDLER_H
