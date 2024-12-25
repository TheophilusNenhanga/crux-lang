#include "file_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static char* currentWorkingDir = NULL;

void initFileSystem(void) {
    char* cwd = (char*)malloc(MAX_PATH_LENGTH * sizeof(char));
    if (cwd != NULL) {
        #ifdef _WIN32
            _getcwd(cwd, MAX_PATH_LENGTH * sizeof(char));
        #else
            getcwd(cwd, MAX_PATH_LENGTH * sizeof(char));
        #endif
        currentWorkingDir = cwd;
    }
}

void freeFileSystem(void) {
    free(currentWorkingDir);
    currentWorkingDir = NULL;
}

char* normalizePath(const char* path) {
    if (path == NULL) return NULL;

    size_t len = strlen(path);
    char* normalized = (char*)malloc(len + 1);
    if (normalized == NULL) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\\') {
            normalized[j++] = '/';
        } else {
            normalized[j++] = path[i];
        }
    }
    normalized[j] = '\0';
    return normalized;
}

char* resolveRelativePath(const char* basePath, const char* relativePath) {
    if (relativePath == NULL) return NULL;

    if (relativePath[0] == '/'
    #ifdef _WIN32
        || (strlen(relativePath) > 1 && relativePath[1] == ':')
    #endif
    ) {
        return strdup(relativePath);
    }

    if (strncmp(relativePath, "./", 2) == 0) {
        relativePath += 2;
    }

    size_t baseLen = strlen(basePath);
    size_t relLen = strlen(relativePath);
    size_t totalLen = baseLen + 1 + relLen + 1;

    char* resolvedPath = (char*)malloc(totalLen);
    if (resolvedPath == NULL) return NULL;

    strcpy(resolvedPath, basePath);

    if (baseLen > 0 && resolvedPath[baseLen - 1] != '/' && resolvedPath[baseLen - 1] != '\\') {
        resolvedPath[baseLen] = '/';
        strcpy(resolvedPath + baseLen + 1, relativePath);
    } else {
        strcpy(resolvedPath + baseLen, relativePath);
    }

    return normalizePath(resolvedPath);
}

FileResult readFile(const char* path) {
    FileResult result = {NULL, NULL};
    FILE* file = fopen(path, "rb");

    if (file == NULL) {
        size_t errorLen = strlen(path) + 32;
        result.error = (char*)malloc(errorLen);
        if (result.error != NULL) {
            snprintf(result.error, errorLen, "Could not open file \"%s\"", path);
        }
        return result;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    result.content = (char*)malloc(fileSize + 1);
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