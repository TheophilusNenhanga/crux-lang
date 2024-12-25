#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

typedef struct {
	char* content;
	char* error;
} FileResult;

#define MAX_PATH_LENGTH 4096

void initFileSystem(void);

void freeFileSystem(void);

FileResult readFile(const char* path);

char* resolveRelativePath(const char* basePath, const char* relativePath);

char* normalizePath(const char* path);

void freeFileResult(FileResult result);

#endif //FILE_HANDLER_H
