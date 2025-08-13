#ifndef COMPILER_H
#define COMPILER_H

#include "object.h"
#include "scanner.h"

ObjectFunction *compile(VM *vm, char *source);

void markCompilerRoots(VM *vm);

/**
 * @brief Parser state used during compilation.
 *
 * Holds the current and previous tokens, error status, and source code being
 * parsed.
 */
typedef struct {
	char *source;
	Token current;
	Token previous;
	Token prevPrevious;
	bool hadError;
	bool panicMode;
} Parser;

// Precedence in order from lowest to highest
typedef enum {
	PREC_NONE, // No precedence
	PREC_ASSIGNMENT, // =
	PREC_OR, // or
	PREC_AND, // and
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_SHIFT, // << >>
	PREC_TERM, // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL, // . () []
	PREC_PRIMARY
} Precedence;

typedef enum {
	COMPOUND_OP_PLUS,
	COMPOUND_OP_MINUS,
	COMPOUND_OP_STAR,
	COMPOUND_OP_SLASH,
	COMPOUND_OP_BACK_SLASH,
	COMPOUND_OP_PERCENT
} CompoundOp;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	ParseFn postfix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth;
	bool isCaptured;
} Local;

typedef struct {
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT, TYPE_ANONYMOUS } FunctionType;

typedef enum { LOOP_FOR, LOOP_WHILE } LoopType;

typedef struct BreakJump BreakJump;

struct BreakJump {
	int jumpOffset;
	struct BreakJump *next;
};

typedef struct {
	LoopType type;
	int continueTarget;
	BreakJump *breakJumps;
	int scopeDepth;
} LoopContext;

typedef struct Compiler Compiler;

struct Compiler {
	VM *owner;
	Compiler *enclosing;
	ObjectFunction *function;
	FunctionType type;
	int localCount;
	int scopeDepth; // 0 is global scope
	int matchDepth;
	int loopDepth;
	LoopContext loopStack[UINT8_COUNT];
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
};

#endif // COMPILER_H
