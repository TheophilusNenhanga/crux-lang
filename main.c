#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "file_handler.h"
#include "vm.h"

static void repl() {
	while (true) {
		char line[1024];
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		interpret(line, NULL);
	}
}

static void runFile(const char *path) {
	FileResult fileResult = readFile(path);
	if (fileResult.error) {
		fprintf(stderr, "Error reading file: %s\n", fileResult.error);
		exit(2);
	}

	const InterpretResult interpretResult = interpret(fileResult.content, path);
	free(fileResult.content);

	if (interpretResult == INTERPRET_COMPILE_ERROR)
		exit(65);
	if (interpretResult == INTERPRET_RUNTIME_ERROR)
		exit(70);
}

int main(const int argc, const char *argv[]) {
	initVM();

	if (argc == 1) {
		repl();
	} else if (argc == 2) {
		runFile(argv[1]);
	} else {
		fprintf(stderr, "Usage: stella [path]\n");
		exit(64);
	}

	freeVM();
	return 0;
}
