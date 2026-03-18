#include "panic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

static ErrorDetails getErrorDetails(const ErrorType type)
{
	switch (type) {
	case SYNTAX:
		return (ErrorDetails){
			"Syntax Error",
		};
	case MATH:
		return (ErrorDetails){
			"Math Error",
		};
	case BOUNDS:
		return (ErrorDetails){
			"Bounds Error",
		};
	case LOOP_EXTENT: {
		return (ErrorDetails){
			"Loop Extent Error",
		};
	}
	case TYPE:
		return (ErrorDetails){
			"Type Error",
		};
	case LIMIT: {
		return (ErrorDetails){"Limit Error"};
	}
	case NAME: {
		return (ErrorDetails){"Name Error"};
	}
	case CLOSURE_EXTENT: {
		return (ErrorDetails){"Closure Extent Error"};
	}
	case LOCAL_EXTENT: {
		return (ErrorDetails){"Local Variable Extent Error"};
	}
	case ARGUMENT_EXTENT: {
		return (ErrorDetails){"Argument Extent Error"};
	}
	case COLLECTION_EXTENT: {
		return (ErrorDetails){"Collection Extent Error"};
	}
	case VARIABLE_EXTENT: {
		return (ErrorDetails){"Variable Extent Error"};
	}
	case RETURN_EXTENT: {
		return (ErrorDetails){"Return Extent Error"};
	}
	case ARGUMENT_MISMATCH: {
		return (ErrorDetails){"Argument Mismatch Error"};
	}
	case STACK_OVERFLOW: {
		return (ErrorDetails){"Stack Overflow Error"};
	}
	case COLLECTION_GET: {
		return (ErrorDetails){"Collection Get Error"};
	}
	case COLLECTION_SET: {
		return (ErrorDetails){"Collection Set Error"};
	}
	case MEMORY: {
		return (ErrorDetails){"Memory Error"};
	}
	case ASSERT: {
		return (ErrorDetails){"Assert Error"};
	}
	case IMPORT_EXTENT: {
		return (ErrorDetails){"Import Extent Error"};
	}
	case IO: {
		return (ErrorDetails){"IO Error"};
	}
	case IMPORT: {
		return (ErrorDetails){"Import Error"};
	}
	case BRANCH_EXTENT: {
		return (ErrorDetails){"Branch Extent Error"};
	}
	case VALUE: {
		return (ErrorDetails){"Value Error"};
	}
	case RUNTIME:
	default:
		return (ErrorDetails){"Runtime Error"};
	}
}

void print_error_line(const int line, const char *source, int startCol, const int length)
{
	const char *lineStart = source;
	for (int currentLine = 1; currentLine < line && *lineStart; currentLine++) {
		lineStart = strchr(lineStart, '\n');
		if (!lineStart)
			return;
		lineStart++;
	}

	const char *lineEnd = strchr(lineStart, '\n');
	if (!lineEnd)
		lineEnd = lineStart + strlen(lineStart);

	const int lineNumWidth = snprintf(NULL, 0, "%d", line);

	fprintf(stderr, "%*d | ", lineNumWidth, line);
	fprintf(stderr, "%.*s\n", (int)(lineEnd - lineStart), lineStart);

	fprintf(stderr, "%*s | ", lineNumWidth, "");

	int relativeStartCol = 0;
	const int maxCol = (int)(lineEnd - lineStart);
	startCol = startCol < maxCol ? startCol : maxCol - 1;
	if (startCol < 0)
		startCol = 0;

	for (int i = 0; i < startCol; i++) {
		const char c = *(lineStart + i);
		if (c == '\t') {
			relativeStartCol += 8 - relativeStartCol % 8;
		} else {
			relativeStartCol++;
		}
	}

	for (int i = 0; i < relativeStartCol; i++) {
		fprintf(stderr, " ");
	}

	fprintf(stderr, "%s^", RED);
	for (int i = 1; i < length && startCol + i < maxCol; i++) {
		fprintf(stderr, "~");
	}
	fprintf(stderr, "%s\n", RESET);
}

// Internal helper — formats the message via va_list and prints the full
// compiler error block, pointing at `token`.  All public compiler_panic*
// variants funnel through here.
static void error_at_vfmt(Parser *parser, const Token *token, ErrorType error_type, const char *format, va_list args)
{
	if (parser->panic_mode)
		return;

	parser->panic_mode = true;
	parser->had_error = true;

	const ErrorDetails details = getErrorDetails(error_type);

	fprintf(stderr, "%s%s%s\n", RED, repeat('=', 60), RESET);

	// "ErrorName: <message> at line N"
	fprintf(stderr, "%s%s: %s", RED, details.name, MAGENTA);
	vfprintf(stderr, format, args);
	fprintf(stderr, " at line %d%s\n", token->line, RESET);

	if (token->type != CRUX_TOKEN_EOF && parser->source != NULL) {
		fprintf(stderr, "\n");

		// Compute the token's column by walking from the start of its
		// line.  We trust token->line (set by the scanner) and
		// token->start (pointer into source).
		int startCol = 0;
		if (token->start >= parser->source) {
			const char *lineStart = parser->source;
			for (int i = 0; i < token->line - 1; i++) {
				const char *newline = strchr(lineStart, '\n');
				if (!newline)
					break;
				lineStart = newline + 1;
			}
			startCol = (int)(token->start - lineStart);
			if (startCol < 0)
				startCol = 0;
		}

		print_error_line(token->line, parser->source, startCol, token->length > 0 ? token->length : 1);
	}

	fprintf(stderr, "%s%s%s\n\n", RED, repeat('=', 60), RESET);
}

void error_at(Parser *parser, const Token *token, const char *message, const ErrorType error_type)
{
	if (parser->panic_mode)
		return;

	parser->panic_mode = true;
	parser->had_error = true;

	const ErrorDetails details = getErrorDetails(error_type);

	fprintf(stderr, "%s%s%s\n", RED, repeat('=', 60), RESET);
	fprintf(stderr, "%s%s: %s%s at line %d%s\n", RED, details.name, MAGENTA, message, token->line, RESET);

	if (token->type != CRUX_TOKEN_EOF && parser->source != NULL) {
		fprintf(stderr, "\n");

		int startCol = 0;
		if (token->start >= parser->source) {
			const char *lineStart = parser->source;
			for (int i = 0; i < token->line - 1; i++) {
				const char *newline = strchr(lineStart, '\n');
				if (!newline)
					break;
				lineStart = newline + 1;
			}
			startCol = (int)(token->start - lineStart);
			if (startCol < 0)
				startCol = 0;
		}

		print_error_line(token->line, parser->source, startCol, token->length > 0 ? token->length : 1);
	}
	fprintf(stderr, "%s%s%s\n\n", RED, repeat('=', 60), RESET);
}

// ── Public compiler_panic* family ────────────────────────────────────────────

void compiler_panic(Parser *parser, const char *message, const ErrorType error_type)
{
	// Points at parser->previous — the token that was just consumed when
	// the error was detected.
	error_at(parser, &parser->previous, message, error_type);
}

void compiler_panicf(Parser *parser, ErrorType error_type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	error_at_vfmt(parser, &parser->previous, error_type, format, args);
	va_end(args);
}

void compiler_panic_at_current(Parser *parser, const char *message, ErrorType error_type)
{
	// Points at parser->current — the unexpected token that has not yet
	// been consumed.  Used by consume() so the caret lands on the actual
	// offending token rather than the one before it.
	error_at(parser, &parser->current, message, error_type);
}

void compiler_panicf_at_current(Parser *parser, ErrorType error_type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	error_at_vfmt(parser, &parser->current, error_type, format, args);
	va_end(args);
}

void runtime_panic(ObjectModuleRecord *module_record, const ErrorType type, const char *format, ...)
{
	const ErrorDetails details = getErrorDetails(type);

	va_list args;
	va_start(args, format);

	fprintf(stderr, "%s%s%s\n", RED, repeat('=', 60), RESET);
	fprintf(stderr, "\n%s%s: %s", RED, details.name, MAGENTA);
	vfprintf(stderr, format, args);
	fprintf(stderr, "%s\n", RESET);
	va_end(args);

	if (module_record == NULL) {
		return;
	}

	fprintf(stderr, "\n%sStack trace (most recent call last):%s", CYAN, RESET);

	const ObjectModuleRecord *traceModule = module_record;
	int globalFrameNumber = 0;

	while (traceModule != NULL) {
		if (!traceModule->is_main) {
			fprintf(stderr, "\n  %s--- imported from module \"%s\" ---%s", MAGENTA,
					traceModule->enclosing_module->path ? traceModule->enclosing_module->path->chars : "<unknown>",
					RESET);
		}

		for (int i = (int)traceModule->frame_count - 1; i >= 0; i--) {
			const CallFrame *frame = &traceModule->frames[i];
			const ObjectFunction *function = frame->closure->function;
			size_t instruction = 0;

			if (function->chunk.code != NULL && frame->ip >= function->chunk.code) {
				instruction = frame->ip - function->chunk.code - 1;
				if (instruction >= (size_t)function->chunk.count) {
					instruction = function->chunk.count > 0 ? function->chunk.count - 1 : 0;
				}
			} else if (function->chunk.count > 0) {
				instruction = function->chunk.count - 1;
			}

			fprintf(stderr, "\n  %s[frame %d]%s ", CYAN, globalFrameNumber++, RESET);

			int line = 0;
			if (function->chunk.lines != NULL && instruction < (size_t)function->chunk.capacity) {
				line = function->chunk.lines[instruction];
			} else if (function->chunk.lines != NULL && function->chunk.capacity > 0) {
				line = function->chunk.lines[0]; // Fallback
			}
			fprintf(stderr, "line %d in ", line);

			const ObjectString *funcModulePath = NULL;
			if (function->module_record != NULL && function->module_record->path != NULL) {
				funcModulePath = function->module_record->path;
			} else if (traceModule->path != NULL) {
				funcModulePath = traceModule->path;
			}

			if (function->name == NULL || function->name->length == 0) {
				if (funcModulePath != NULL) {
					if (traceModule->is_repl) {
						fprintf(stderr,
								"%sscript from "
								"\"repl\" %s",
								CYAN, RESET);
					} else {
						fprintf(stderr,
								"%sscript from \"%s\" "
								"%s",
								CYAN, funcModulePath->chars, RESET);
					}
				} else {
					fprintf(stderr, "%s<script>%s", CYAN, RESET);
				}
			} else {
				if (funcModulePath != NULL) {
					fprintf(stderr, "%s%s() from \"%s\"%s", CYAN, function->name->chars, funcModulePath->chars, RESET);
				} else {
					fprintf(stderr, "%s%s()%s", CYAN, function->name->chars, RESET);
				}
			}
		}
		traceModule = traceModule->enclosing_module;
	}
	fprintf(stderr, "\n%s%s%s\n\n", RED, repeat('=', 60), RESET);

	module_record->owner->is_exiting = true;
	reset_stack(module_record);
}

char *repeat(const char c, const int count)
{
	static char buffer[256];
	int i;
	for (i = 0; i < count && i < (int)sizeof(buffer) - 1; i++) {
		buffer[i] = c;
	}
	buffer[i] = '\0';
	return buffer;
}

/**
 * Creates a formatted error message for type mismatches with actual type
 * information.
 */
char *type_error_message(VM *vm, const Value value, const char *expected_type)
{
	static char buffer[1024];

	const Value typeValue = typeof_value(vm, value);
	char *actualType = AS_C_STRING(typeValue);

	snprintf(buffer, sizeof(buffer), "Expected type '%s', but got '%s'.", expected_type, actualType);
	return buffer;
}
