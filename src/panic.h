#ifndef PANIC_H
#define PANIC_H

#include "scanner.h"
#include "compiler.h"

#define RED "\x1b[31m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define RESET "\x1b[0m"

typedef struct {
  const char *name;
  const char *hint;
} ErrorDetails;

/**
 * Displays a runtime error message with formatting and stack trace.
 */
void runtimePanic(ObjectModuleRecord* moduleRecord, bool shouldExit, ErrorType type, const char *format, ...);

/**
 * Creates a formatted error message for type mismatches with type
 * information.
 */
char *typeErrorMessage(VM *vm, Value value, const char *expectedType);

void printErrorLine(int line, const char *source, int startCol, int length);
void compilerPanic(Parser *parser, const char *message, ErrorType errorType);
void errorAt(Parser *parser, const Token *token, const char *message,
             ErrorType errorType);

char *repeat(char c, int count);

#endif // PANIC_H
