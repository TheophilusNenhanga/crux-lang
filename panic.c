#include "panic.h"

#include <stdarg.h>
#include <stdio.h>

#include "vm.h"

static ErrorDetails getErrorDetails(ErrorType type) {
	switch (type) {
		case SYNTAX:
			return (ErrorDetails) {
					"Syntax Error",
					"Check for missing delimiters or incorrect syntax",
			};
		case DIVISION_BY_ZERO:
			return (ErrorDetails) {
					"Zero Division Error",
					"Divide by a non-zero number",
			};
		case INDEX_OUT_OF_BOUNDS:
			return (ErrorDetails) {
					"Index Error",
					"Array index must be within the array's size",
			};
		case LOOP_EXTENT: {
			return (ErrorDetails) {
					"Loop Extent Error",
					"Loop body cannot exceed 65535 statements",
			};
		}
		case TYPE:
			return (ErrorDetails) {
					"Type Error",
					"Operation not supported for these types",
			};
		case LIMIT: {
			return (ErrorDetails) {"Stella Limit Error", "The program cannot handle this many constants"};
		}
		case NAME: {
			return (ErrorDetails) {"Name Error", "The name you invoked caused an error"};
		}
		case CLOSURE_EXTENT: {
			return (ErrorDetails) {"Closure Extent Error", "Functions cannot close over 255 variables."};
		}
		case LOCAL_EXTENT: {
			return (ErrorDetails) {"Local Variable Extent Error", "Functions cannot have more than 255 local ."};
		}
		case ARGUMENT_EXTENT: {
			return (ErrorDetails) {"Argument Extent Error", "Functions cannot have more than 255 arguments."};
		}
		case COLLECTION_EXTENT: {
			return (ErrorDetails) {"Collection Extent Error",
														 "Collections cannot have more than 65535 elements in their definition."};
		}
		case VARIABLE_EXTENT: {
			return (ErrorDetails) {"Variable Extent Error", "Cannot declare more than 255 variables at a time."};
		}
		case VARIABLE_DECLARATION_MISMATCH: {
			return (ErrorDetails) {"Mismatch Error"
														 "The number of variable names and expressions must be equal."};
		}
		case RETURN_EXTENT: {
			return (ErrorDetails) {"Return Extent Error", "Cannot return more than 255 values at a time."};
		}
		case ARGUMENT_MISMATCH: {
			return (ErrorDetails) {
					"Mismatch Error",
					"The number of arguments in the call must match the number of arguments in the declaration."};
		}
		case STACK_OVERFLOW: {
			return (ErrorDetails) {"Stack Overflow Error",
														 "Too many stacks created. There may be a unterminated recursive call."};
		}
		case COLLECTION_GET: {
			return (ErrorDetails) {"Collection Get Error", ""};
		}
		case COLLECTION_SET: {
			return (ErrorDetails) {"Collection Set Error", "Try adding a different value to the collection"};
		}
		case UNPACK_MISMATCH: {
			return (ErrorDetails) {"Unpack Mismatch Error", "Ensure that you assign all unpacked values"};
		}
		case MEMORY: {
			return (ErrorDetails) {"Memory Error", "Cannot allocate more memory."};
		}
		case ASSERT: {
			return (ErrorDetails){"Assert Error", "The state of your program does not match your expectations"};
		}
		case RUNTIME:
		default:
			return (ErrorDetails) {
					"Runtime Error",
					"An error occurred during program execution",
			};
	}
}

void printErrorLine(int lineNumber, const char *source, int startCol, int length) {
	const char *lineStart = source;
	for (int currentLine = 1; currentLine < lineNumber && *lineStart; currentLine++) {
		lineStart = strchr(lineStart, '\n');
		if (!lineStart)
			return;
		lineStart++;
	}


	const char *lineEnd = strchr(lineStart, '\n');
	if (!lineEnd)
		lineEnd = lineStart + strlen(lineStart);

	// Calculate padding for line numbers (handles up to 9999 lines)
	int lineNumWidth = snprintf(NULL, 0, "%d", lineNumber);

	fprintf(stderr, "%*d | ", lineNumWidth, lineNumber);
	fprintf(stderr, "%.*s\n", (int) (lineEnd - lineStart), lineStart);

	fprintf(stderr, "%*s | ", lineNumWidth, "");


	for (int i = 0; i < startCol; i++) {
		char c = lineStart[i];
		fprintf(stderr, "%c", (c == '\t') ? '\t' : ' ');
	}

	fprintf(stderr, RED "^");
	for (int i = 1; i < length; i++) {
		printf("~");
	}
	fprintf(stderr, RESET "\n");
}

void errorAt(Parser *parser, Token *token, const char *message, ErrorType errorType) {
	if (parser->panicMode) {
		return;
	}
	parser->panicMode = true;

	ErrorDetails details = getErrorDetails(errorType);
	fprintf(stderr, "%s%s: %s%s at line %d%s\n", RED, details.name, MAGENTA, message, token->line, RESET);

	if (token->type != TOKEN_EOF) {
		fprintf(stderr, "\n");
		printErrorLine(token->line, parser->source, token->start - parser->source, token->length);
	}

	fprintf(stderr, "\n%s%s%s\n", MAGENTA, details.hint, RESET);
	parser->hadError = true;
}

void compilerPanic(Parser *parser, const char *message, ErrorType errorType) {
	errorAt(parser, &parser->previous, message, errorType);
}

void runtimePanic(ErrorType type, const char *format, ...) {
	ErrorDetails details = getErrorDetails(type);

	va_list args;
	va_start(args, format);

	fprintf(stderr, "\n%s%s: %s", RED, details.name, MAGENTA);
	vfprintf(stderr, format, args);
	fprintf(stderr, "%s", RESET);

	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame *frame = &vm.frames[i];
		ObjectFunction *function = frame->closure->function;
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "\n[line %d] in ", function->chunk.lines[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script");
		} else {
			printf("%s()", function->name->chars);
		}
	}

	fprintf(stderr, "\n%s%s%s\n", MAGENTA, details.hint, RESET);
	va_end(args);

	resetStack();
}
