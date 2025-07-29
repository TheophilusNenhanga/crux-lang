#include "panic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "std/core.h"
#include "vm/vm.h"
#include "vm/vm_helpers.h"

static ErrorDetails getErrorDetails(const ErrorType type) {
  switch (type) {
  case SYNTAX:
    return (ErrorDetails){
        "Syntax Error",
        "Check for missing delimiters or incorrect syntax",
    };
  case MATH:
    return (ErrorDetails){
        "Math Error",
        "Divide by a non-zero number",
    };
  case BOUNDS:
    return (ErrorDetails){
        "Bounds Error",
        "Use an index that is greater than or equal to zero, but less than the "
        "size of the container by one.",
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
    return (ErrorDetails){"Name Error", "Double check that this name exists."};
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
                          "Too many frames on the stack. There may be an "
                          "unterminated recursive call."};
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
        "The assert statement failed."};
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

void printErrorLine(const int line, const char *source, int startCol,
                    const int length) {
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

void errorAt(Parser *parser, const Token *token, const char *message,
             const ErrorType errorType) {
  if (parser->panicMode) {
    return;
  }
  parser->panicMode = true;
  fprintf(stderr, "%s%s%s\n", RED, repeat('=', 60), RESET);
  const ErrorDetails details = getErrorDetails(errorType);
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
  fprintf(stderr, "%s%s%s\n\n", RED, repeat('=', 60), RESET);
  parser->hadError = true;
}

void compilerPanic(Parser *parser, const char *message,
                   const ErrorType errorType) {
  errorAt(parser, &parser->previous, message, errorType);
}

void runtimePanic(ObjectModuleRecord *moduleRecord, bool shouldExit,
                  const ErrorType type, const char *format, ...) {
  const ErrorDetails details = getErrorDetails(type);

  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s%s%s\n", RED, repeat('=', 60), RESET);
  fprintf(stderr, "\n%s%s: %s", RED, details.name, MAGENTA);
  vfprintf(stderr, format, args);
  fprintf(stderr, "%s\n", RESET);
  va_end(args);

  fprintf(stderr, "\n%sStack trace (most recent call last):%s", CYAN, RESET);

  const ObjectModuleRecord *traceModule = moduleRecord;
  int globalFrameNumber = 0;

  while (traceModule != NULL) {
    if (!traceModule->isMain) {
      fprintf(stderr, "\n  %s--- imported from module \"%s\" ---%s", MAGENTA,
              traceModule->enclosingModule->path
                  ? traceModule->enclosingModule->path->chars
                  : "<unknown>",
              RESET);
    }

    for (int i = (int)traceModule->frameCount - 1; i >= 0; i--) {
      const CallFrame *frame = &traceModule->frames[i];
      const ObjectFunction *function = frame->closure->function;
      size_t instruction = 0;

      if (function->chunk.code != NULL && frame->ip >= function->chunk.code) {
        instruction = frame->ip - function->chunk.code - 1;
        if (instruction >= function->chunk.count) {
          instruction =
              function->chunk.count > 0 ? function->chunk.count - 1 : 0;
        }
      } else if (function->chunk.count > 0) {
        instruction = function->chunk.count - 1;
      }

      fprintf(stderr, "\n  %s[frame %d]%s ", CYAN, globalFrameNumber++, RESET);

      int line = 0;
      if (function->chunk.lines != NULL &&
          instruction < function->chunk.capacity) {
        line = function->chunk.lines[instruction];
      } else if (function->chunk.lines != NULL &&
                 function->chunk.capacity > 0) {
        line = function->chunk.lines[0]; // Fallback
      }
      fprintf(stderr, "line %d in ", line);

      const ObjectString *funcModulePath = NULL;
      if (function->moduleRecord != NULL &&
          function->moduleRecord->path != NULL) {
        funcModulePath = function->moduleRecord->path;
      } else if (traceModule->path != NULL) {
        funcModulePath = traceModule->path;
      }

      if (function->name == NULL || function->name->length == 0) {
        if (funcModulePath != NULL) {
          if (traceModule->isRepl) {
            fprintf(stderr, "%sscript from \"repl\" %s", CYAN, RESET);
          } else {
            fprintf(stderr, "%sscript from \"%s\" %s", CYAN,
                    funcModulePath->chars, RESET);
          }
        } else {
          fprintf(stderr, "%s<script>%s", CYAN, RESET);
        }
      } else {
        if (funcModulePath != NULL) {
          fprintf(stderr, "%s%s() from \"%s\"%s", CYAN, function->name->chars,
                  funcModulePath->chars, RESET);
        } else {
          fprintf(stderr, "%s%s()%s", CYAN, function->name->chars, RESET);
        }
      }
    }
    traceModule = traceModule->enclosingModule;
  }

  fprintf(stderr, "\n\n%sSuggestion: %s%s\n", CYAN, MAGENTA, details.hint);
  fprintf(stderr, "%s%s%s\n\n", RED, repeat('=', 60), RESET);

  if (moduleRecord != NULL) {
    resetStack(moduleRecord);
  }

  if (shouldExit) {
    exit(RUNTIME_EXIT_CODE);
  }
}

char *repeat(const char c, const int count) {
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
char *typeErrorMessage(VM *vm, const Value value, const char *expectedType) {
  static char buffer[1024];

  const Value typeValue = typeofValue(vm, value);
  char *actualType = AS_C_STRING(typeValue);

  snprintf(buffer, sizeof(buffer), "Expected type '%s', but got '%s'.",
           expectedType, actualType);
  return buffer;
}
