#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "file_handler.h"
#ifndef _WIN32
#include "linenoise.h"
#endif
#include "vm.h"

static int repl(VM *vm)
{
	char *cruxDir = get_crux_dir();
	char *historyPath = NULL;
	if (cruxDir) {
		historyPath = combine_paths(cruxDir, "repl_history");
		free(cruxDir);
	}

	const char *finalPath = historyPath ? historyPath : ".crux_history";

#ifndef _WIN32
	linenoiseHistoryLoad(finalPath);
	char *line;

	while ((line = linenoise(CYAN "> " RESET)) != NULL) {
		if (line[0] != '\0') {
			linenoiseHistoryAdd(line);
			linenoiseHistorySave(finalPath);
			InterpretResult res = interpret(vm, line);
			if (res == INTERPRET_EXIT) {
				linenoiseFree(line);
				break;
			}
		}
		linenoiseFree(line);
	}
#else
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
#endif

	if (historyPath)
		free(historyPath);

	return vm->exit_code;
}

/**
 * Reads the content of the specified file, interprets it,
 * and exits with an appropriate status code if errors occur:
 * - Exit code 2: File reading error
 * - Exit code 65: Compilation error
 * - Exit code 70: Runtime error
 */
static int runFile(VM *vm, const char *path)
{
	const FileResult fileResult = read_file(path);
	if (fileResult.error) {
		fprintf(stderr, "Error reading file: %s\n", fileResult.error);
		return 2;
	}
	const InterpretResult interpretResult = interpret(vm, fileResult.content);
	free(fileResult.content);

	if (interpretResult == INTERPRET_COMPILE_ERROR)
		return COMPILER_EXIT_CODE;
	if (interpretResult == INTERPRET_RUNTIME_ERROR)
		return RUNTIME_EXIT_CODE;
	if (interpretResult == INTERPRET_EXIT)
		return vm->exit_code;

	return 0;
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
	int exit_code = 0;

	if (argc == 1) {
		exit_code = repl(vm);
	} else if (argc == 2) {
		exit_code = runFile(vm, argv[1]);
	} else {
#ifdef _WIN32
		fprintf(stderr, "Usage: & .\\[crux.exe] [path]\n");
#else
		fprintf(stderr, "Usage: ./[crux] [path]\n");
#endif
		exit_code = 64;
	}

	free_vm(vm);
	return exit_code;
}
