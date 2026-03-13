#ifndef PANIC_H
#define PANIC_H

#include <stdarg.h>
#include "../include/scanner.h"
#include "compiler.h"

typedef struct {
	const char *name;
	const char *hint;
} ErrorDetails;

/**
 * Displays a runtime error message with formatting and stack trace.
 */
void runtime_panic(ObjectModuleRecord *module_record,
		   ErrorType type, const char *format, ...);

/**
 * Creates a formatted error message for type mismatches with type
 * information.
 */
char *type_error_message(VM *vm, Value value, const char *expected_type);

void print_error_line(int line, const char *source, int startCol, int length);

/**
 * Report a compile error pointing at the given token.
 * The message is a plain string.
 */
void error_at(Parser *parser, const Token *token, const char *message,
	      ErrorType error_type);

/**
 * Report a compile error at parser->previous (the last consumed token).
 * Plain string message.
 */
void compiler_panic(Parser *parser, const char *message, ErrorType error_type);

/**
 * Report a compile error at parser->previous with a printf-style format string.
 */
void compiler_panicf(Parser *parser, ErrorType error_type, const char *format,
		     ...) __attribute__((format(printf, 3, 4)));

/**
 * Report a compile error at parser->current (the not-yet-consumed token).
 * Used by consume() when the unexpected token hasn't been advanced past yet.
 * Plain string message.
 */
void compiler_panic_at_current(Parser *parser, const char *message,
			       ErrorType error_type);

/**
 * Report a compile error at parser->current with a printf-style format string.
 */
void compiler_panicf_at_current(Parser *parser, ErrorType error_type,
				const char *format, ...)
	__attribute__((format(printf, 3, 4)));

char *repeat(char c, int count);

#endif // PANIC_H
