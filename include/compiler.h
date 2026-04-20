#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include "garbage_collector.h"
#include "object.h"
#include "scanner.h"
#include "type_system.h"
#include "common.h"

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
	jmp_buf jump_buffer;
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
	PREC_IN, // in
	PREC_SHIFT, // << >>
	PREC_TERM, // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL, // . () []
	PREC_COERCE, // as
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

typedef struct {
	int tracked_local_index;
	ObjectString *tracked_global_name;
	bool tracked_is_typeof;
	ObjectTypeRecord *tracked_literal_type;

	int local_index;
	ObjectString *global_name;
	ObjectTypeRecord *narrowed_to;
	ObjectTypeRecord *stripped_down;
} NarrowingInfo;

typedef enum {
    MATCH_PATTERN_DEFAULT,
    MATCH_PATTERN_OK,
    MATCH_PATTERN_ERR,
    MATCH_PATTERN_SOME,
    MATCH_PATTERN_NONE,
    MATCH_PATTERN_EXPRESSION,
    MATCH_PATTERN_TYPE
} MatchPatternType;

typedef struct {
    MatchPatternType type;
    int jump_if_not_match;
    uint16_t binding_slot;
    ObjectTypeRecord* type_produced;
    bool is_binding;
} MatchPattern;

typedef struct {
    ObjectTypeRecord* target_type;
    ObjectTypeRecord* resultant_type;
    bool has_ok;
    bool has_err;
    bool has_some;
    bool has_none;
    bool has_literal;
    bool has_type;
    bool has_default;
    ObjectTypeRecord** matched_types;
    int matched_types_count;
    int matched_types_capacity;
}MatchExhaustiveness;

typedef struct {
    MatchPattern *patterns;
    int pattern_count;
    int pattern_capacity;
    MatchExhaustiveness exhaustiveness;
    int* end_jumps;
    int jump_count;
    int jump_capacity;
} MatchCompiler;

struct Compiler {
	VM *owner;
	Compiler *enclosing;
	Compiler *enclosed;
	ObjectFunction *function;
	FunctionType type;
	ObjectTypeRecord *return_type;
	int local_count;
	int scope_depth; // 0 is global scope
	int loop_depth; // 0 is no loop
	LoopContext loop_stack[UINT8_COUNT];
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
	ObjectTypeRecord *type_stack[UINT8_COUNT];
	int type_stack_count;
	ObjectTypeTable *type_table;
	ObjectTypeRecord *last_give_type;
	Parser *parser;
	bool has_return;
	NarrowingInfo current_narrowing;
    MatchCompiler match_compiler[MATCH_NEST_DEPTH];
    int match_depth;
};
typedef void (*ParseFn)(Compiler *compiler, const bool can_assign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	ParseFn postfix;
	Precedence precedence;
} ParseRule;

#define T_ANY new_type_rec(compiler->owner, ANY_TYPE)
#define T_BOOL new_type_rec(compiler->owner, BOOL_TYPE)
#define T_INT new_type_rec(compiler->owner, INT_TYPE)
#define T_FLOAT new_type_rec(compiler->owner, FLOAT_TYPE)
#define T_STRING new_type_rec(compiler->owner, STRING_TYPE)
#define T_NIL new_type_rec(compiler->owner, NIL_TYPE)
#define T_ERROR new_type_rec(compiler->owner, ERROR_TYPE)

void mark_compiler_roots(VM *vm, const Compiler *compiler);
ObjectFunction *compile(VM *vm, Compiler *compiler, Compiler *enclosing, char *source);

ObjectTypeRecord *parse_type_record(Compiler *compiler);
void push_type_record(Compiler *compiler, ObjectTypeRecord *type_record);
ObjectTypeRecord *pop_type_record(Compiler *compiler);
ObjectTypeRecord *peek_type_record(const Compiler *compiler);

bool is_valid_table_key_type(ObjectTypeRecord *type);
void emit_words(const Compiler *compiler, uint16_t word1, uint16_t word2);
void emit_word(const Compiler *compiler, uint16_t word);

/**
 * lookup a callable in a vm stdlib table
 * @param compiler The current compiler
 * @param type_table The vm owned type table to look for a callable from
 * @param name_token The name of the callable
 * @return the callable otherwise NULL if no callable found
 */
const ObjectNativeCallable *lookup_stdlib_method(const Compiler *compiler, const Table *type_table,
												 const Token *name_token);
void consume_identifier_like(const Compiler *compiler, const char *message);
bool check_identifier_like(const Compiler *compiler);
bool is_identifier_like(CruxTokenType type);
bool match(const Compiler *compiler, CruxTokenType type);
bool check(const Compiler *compiler, CruxTokenType type);
void advance(const Compiler *compiler);
void consume(const Compiler *compiler, CruxTokenType type, const char *message);
Chunk *current_chunk(const Compiler *compiler);

/**
 * emits an OP_LOOP instruction
 * @param loopStart The starting point of the loop
 */
void emit_loop(const Compiler *compiler, int loopStart);

/**
 * Emits a jump instruction and placeholders for the jump offset.
 *
 * @param instruction The opcode for the jump instruction (e.g., OP_JUMP,
 * OP_JUMP_IF_FALSE).
 * @return The index of the jump instruction in the bytecode, used for patching
 * later.
 */
int emit_jump(const Compiler *compiler, uint16_t instruction);

/**
 * Patches a jump instruction with the calculated offset.
 */
void patch_jump(const Compiler *compiler, int offset);

void emit_return(const Compiler *compiler);

uint16_t make_constant(const Compiler *compiler, Value value);

void emit_constant(const Compiler *compiler, Value value);

void push_loop_context(Compiler *compiler, LoopType type, int continueTarget);

void pop_loop_context(Compiler *compiler);

void add_break_jump(Compiler *compiler, int jumpOffset);

uint16_t identifier_constant(const Compiler *compiler, const Token *name);

void begin_scope(Compiler *compiler);

void cleanupLocalsToDepth(Compiler *compiler, int targetDepth);

void emit_cleanup_for_jump(const Compiler *compiler, int targetDepth);

/**
 * Adds an upvalue to the current function.
 *
 * An upvalue is a variable captured from an enclosing function's scope.
 *
 * @param compiler The current compiler.
 * @param index The index of the local variable in the enclosing function, or
 * the index of the upvalue.
 * @param isLocal true if the upvalue is a local variable in the enclosing
 * function, false if it's an upvalue itself.
 * @param type The type of the upvalue.
 * @return The index of the added upvalue in the current function's upvalue
 * array.
 */
int add_upvalue(Compiler *compiler, const uint16_t index, const bool isLocal, ObjectTypeRecord *type);

int get_current_continue_target(const Compiler *compiler);

/**
 * Resolves a local variable in the current compiler's scope.
 *
 * Searches the current compiler's local variables for a variable with the given
 * name.
 *
 * @param compiler The compiler whose scope to search.
 * @param name The token representing the variable name.
 * @return The index of the local variable if found, -1 otherwise.
 */
int resolve_local(const Compiler *compiler, const Token *name);

bool identifiers_equal(const Token *a, const Token *b);

/**
 * Ends the current scope.
 * Decreases the scope depth and emits OP_POP instructions to remove local
 * variables that go out of scope.
 */
void end_scope(Compiler *compiler);

OpCode get_compound_opcode(const Compiler *compiler,  OpCode setOp, int op);

/**
 * Defines a variable, emitting the bytecode to store its value.
 *
 * If the variable is global, emits OP_DEFINE_GLOBAL. Otherwise, it's a local
 * variable, and no bytecode is emitted at definition time (local variables are
 * implicitly defined when they are declared and initialized).
 *
 * @param compiler
 * @param global The index of the variable name constant in the constant pool
 * (for global variables).
 * @param is_public if this is a public declaration
 */
void define_variable(Compiler *compiler, uint16_t global, bool is_public);

/**
 * Check math types for compound operations
 * @param compiler
 * @param lhs_type
 * @param rhs_type
 * @param op
 */
void check_compound_type_math(const Compiler *compiler, ObjectTypeRecord *lhs_type, ObjectTypeRecord *rhs_type, int op);

int match_compound_op(const Compiler *compiler);

/**
 * Parses a variable name and returns its constant pool index.
 *
 * @param compiler current compiler
 * @param errorMessage The error message to display if an identifier is not
 * found.
 * @return The index of the variable name constant in the constant pool.
 */
uint16_t parse_variable(Compiler *compiler, const char *errorMessage);

/**
 * Marks the most recently declared local variable as initialized.
 *
 * This prevents reading a local variable before it has been assigned a value.
 */
void mark_initialized(Compiler *compiler);

/**
 * Declares a variable in the current scope.
 *
 * Adds the variable name to the list of local variables, but does not mark it
 * as initialized yet.
 */
void declare_variable(Compiler *compiler);

/**
 * Adds a local variable to the current scope.
 *
 * @param compiler
 * @param name The token representing the name of the local variable.
 * @param type
 */
void add_local(Compiler *compiler, Token name, ObjectTypeRecord *type);

/**
 * Resolves an upvalue in enclosing scopes.
 *
 * Searches enclosing compiler scopes for a local variable or upvalue with the
 * given name.
 *
 * @param compiler The current compiler.
 * @param name The token representing the variable name.
 * @return The index of the resolved upvalue if found, -1 otherwise.
 */
int resolve_upvalue(Compiler *compiler, Token *name);

void ensure_local_name_available(const Compiler *compiler, const Token name);

void declare_named_variable(Compiler *compiler, Token name, ObjectTypeRecord *type);

bool match_type_name(const Compiler *compiler);

TypeMask type_token_type_to_mask(CruxTokenType token_type);
/**
 * Checks if the previous opcode in the current chunk matches the given opcode.
 *
 * @param compiler The current compiler.
 * @param op The opcode to check for.
 * @param distance The distance from the current instruction to check
 * @return true if the previous opcode matches, false otherwise.
 */
bool check_previous_op_code(const Compiler *compiler, OpCode op, int distance);
bool set_previous_op_code(const Compiler *compiler, OpCode op, int distance);

#endif // COMPILER_H
