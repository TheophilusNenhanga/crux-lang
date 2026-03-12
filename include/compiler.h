#ifndef COMPILER_H
#define COMPILER_H

#include "object.h"
#include "scanner.h"
#include "type_system.h"

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
	bool had_error;
	bool panic_mode;
	Scanner *scanner;
} Parser;

// Precedence in order from lowest to highest
typedef enum {
	PREC_NONE, // No precedence
	PREC_ASSIGNMENT, // =
	PREC_OR, // or
	PREC_AND, // and
	PREC_BITWISE_OR, // |
	PREC_BITWISE_XOR, // ^
	PREC_BITWISE_AND, // &
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

typedef struct {
	Token name;
	int depth;
	bool is_captured;
	ObjectTypeRecord *type;
} Local;

typedef struct {
	uint8_t index;
	bool is_local;
	ObjectTypeRecord *type;
} Upvalue;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT, TYPE_METHOD, TYPE_ANONYMOUS } FunctionType;

typedef enum { LOOP_FOR, LOOP_WHILE } LoopType;

typedef struct BreakJump BreakJump;

struct BreakJump {
	int jumpOffset;
	BreakJump *next;
};

typedef struct {
	LoopType type;
	int continue_target;
	BreakJump *break_jumps;
	int scope_depth;
} LoopContext;

struct Compiler {
	VM *owner;
	Compiler *enclosing;
	Compiler* enclosed;
	ObjectFunction *function;
	FunctionType type;
	ObjectTypeRecord *return_type;
	int local_count;
	int scope_depth; // 0 is global scope
	int match_depth;
	int loop_depth;
	LoopContext loop_stack[UINT8_COUNT];
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
	ObjectTypeRecord *type_stack[UINT8_COUNT];
	int type_stack_count;
	ObjectTypeTable *type_table;
	ObjectTypeRecord *last_give_type;
	Parser* parser;
	bool has_return;
};
typedef void (*ParseFn)(Compiler *compiler, bool can_assign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	ParseFn postfix;
	Precedence precedence;
} ParseRule;


void mark_compiler_roots(VM *vm, Compiler *compiler);
ObjectFunction *compile(VM *vm, Compiler* compiler, Compiler* enclosing, char *source);

ObjectTypeRecord *parse_type_record(Compiler* compiler);
void push_type_record(Compiler* compiler, ObjectTypeRecord *type_record);
ObjectTypeRecord *pop_type_record(Compiler* compiler);
ObjectTypeRecord *peek_type_record(Compiler* compiler);

#endif // COMPILER_H
