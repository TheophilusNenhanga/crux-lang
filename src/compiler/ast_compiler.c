#include "compiler/ast_compiler.h"

#include <stdlib.h>

void init_ast_scanner(ASTScanner* scanner, const char* source)
{
	if (scanner == NULL) {
		return;
	}
	scanner->start = source;
	scanner->current = source;
	scanner->line = 1;
}

ObjectFunction * ast_compile(VM* vm, const char* source)
{
	ASTScanner *scanner = malloc(sizeof(ASTScanner));
	if (scanner == NULL) return NULL;
	init_ast_scanner(scanner, source);


	ASTCompiler* compiler = malloc(sizeof(ASTCompiler));
	if (compiler == NULL) {
		free(scanner);
		return NULL;
	}



	free(compiler);
	free(scanner);
}

