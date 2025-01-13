#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

typedef struct {
	char* content;
	char* error;
} FileResult;

#define MAX_PATH_LENGTH 4096

FileResult readFile(const char* path);

char* resolvePath(const char* basePath, const char* importPath);

void freeFileResult(FileResult result);

#endif //FILE_HANDLER_H
