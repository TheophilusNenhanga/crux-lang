#ifndef COMPILER_H
#define COMPILER_H

#include "object.h"
#include "scanner.h"

ObjectFunction *compile(VM *vm, char *source);

void mark_compiler_roots(VM *vm);

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
	Token prev_previous;
	bool had_error;
	bool panic_mode;
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

typedef void (*ParseFn)(bool can_assign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	ParseFn postfix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth;
	bool is_captured;
} Local;

typedef struct {
	uint8_t index;
	bool is_local;
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
	int continue_target;
	BreakJump *break_jumps;
	int scope_depth;
} LoopContext;

typedef struct Compiler Compiler;

struct Compiler {
	VM *owner;
	Compiler *enclosing;
	ObjectFunction *function;
	FunctionType type;
	int local_count;
	int scope_depth; // 0 is global scope
	int match_depth;
	int loop_depth;
	LoopContext loop_stack[UINT8_COUNT];
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
};

#endif // COMPILER_H
