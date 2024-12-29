#ifndef PANIC_H
#define PANIC_H

#include "compiler.h"
#include "scanner.h"

#define RED "\x1b[31m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define RESET "\x1b[0m"

typedef struct {
	const char *name;
	const char *hint;
} ErrorDetails;

void runtimePanic(VM *vm, ErrorType type, const char *format, ...);

void printErrorLine(int line, const char *source, int startCol, int length);
void compilerPanic(Parser *parser, const char *message, ErrorType errorType);
void errorAt(Parser *parser, Token *token, const char *message, ErrorType errorType);

#endif // PANIC_H
