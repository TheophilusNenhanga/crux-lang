#include <stdio.h>
#include <stdlib.h>

#include "file_handler.h"
#include "vm/vm.h"
#include "common.h"

/**
 * Starts an interactive Read-Eval-Print Loop (REPL) for the language
 */
static void repl(VM *vm) {
  while (true) {
    char line[1024];
          printf(CYAN "> " RESET);
          printf(GREEN);
          fflush(stdout);
          if (!fgets(line, sizeof(line), stdin)) {
                  printf(RESET "\n");
                  break;
          }
          printf(RESET);
    interpret(vm, line);
  }
}

/**
 * Reads the content of the specified file, interprets it,
 * and exits with an appropriate status code if errors occur:
 * - Exit code 2: File reading error
 * - Exit code 65: Compilation error
 * - Exit code 70: Runtime error
 */
static void runFile(VM *vm, const char *path) {
  const FileResult fileResult = readFile(path);
  if (fileResult.error) {
    fprintf(stderr, "Error reading file: %s\n", fileResult.error);
    exit(2);
  }
  const InterpretResult interpretResult = interpret(vm, fileResult.content);
  free(fileResult.content);

  if (interpretResult == INTERPRET_COMPILE_ERROR)
    exit(COMPILER_EXIT_CODE);
  if (interpretResult == INTERPRET_RUNTIME_ERROR)
    exit(RUNTIME_EXIT_CODE);
}

/**
 * Initializes the virtual machine and either:
 * - Starts a REPL session if no arguments are provided
 * - Executes a source file if one argument (file path) is provided
 * - Displays usage information otherwise
 *
 */
int main(const int argc, const char *argv[]) {

  VM *vm = newVM(argc, argv);

  if (argc == 1) {
    repl(vm);
  } else if (argc == 2) {
    runFile(vm, argv[1]);
  } else {
#ifdef _WIN32
    fprintf(stderr, "Usage: & .\\[crux.exe] [path]\n");
#else
    fprintf(stderr, "Usage: ./[crux] [path]\n");
#endif
    exit(64);
  }

  freeVM(vm);
  return 0;
}
