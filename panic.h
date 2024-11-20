#ifndef PANIC_H
#define PANIC_H

#include "compiler.h"
#include "scanner.h"

void runtimePanic();


void compilerPanic(Parser* parser, const char *message, ErrorType type);
void errorAt(Parser *parser, Token *token, const char *message);
void errorAtCurrent(Parser* parser, const char *message);

#endif // PANIC_H
