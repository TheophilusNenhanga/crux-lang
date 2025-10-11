#ifndef AST_COMPILER_H
#define AST_COMPILER_H

#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "stdbool.h"

typedef struct
{
	char* source;
	Token current;
	Token previous;
	Token prev_previous;
	bool had_error;
	bool panic_mode;
} ASTParser;

typedef struct ASTCompiler ASTCompiler;

struct ASTCompiler{
	VM* owner;
	ASTCompiler* enclosing;
	ObjectFunction* function;
	FunctionType function_type;
	int local_count;
	int scope_depth;
	int match_depth;
	int loop_depth;
	LoopContext loop_context[UINT8_COUNT];
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
};

typedef struct {
	const char *start;
	const char *current;
	int line;
} ASTScanner;

#endif // AST_COMPILER_H
