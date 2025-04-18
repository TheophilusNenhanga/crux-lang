#include "panic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "std/core.h"
#include "vm.h"

static ErrorDetails getErrorDetails(ErrorType type) {
  switch (type) {
  case SYNTAX:
    return (ErrorDetails){
        "Syntax Error",
        "Check for missing delimiters or incorrect syntax",
    };
  case DIVISION_BY_ZERO:
    return (ErrorDetails){
        "Zero Division Error",
        "Divide by a non-zero number",
    };
  case INDEX_OUT_OF_BOUNDS:
    return (ErrorDetails){
        "Index Error",
        "Array index must be within the array's size",
    };
  case LOOP_EXTENT: {
    return (ErrorDetails){
        "Loop Extent Error",
        "Loop body cannot exceed 65535 statements",
    };
  }
  case TYPE:
    return (ErrorDetails){
        "Type Error",
        "Check that all values in the operation are of the expected types",
    };
  case LIMIT: {
    return (ErrorDetails){"Limit Error",
                          "The program cannot handle this many constants"};
  }
  case NAME: {
    return (ErrorDetails){"Name Error", "The name you invoked caused an error. "
                                        "Double check that this name exists."};
  }
  case CLOSURE_EXTENT: {
    return (ErrorDetails){"Closure Extent Error",
                          "Functions cannot close over 255 variables."};
  }
  case LOCAL_EXTENT: {
    return (ErrorDetails){
        "Local Variable Extent Error",
        "Functions cannot have more than 255 local variables."};
  }
  case ARGUMENT_EXTENT: {
    return (ErrorDetails){"Argument Extent Error",
                          "Functions cannot have more than 255 arguments."};
  }
  case COLLECTION_EXTENT: {
    return (ErrorDetails){"Collection Extent Error",
                          "Collections cannot have more than 65535 elements in "
                          "their definition."};
  }
  case VARIABLE_EXTENT: {
    return (ErrorDetails){"Variable Extent Error",
                          "Cannot declare more than 255 variables at a time."};
  }
  case VARIABLE_DECLARATION_MISMATCH: {
    return (ErrorDetails){
        "Mismatch Error"
        "The number of variable names and expressions must be equal."};
  }
  case RETURN_EXTENT: {
    return (ErrorDetails){"Return Extent Error",
                          "Cannot return more than 255 values at a time."};
  }
  case ARGUMENT_MISMATCH: {
    return (ErrorDetails){"Argument Mismatch Error",
                          "The number of arguments in the call must match the "
                          "function's declared parameters."};
  }
  case STACK_OVERFLOW: {
    return (ErrorDetails){"Stack Overflow Error",
                          "Too many frames on the stack. There may be "
                          "anunterminated recursive call."};
  }
  case COLLECTION_GET: {
    return (ErrorDetails){"Collection Get Error",
                          "Could not retrieve value from collection."};
  }
  case COLLECTION_SET: {
    return (ErrorDetails){
        "Collection Set Error",
        "Could not set value in collection. Check the key type and value."};
  }
  case UNPACK_MISMATCH: {
    return (ErrorDetails){"Unpack Mismatch Error",
                          "Ensure that you assign all unpacked values."};
  }
  case MEMORY: {
    return (ErrorDetails){"Memory Error", "Cannot allocate more memory."};
  }
  case ASSERT: {
    return (ErrorDetails){
        "Assert Error",
        "The assert statement failed. Check your program's logic."};
  }
  case IMPORT_EXTENT: {
    return (ErrorDetails){"Import Extent Error",
                          "Cannot import any more names."};
  }
  case IO: {
    return (ErrorDetails){"IO Error", "An error occurred while reading from or "
                                      "writing to a file. Check if the file "
                                      "exists at the specified location."};
  }
  case IMPORT: {
    return (ErrorDetails){"Import Error",
                          "An error occurred while importing a module. Check "
                          "the module path and name."};
  }
  case RUNTIME:
  default:
    return (ErrorDetails){
        "Runtime Error",
        "An error occurred during program execution",
    };
  }
}

void printErrorLine(int line, const char *source, int startCol, int length) {
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

  int lineNumWidth = snprintf(NULL, 0, "%d", line);

  fprintf(stderr, "%*d | ", lineNumWidth, line);
  fprintf(stderr, "%.*s\n", (int)(lineEnd - lineStart), lineStart);

  fprintf(stderr, "%*s | ", lineNumWidth, "");

  int relativeStartCol = 0;
  int maxCol = (int)(lineEnd - lineStart);
  startCol = startCol < maxCol ? startCol : maxCol - 1;
  if (startCol < 0)
    startCol = 0;

  for (int i = 0; i < startCol; i++) {
    char c = *(lineStart + i);
    if (c == '\t') {
      relativeStartCol += 8 - (relativeStartCol % 8);
    } else {
      relativeStartCol++;
    }
  }

  for (int i = 0; i < relativeStartCol; i++) {
    fprintf(stderr, " ");
  }

  fprintf(stderr, "%s^", RED);
  for (int i = 1; i < length && (startCol + i) < maxCol; i++) {
    fprintf(stderr, "~");
  }
  fprintf(stderr, "%s\n", RESET);
}

void errorAt(Parser *parser, Token *token, const char *message,
             ErrorType errorType) {
  if (parser->panicMode) {
    return;
  }
  parser->panicMode = true;

  ErrorDetails details = getErrorDetails(errorType);
  fprintf(stderr, "%s%s: %s%s at line %d%s\n", RED, details.name, MAGENTA,
          message, token->line, RESET);

  if (token->type != TOKEN_EOF) {
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
    }

    printErrorLine(token->line, parser->source, startCol, token->length);
  }

  fprintf(stderr, "\n%s%s%s\n", MAGENTA, details.hint, RESET);
  parser->hadError = true;
}

void compilerPanic(Parser *parser, const char *message, ErrorType errorType) {
  errorAt(parser, &parser->previous, message, errorType);
}

void runtimePanic(VM *vm, ErrorType type, const char *format, ...) {
  ErrorDetails details = getErrorDetails(type);

  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s%s%s\n", RED, repeat('=', 50), RESET);
  fprintf(stderr, "\n%s%s: %s", RED, details.name, MAGENTA);
  vfprintf(stderr, format, args);
  fprintf(stderr, "%s", RESET);
  va_end(args);

  // Print stack trace
  fprintf(stderr, "\n\n%sStack trace:%s", CYAN, RESET);
  for (int i = vm->frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm->frames[i];
    ObjectFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;

    // Show more detailed frame information
    fprintf(stderr, "\n%s[frame %d]%s ", CYAN, vm->frameCount - i, RESET);
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

    if (function->name == NULL) {
      fprintf(stderr, "%s<script>%s", CYAN, RESET);
    } else {
      fprintf(stderr, "%s%s()%s", CYAN, function->name->chars, RESET);
    }
  }

  fprintf(stderr, "\n\n%sSuggestion: %s%s\n", CYAN, MAGENTA, details.hint);

  fprintf(stderr, "\n%s%s%s\n", RED, repeat('=', 50), RESET);

  resetStack(vm);
}

char *repeat(char c, int count) {
  static char buffer[256];
  int i;
  for (i = 0; i < count && i < sizeof(buffer) - 1; i++) {
    buffer[i] = c;
  }
  buffer[i] = '\0';
  return buffer;
}

/**
 * Creates a formatted error message for type mismatches with actual type
 * information.
 */
char *typeErrorMessage(VM *vm, Value value, const char *expectedType) {
  static char buffer[256];

  Value typeValue = typeFunction_(vm, 1, &value);
  char* actualType = AS_C_STRING(typeValue);

  snprintf(buffer, sizeof(buffer),
           "Expected type '%s', but got '%s'.", expectedType, actualType);
  return buffer;
}
