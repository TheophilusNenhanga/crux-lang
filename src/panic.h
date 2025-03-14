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

/**
 * Displays a runtime error message with enhanced formatting and stack trace.
 * 
 * @param vm The virtual machine instance
 * @param type The type of error
 * @param format Format string for the error message
 * @param ... Additional arguments based on the format string
 */
void runtimePanic(VM *vm, ErrorType type, const char *format, ...);

/**
 * Creates a formatted error message for type mismatches with actual type information.
 * 
 * @param vm The virtual machine
 * @param value The value that caused the type error
 * @param expectedType Description of the expected type(s)
 * @return A formatted error message
 */
char* typeErrorMessage(VM *vm, Value value, const char* expectedType);

void printErrorLine(int line, const char *source, int startCol, int length);
void compilerPanic(Parser *parser, const char *message, ErrorType errorType);
void errorAt(Parser *parser, Token *token, const char *message, ErrorType errorType);

// Helper function to repeat a character
char* repeat(char c, int count);

#endif // PANIC_H
