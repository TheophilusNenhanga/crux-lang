#include "panic.h"

#include <stdio.h>


void runtimePanic() {

}

void errorAt(Parser *parser, Token *token, const char *message) {
	if (parser->panicMode) {
		return;
	}
	parser->panicMode = true;
	fprintf(stderr, "-------COMPILER ERROR-------\n");
	fprintf(stderr, "[line %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}
	fprintf(stderr, ": %s\n", message);
	parser->hadError = true;
}

void errorAtCurrent(Parser* parser, const char *message) {
	errorAt(parser, &parser->current, message);
}

void compilerPanic(Parser* parser, const char *message) {
	errorAt(parser, &parser->previous, message);
}