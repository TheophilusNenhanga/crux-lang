#include <stdio.h>
#include <stdlib.h>

#include "file_handler.h"
#include "object.h"
#include "vm.h"

/**
* @brief Starts an interactive Read-Eval-Print Loop (REPL) for the language
*
* Continuously prompts the user for input, reads a line of code,
* interprets it, and displays the result until EOF is reached
* (e.g., when the user presses Ctrl+D).
*
* @param vm Pointer to the virtual machine instance
*/
static void repl(VM *vm) {
	while (true) {
		char line[1024];
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		interpret(vm, line);
	}
}

/**
* @brief Executes code from a file
*
* Reads the content of the specified file, interprets it,
* and exits with an appropriate status code if errors occur:
* - Exit code 2: File reading error
* - Exit code 65: Compilation error
* - Exit code 70: Runtime error
*
* @param vm Pointer to the virtual machine instance
* @param path Path to the source file to execute
*/
static void runFile(VM *vm, const char *path) {
	FileResult fileResult = readFile(path);
	if (fileResult.error) {
		fprintf(stderr, "Error reading file: %s\n", fileResult.error);
		exit(2);
	}
	const InterpretResult interpretResult = interpret(vm, fileResult.content);
	free(fileResult.content);

	if (interpretResult == INTERPRET_COMPILE_ERROR)
		exit(65);
	if (interpretResult == INTERPRET_RUNTIME_ERROR)
		exit(70);
}

/**
* @brief Program entry point
*
* Initializes the virtual machine and either:
* - Starts a REPL session if no arguments are provided
* - Executes a source file if one argument (file path) is provided
* - Displays usage information otherwise
*
* @param argc Number of command-line arguments
* @param argv Array of command-line argument strings
* @return Program exit code (0 for success)
*/
int main(const int argc, const char *argv[]) {

	VM *vm = newVM();
	
	if (argc == 1) {
		vm->module = newModule(vm, "./");
		repl(vm);
	} else if (argc == 2) {
		vm->module = newModule(vm, argv[1]);
		runFile(vm, argv[1]);
	} else {
		fprintf(stderr, "Usage: stella [path]\n");
		exit(64);
	}

	freeVM(vm);
	return 0;
}
