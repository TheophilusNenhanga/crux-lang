#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdbool.h>

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
FileResult read_file(const char *path);

/**
 * @brief Resolves a potentially relative import path to an absolute path
 *
 * Given a base path and an import path, resolves the import path to an
 * absolute path. If the import path is already absolute, resolves it directly.
 * Otherwise, combines it with the directory of the base path before resolving.
 * Handles platform-specific path resolution.
 *
 * @param base_path The base file path (typically the current file's path)
 * @param import_path The import path to resolve (may be relative or absolute)
 * @return Dynamically allocated string containing the resolved absolute path
 * (caller must free)
 */
char *resolve_path(const char *base_path, const char *import_path);

/**
 * @brief Frees memory allocated by a FileResult struct
 *
 * Releases the memory for both the content and error fields
 * of a FileResult struct.
 *
 * @param result The FileResult struct to free
 */
void free_file_result(FileResult result);

/**
 * @brief Gets the user's home directory
 *
 * @return Dynamically allocated string containing the home directory (caller must free)
 */
char *get_home_dir(void);

/**
 * @brief Gets the .crux directory in the user's home directory
 *
 * Ensures the directory exists before returning.
 *
 * @return Dynamically allocated string containing the path to .crux (caller must free)
 */
char *get_crux_dir(void);

/**
 * @brief Ensures a directory exists, creating it if necessary
 *
 * @param path The path to the directory
 * @return true if the directory exists or was created, false otherwise
 */
bool ensure_dir_exists(const char *path);

/**
 * @brief Combines a base path with a relative path
 *
 * @param base The base directory path
 * @param relative The relative path to append
 * @return Dynamically allocated string containing the combined path (caller must free)
 */
char *combine_paths(const char *base, const char *relative);

#endif // FILE_HANDLER_H
