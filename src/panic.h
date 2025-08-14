#ifndef PANIC_H
#define PANIC_H

#include "compiler.h"
#include "scanner.h"

typedef struct {
	const char *name;
	const char *hint;
} ErrorDetails;

/**
 * Displays a runtime error message with formatting and stack trace.
 */
void runtime_panic(ObjectModuleRecord *module_record, bool should_exit,
		  ErrorType type, const char *format, ...);

/**
 * Creates a formatted error message for type mismatches with type
 * information.
 */
char *type_error_message(VM *vm, Value value, const char *expected_type);

void print_error_line(int line, const char *source, int startCol, int length);
void compiler_panic(Parser *parser, const char *message, ErrorType error_type);
void error_at(Parser *parser, const Token *token, const char *message,
	     ErrorType error_type);

char *repeat(char c, int count);

#endif // PANIC_H
