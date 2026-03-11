#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "file_handler.h"
#include "linenoise.h"
#include "vm.h"

static void repl(VM *vm)
{
	char *cruxDir = get_crux_dir();
	char *historyPath = NULL;
	if (cruxDir) {
		historyPath = combine_paths(cruxDir, "history");
		free(cruxDir);
	}

	const char *finalPath = historyPath ? historyPath : ".crux_history";

	linenoiseHistoryLoad(finalPath);
	char *line;

	while ((line = linenoise(CYAN "> " RESET)) != NULL) {
		if (line[0] != '\0') {
			linenoiseHistoryAdd(line);
			linenoiseHistorySave(finalPath);
			interpret(vm, line);
		}
		linenoiseFree(line);
	}

	if (historyPath)
		free(historyPath);
}

/**
 * Reads the content of the specified file, interprets it,
 * and exits with an appropriate status code if errors occur:
 * - Exit code 2: File reading error
 * - Exit code 65: Compilation error
 * - Exit code 70: Runtime error
 */
static void runFile(VM *vm, const char *path)
{
	const FileResult fileResult = read_file(path);
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
int main(const int argc, const char *argv[])
{
	VM *vm = new_vm(argc, argv);

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

	free_vm(vm);
	return 0;
}
