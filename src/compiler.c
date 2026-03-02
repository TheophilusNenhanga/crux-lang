#include "compiler.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "file_handler.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
#include "type_system.h"
#include "value.h"

Parser parser;

Compiler *current = NULL;

Chunk *compilingChunk;

static void expression(void);

static void parse_precedence(Precedence precedence);

static ParseRule *get_rule(CruxTokenType type);

static void binary(bool can_assign);

static void unary(bool can_assign);

static void grouping(bool can_assign);

static void number(bool can_assign);

static void statement(void);

static void declaration(void);

static Chunk *current_chunk(void)
{
	return &current->function->chunk;
}

static void advance(void)
{
	parser.prev_previous = parser.previous;
	parser.previous = parser.current;
	for (;;) {
		parser.current = scan_token();
		if (parser.current.type != TOKEN_ERROR)
			break;
		compiler_panic(&parser, parser.current.start, SYNTAX);
	}
}

static void consume(const CruxTokenType type, const char *message)
{
	if (parser.current.type == type) {
		advance();
		return;
	}
	compiler_panic_at_current(&parser, message, SYNTAX);
}

static bool check(const CruxTokenType type)
{
	return parser.current.type == type;
}

static bool match(const CruxTokenType type)
{
	if (!check(type))
		return false;
	advance();
	return true;
}

static bool is_identifier_like(const CruxTokenType type)
{
	switch (type) {
	case TOKEN_IDENTIFIER:
	// Type names that are also valid constructor/import names:
	case TOKEN_RANDOM_TYPE:
	case TOKEN_BUFFER_TYPE:
	case TOKEN_RANGE_TYPE:
	case TOKEN_VECTOR_TYPE:
	case TOKEN_MATRIX_TYPE:
	case TOKEN_COMPLEX_TYPE:
	case TOKEN_SET_TYPE:
	case TOKEN_TUPLE_TYPE:
	case TOKEN_FILE_TYPE:
		return true;
	default:
		return false;
	}
}

static bool check_identifier_like(void)
{
	return is_identifier_like(parser.current.type);
}

static void consume_identifier_like(const char *message)
{
	if (check_identifier_like()) {
		advance();
		return;
	}
	compiler_panic(&parser, message, SYNTAX);
}

static bool match_identifier_like(void)
{
	if (!check_identifier_like())
		return false;
	advance();
	return true;
}

static void emit_word(const uint16_t word)
{
	write_chunk(current->owner, current_chunk(), word,
		    parser.previous.line);
}

static void emit_words(const uint16_t word1, const uint16_t word2)
{
	emit_word(word1);
	emit_word(word2);
}

static bool is_valid_table_key_type(TypeRecord *type)
{
	if (!type)
		return false;
	if (type->base_type == ANY_TYPE)
		return true;
	return (type->base_type & HASHABLE_TYPE) != 0;
}

TypeRecord *parse_type_record()
{
	TypeRecord *type_record = NULL;

	if (match(TOKEN_INT_TYPE)) {
		type_record = new_type_rec(&current->type_arena, INT_TYPE);
	} else if (match(TOKEN_FLOAT_TYPE)) {
		type_record = new_type_rec(&current->type_arena, FLOAT_TYPE);
	} else if (match(TOKEN_BOOL_TYPE)) {
		type_record = new_type_rec(&current->type_arena, BOOL_TYPE);
	} else if (match(TOKEN_STRING_TYPE)) {
		type_record = new_type_rec(&current->type_arena, STRING_TYPE);
	} else if (match(TOKEN_NIL_TYPE)) {
		type_record = new_type_rec(&current->type_arena, NIL_TYPE);
	} else if (match(TOKEN_ANY_TYPE)) {
		type_record = new_type_rec(&current->type_arena, ANY_TYPE);
	} else if (match(TOKEN_ARRAY_TYPE)) {
		if (match(TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(&current->type_arena,
						   ARRAY_TYPE);
			type_record->as.array_type.element_type =
				parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after array element type.");
		} else {
			compiler_panic(
				&parser,
				"Expected '[' for array type definition.",
				TYPE);
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
	} else if (match(TOKEN_TABLE_TYPE)) {
		if (match(TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(&current->type_arena,
						   TABLE_TYPE);
			TypeRecord *key_type = parse_type_record();
			// Task 4: enforce hashable key types
			if (!is_valid_table_key_type(key_type)) {
				char got[64], msg[192];
				type_mask_name(key_type->base_type, got,
					       sizeof(got));
				snprintf(msg, sizeof(msg),
					 "Table key type '%s' is not hashable. "
					 "Keys must be Int, Float, String, "
					 "Bool, or Nil.",
					 got);
				compiler_panic(&parser, msg, TYPE);
			}
			type_record->as.table_type.key_type = key_type;
			consume(TOKEN_COLON, "Expected ':' after key type.");
			type_record->as.table_type.value_type =
				parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after table element type.");
		} else {
			compiler_panic(
				&parser,
				"Expected '[' for table type definition.",
				TYPE);
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
	} else if (match(TOKEN_VECTOR_TYPE)) {
		// TODO: change vector from Vector[type] to Vector[dim_count]
		if (match(TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(&current->type_arena,
						   VECTOR_TYPE);
			type_record->as.vector_type.element_type =
				parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after vector element type.");
		} else {
			compiler_panic(
				&parser,
				"Expected '[' for vector type definition.",
				TYPE);
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
	} else if (match(TOKEN_MATRIX_TYPE)) {
		type_record = new_type_rec(&current->type_arena, MATRIX_TYPE);
	} else if (match(TOKEN_BUFFER_TYPE)) {
		type_record = new_type_rec(&current->type_arena, BUFFER_TYPE);
	} else if (match(TOKEN_ERROR_TYPE)) {
		type_record = new_type_rec(&current->type_arena, ERROR_TYPE);
	} else if (match(TOKEN_RESULT_TYPE)) {
		if (match(TOKEN_LEFT_SQUARE)) {
			TypeRecord *value_type = parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after result value type.");
			type_record = new_result_type_rec(&current->type_arena,
							  value_type);
		} else {
			compiler_panic(
				&parser,
				"Expected '[' for result type definition.",
				TYPE);
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
	} else if (match(TOKEN_RANGE_TYPE)) {
		type_record = new_type_rec(&current->type_arena, RANGE_TYPE);
	} else if (match(TOKEN_TUPLE_TYPE)) {
		// Task 1: was incorrectly creating VECTOR_TYPE — fixed to
		// TUPLE_TYPE
		if (match(TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(&current->type_arena,
						   TUPLE_TYPE);
			type_record->as.tuple_type.element_type =
				parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after tuple element type.");
		} else {
			compiler_panic(
				&parser,
				"Expected '[' for tuple type definition.",
				TYPE);
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
	} else if (match(TOKEN_COMPLEX_TYPE)) {
		type_record = new_type_rec(&current->type_arena, COMPLEX_TYPE);
	} else if (match(TOKEN_LEFT_PAREN)) {
		// Function type: (ParamType, ...) -> ReturnType
		// The arg_types array is heap-allocated and intentionally not
		// freed here — it is owned by the TypeRecord for the duration
		// of compilation and freed when the compiler is torn down.
		int param_capacity = 4;
		int param_count = 0;
		TypeRecord **param_types = malloc(sizeof(TypeRecord *) *
						  param_capacity);
		if (!param_types) {
			compiler_panic(&parser, "Memory allocation failed.",
				       MEMORY);
			return new_type_rec(&current->type_arena, ANY_TYPE);
		}

		do {
			TypeRecord *inner = parse_type_record();
			if (!inner) {
				compiler_panic(&parser, "Expected type.", TYPE);
				inner = new_type_rec(&current->type_arena,
						     ANY_TYPE);
			}
			if (param_count == param_capacity) {
				param_capacity *= 2;
				TypeRecord **grown = realloc(
					param_types,
					sizeof(TypeRecord *) * param_capacity);
				if (!grown) {
					free(param_types);
					compiler_panic(
						&parser,
						"Memory allocation failed.",
						MEMORY);
					return new_type_rec(
						&current->type_arena, ANY_TYPE);
				}
				param_types = grown;
			}
			param_types[param_count++] = inner;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_PAREN,
			"Expected ')' to end function argument types.");

		consume(TOKEN_ARROW, "Expected '->' to separate function "
				     "argument types from return type.");
		TypeRecord *return_type = parse_type_record();
		if (!return_type) {
			compiler_panic(&parser, "Expected type.", TYPE);
			free(param_types);
			return new_type_rec(&current->type_arena, ANY_TYPE);
		}

		// Shrink to exact size — we keep this array alive.
		if (param_count > 0) {
			TypeRecord **shrunk = realloc(param_types,
						      sizeof(TypeRecord *) *
							      param_count);
			if (shrunk)
				param_types = shrunk;
		}

		type_record = new_type_rec(&current->type_arena, FUNCTION_TYPE);
		type_record->as.function_type.arg_types = param_types;
		type_record->as.function_type.arg_count = param_count;
		type_record->as.function_type.return_type = return_type;
	} else if (match(TOKEN_SET_TYPE)) {
		if (match(TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(&current->type_arena,
						   SET_TYPE);
			type_record->as.set_type.element_type =
				parse_type_record();
			consume(TOKEN_RIGHT_SQUARE,
				"Expected ']' after set element type.");
		} else {
			type_record = new_type_rec(&current->type_arena,
						   SET_TYPE);
		}
	} else if (match(TOKEN_RANDOM_TYPE)) {
		type_record = new_type_rec(&current->type_arena, RANDOM_TYPE);
	} else if (match(TOKEN_FILE_TYPE)) {
		type_record = new_type_rec(&current->type_arena, FILE_TYPE);
	} else if (check(TOKEN_IDENTIFIER)) {
		// Task 3: look up a user-defined struct (or other named) type.
		// Walk the compiler chain so inner functions see enclosing
		// struct declarations.
		advance();
		ObjectString *name_str = copy_string(current->owner,
						     parser.previous.start,
						     parser.previous.length);
		TypeRecord *found = NULL;
		Compiler *comp = current;
		while (comp != NULL) {
			if (type_table_get(&comp->type_table, name_str, &found))
				break;
			comp = comp->enclosing;
		}
		if (!found) {
			// Forward reference or unknown name — degrade to
			// ANY_TYPE. The pre-scanner resolves most of these
			// before the main pass begins.
			type_record = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		} else {
			type_record = found;
		}
	} else {
		compiler_panic(&parser, "Expected type.", TYPE);
		type_record = new_type_rec(&current->type_arena, ANY_TYPE);
	}

	// Task 2: union type parsing — if a '|' follows the first type,
	// collect additional variants into a union TypeRecord.
	// Note: TOKEN_PIPE is also used as bitwise-OR in binary(), but
	// parse_type_record() is only called in type-annotation context
	// where '|' always means union.
	if (type_record && match(TOKEN_PIPE)) {
		int cap = 4;
		int count = 1;
		// Heap-allocate the variants array; it is owned by the
		// TypeRecord and lives for the duration of compilation.
		TypeRecord **variants = malloc(sizeof(TypeRecord *) * cap);
		if (!variants) {
			compiler_panic(&parser, "Memory allocation failed.",
				       MEMORY);
			return type_record; // return first variant as fallback
		}
		variants[0] = type_record;

		do {
			if (count == cap) {
				cap *= 2;
				TypeRecord **grown = realloc(
					variants, sizeof(TypeRecord *) * cap);
				if (!grown) {
					free(variants);
					compiler_panic(
						&parser,
						"Memory allocation failed.",
						MEMORY);
					return type_record;
				}
				variants = grown;
			}
			variants[count++] = parse_type_record();
		} while (match(TOKEN_PIPE));

		// element_names can be NULL for anonymous unions.
		type_record = new_union_type_rec(&current->type_arena, variants,
						 NULL, count);
		// Do NOT free(variants) — new_union_type_rec stores the
		// pointer and it must remain valid for the life of the
		// TypeRecord (compiler arena duration).
	}

	return type_record;
}

/**
 * emits an OP_LOOP instruction
 * @param loopStart The starting point of the loop
 */
static void emit_loop(const int loopStart)
{
	emit_word(OP_LOOP);
	const int offset = current_chunk()->count - loopStart +
			   1; // +1 takes into account the size of the OP_LOOP
	if (offset > UINT16_MAX) {
		compiler_panic(&parser, "Loop body too large.", LOOP_EXTENT);
	}
	emit_word(offset);
}

/**
 * Emits a jump instruction and placeholders for the jump offset.
 *
 * @param instruction The opcode for the jump instruction (e.g., OP_JUMP,
 * OP_JUMP_IF_FALSE).
 * @return The index of the jump instruction in the bytecode, used for patching
 * later.
 */
static int emit_jump(const uint16_t instruction)
{
	emit_word(instruction);
	emit_word(0xffff);
	return current_chunk()->count - 1;
}

/**
 * Patches a jump instruction with the calculated offset.
 */
static void patch_jump(const int offset)
{
	// -1 to adjust for the bytecode for the jump offset itself
	const int jump = current_chunk()->count - offset - 1;
	if (jump > UINT16_MAX) {
		compiler_panic(&parser, "Too much code to jump over.",
			       BRANCH_EXTENT);
	}
	current_chunk()->code[offset] = (uint16_t)jump;
}

static void emit_return(void)
{
	emit_word(OP_NIL_RETURN);
}

static uint16_t make_constant(const Value value)
{
	const int constant = add_constant(current->owner, current_chunk(),
					  value);
	if (constant >= UINT16_MAX) {
		compiler_panic(&parser, "Too many constants in one chunk.",
			       LIMIT);
		return 0;
	}
	return (uint16_t)constant;
}

static void emit_constant(const Value value)
{
	const uint16_t constant = make_constant(value);
	if (constant >= UINT16_MAX) {
		compiler_panic(&parser, "Too many constants", SYNTAX);
	}
	emit_words(OP_CONSTANT, constant);
}

static void push_loop_context(const LoopType type, const int continueTarget)
{
	if (current->loop_depth >= UINT8_MAX) {
		compiler_panic(&parser, "Too many nested loops.", LOOP_EXTENT);
		return;
	}

	LoopContext *context = &current->loop_stack[current->loop_depth++];
	context->type = type;
	context->continue_target = continueTarget;
	context->break_jumps = NULL;
	context->scope_depth = current->scope_depth;
}

static void pop_loop_context(void)
{
	if (current->loop_depth <= 0) {
		return;
	}

	const LoopContext *context =
		&current->loop_stack[--current->loop_depth];

	// Patch all break jumps to jump to current position
	BreakJump *breakJump = context->break_jumps;
	while (breakJump != NULL) {
		patch_jump(breakJump->jumpOffset);
		BreakJump *next = breakJump->next;
		FREE(current->owner, BreakJump, breakJump);
		breakJump = next;
	}
}

static void add_break_jump(const int jumpOffset)
{
	if (current->loop_depth <= 0) {
		compiler_panic(&parser, "Cannot use 'break' outside of a loop.",
			       SYNTAX);
		return;
	}

	LoopContext *context = &current->loop_stack[current->loop_depth - 1];

	BreakJump *breakJump = ALLOCATE(current->owner, BreakJump, 1);
	breakJump->jumpOffset = jumpOffset;
	breakJump->next = context->break_jumps;
	context->break_jumps = breakJump;
}

void push_type_record(TypeRecord *type_record)
{
	current->type_stack[current->type_stack_count++] = type_record;
}

TypeRecord *pop_type_record(void)
{
	// Task 8: guard against underflow during error recovery.
	// After compiler_panic sets panic_mode, code may continue briefly
	// before synchronize() is called, causing extra pop calls on an
	// already-empty stack. Return an ANY_TYPE sentinel instead of
	// accessing type_stack[-1].
	if (current->type_stack_count <= 0) {
		return new_type_rec(&current->type_arena, ANY_TYPE);
	}
	return current->type_stack[--current->type_stack_count];
}

// Task 8: inspect the top of the type stack without consuming it.
// Returns NULL if the stack is empty.
TypeRecord *peek_type_record(void)
{
	if (current->type_stack_count <= 0)
		return NULL;
	return current->type_stack[current->type_stack_count - 1];
}

static void init_compiler(Compiler *compiler, const FunctionType type, VM *vm)
{
	compiler->type_stack_count = 0;
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->local_count = 0;
	compiler->scope_depth = 0;
	compiler->match_depth = 0;
	compiler->loop_depth = 0;
	compiler->owner = vm;

	init_type_table(&compiler->type_table);
	type_arena_reset(&compiler->type_arena);

	compiler->function = new_function(compiler->owner);
	current = compiler;

	if (type == TYPE_ANONYMOUS) {
		current->function->name = copy_string(current->owner,
						      "anonymous", 9);
	} else if (type != TYPE_SCRIPT) {
		current->function->name = copy_string(current->owner,
						      parser.previous.start,
						      parser.previous.length);
	}

	Local *local = &current->locals[current->local_count++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
	local->is_captured = false;

	if (type != TYPE_FUNCTION) {
		local->name.start = "self";
		local->name.length = 4;
	} else {
		local->name.start = "";
		local->name.length = 0;
	}
}

static uint16_t identifier_constant(const Token *name)
{
	return make_constant(OBJECT_VAL(
		copy_string(current->owner, name->start, name->length)));
}

static void begin_scope(void)
{
	current->scope_depth++;
}

static void cleanupLocalsToDepth(const int targetDepth)
{
	while (current->local_count > 0 &&
	       current->locals[current->local_count - 1].depth > targetDepth) {
		if (current->locals[current->local_count - 1].is_captured) {
			emit_word(OP_CLOSE_UPVALUE);
		} else {
			emit_word(OP_POP);
		}
		current->local_count--;
	}
}

/**
 * Ends the current scope.
 *
 * Decreases the scope depth and emits OP_POP instructions to remove local
 * variables that go out of scope.
 */
static void end_scope(void)
{
	current->scope_depth--;
	cleanupLocalsToDepth(current->scope_depth);
}

static bool identifiers_equal(const Token *a, const Token *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

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
static int resolve_local(const Compiler *compiler, const Token *name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		const Local *local = &compiler->locals[i];
		if (identifiers_equal(name, &local->name)) {
			if (local->depth == -1) {
				compiler_panic(&parser,
					       "Cannot read local variable in "
					       "its own initializer",
					       NAME);
			}
			return i;
		}
	}
	return -1;
}

static int get_current_continue_target(void)
{
	if (current->loop_depth <= 0) {
		compiler_panic(&parser,
			       "Cannot use 'continue' outside of a loop.",
			       SYNTAX);
		return -1;
	}

	return current->loop_stack[current->loop_depth - 1].continue_target;
}

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
static int add_upvalue(Compiler *compiler, const uint16_t index,
		       const bool isLocal, TypeRecord *type)
{
	const int upvalueCount = compiler->function->upvalue_count;

	for (int i = 0; i < upvalueCount; i++) {
		Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->is_local == isLocal) {
			if (!upvalue->type && type) {
				upvalue->type = type;
			}
			return i;
		}
	}

	if (upvalueCount >= UINT16_MAX) {
		compiler_panic(&parser,
			       "Too many closure variables in function.",
			       CLOSURE_EXTENT);
		return 0;
	}

	compiler->upvalues[upvalueCount].is_local = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	compiler->upvalues[upvalueCount].type = type;
	return compiler->function->upvalue_count++;
}

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
static int resolve_upvalue(Compiler *compiler, Token *name)
{
	if (compiler->enclosing == NULL)
		return -1;

	const int local = resolve_local(compiler->enclosing, name);
	if (local != -1) {
		(compiler->enclosing)->locals[local].is_captured = true;
		TypeRecord *type = compiler->enclosing->locals[local].type;
		return add_upvalue(compiler, (uint16_t)local, true, type);
	}

	const int upValue = resolve_upvalue(compiler->enclosing, name);
	if (upValue != -1) {
		TypeRecord *type = compiler->enclosing->upvalues[upValue].type;
		return add_upvalue(compiler, (uint16_t)upValue, false, type);
	}

	return -1;
}

/**
 * Adds a local variable to the current scope.
 *
 * @param name The token representing the name of the local variable.
 */
static void add_local(const Token name, TypeRecord *type)
{
	if (current->local_count == UINT16_MAX) {
		compiler_panic(&parser, "Too many local variables in function.",
			       LOCAL_EXTENT);
		return;
	}

	Local *local = &current->locals[current->local_count++];
	local->name = name;
	local->depth = -1;
	local->is_captured = false;
	local->type = type; // NULL until the initializer is complete
}

/**
 * Declares a variable in the current scope.
 *
 * Adds the variable name to the list of local variables, but does not mark it
 * as initialized yet.
 */
static void declare_variable(void)
{
	if (current->scope_depth == 0)
		return;

	const Token *name = &parser.previous;

	for (int i = current->local_count - 1; i >= 0; i--) {
		const Local *local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scope_depth) {
			break;
		}
		if (identifiers_equal(name, &local->name)) {
			compiler_panic(
				&parser,
				"Cannot redefine variable in the same scope",
				NAME);
		}
	}

	add_local(*name, NULL);
}

/**
 * Marks the most recently declared local variable as initialized.
 *
 * This prevents reading a local variable before it has been assigned a value.
 */
static void mark_initialized(void)
{
	if (current->scope_depth == 0)
		return;
	current->locals[current->local_count - 1].depth = current->scope_depth;
}

/**
 * Parses a variable name and returns its constant pool index.
 *
 * @param errorMessage The error message to display if an identifier is not
 * found.
 * @return The index of the variable name constant in the constant pool.
 */
static uint16_t parse_variable(const char *errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);
	declare_variable();
	if (current->scope_depth > 0)
		return 0;
	return identifier_constant(&parser.previous);
}

/**
 * Defines a variable, emitting the bytecode to store its value.
 *
 * If the variable is global, emits OP_DEFINE_GLOBAL. Otherwise, it's a local
 * variable, and no bytecode is emitted at definition time (local variables are
 * implicitly defined when they are declared and initialized).
 *
 * @param global The index of the variable name constant in the constant pool
 * (for global variables).
 */
static void define_variable(const uint16_t global)
{
	if (current->scope_depth > 0) {
		mark_initialized();
		return;
	}
	if (global >= UINT16_MAX) {
		compiler_panic(&parser, "Too many variables.", SYNTAX);
	}
	emit_words(OP_DEFINE_GLOBAL, global);
}

static OpCode get_compound_opcode(const OpCode setOp, const CompoundOp op)
{
	switch (setOp) {
	case OP_SET_LOCAL: {
		switch (op) {
		case COMPOUND_OP_PLUS:
			return OP_SET_LOCAL_PLUS;
		case COMPOUND_OP_MINUS:
			return OP_SET_LOCAL_MINUS;
		case COMPOUND_OP_STAR:
			return OP_SET_LOCAL_STAR;
		case COMPOUND_OP_SLASH:
			return OP_SET_LOCAL_SLASH;
		case COMPOUND_OP_BACK_SLASH:
			return OP_SET_LOCAL_INT_DIVIDE;
		case COMPOUND_OP_PERCENT:
			return OP_SET_LOCAL_MODULUS;
		default: {
			compiler_panic(&parser,
				       "Compiler Error: Failed to create "
				       "bytecode for compound operation.",
				       RUNTIME);
			break;
		}
		}
		break;
	}
	case OP_SET_UPVALUE: {
		switch (op) {
		case COMPOUND_OP_PLUS:
			return OP_SET_UPVALUE_PLUS;
		case COMPOUND_OP_MINUS:
			return OP_SET_UPVALUE_MINUS;
		case COMPOUND_OP_STAR:
			return OP_SET_UPVALUE_STAR;
		case COMPOUND_OP_SLASH:
			return OP_SET_UPVALUE_SLASH;
		case COMPOUND_OP_BACK_SLASH:
			return OP_SET_UPVALUE_INT_DIVIDE;
		case COMPOUND_OP_PERCENT:
			return OP_SET_UPVALUE_MODULUS;
		default: {
			compiler_panic(&parser,
				       "Compiler Error: Failed to create "
				       "bytecode for compound operation.",
				       RUNTIME);
			break;
		}
		}
		break;
	}
	case OP_SET_GLOBAL: {
		switch (op) {
		case COMPOUND_OP_PLUS:
			return OP_SET_GLOBAL_PLUS;
		case COMPOUND_OP_MINUS:
			return OP_SET_GLOBAL_MINUS;
		case COMPOUND_OP_STAR:
			return OP_SET_GLOBAL_STAR;
		case COMPOUND_OP_SLASH:
			return OP_SET_GLOBAL_SLASH;
		case COMPOUND_OP_BACK_SLASH:
			return OP_SET_GLOBAL_INT_DIVIDE;
		case COMPOUND_OP_PERCENT:
			return OP_SET_GLOBAL_MODULUS;
		default: {
			compiler_panic(&parser,
				       "Compiler Error: Failed to create "
				       "bytecode for compound operation.",
				       RUNTIME);
			break;
		}
		}
		break;
	}
	default:
		return setOp; // Should never happen
	}
	return setOp;
}

/**
 * Parses a named variable (local, upvalue, or global).
 *
 * @param name The token representing the variable name.
 * @param can_assign Whether the variable expression can be the target of an
 * assignment.
 */
static void named_variable(Token name, const bool can_assign)
{
	uint16_t getOp, setOp;
	int arg = resolve_local(current, &name);
	TypeRecord *var_type = NULL;

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
		var_type = current->locals[arg].type;
	} else if ((arg = resolve_upvalue(current, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
		var_type = current->upvalues[arg].type;
	} else {
		arg = identifier_constant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;

		// Walk the compiler chain so inner functions can see globals
		// declared in an enclosing script-level compiler.
		ObjectString *name_str = copy_string(current->owner, name.start,
						     name.length);
		Compiler *comp = current;
		while (comp != NULL) {
			if (type_table_get(&comp->type_table, name_str,
					   &var_type))
				break;
			comp = comp->enclosing;
		}
	}

	if (can_assign) {
		// ---- Plain assignment: x = expr ----
		if (match(TOKEN_EQUAL)) {
			expression();
			TypeRecord *value_type = pop_type_record();

			if (var_type && value_type &&
			    var_type->base_type != ANY_TYPE &&
			    value_type->base_type != ANY_TYPE) {
				if (!types_compatible(var_type, value_type)) {
					char expected[64], got[64], msg[160];
					type_mask_name(var_type->base_type,
						       expected,
						       sizeof(expected));
					type_mask_name(value_type->base_type,
						       got, sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Cannot assign '%s' to "
						 "variable of type '%s'.",
						 got, expected);
					compiler_panic(&parser, msg, TYPE);
				}
			}

			emit_words(setOp, arg);
			return;
		}

		// ---- Compound assignment: x += expr, etc. ----
		CompoundOp op;
		bool isCompoundAssignment = true;

		if (match(TOKEN_PLUS_EQUAL)) {
			op = COMPOUND_OP_PLUS;
		} else if (match(TOKEN_MINUS_EQUAL)) {
			op = COMPOUND_OP_MINUS;
		} else if (match(TOKEN_STAR_EQUAL)) {
			op = COMPOUND_OP_STAR;
		} else if (match(TOKEN_SLASH_EQUAL)) {
			op = COMPOUND_OP_SLASH;
		} else if (match(TOKEN_BACK_SLASH_EQUAL)) {
			op = COMPOUND_OP_BACK_SLASH;
		} else if (match(TOKEN_PERCENT_EQUAL)) {
			op = COMPOUND_OP_PERCENT;
		} else {
			isCompoundAssignment = false;
		}

		if (isCompoundAssignment) {
			expression();
			TypeRecord *rhs_type = pop_type_record();

			if (var_type && rhs_type &&
			    var_type->base_type != ANY_TYPE &&
			    rhs_type->base_type != ANY_TYPE) {
				const bool var_num = var_type->base_type ==
							     INT_TYPE ||
						     var_type->base_type ==
							     FLOAT_TYPE;
				const bool rhs_num = rhs_type->base_type ==
							     INT_TYPE ||
						     rhs_type->base_type ==
							     FLOAT_TYPE;

				if (op == COMPOUND_OP_PLUS &&
				    var_type->base_type == STRING_TYPE) {
					// String += String only
					if (rhs_type->base_type !=
					    STRING_TYPE) {
						compiler_panic(
							&parser,
							"'+=' on a String "
							"requires a String "
							"right-hand side.",
							TYPE);
					}
				} else if (op == COMPOUND_OP_BACK_SLASH ||
					   op == COMPOUND_OP_PERCENT) {
					// Integer-only operators
					if (var_type->base_type != INT_TYPE ||
					    rhs_type->base_type != INT_TYPE) {
						compiler_panic(&parser,
							       "This compound "
							       "operator "
							       "requires Int "
							       "operands.",
							       TYPE);
					}
				} else if (!var_num || !rhs_num) {
					compiler_panic(
						&parser,
						"Compound assignment requires "
						"numeric operands.",
						TYPE);
				}
			}

			const OpCode compoundOp = get_compound_opcode(setOp,
								      op);
			emit_words(compoundOp, arg);
			return;
		}
	}

	// ---- Read ----
	emit_words(getOp, arg);
	// Always push a type so downstream expressions stay balanced.
	push_type_record(
		var_type ? var_type
			 : new_type_rec(&current->type_arena, ANY_TYPE));
}
// argument_list() was removed — infix_call() and dot() now inline their own
// argument parsing loops so they can collect and validate argument types.

static void and_(bool can_assign)
{
	(void)can_assign;

	// Left operand type is already on the stack from before
	// the infix rule was called — check it now before emitting
	// the jump.
	TypeRecord *left_type = pop_type_record();
	if (left_type && left_type->base_type != BOOL_TYPE &&
	    left_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "Left operand of 'and' must be of type 'Bool'.",
			       TYPE);
	}

	const int endJump = emit_jump(OP_JUMP_IF_FALSE);
	emit_word(OP_POP);
	parse_precedence(PREC_AND);

	// Right operand type was just pushed by parse_precedence.
	TypeRecord *right_type = pop_type_record();
	if (right_type && right_type->base_type != BOOL_TYPE &&
	    right_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "Right operand of 'and' must be of type 'Bool'.",
			       TYPE);
	}

	patch_jump(endJump);

	// Always push so the stack stays balanced even after a panic.
	push_type_record(new_type_rec(&current->type_arena, BOOL_TYPE));
}

static void or_(bool can_assign)
{
	(void)can_assign;

	// Same reasoning as and_ — left type is already on the stack.
	TypeRecord *left_type = pop_type_record();
	if (left_type && left_type->base_type != BOOL_TYPE &&
	    left_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "Left operand of 'or' must be of type 'Bool'.",
			       TYPE);
	}

	const int elseJump = emit_jump(OP_JUMP_IF_FALSE);
	const int endJump = emit_jump(OP_JUMP);
	patch_jump(elseJump);
	emit_word(OP_POP);
	parse_precedence(PREC_OR);

	TypeRecord *right_type = pop_type_record();
	if (right_type && right_type->base_type != BOOL_TYPE &&
	    right_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "Right operand of 'or' must be of type 'Bool'.",
			       TYPE);
	}

	patch_jump(endJump);

	// Always push so the stack stays balanced even after a panic.
	push_type_record(new_type_rec(&current->type_arena, BOOL_TYPE));
}

static ObjectFunction *end_compiler(void)
{
	emit_return();
	push_type_record(current->return_type);
	ObjectFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.had_error) {
		disassemble_chunk(current_chunk(),
				  function->name != NULL ? function->name->chars
							 : "<script>");
	}
#endif

	function->module_record = current->owner->current_module_record;
	current = current->enclosing;
	return function;
}

static void binary(bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = parser.previous.type;
	const ParseRule *rule = get_rule(operatorType);
	parse_precedence(rule->precedence + 1);

	TypeRecord *right_type = pop_type_record();
	TypeRecord *left_type = pop_type_record();
	TypeRecord *result_type = NULL;

	const bool either_any = (left_type &&
				 left_type->base_type == ANY_TYPE) ||
				(right_type &&
				 right_type->base_type == ANY_TYPE) ||
				!left_type || !right_type;

	switch (operatorType) {
	case TOKEN_EQUAL_EQUAL:
		emit_word(OP_EQUAL);
		result_type = new_type_rec(&current->type_arena, BOOL_TYPE);
		break;

	case TOKEN_BANG_EQUAL:
		emit_word(OP_NOT_EQUAL);
		result_type = new_type_rec(&current->type_arena, BOOL_TYPE);
		break;

	case TOKEN_GREATER:
	case TOKEN_GREATER_EQUAL:
	case TOKEN_LESS:
	case TOKEN_LESS_EQUAL: {
		if (!either_any) {
			const bool left_num = left_type->base_type ==
						      INT_TYPE ||
					      left_type->base_type ==
						      FLOAT_TYPE;
			const bool right_num = right_type->base_type ==
						       INT_TYPE ||
					       right_type->base_type ==
						       FLOAT_TYPE;
			const bool both_str = left_type->base_type ==
						      STRING_TYPE &&
					      right_type->base_type ==
						      STRING_TYPE;

			if (!((left_num && right_num) || both_str)) {
				compiler_panic(
					&parser,
					"Comparison operator requires numeric "
					"or String operands.",
					TYPE);
			}
		}

		switch (operatorType) {
		case TOKEN_GREATER:
			emit_word(OP_GREATER);
			break;
		case TOKEN_GREATER_EQUAL:
			emit_word(OP_GREATER_EQUAL);
			break;
		case TOKEN_LESS:
			emit_word(OP_LESS);
			break;
		case TOKEN_LESS_EQUAL:
			emit_word(OP_LESS_EQUAL);
			break;
		default:
			break;
		}

		result_type = new_type_rec(&current->type_arena, BOOL_TYPE);
		break;
	}

	case TOKEN_PLUS: {
		emit_word(OP_ADD);

		if (either_any) {
			result_type = new_type_rec(&current->type_arena,
						   ANY_TYPE);
			break;
		}

		const bool left_str = left_type->base_type == STRING_TYPE;
		const bool right_str = right_type->base_type == STRING_TYPE;

		if (left_str || right_str) {
			if (!left_str || !right_str) {
				compiler_panic(
					&parser,
					"Cannot use '+' between String and "
					"non-String.",
					TYPE);
			}
			result_type = new_type_rec(&current->type_arena,
						   STRING_TYPE);
			break;
		}

		const bool left_num = left_type->base_type == INT_TYPE ||
				      left_type->base_type == FLOAT_TYPE;
		const bool right_num = right_type->base_type == INT_TYPE ||
				       right_type->base_type == FLOAT_TYPE;

		if (!left_num || !right_num) {
			compiler_panic(&parser,
				       "'+' requires numeric or String "
				       "operands.",
				       TYPE);
		}

		// Int + Float => Float
		if (left_type->base_type == FLOAT_TYPE ||
		    right_type->base_type == FLOAT_TYPE) {
			result_type = new_type_rec(&current->type_arena,
						   FLOAT_TYPE);
		} else {
			result_type = new_type_rec(&current->type_arena,
						   INT_TYPE);
		}
		break;
	}

	case TOKEN_MINUS:
	case TOKEN_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type ==
						      INT_TYPE ||
					      left_type->base_type ==
						      FLOAT_TYPE;
			const bool right_num = right_type->base_type ==
						       INT_TYPE ||
					       right_type->base_type ==
						       FLOAT_TYPE;

			if (!left_num || !right_num) {
				compiler_panic(&parser,
					       operatorType == TOKEN_MINUS
						       ? "'-' requires numeric "
							 "operands."
						       : "'*' requires numeric "
							 "operands.",
					       TYPE);
			}
		}

		emit_word(operatorType == TOKEN_MINUS ? OP_SUBTRACT
						      : OP_MULTIPLY);

		if (either_any) {
			result_type = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		} else if (left_type->base_type == FLOAT_TYPE ||
			   right_type->base_type == FLOAT_TYPE) {
			result_type = new_type_rec(&current->type_arena,
						   FLOAT_TYPE);
		} else {
			result_type = new_type_rec(&current->type_arena,
						   INT_TYPE);
		}
		break;
	}

	case TOKEN_SLASH: {
		if (!either_any) {
			const bool left_num = left_type->base_type ==
						      INT_TYPE ||
					      left_type->base_type ==
						      FLOAT_TYPE;
			const bool right_num = right_type->base_type ==
						       INT_TYPE ||
					       right_type->base_type ==
						       FLOAT_TYPE;
			if (!left_num || !right_num) {
				compiler_panic(&parser,
					       "'/' requires numeric operands.",
					       TYPE);
			}
		}
		emit_word(OP_DIVIDE);
		result_type = new_type_rec(&current->type_arena, FLOAT_TYPE);
		break;
	}

	case TOKEN_PERCENT:
	case TOKEN_BACKSLASH: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE ||
			    right_type->base_type != INT_TYPE) {
				compiler_panic(
					&parser,
					operatorType == TOKEN_PERCENT
						? "'%' requires Int operands."
						: "'\\' requires Int operands.",
					TYPE);
			}
		}
		emit_word(operatorType == TOKEN_PERCENT ? OP_MODULUS
							: OP_INT_DIVIDE);
		result_type = new_type_rec(&current->type_arena, INT_TYPE);
		break;
	}

	case TOKEN_RIGHT_SHIFT:
	case TOKEN_LEFT_SHIFT:
	case TOKEN_AMPERSAND:
	case TOKEN_CARET:
	case TOKEN_PIPE: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE ||
			    right_type->base_type != INT_TYPE) {
				compiler_panic(&parser,
					       "Bitwise operators require Int "
					       "operands.",
					       TYPE);
			}
		}

		switch (operatorType) {
		case TOKEN_RIGHT_SHIFT:
			emit_word(OP_RIGHT_SHIFT);
			break;
		case TOKEN_LEFT_SHIFT:
			emit_word(OP_LEFT_SHIFT);
			break;
		case TOKEN_AMPERSAND:
			emit_word(OP_BITWISE_AND);
			break;
		case TOKEN_CARET:
			emit_word(OP_BITWISE_XOR);
			break;
		case TOKEN_PIPE:
			emit_word(OP_BITWISE_OR);
			break;
		default:
			break;
		}

		result_type = new_type_rec(&current->type_arena, INT_TYPE);
		break;
	}

	case TOKEN_STAR_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type ==
						      INT_TYPE ||
					      left_type->base_type ==
						      FLOAT_TYPE;
			const bool right_num = right_type->base_type ==
						       INT_TYPE ||
					       right_type->base_type ==
						       FLOAT_TYPE;
			if (!left_num || !right_num) {
				compiler_panic(
					&parser,
					"'**' requires numeric operands.",
					TYPE);
			}
		}
		emit_word(OP_POWER);
		result_type = new_type_rec(&current->type_arena, FLOAT_TYPE);
		break;
	}

	default:
		// Should be unreachable.
		result_type = new_type_rec(&current->type_arena, ANY_TYPE);
		break;
	}

	push_type_record(result_type);
}

static void infix_call(bool can_assign)
{
	(void)can_assign;

	// The callee's TypeRecord was pushed by whichever expression produced
	// the function value (named_variable, dot GET_PROPERTY, etc.). Pop it
	// before compiling arguments so their types sit on top of the stack.
	TypeRecord *func_type = pop_type_record();

	// Compile argument list, collecting each arg's type as we go.
	uint16_t arg_count = 0;
	TypeRecord *arg_types[UINT8_COUNT];

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			if (arg_count == UINT16_MAX) {
				compiler_panic(&parser,
					       "Cannot have more than 65535 "
					       "arguments.",
					       ARGUMENT_EXTENT);
			}
			expression();
			arg_types[arg_count] = pop_type_record();
			arg_count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");

	emit_words(OP_CALL, arg_count);

	// Type-check only when the callee type is statically known.
	if (func_type && func_type->base_type == FUNCTION_TYPE) {
		int expected_count = func_type->as.function_type.arg_count;

		if ((int)arg_count != expected_count) {
			compiler_panicf(&parser, ARGUMENT_MISMATCH,
					"Expected %d argument(s), got %d.",
					expected_count, (int)arg_count);
		} else {
			for (int i = 0; i < (int)arg_count; i++) {
				TypeRecord *expected =
					func_type->as.function_type
						.arg_types[i];
				TypeRecord *got = arg_types[i];
				if (expected && got &&
				    expected->base_type != ANY_TYPE &&
				    got->base_type != ANY_TYPE) {
					if (!types_compatible(expected, got)) {
						char exp_name[64], got_name[64];
						type_mask_name(
							expected->base_type,
							exp_name,
							sizeof(exp_name));
						type_mask_name(
							got->base_type,
							got_name,
							sizeof(got_name));
						compiler_panicf(
							&parser, TYPE,
							"Argument %d type "
							"mismatch: expected "
							"'%s', got '%s'.",
							i + 1, exp_name,
							got_name);
					}
				}
			}
		}

		TypeRecord *ret = func_type->as.function_type.return_type;
		push_type_record(
			ret ? ret
			    : new_type_rec(&current->type_arena, ANY_TYPE));
	} else {
		// Unknown callee type — allow through without checking.
		push_type_record(new_type_rec(&current->type_arena, ANY_TYPE));
	}
}

static void literal(bool can_assign)
{
	(void)can_assign;
	switch (parser.previous.type) {
	case TOKEN_FALSE:
		emit_word(OP_FALSE);
		push_type_record(new_type_rec(&current->type_arena, BOOL_TYPE));
		break;
	case TOKEN_NIL:
		emit_word(OP_NIL);
		push_type_record(new_type_rec(&current->type_arena, NIL_TYPE));
		break;
	case TOKEN_TRUE:
		emit_word(OP_TRUE);
		push_type_record(new_type_rec(&current->type_arena, BOOL_TYPE));
		break;
	default:
		return; // unreachable
	}
}

static void dot(const bool can_assign)
{
	consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
	const uint16_t name_constant = identifier_constant(&parser.previous);
	const Token method_name_token = parser.previous;

	if (name_constant >= UINT16_MAX) {
		compiler_panic(&parser, "Too many constants.", SYNTAX);
	}

	// The object's TypeRecord was pushed by the prefix expression (e.g.
	// named_variable). Pop it now so we can use it for lookups, and so it
	// doesn't leak from the type stack.
	TypeRecord *object_type = pop_type_record();
	if (!object_type)
		object_type = new_type_rec(&current->type_arena, ANY_TYPE);

	if (can_assign && match(TOKEN_EQUAL)) {
		// ── OP_SET_PROPERTY ──────────────────────────────────────────
		expression();
		TypeRecord *value_type = pop_type_record();

		if (object_type->base_type == STRUCT_TYPE && value_type) {
			TypeTable *field_types =
				object_type->as.struct_type.field_types;
			ObjectString *field_name = copy_string(
				current->owner, method_name_token.start,
				method_name_token.length);
			TypeRecord *field_type = NULL;
			if (type_table_get(field_types, field_name,
					   &field_type)) {
				if (field_type &&
				    field_type->base_type != ANY_TYPE &&
				    value_type->base_type != ANY_TYPE &&
				    !types_compatible(field_type, value_type)) {
					char exp[64], got[64], msg[160];
					type_mask_name(field_type->base_type,
						       exp, sizeof(exp));
					type_mask_name(value_type->base_type,
						       got, sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Cannot assign '%s' to field "
						 "of type '%s'.",
						 got, exp);
					compiler_panic(&parser, msg, TYPE);
				}
			} else {
				char msg[160];
				snprintf(msg, sizeof(msg),
					 "Struct has no field '%.*s'.",
					 (int)method_name_token.length,
					 method_name_token.start);
				compiler_panic(&parser, msg, NAME);
			}
		}

		emit_words(OP_SET_PROPERTY, name_constant);
		// Push NIL_TYPE to maintain type-stack invariant #1.
		push_type_record(new_type_rec(&current->type_arena, NIL_TYPE));
		return;
	}

	if (match(TOKEN_LEFT_PAREN)) {
		// ── OP_INVOKE ────────────────────────────────────────────────
		uint16_t arg_count = 0;
		TypeRecord *arg_types[UINT8_COUNT];

		if (!check(TOKEN_RIGHT_PAREN)) {
			do {
				expression();
				arg_types[arg_count] = pop_type_record();
				arg_count++;
			} while (match(TOKEN_COMMA));
		}
		consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

		emit_words(OP_INVOKE, name_constant);
		emit_word(arg_count);

		// Look up the method's TypeRecord.
		TypeRecord *method_type = NULL;

		if (object_type->base_type == STRUCT_TYPE) {
			// Struct method: look for a function-typed field.
			TypeTable *field_types =
				object_type->as.struct_type.field_types;
			ObjectString *field_name = copy_string(
				current->owner, method_name_token.start,
				method_name_token.length);
			type_table_get(field_types, field_name, &method_type);

		} else if (object_type->base_type != ANY_TYPE) {
			// Stdlib method: look up "TypeName.methodName" in the
			// compiler's type_table (seeded from
			// vm->stdlib_type_table when that infrastructure
			// exists; degrades gracefully to ANY_TYPE when not).
			char type_name[64];
			type_mask_name(object_type->base_type, type_name,
				       sizeof(type_name));
			char key[128];
			snprintf(key, sizeof(key), "%s.%.*s", type_name,
				 (int)method_name_token.length,
				 method_name_token.start);

			ObjectString *key_str = copy_string(current->owner, key,
							    strlen(key));
			Compiler *comp = current;
			while (comp != NULL) {
				if (type_table_get(&comp->type_table, key_str,
						   &method_type))
					break;
				comp = comp->enclosing;
			}
		}

		// Validate arg types and push the return type.
		if (method_type && method_type->base_type == FUNCTION_TYPE) {
			int expected_count =
				method_type->as.function_type.arg_count;
			// OP_INVOKE passes the receiver (self) as arg[0].
			// The Callable's declared arity includes self, so the
			// first param_type slot is the receiver. User-supplied
			// args start at index 1 (param_offset).
			int param_offset = 1;
			int user_params = expected_count - param_offset;

			if (user_params < 0)
				user_params = 0;

			if ((int)arg_count != user_params &&
			    expected_count != 0) {
				char msg[128];
				snprintf(msg, sizeof(msg),
					 "Method expects %d argument(s), "
					 "got %d.",
					 user_params, (int)arg_count);
				compiler_panic(&parser, msg, ARGUMENT_MISMATCH);
			} else {
				for (int i = 0; i < (int)arg_count; i++) {
					TypeRecord *expected =
						method_type->as.function_type
							.arg_types
								[i +
								 param_offset];
					TypeRecord *got = arg_types[i];
					if (expected && got &&
					    expected->base_type != ANY_TYPE &&
					    got->base_type != ANY_TYPE) {
						if (!types_compatible(expected,
								      got)) {
							char exp_name[64],
								got_name[64],
								msg[160];
							type_mask_name(
								expected->base_type,
								exp_name,
								sizeof(exp_name));
							type_mask_name(
								got->base_type,
								got_name,
								sizeof(got_name));
							snprintf(msg,
								 sizeof(msg),
								 "Argument %d "
								 "type "
								 "mismatch: "
								 "expected "
								 "'%s', "
								 "got '%s'.",
								 i + 1,
								 exp_name,
								 got_name);
							compiler_panic(&parser,
								       msg,
								       TYPE);
						}
					}
				}
			}

			TypeRecord *ret =
				method_type->as.function_type.return_type;
			push_type_record(
				ret ? ret
				    : new_type_rec(&current->type_arena,
						   ANY_TYPE));
		} else {
			push_type_record(
				new_type_rec(&current->type_arena, ANY_TYPE));
		}
		return;
	}

	// ── OP_GET_PROPERTY ──────────────────────────────────────────────────
	emit_words(OP_GET_PROPERTY, name_constant);

	TypeRecord *result_type = NULL;
	if (object_type->base_type == STRUCT_TYPE) {
		TypeTable *field_types =
			object_type->as.struct_type.field_types;
		ObjectString *field_name =
			copy_string(current->owner, method_name_token.start,
				    method_name_token.length);
		TypeRecord *field_type = NULL;
		if (type_table_get(field_types, field_name, &field_type)) {
			result_type = field_type;
		} else {
			char msg[160];
			snprintf(msg, sizeof(msg),
				 "Struct has no field '%.*s'.",
				 (int)method_name_token.length,
				 method_name_token.start);
			compiler_panic(&parser, msg, NAME);
		}
	}

	push_type_record(
		result_type ? result_type
			    : new_type_rec(&current->type_arena, ANY_TYPE));
}

void struct_instance(const bool can_assign)
{
	consume(TOKEN_IDENTIFIER,
		"Expected struct name to start initialization.");

	// Capture the struct name token before named_variable advances
	// the parser.
	const Token struct_name_token = parser.previous;
	(void)struct_name_token; // used for error messages below

	named_variable(parser.previous, can_assign);

	// named_variable pushes the type of the name it resolved — for a
	// struct this should be a STRUCT_TYPE record. Pop it now so we can
	// use its field table during initializer validation.
	TypeRecord *struct_type = pop_type_record();

	// Validate that the name actually refers to a struct. ANY_TYPE means
	// the pre-scan hasn't run yet or the name came from an import, so we
	// allow it through and skip field-level checking.
	const bool type_known = struct_type &&
				struct_type->base_type != ANY_TYPE;
	if (type_known && struct_type->base_type != STRUCT_TYPE) {
		compiler_panic(&parser, "'new' requires a struct type name.",
			       TYPE);
		// Push ANY_TYPE so the stack stays balanced.
		push_type_record(new_type_rec(&current->type_arena, ANY_TYPE));
		return;
	}

	if (!match(TOKEN_LEFT_BRACE)) {
		compiler_panic(&parser,
			       "Expected '{' to start struct instance.",
			       SYNTAX);
		push_type_record(new_type_rec(&current->type_arena, ANY_TYPE));
		return;
	}

	// Track which field names the programmer supplied so we can report
	// missing required fields after the loop. We use a small fixed
	// bitfield keyed by field index (structs are limited to UINT16_MAX
	// fields, but in practice they're small — use a heap array of bools
	// for safety).
	int declared_field_count =
		type_known ? struct_type->as.struct_type.field_count : 0;
	bool *field_seen = NULL;
	if (declared_field_count > 0) {
		field_seen = ALLOCATE(current->owner, bool,
				      declared_field_count);
		for (int i = 0; i < declared_field_count; i++) {
			field_seen[i] = false;
		}
	}

	uint16_t fieldCount = 0;
	emit_word(OP_STRUCT_INSTANCE_START);

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount == UINT16_MAX) {
				compiler_panic(&parser,
					       "Too many fields in struct "
					       "initializer.",
					       SYNTAX);
				if (field_seen)
					FREE_ARRAY(current->owner, bool,
						   field_seen,
						   declared_field_count);
				push_type_record(
					new_type_rec(&current->type_arena,
						     ANY_TYPE));
				return;
			}

			consume(TOKEN_IDENTIFIER, "Expected field name.");
			ObjectString *fieldName = copy_string(
				current->owner, parser.previous.start,
				parser.previous.length);

			consume(TOKEN_COLON,
				"Expected ':' after struct field name.");

			expression();
			TypeRecord *value_type = pop_type_record();

			if (type_known) {
				TypeTable *field_types =
					struct_type->as.struct_type.field_types;
				ObjectStruct *definition =
					struct_type->as.struct_type.definition;

				// Check the field exists on the struct.
				Value field_index_val;
				if (!table_get(&definition->fields, fieldName,
					       &field_index_val)) {
					char msg[160];
					snprintf(msg, sizeof(msg),
						 "Struct has no field '%.*s'.",
						 (int)fieldName->length,
						 fieldName->chars);
					compiler_panic(&parser, msg, NAME);
				} else {
					// Mark as seen.
					const int field_index = (int)AS_INT(
						field_index_val);
					if (field_index >= 0 &&
					    field_index <
						    declared_field_count) {
						if (field_seen[field_index]) {
							char msg[160];
							snprintf(
								msg,
								sizeof(msg),
								"Field '%.*s' "
								"specified "
								"more "
								"than once.",
								(int)fieldName
									->length,
								fieldName
									->chars);
							compiler_panic(&parser,
								       msg,
								       NAME);
						}
						field_seen[field_index] = true;
					}

					// Validate the value type against the
					// declared field type.
					TypeRecord *declared_field_type = NULL;
					type_table_get(field_types, fieldName,
						       &declared_field_type);

					if (declared_field_type && value_type &&
					    declared_field_type->base_type !=
						    ANY_TYPE &&
					    value_type->base_type != ANY_TYPE) {
						if (!types_compatible(
							    declared_field_type,
							    value_type)) {
							char expected[64],
								got[64],
								msg[200];
							type_mask_name(
								declared_field_type
									->base_type,
								expected,
								sizeof(expected));
							type_mask_name(
								value_type
									->base_type,
								got,
								sizeof(got));
							snprintf(
								msg,
								sizeof(msg),
								"Field '%.*s' "
								"expects type "
								"'%s', got "
								"'%s'.",
								(int)fieldName
									->length,
								fieldName
									->chars,
								expected, got);
							compiler_panic(&parser,
								       msg,
								       TYPE);
						}
					}
				}
			}

			const uint16_t fieldNameConstant = make_constant(
				OBJECT_VAL(fieldName));
			emit_words(OP_STRUCT_NAMED_FIELD, fieldNameConstant);
			fieldCount++;
		} while (match(TOKEN_COMMA));
	}

	// Check for missing fields — every declared field must be provided.
	if (type_known && field_seen) {
		ObjectStruct *definition =
			struct_type->as.struct_type.definition;

		for (int i = 0; i < declared_field_count; i++) {
			if (!field_seen[i]) {
				// Find the field name for the error message by
				// scanning the struct's fields table.
				// Fields table maps name -> INT index, so walk
				// it to find index i.
				const char *missing = "<unknown>";
				int missing_len = 0;
				for (int e = 0; e < definition->fields.capacity;
				     e++) {
					Entry *entry =
						&definition->fields.entries[e];
					if (entry->key != NULL &&
					    AS_INT(entry->value) == i) {
						missing = entry->key->chars;
						missing_len =
							(int)entry->key->length;
						break;
					}
				}
				char msg[160];
				snprintf(msg, sizeof(msg),
					 "Missing required field '%.*s' in "
					 "struct initializer.",
					 missing_len, missing);
				compiler_panic(&parser, msg, NAME);
				break; // Report one missing field at a time.
			}
		}
		FREE_ARRAY(current->owner, bool, field_seen,
			   declared_field_count);
	}

	if (fieldCount != 0) {
		consume(TOKEN_RIGHT_BRACE,
			"Expected '}' after struct field list.");
	}

	emit_word(OP_STRUCT_INSTANCE_END);

	// Push the struct's type so callers know what new Foo{} produced.
	push_type_record(
		struct_type ? struct_type
			    : new_type_rec(&current->type_arena, ANY_TYPE));
}
/**
 * Parses an expression. Entry point for expression parsing.
 */
static void expression(void)
{
	parse_precedence(PREC_ASSIGNMENT);
}

static void variable(const bool can_assign)
{
	named_variable(parser.previous, can_assign);
}

static void block(void)
{
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(const FunctionType type)
{
	Compiler compiler;
	init_compiler(&compiler, type, current->owner);
	begin_scope();

	// Collect param types in a heap array so we can copy them to the
	// enclosing arena after end_compiler() restores current.
	int param_capacity = 4;
	int param_count = 0;
	TypeRecord **param_types = malloc(sizeof(TypeRecord *) *
					  param_capacity);
	if (!param_types) {
		compiler_panic(&parser, "Memory allocation failed.", MEMORY);
		return;
	}

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > UINT8_MAX) {
				compiler_panic(
					&parser,
					"Functions cannot have more than "
					"255 arguments.",
					ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(
				"Expected parameter name.");

			// Optional type annotation: fn foo(x: Int) ...
			TypeRecord *param_type = NULL;
			if (match(TOKEN_COLON)) {
				param_type = parse_type_record();
			} else {
				param_type = new_type_rec(&current->type_arena,
							  ANY_TYPE);
			}

			// Stamp the type onto the just-added local slot.
			// parse_variable -> declare_variable -> add_local
			// already incremented local_count, so -1 is correct.
			current->locals[current->local_count - 1].type =
				param_type;

			if (param_count == param_capacity) {
				param_capacity *= 2;
				TypeRecord **grown = realloc(
					param_types,
					sizeof(TypeRecord *) * param_capacity);
				if (!grown) {
					free(param_types);
					compiler_panic(
						&parser,
						"Memory allocation failed.",
						MEMORY);
					return;
				}
				param_types = grown;
			}
			param_types[param_count++] = param_type;

			define_variable(constant);
			// TODO: consume type annotation
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	// Return type annotation: fn foo() -> Int { ... }
	// Defaults to ANY_TYPE so return_statement skips validation when
	// the programmer omits the annotation.
	TypeRecord *return_type = NULL;
	if (match(TOKEN_ARROW)) {
		return_type = parse_type_record();
	} else {
		return_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}
	// Set on the inner compiler so return_statement can validate
	// return expressions against it.
	current->return_type = return_type;

	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	// end_compiler() restores current to the enclosing compiler.
	// The local variable `compiler` is still on the stack and valid
	// to read — use it to copy types into the enclosing arena.
	ObjectFunction *fn = end_compiler();
	emit_words(OP_CLOSURE, make_constant(OBJECT_VAL(fn)));

	for (int i = 0; i < fn->upvalue_count; i++) {
		emit_word(compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler.upvalues[i].index);
	}

	// Copy param types and return type into the enclosing compiler's arena
	// before `compiler` (and its type_arena) goes out of scope.
	TypeRecord **enclosing_param_types = NULL;
	if (param_count > 0) {
		enclosing_param_types = malloc(sizeof(TypeRecord *) *
					       param_count);
		if (!enclosing_param_types) {
			free(param_types);
			compiler_panic(&parser, "Memory allocation failed.",
				       MEMORY);
			return;
		}
		for (int i = 0; i < param_count; i++) {
			enclosing_param_types[i] = copy_type_rec_to_arena(
				&current->type_arena, param_types[i]);
		}
	}

	TypeRecord *enclosing_return_type =
		copy_type_rec_to_arena(&current->type_arena, return_type);

	TypeRecord *func_type = new_function_type_rec(&current->type_arena,
						      enclosing_param_types,
						      param_count,
						      enclosing_return_type);
	free(param_types);

	// Push so fn_declaration (or any expression context) can consume it.
	push_type_record(func_type);
}

static void fn_declaration(void)
{
	const uint16_t global = parse_variable("Expected function name.");

	// parser.previous is the function name identifier right now.
	// Capture it before function() calls consume() and advances the token.
	const Token fn_name_token = parser.previous;

	// Save local index in case this is a local-scope function.
	const int local_index = (current->scope_depth > 0)
					? current->local_count - 1
					: -1;

	mark_initialized();
	function(TYPE_FUNCTION);

	// function() always pushes a TypeRecord — pop it and register it.
	TypeRecord *fn_type = pop_type_record();

	if (fn_type) {
		if (current->scope_depth == 0) {
			// Global function: enter into the type table so
			// named_variable can look it up from any scope.
			ObjectString *name_str =
				copy_string(current->owner, fn_name_token.start,
					    fn_name_token.length);
			type_table_set(&current->type_table, name_str, fn_type);
		} else {
			// Local function: stamp the type directly onto the
			// local slot that declare_variable already reserved.
			current->locals[local_index].type = fn_type;
		}
	}

	define_variable(global);
}

static void anonymous_function(bool can_assign)
{
	(void)can_assign;
	Compiler compiler;
	init_compiler(&compiler, TYPE_ANONYMOUS, current->owner);

	int param_capacity = 4;
	int param_count = 0;
	TypeRecord **param_types = malloc(sizeof(TypeRecord *) *
					  param_capacity);
	if (!param_types) {
		compiler_panic(&parser, "Memory allocation failed.", MEMORY);
		return;
	}

	begin_scope();
	consume(TOKEN_LEFT_PAREN, "Expected '(' to start argument list.");

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > UINT8_MAX) {
				compiler_panic(
					&parser,
					"Functions cannot have more than "
					"255 arguments.",
					ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(
				"Expected parameter name.");

			// Anonymous functions require type annotations on all
			// parameters — there is no inference fallback.
			consume(TOKEN_COLON,
				"Expected ':' after parameter name.");
			TypeRecord *param_type = parse_type_record();

			// Store on the local slot that parse_variable just
			// created via declare_variable -> add_local.
			current->locals[current->local_count - 1].type =
				param_type;

			if (param_count == param_capacity) {
				param_capacity *= 2;
				TypeRecord **grown = realloc(
					param_types,
					sizeof(TypeRecord *) * param_capacity);
				if (!grown) {
					free(param_types);
					compiler_panic(
						&parser,
						"Memory allocation failed.",
						MEMORY);
					return;
				}
				param_types = grown;
			}
			param_types[param_count++] = param_type;

			define_variable(constant);
		} while (match(TOKEN_COMMA));
	}

	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
	consume(TOKEN_ARROW, "Expected '->' after argument list.");
	TypeRecord *annotated_return_type = parse_type_record();

	// Set on the inner compiler so return_statement validates correctly.
	current->return_type = annotated_return_type;

	consume(TOKEN_LEFT_BRACE, "Expected '{' before function body.");
	block();

	ObjectFunction *fn = end_compiler();
	const uint16_t constantIndex = make_constant(OBJECT_VAL(fn));
	emit_words(OP_ANON_FUNCTION, constantIndex);

	for (int i = 0; i < fn->upvalue_count; i++) {
		emit_word(compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler.upvalues[i].index);
	}

	// Copy types out of the inner compiler's stack-allocated arena
	// before it goes out of scope.
	TypeRecord **enclosing_param_types = NULL;
	if (param_count > 0) {
		enclosing_param_types = malloc(sizeof(TypeRecord *) *
					       param_count);
		if (!enclosing_param_types) {
			free(param_types);
			compiler_panic(&parser, "Memory allocation failed.",
				       MEMORY);
			return;
		}
		for (int i = 0; i < param_count; i++) {
			enclosing_param_types[i] = copy_type_rec_to_arena(
				&current->type_arena, param_types[i]);
		}
	}

	TypeRecord *enclosing_return_type = copy_type_rec_to_arena(
		&current->type_arena, annotated_return_type);

	TypeRecord *func_type = new_function_type_rec(&current->type_arena,
						      enclosing_param_types,
						      param_count,
						      enclosing_return_type);
	free(param_types);
	push_type_record(func_type);
}

static void array_literal(bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	TypeRecord *element_type = NULL;

	if (!match(TOKEN_RIGHT_SQUARE)) {
		do {
			expression();

			TypeRecord *expr_type = pop_type_record();

			if (elementCount == 0) {
				// First element establishes the array's type.
				element_type = expr_type;
			} else if (element_type && expr_type) {
				// Subsequent elements must be compatible.
				// ANY_TYPE on either side means we widen to
				// Any.
				if (element_type->base_type == ANY_TYPE ||
				    expr_type->base_type == ANY_TYPE) {
					element_type = new_type_rec(
						&current->type_arena, ANY_TYPE);
				} else if (!types_compatible(element_type,
							     expr_type)) {
					char expected[64], got[64], msg[160];
					type_mask_name(element_type->base_type,
						       expected,
						       sizeof(expected));
					type_mask_name(expr_type->base_type,
						       got, sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Inconsistent element types "
						 "in array literal: expected "
						 "'%s', got '%s'.",
						 expected, got);
					compiler_panic(&parser, msg, TYPE);
				}
			}

			if (elementCount >= UINT16_MAX) {
				compiler_panic(&parser,
					       "Too many elements in array "
					       "literal.",
					       COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_SQUARE,
			"Expected ']' after array elements.");
	}

	if (!element_type) {
		// Empty array literal — element type is unknown.
		element_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}

	emit_word(OP_ARRAY);
	emit_word(elementCount);

	TypeRecord *array_type = new_array_type_rec(&current->type_arena,
						    element_type);
	push_type_record(array_type);
}

static void table_literal(bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	TypeRecord *table_key_type = NULL;
	TypeRecord *table_value_type = NULL;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			TypeRecord *key_type = pop_type_record();
			consume(TOKEN_COLON, "Expected ':' after table key.");
			expression();
			TypeRecord *value_type = pop_type_record();

			if (!table_key_type) {
				table_key_type = key_type;
			} else if (table_key_type->base_type != ANY_TYPE &&
				   key_type &&
				   key_type->base_type != ANY_TYPE) {
				// Keys must be exactly equal — a table can't
				// have mixed key types at runtime.
				if (!types_equal(table_key_type, key_type)) {
					char expected[64], got[64], msg[160];
					type_mask_name(
						table_key_type->base_type,
						expected, sizeof(expected));
					type_mask_name(key_type->base_type, got,
						       sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Inconsistent key types in "
						 "table literal: expected "
						 "'%s', got '%s'.",
						 expected, got);
					compiler_panic(&parser, msg, TYPE);
				}
			}

			if (!table_value_type) {
				table_value_type = value_type;
			} else if (table_value_type->base_type != ANY_TYPE &&
				   value_type &&
				   value_type->base_type != ANY_TYPE) {
				// Values use compatible rather than equal so
				// Int and Float can coexist — both widen to
				// Float.
				if (!types_compatible(table_value_type,
						      value_type)) {
					char expected[64], got[64], msg[160];
					type_mask_name(
						table_value_type->base_type,
						expected, sizeof(expected));
					type_mask_name(value_type->base_type,
						       got, sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Inconsistent value types in "
						 "table literal: expected "
						 "'%s', got '%s'.",
						 expected, got);
					compiler_panic(&parser, msg, TYPE);
				}
				// Widen to Float if we see a mix of Int/Float.
				if ((table_value_type->base_type == INT_TYPE &&
				     value_type->base_type == FLOAT_TYPE) ||
				    (table_value_type->base_type ==
					     FLOAT_TYPE &&
				     value_type->base_type == INT_TYPE)) {
					table_value_type = new_type_rec(
						&current->type_arena,
						FLOAT_TYPE);
				}
			}

			if (elementCount >= UINT16_MAX) {
				compiler_panic(
					&parser,
					"Too many elements in table literal.",
					COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_BRACE,
			"Expected '}' after table elements.");
	}

	if (!table_key_type) {
		table_key_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}
	if (!table_value_type) {
		table_value_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}

	// Emit bytecode first, then push the type — the type record is
	// compiler metadata, not part of the instruction stream.
	emit_word(OP_TABLE);
	emit_word(elementCount);

	TypeRecord *table_type = new_table_type_rec(&current->type_arena,
						    table_key_type,
						    table_value_type);
	push_type_record(table_type);
}

static void collection_index(const bool can_assign)
{
	// The collection's type is on the top of the type stack — pop it
	// now so we know what element type to push after the index op, and
	// so it doesn't leak.
	TypeRecord *collection_type = pop_type_record();

	expression();
	TypeRecord *index_type = pop_type_record();

	// Validate the index type against the collection type.
	if (collection_type && index_type &&
	    index_type->base_type != ANY_TYPE &&
	    collection_type->base_type != ANY_TYPE) {
		if (collection_type->base_type == ARRAY_TYPE) {
			if (index_type->base_type != INT_TYPE) {
				compiler_panic(
					&parser,
					"Array index must be of type 'Int'.",
					TYPE);
			}
		} else if (collection_type->base_type == TABLE_TYPE) {
			TypeRecord *key_type =
				collection_type->as.table_type.key_type;
			if (key_type && key_type->base_type != ANY_TYPE &&
			    !types_compatible(key_type, index_type)) {
				char expected[64], got[64], msg[160];
				type_mask_name(key_type->base_type, expected,
					       sizeof(expected));
				type_mask_name(index_type->base_type, got,
					       sizeof(got));
				snprintf(msg, sizeof(msg),
					 "Table key type mismatch: expected "
					 "'%s', got '%s'.",
					 expected, got);
				compiler_panic(&parser, msg, TYPE);
			}
		}
	}

	consume(TOKEN_RIGHT_SQUARE, "Expected ']' after index.");

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		TypeRecord *value_type = pop_type_record();

		// On a set, validate the value type against the collection's
		// element/value type if known.
		if (collection_type && value_type &&
		    value_type->base_type != ANY_TYPE &&
		    collection_type->base_type != ANY_TYPE) {
			TypeRecord *expected_value = NULL;
			if (collection_type->base_type == ARRAY_TYPE) {
				expected_value = collection_type->as.array_type
							 .element_type;
			} else if (collection_type->base_type == TABLE_TYPE) {
				expected_value = collection_type->as.table_type
							 .value_type;
			}
			if (expected_value &&
			    expected_value->base_type != ANY_TYPE &&
			    !types_compatible(expected_value, value_type)) {
				char expected[64], got[64], msg[160];
				type_mask_name(expected_value->base_type,
					       expected, sizeof(expected));
				type_mask_name(value_type->base_type, got,
					       sizeof(got));
				snprintf(msg, sizeof(msg),
					 "Cannot assign '%s' to collection "
					 "of element type '%s'.",
					 got, expected);
				compiler_panic(&parser, msg, TYPE);
			}
		}

		emit_word(OP_SET_COLLECTION);
		// A set expression leaves no useful value — push Nil.
		push_type_record(new_type_rec(&current->type_arena, NIL_TYPE));
	} else {
		emit_word(OP_GET_COLLECTION);

		// Push the element type so downstream expressions know what
		// they received.
		TypeRecord *result_type = NULL;
		if (collection_type) {
			if (collection_type->base_type == ARRAY_TYPE) {
				result_type = collection_type->as.array_type
						      .element_type;
			} else if (collection_type->base_type == TABLE_TYPE) {
				result_type = collection_type->as.table_type
						      .value_type;
			}
		}
		if (!result_type) {
			result_type = new_type_rec(&current->type_arena,
						   ANY_TYPE);
		}
		push_type_record(result_type);
	}
}

static void var_declaration(void)
{
	const uint16_t global = parse_variable("Expected Variable Name.");

	const Token var_name = parser.previous;

	TypeRecord *annotated_type = NULL;
	if (match(TOKEN_COLON)) {
		annotated_type = parse_type_record();
	}

	TypeRecord *value_type = NULL;
	if (match(TOKEN_EQUAL)) {
		expression();
		value_type = pop_type_record();

		// Only validate if both sides are known (annotated_type is NULL
		// when no annotation was given; types_compatible returns false
		// for NULL, which would cause a NULL dereference below).
		if (annotated_type && value_type &&
		    annotated_type->base_type != ANY_TYPE &&
		    value_type->base_type != ANY_TYPE &&
		    !types_compatible(annotated_type, value_type)) {
			char expected[64], got[64];
			type_mask_name(annotated_type->base_type, expected,
				       sizeof(expected));
			type_mask_name(value_type->base_type, got, sizeof(got));
			compiler_panicf(&parser, TYPE,
					"Type mismatch in variable "
					"declaration: expected '%s', got '%s'.",
					expected, got);
		}
	} else {
		// No initializer — implicit nil.
		// If an annotation was given, nil is only acceptable for
		// Nil-typed or Any-typed variables.
		if (annotated_type && annotated_type->base_type != NIL_TYPE &&
		    annotated_type->base_type != ANY_TYPE) {
			compiler_panic(&parser,
				       "Variable with non-Nil type must be "
				       "initialized.",
				       TYPE);
		}
		emit_word(OP_NIL);
		value_type = new_type_rec(&current->type_arena, NIL_TYPE);
	}

	TypeRecord *resolved_type = annotated_type ? annotated_type
						   : value_type;
	if (!resolved_type) {
		resolved_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}

	consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

	if (current->scope_depth > 0) {
		// Local: stamp the type onto the slot before marking it live.
		// local_count - 1 is the slot add_local just created.
		current->locals[current->local_count - 1].type = resolved_type;
	} else {
		// Global: register name → type in this compiler's type table
		// so named_variable can look it up on future reads/writes.
		ObjectString *name_str = copy_string(current->owner,
						     var_name.start,
						     var_name.length);
		type_table_set(&current->type_table, name_str, resolved_type);
	}
	define_variable(global);
}

static void expression_statement(void)
{
	expression();
	pop_type_record(); // discard — value is unused
	consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
	emit_word(OP_POP);
}

static void while_statement(void)
{
	begin_scope();
	const int loopStart = current_chunk()->count;
	push_loop_context(LOOP_WHILE, loopStart);

	expression();

	// Condition must be Bool
	TypeRecord *condition_type = pop_type_record();
	if (condition_type && condition_type->base_type != BOOL_TYPE &&
	    condition_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "'while' condition must be of type 'Bool'.",
			       TYPE);
	}

	const int exitJump = emit_jump(OP_JUMP_IF_FALSE);
	emit_word(OP_POP);
	statement();
	emit_loop(loopStart);
	patch_jump(exitJump);
	emit_word(OP_POP);

	pop_loop_context();
	end_scope();
}

static void for_statement(void)
{
	begin_scope();

	if (match(TOKEN_SEMICOLON)) {
		// no initializer
	} else if (match(TOKEN_LET)) {
		var_declaration();
	} else {
		expression_statement();
	}

	int loopStart = current_chunk()->count;
	int exitJump = -1;

	if (!match(TOKEN_SEMICOLON)) {
		expression();

		// Condition must be Bool
		TypeRecord *condition_type = pop_type_record();
		if (condition_type && condition_type->base_type != BOOL_TYPE &&
		    condition_type->base_type != ANY_TYPE) {
			compiler_panic(
				&parser,
				"'for' condition must be of type 'Bool'.",
				TYPE);
		}

		consume(TOKEN_SEMICOLON, "Expected ';' after loop condition");
		exitJump = emit_jump(OP_JUMP_IF_FALSE);
		emit_word(OP_POP);
	}

	const int bodyJump = emit_jump(OP_JUMP);
	const int incrementStart = current_chunk()->count;
	push_loop_context(LOOP_FOR, incrementStart);

	// no type enforcement, but pop type
	expression();
	pop_type_record();
	emit_word(OP_POP);

	emit_loop(loopStart);
	loopStart = incrementStart;
	patch_jump(bodyJump);

	statement();
	emit_loop(loopStart);

	if (exitJump != -1) {
		patch_jump(exitJump);
		emit_word(OP_POP);
	}

	pop_loop_context();
	end_scope();
}

static void if_statement(void)
{
	expression();

	// Condition must be Bool
	TypeRecord *condition_type = pop_type_record();
	if (condition_type && condition_type->base_type != BOOL_TYPE &&
	    condition_type->base_type != ANY_TYPE) {
		compiler_panic(&parser,
			       "'if' condition must be of type 'Bool'.", TYPE);
	}

	const int thenJump = emit_jump(OP_JUMP_IF_FALSE);
	emit_word(OP_POP);
	statement();

	const int elseJump = emit_jump(OP_JUMP);
	patch_jump(thenJump);
	emit_word(OP_POP);

	if (match(TOKEN_ELSE))
		statement();

	patch_jump(elseJump);
}

static void return_statement(void)
{
	if (current->type == TYPE_SCRIPT) {
		compiler_panic(&parser,
			       "Cannot use <return> outside of a function.",
			       SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		// check that the function expects Nil
		if (current->return_type &&
		    current->return_type->base_type != NIL_TYPE &&
		    current->return_type->base_type != ANY_TYPE) {
			compiler_panic(
				&parser,
				"Non-Nil return type requires a return value.",
				TYPE);
		}
		emit_return();
	} else {
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after return value.");

		TypeRecord *value_type = pop_type_record();

		// Only validate if both sides are known
		if (value_type && current->return_type &&
		    current->return_type->base_type != ANY_TYPE &&
		    value_type->base_type != ANY_TYPE) {
			if (!types_compatible(current->return_type,
					      value_type)) {
				char expected[64];
				char got[64];
				type_mask_name(current->return_type->base_type,
					       expected, sizeof(expected));
				type_mask_name(value_type->base_type, got,
					       sizeof(got));
				char msg[160];
				snprintf(msg, sizeof(msg),
					 "Return type mismatch: expected '%s', "
					 "got '%s'.",
					 expected, got);
				compiler_panic(&parser, msg, TYPE);
			}
		}

		emit_word(OP_RETURN);
	}
}

static void use_statement(void)
{
	bool hasParen = false;
	if (parser.current.type == TOKEN_LEFT_PAREN) {
		consume(TOKEN_LEFT_PAREN, "Expected '(' after use statement.");
		hasParen = true;
	}

	uint16_t nameCount = 0;
	uint16_t names[UINT8_MAX];
	uint16_t aliases[UINT8_MAX];
	bool aliasPresence[UINT8_MAX];

	// Keep the token text for each name so we can register types.
	Token nameTokens[UINT8_MAX];
	Token aliasTokens[UINT8_MAX];

	for (int i = 0; i < UINT8_MAX; i++) {
		names[i] = 0;
		aliases[i] = 0;
		aliasPresence[i] = false;
	}

	do {
		if (nameCount >= UINT8_MAX) {
			compiler_panic(&parser,
				       "Cannot import more than 255 names "
				       "from another module.",
				       IMPORT_EXTENT);
		}
		consume_identifier_like("Expected name to import from module.");
		uint16_t name;
		if (parser.current.type == TOKEN_AS) {
			nameTokens[nameCount] = parser.previous;
			name = identifier_constant(&parser.previous);
			consume(TOKEN_AS, "Expected 'as' keyword.");
			consume(TOKEN_IDENTIFIER,
				"Expected alias name after 'as'.");
			aliasTokens[nameCount] = parser.previous;
			const uint16_t alias = identifier_constant(
				&parser.previous);
			aliases[nameCount] = alias;
			aliasPresence[nameCount] = true;
		} else {
			nameTokens[nameCount] = parser.previous;
			name = identifier_constant(&parser.previous);
		}

		names[nameCount] = name;
		nameCount++;
	} while (match(TOKEN_COMMA));

	if (hasParen) {
		consume(TOKEN_RIGHT_PAREN,
			"Expected ')' after last imported name.");
	}

	consume(TOKEN_FROM, "Expected 'from' after 'use' statement.");
	consume(TOKEN_STRING, "Expected string literal for module name.");

	bool isNative = false;
	if (memcmp(parser.previous.start, "\"crux:", 6) == 0) {
		isNative = true;
	}

	uint16_t module;
	if (isNative) {
		module = make_constant(OBJECT_VAL(
			copy_string(current->owner, parser.previous.start + 6,
				    parser.previous.length - 7)));
		emit_words(OP_USE_NATIVE, nameCount);
		for (uint16_t i = 0; i < nameCount; i++) {
			emit_word(names[i]);
		}
		for (uint16_t i = 0; i < nameCount; i++) {
			if (aliasPresence[i]) {
				emit_word(aliases[i]);
			} else {
				emit_word(names[i]);
			}
		}
		emit_word(module);
	} else {
		module = make_constant(OBJECT_VAL(
			copy_string(current->owner, parser.previous.start + 1,
				    parser.previous.length - 2)));
		emit_words(OP_USE_MODULE, module);
		emit_words(OP_FINISH_USE, nameCount);
		for (uint16_t i = 0; i < nameCount; i++) {
			emit_word(names[i]);
		}
		for (uint16_t i = 0; i < nameCount; i++) {
			if (aliasPresence[i]) {
				emit_word(aliases[i]);
			} else {
				emit_word(names[i]);
			}
		}
	}

	consume(TOKEN_SEMICOLON, "Expected ';' after import statement.");

	// Register each imported name in the type table as ANY_TYPE.
	// The VM resolves real values at runtime; we just need the names
	// present so named_variable doesn't treat them as unknown globals
	// and produce false type errors. The pre-pass will replace these
	// with accurate types once it exists.
	TypeRecord *any = new_type_rec(&current->type_arena, ANY_TYPE);
	for (uint16_t i = 0; i < nameCount; i++) {
		// The visible name is the alias when present, otherwise the
		// original name.
		Token visible = aliasPresence[i] ? aliasTokens[i]
						 : nameTokens[i];
		ObjectString *name_str = copy_string(current->owner,
						     visible.start,
						     visible.length);
		if (current->scope_depth == 0) {
			type_table_set(&current->type_table, name_str, any);
		} else {
			// Local-scope imports are unusual but legal — stamp
			// the type onto the most recently declared local.
			// (declare_variable has already been called for each
			// name by the VM at runtime, but at compile time we
			// don't have a local slot; we can only record it in
			// the scope's type table.)
			type_table_set(&current->type_table, name_str, any);
		}
	}
}

static void struct_declaration(void)
{
	consume(TOKEN_IDENTIFIER, "Expected struct name.");
	const Token structName = parser.previous;

	GC_PROTECT_START(current->owner->current_module_record);

	ObjectString *structNameString = copy_string(current->owner,
						     structName.start,
						     structName.length);
	GC_PROTECT(current->owner->current_module_record,
		   OBJECT_VAL(structNameString));

	const uint16_t nameConstant = identifier_constant(&structName);
	ObjectStruct *structObject = new_struct_type(current->owner,
						     structNameString);
	GC_PROTECT(current->owner->current_module_record,
		   OBJECT_VAL(structObject));

	// Reserve the local slot (or note the global name) before we start
	// parsing the body so the struct can refer to itself recursively.
	declare_variable();

	const int local_index = (current->scope_depth > 0)
					? current->local_count - 1
					: -1;

	const uint16_t structConstant = make_constant(OBJECT_VAL(structObject));
	emit_words(OP_STRUCT, structConstant);
	define_variable(nameConstant);

	consume(TOKEN_LEFT_BRACE, "Expected '{' before struct body.");

	// Build a TypeTable mapping field name -> TypeRecord.
	// This is heap-allocated so it outlives the compiler's type_arena.
	TypeTable *field_types = ALLOCATE(current->owner, TypeTable, 1);
	init_type_table(field_types);
	int fieldCount = 0;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount >= UINT16_MAX) {
				compiler_panic(&parser,
					       "Too many fields in struct.",
					       SYNTAX);
				break;
			}

			consume(TOKEN_IDENTIFIER, "Expected field name.");
			ObjectString *fieldName = copy_string(
				current->owner, parser.previous.start,
				parser.previous.length);

			GC_PROTECT(current->owner->current_module_record,
				   OBJECT_VAL(fieldName));

			Value fieldNameCheck;
			if (table_get(&structObject->fields, fieldName,
				      &fieldNameCheck)) {
				compiler_panic(&parser,
					       "Duplicate field name in struct "
					       "declaration.",
					       SYNTAX);
				break;
			}

			// Optional field type annotation: fieldName: Type
			TypeRecord *field_type = NULL;
			if (match(TOKEN_COLON)) {
				field_type = parse_type_record();
			} else {
				field_type = new_type_rec(&current->type_arena,
							  ANY_TYPE);
			}

			type_table_set(field_types, fieldName, field_type);

			table_set(current->owner, &structObject->fields,
				  fieldName, INT_VAL(fieldCount));
			fieldCount++;
		} while (match(TOKEN_COMMA));
	}

	if (fieldCount != 0) {
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after struct body.");
	}

	GC_PROTECT_END(current->owner->current_module_record);

	// Build the TypeRecord for this struct and register it so that
	// named_variable and parse_type_record can look it up by name.
	TypeRecord *struct_type = new_struct_type_rec(&current->type_arena,
						      structObject, field_types,
						      fieldCount);

	if (current->scope_depth == 0) {
		// Global struct — enter it into the compiler's type table
		// under the struct's own name.
		type_table_set(&current->type_table, structNameString,
			       struct_type);
	} else {
		// Local struct — stamp the type onto the reserved local slot.
		current->locals[local_index].type = struct_type;
	}
}

static void result_unwrap(bool can_assign)
{
	(void)can_assign;

	TypeRecord *type = pop_type_record();

	if (!type || type->base_type == ANY_TYPE) {
		// Unknown type — allow it through, runtime will catch errors.
		emit_word(OP_UNWRAP);
		push_type_record(new_type_rec(&current->type_arena, ANY_TYPE));
		return;
	}

	if (type->base_type != RESULT_TYPE) {
		compiler_panic(&parser,
			       "'?' operator requires a 'Result' type.", TYPE);
		// Push ANY_TYPE so the stack stays balanced after the panic.
		push_type_record(new_type_rec(&current->type_arena, ANY_TYPE));
		return;
	}

	emit_word(OP_UNWRAP);

	// Push the inner ok_type, falling back to ANY_TYPE if unknown.
	TypeRecord *ok_type = type->as.result_type.ok_type;
	push_type_record(
		ok_type ? ok_type
			: new_type_rec(&current->type_arena, ANY_TYPE));
}

/**
 * Synchronizes the parser after encountering a syntax error.
 *
 * Discards tokens until a statement boundary is found to minimize cascading
 * errors.
 */
static void synchronize(void)
{
	parser.panic_mode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;
		switch (parser.current.type) {
		case TOKEN_STRUCT:
		case TOKEN_PUB:
		case TOKEN_FN:
		case TOKEN_LET:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_RETURN:
			return;
		default:;
		}
		advance();
	}
}

static void public_declaration(void)
{
	if (current->scope_depth > 0) {
		compiler_panic(
			&parser,
			"Cannot declare public members in a local scope.",
			SYNTAX);
	}
	emit_word(OP_PUB);
	if (match(TOKEN_FN)) {
		fn_declaration();
	} else if (match(TOKEN_LET)) {
		var_declaration();
	} else if (match(TOKEN_STRUCT)) {
		struct_declaration();
	} else {
		compiler_panic(&parser,
			       "Expected 'fn', 'let', or 'struct' after 'pub'.",
			       SYNTAX);
	}
}

static void begin_match_scope(void)
{
	if (current->match_depth > 0) {
		compiler_panic(&parser,
			       "Nesting match statements is not allowed.",
			       SYNTAX);
	}
	current->match_depth++;
}

static void end_match_scope(void)
{
	current->match_depth--;
}

static void give_statement(void)
{
	if (current->match_depth == 0) {
		compiler_panic(
			&parser,
			"'give' can only be used inside a match expression.",
			SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		emit_word(OP_NIL);
		current->last_give_type = new_type_rec(&current->type_arena,
						       NIL_TYPE);
	} else {
		expression();
		// Record for match_expression to check arm consistency.
		current->last_give_type = pop_type_record();
		consume(TOKEN_SEMICOLON, "Expected ';' after give statement.");
	}

	emit_word(OP_GIVE);
}

static void match_expression(bool can_assign)
{
	(void)can_assign;
	begin_match_scope();

	expression();

	// The target's type tells us what patterns are valid.
	TypeRecord *target_type = pop_type_record();
	if (!target_type)
		target_type = new_type_rec(&current->type_arena, ANY_TYPE);

	consume(TOKEN_LEFT_BRACE, "Expected '{' after match target.");

	int *endJumps = ALLOCATE(current->owner, int, 8);
	int jumpCount = 0;
	int jumpCapacity = 8;

	emit_word(OP_MATCH);

	bool hasDefault = false;
	bool hasOkPattern = false;
	bool hasErrPattern = false;

	// Track the type produced by each arm for consistency checking.
	// NULL means we haven't seen an arm yet.
	TypeRecord *arm_type = NULL;

	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		int jumpIfNotMatch = -1;
		uint16_t bindingSlot = UINT16_MAX;
		bool hasBinding = false;
		current->last_give_type = NULL;

		if (match(TOKEN_DEFAULT)) {
			if (hasDefault) {
				compiler_panic(&parser,
					       "Cannot have multiple default "
					       "patterns.",
					       SYNTAX);
			}
			hasDefault = true;

		} else if (match(TOKEN_OK)) {
			// Ok/Err patterns require a Result target.
			if (target_type->base_type != RESULT_TYPE &&
			    target_type->base_type != ANY_TYPE) {
				compiler_panic(
					&parser,
					"'Ok' pattern requires a 'Result' "
					"match target.",
					TYPE);
			}
			if (hasOkPattern) {
				compiler_panic(
					&parser,
					"Cannot have multiple 'Ok' patterns.",
					SYNTAX);
			}
			hasOkPattern = true;
			jumpIfNotMatch = emit_jump(OP_RESULT_MATCH_OK);

			if (match(TOKEN_LEFT_PAREN)) {
				begin_scope();
				hasBinding = true;
				consume(TOKEN_IDENTIFIER,
					"Expected identifier after 'Ok' "
					"pattern.");
				declare_variable();
				bindingSlot = current->local_count - 1;

				// Stamp the binding's type from the Result's
				// ok_type, falling back to ANY_TYPE.
				TypeRecord *ok_type =
					(target_type->base_type == RESULT_TYPE)
						? target_type->as.result_type
							  .ok_type
						: NULL;
				current->locals[bindingSlot].type =
					ok_type ? ok_type
						: new_type_rec(
							  &current->type_arena,
							  ANY_TYPE);

				mark_initialized();
				consume(TOKEN_RIGHT_PAREN,
					"Expected ')' after identifier.");
			}

		} else if (match(TOKEN_ERR)) {
			if (target_type->base_type != RESULT_TYPE &&
			    target_type->base_type != ANY_TYPE) {
				compiler_panic(
					&parser,
					"'Err' pattern requires a 'Result' "
					"match target.",
					TYPE);
			}
			if (hasErrPattern) {
				compiler_panic(
					&parser,
					"Cannot have multiple 'Err' patterns.",
					SYNTAX);
			}
			hasErrPattern = true;
			jumpIfNotMatch = emit_jump(OP_RESULT_MATCH_ERR);

			if (match(TOKEN_LEFT_PAREN)) {
				begin_scope();
				hasBinding = true;
				consume(TOKEN_IDENTIFIER,
					"Expected identifier after 'Err' "
					"pattern.");
				declare_variable();
				bindingSlot = current->local_count - 1;

				// Err bindings always hold an ObjectError.
				current->locals[bindingSlot].type =
					new_type_rec(&current->type_arena,
						     ERROR_TYPE);

				mark_initialized();
				consume(TOKEN_RIGHT_PAREN,
					"Expected ')' after identifier.");
			}

		} else {
			// Value pattern: the expression must be compatible
			// with the target type.
			expression();
			TypeRecord *pattern_type = pop_type_record();

			if (pattern_type && target_type &&
			    pattern_type->base_type != ANY_TYPE &&
			    target_type->base_type != ANY_TYPE) {
				if (!types_compatible(target_type,
						      pattern_type)) {
					char expected[64], got[64], msg[160];
					type_mask_name(target_type->base_type,
						       expected,
						       sizeof(expected));
					type_mask_name(pattern_type->base_type,
						       got, sizeof(got));
					snprintf(msg, sizeof(msg),
						 "Pattern type '%s' is not "
						 "compatible with match target "
						 "type '%s'.",
						 got, expected);
					compiler_panic(&parser, msg, TYPE);
				}
			}

			jumpIfNotMatch = emit_jump(OP_MATCH_JUMP);
		}

		consume(TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

		if (bindingSlot != UINT16_MAX) {
			emit_words(OP_RESULT_BIND, bindingSlot);
		}

		// Compile arm body and track the type it produces.
		TypeRecord *this_arm_type = NULL;

		if (match(TOKEN_LEFT_BRACE)) {
			block();
			// Blocks produce values only via give — read it back.
			this_arm_type =
				current->last_give_type
					? current->last_give_type
					: new_type_rec(&current->type_arena,
						       NIL_TYPE);
		} else if (match(TOKEN_GIVE)) {
			if (match(TOKEN_SEMICOLON)) {
				emit_word(OP_NIL);
				this_arm_type = new_type_rec(
					&current->type_arena, NIL_TYPE);
			} else {
				expression();
				this_arm_type = pop_type_record();
				consume(TOKEN_SEMICOLON,
					"Expected ';' after give expression.");
			}
			emit_word(OP_GIVE);
		} else {
			expression();
			this_arm_type = pop_type_record();
			consume(TOKEN_SEMICOLON,
				"Expected ';' after expression.");
		}

		// Check arm type consistency.
		if (!this_arm_type)
			this_arm_type = new_type_rec(&current->type_arena,
						     ANY_TYPE);

		if (arm_type == NULL) {
			arm_type = this_arm_type;
		} else if (arm_type->base_type != ANY_TYPE &&
			   this_arm_type->base_type != ANY_TYPE) {
			if (!types_compatible(arm_type, this_arm_type)) {
				char expected[64], got[64], msg[160];
				type_mask_name(arm_type->base_type, expected,
					       sizeof(expected));
				type_mask_name(this_arm_type->base_type, got,
					       sizeof(got));
				snprintf(msg, sizeof(msg),
					 "Match arms produce inconsistent "
					 "types: expected '%s', got '%s'.",
					 expected, got);
				compiler_panic(&parser, msg, TYPE);
			}
		}

		if (hasBinding) {
			end_scope();
		}

		if (jumpCount + 1 > jumpCapacity) {
			const int oldCapacity = jumpCapacity;
			jumpCapacity = GROW_CAPACITY(oldCapacity);
			endJumps = GROW_ARRAY(current->owner, int, endJumps,
					      oldCapacity, jumpCapacity);
		}

		endJumps[jumpCount++] = emit_jump(OP_JUMP);

		if (jumpIfNotMatch != -1) {
			patch_jump(jumpIfNotMatch);
		}
	}

	if (jumpCount == 0) {
		compiler_panic(&parser,
			       "'match' expression must have at least one arm.",
			       SYNTAX);
	}

	if (hasOkPattern || hasErrPattern) {
		if (!hasDefault && !(hasOkPattern && hasErrPattern)) {
			compiler_panic(&parser,
				       "Result 'match' must have both 'Ok' and "
				       "'Err' patterns, or a default case.",
				       SYNTAX);
		}
	} else if (!hasDefault) {
		compiler_panic(&parser,
			       "'match' expression must have a 'default' case.",
			       SYNTAX);
	}

	for (int i = 0; i < jumpCount; i++) {
		patch_jump(endJumps[i]);
	}

	emit_word(OP_MATCH_END);

	FREE_ARRAY(current->owner, int, endJumps, jumpCapacity);
	consume(TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
	end_match_scope();

	// Push the unified arm type so callers know what the match produces.
	push_type_record(
		arm_type ? arm_type
			 : new_type_rec(&current->type_arena, ANY_TYPE));
}

static void continue_statement(void)
{
	consume(TOKEN_SEMICOLON, "Expected ';' after 'continue',");
	const int continueTarget = get_current_continue_target();
	if (continueTarget == -1) {
		return;
	}
	const LoopContext *loopContext =
		&current->loop_stack[current->loop_depth - 1];
	cleanupLocalsToDepth(loopContext->scope_depth);
	emit_loop(continueTarget);
}

static void break_statement(void)
{
	consume(TOKEN_SEMICOLON, "Expected ';' after 'break'.");
	if (current->loop_depth <= 0) {
		compiler_panic(&parser, "Cannot use 'break' outside of a loop.",
			       SYNTAX);
		return;
	}
	const LoopContext *loopContext =
		&current->loop_stack[current->loop_depth - 1];
	cleanupLocalsToDepth(loopContext->scope_depth);
	add_break_jump(emit_jump(OP_JUMP));
}

static void panic_statement(void)
{
	expression();
	TypeRecord *type = pop_type_record();

	if (type && type->base_type != STRING_TYPE &&
	    type->base_type != ANY_TYPE) {
		char got[64], msg[128];
		type_mask_name(type->base_type, got, sizeof(got));
		snprintf(msg, sizeof(msg),
			 "'panic' requires a 'String', got '%s'.", got);
		compiler_panic(&parser, msg, TYPE);
	}

	consume(TOKEN_SEMICOLON, "Expected ';' after 'panic'.");
	emit_word(OP_PANIC);
}

static void declaration(void)
{
	if (match(TOKEN_LET)) {
		var_declaration();
	} else if (match(TOKEN_FN)) {
		fn_declaration();
	} else if (match(TOKEN_STRUCT)) {
		struct_declaration();
	} else if (match(TOKEN_PUB)) {
		public_declaration();
	} else {
		statement();
	}

	if (parser.panic_mode)
		synchronize();
}

static void statement(void)
{
	if (match(TOKEN_IF)) {
		if_statement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		begin_scope();
		block();
		end_scope();
	} else if (match(TOKEN_WHILE)) {
		while_statement();
	} else if (match(TOKEN_FOR)) {
		for_statement();
	} else if (match(TOKEN_RETURN)) {
		return_statement();
	} else if (match(TOKEN_USE)) {
		use_statement();
	} else if (match(TOKEN_GIVE)) {
		give_statement();
	} else if (match(TOKEN_BREAK)) {
		break_statement();
	} else if (match(TOKEN_CONTINUE)) {
		continue_statement();
	} else if (match(TOKEN_PANIC)) {
		panic_statement();
	} else {
		expression_statement();
	}
}

static void grouping(bool can_assign)
{
	(void)can_assign;
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool can_assign)
{
	(void)can_assign;
	char *end;
	errno = 0;

	const char *numberStart = parser.previous.start;
	const double number = strtod(numberStart, &end);

	if (end == numberStart) {
		compiler_panic(&parser, "Failed to form number", SYNTAX);
		return;
	}
	if (errno == ERANGE) {
		emit_constant(FLOAT_VAL(number));
		push_type_record(
			new_type_rec(&current->type_arena, FLOAT_TYPE));
		return;
	}
	bool hasDecimalNotation = false;
	for (const char *c = numberStart; c < end; c++) {
		if (*c == '.' || *c == 'e' || *c == 'E') {
			hasDecimalNotation = true;
			break;
		}
	}
	if (hasDecimalNotation) {
		emit_constant(FLOAT_VAL(number));
		push_type_record(
			new_type_rec(&current->type_arena, FLOAT_TYPE));
	} else {
		const int32_t integer = (int32_t)number;
		if ((double)integer == number) {
			emit_constant(INT_VAL(integer));
			push_type_record(
				new_type_rec(&current->type_arena, INT_TYPE));
		} else {
			emit_constant(FLOAT_VAL(number));
			push_type_record(
				new_type_rec(&current->type_arena, FLOAT_TYPE));
		}
	}
}

static char process_escape_sequence(const char escape, bool *hasError)
{
	*hasError = false;
	switch (escape) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'r':
		return '\r';
	case '\\':
		return '\\';
	case '"':
		return '"';
	case '\'':
		return '\'';
	case '0':
		return ' ';
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'v':
		return '\v';
	default: {
		*hasError = true;
		return '\0';
	}
	}
}

static void string(bool can_assign)
{
	(void)can_assign;
	char *processed = ALLOCATE(current->owner, char,
				   parser.previous.length);

	if (processed == NULL) {
		compiler_panic(&parser,
			       "Cannot allocate memory for string expression.",
			       MEMORY);
		return;
	}

	int processedLength = 0;
	const char *src = (char *)parser.previous.start + 1;
	const int srcLength = parser.previous.length - 2;

	if (srcLength == 0) {
		ObjectString *string = copy_string(current->owner, "", 0);
		emit_constant(OBJECT_VAL(string));
		FREE_ARRAY(current->owner, char, processed,
			   parser.previous.length);
		return;
	}

	for (int i = 0; i < srcLength; i++) {
		if (src[i] == '\\') {
			if (i + 1 >= srcLength) {
				compiler_panic(
					&parser,
					"Unterminated escape sequence at "
					"end of string",
					SYNTAX);
				FREE_ARRAY(current->owner, char, processed,
					   parser.previous.length);
				return;
			}

			bool error;
			const char escaped = process_escape_sequence(src[i + 1],
								     &error);
			if (error) {
				char errorMessage[64];
				snprintf(errorMessage, 64,
					 "Unexpected escape sequence '\\%c'",
					 src[i + 1]);
				compiler_panic(&parser, errorMessage, SYNTAX);
				FREE_ARRAY(current->owner, char, processed,
					   parser.previous.length);
				return;
			}

			processed[processedLength++] = escaped;
			i++;
		} else {
			processed[processedLength++] = src[i];
		}
	}

	char *temp = GROW_ARRAY(current->owner, char, processed,
				parser.previous.length, processedLength + 1);
	if (temp == NULL) {
		compiler_panic(&parser,
			       "Cannot allocate memory for string expression.",
			       MEMORY);
		FREE_ARRAY(current->owner, char, processed,
			   parser.previous.length);
		return;
	}
	processed = temp;
	processed[processedLength] = '\0';
	ObjectString *string = take_string(current->owner, processed,
					   processedLength);
	push_type_record(new_type_rec(&current->type_arena, STRING_TYPE));
	emit_constant(OBJECT_VAL(string));
}

static void unary(bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = parser.previous.type;

	// compile the operand
	parse_precedence(PREC_UNARY);

	switch (operatorType) {
	case TOKEN_NOT: {
		// check if this is a boolean type
		TypeRecord *bool_expected = pop_type_record();
		if (!bool_expected || bool_expected->base_type != BOOL_TYPE) {
			compiler_panic(
				&parser,
				"Expected 'Bool'type for 'not' operator.",
				TYPE);
		}
		push_type_record(bool_expected);
		emit_word(OP_NOT);
		break;
	}
	case TOKEN_MINUS: {
		// check if this is a negatable type
		TypeRecord *num_expected = pop_type_record();
		if (!num_expected ||
		    !(num_expected->base_type & (INT_TYPE | FLOAT_TYPE))) {
			compiler_panic(
				&parser,
				"Expected 'Int | Float' type for '-' operator.",
				TYPE);
		}
		push_type_record(num_expected);
		emit_word(OP_NEGATE);
		break;
	}
	default:
		return; // unreachable
	}
}

static void typeof_expression(bool can_assign)
{
	(void)can_assign;
	parse_precedence(PREC_UNARY);
	emit_word(OP_TYPEOF); // this will emit a string representation of the
			      // type at runtime
	push_type_record(new_type_rec(&current->type_arena, STRING_TYPE));
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, infix_call, NULL, PREC_CALL},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {table_literal, NULL, NULL, PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_SQUARE] = {array_literal, collection_index, NULL,
			       PREC_CALL},
	[TOKEN_RIGHT_SQUARE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_DOT] = {NULL, dot, NULL, PREC_CALL},
	[TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
	[TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
	[TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_BACKSLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_STAR] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_STAR_STAR] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_PERCENT] = {NULL, binary, NULL, PREC_FACTOR},
	[TOKEN_LEFT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
	[TOKEN_RIGHT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
	[TOKEN_AMPERSAND] = {NULL, binary, NULL, PREC_BITWISE_AND},
	[TOKEN_CARET] = {NULL, binary, NULL, PREC_BITWISE_XOR},
	[TOKEN_PIPE] = {NULL, binary, NULL, PREC_BITWISE_OR},
	[TOKEN_NOT] = {unary, NULL, NULL, PREC_NONE},
	[TOKEN_BANG_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[TOKEN_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[TOKEN_GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_LESS] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
	[TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
	[TOKEN_INT] = {number, NULL, NULL, PREC_NONE},
	[TOKEN_FLOAT] = {number, NULL, NULL, PREC_NONE},
	[TOKEN_CONTINUE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_BREAK] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_AND] = {NULL, and_, NULL, PREC_AND},
	[TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FN] = {anonymous_function, NULL, NULL, PREC_NONE},
	[TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_NIL] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_OR] = {NULL, or_, NULL, PREC_OR},
	[TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
	[TOKEN_LET] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_USE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FROM] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_PUB] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ERROR] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_DEFAULT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_EQUAL_ARROW] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MATCH] = {match_expression, NULL, NULL, PREC_PRIMARY},
	[TOKEN_TYPEOF] = {typeof_expression, NULL, NULL, PREC_UNARY},
	[TOKEN_STRUCT] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_NEW] = {struct_instance, NULL, NULL, PREC_UNARY},
	[TOKEN_EOF] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_QUESTION_MARK] = {NULL, NULL, result_unwrap, PREC_CALL},
	[TOKEN_PANIC] = {NULL, NULL, NULL, PREC_NONE},
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parse_precedence(const Precedence precedence)
{
	advance();
	ParseFn prefixRule;
	prefixRule = get_rule(parser.previous.type)->prefix;
	if (is_identifier_like(parser.previous.type)) {
		prefixRule = get_rule(TOKEN_IDENTIFIER)->prefix;
	}
	if (prefixRule == NULL) {
		compiler_panic(&parser, "Expected expression.", SYNTAX);
		return;
	}

	const bool can_assign = precedence <= PREC_ASSIGNMENT;
	prefixRule(can_assign);

	while (precedence <= get_rule(parser.current.type)->precedence) {
		advance();
		const ParseRule *rule = get_rule(parser.previous.type);
		if (rule->infix != NULL) {
			rule->infix(can_assign);
		} else if (rule->postfix != NULL) {
			rule->postfix(can_assign);
		}
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		compiler_panic(&parser, "Invalid Assignment Target", SYNTAX);
	}
}

static ParseRule *get_rule(const CruxTokenType type)
{
	return &rules[type]; // Returns the rule at the given index
}

// ── Pre-scanner
// ───────────────────────────────────────────────────────────────
//
// A lightweight two-sub-pass scanner that collects top-level struct and
// function signatures before the main compilation pass begins. This gives the
// single-pass compiler knowledge of forward-referenced names (e.g. a function
// declared after its call site, or mutual recursion).
//
// Sub-pass 1 collects struct declarations (so struct-typed parameters in
// function signatures resolve correctly in sub-pass 2).
// Sub-pass 2 collects function signatures.
//
// Implementation notes:
// - Uses the shared global `parser` and `current` compiler pointer, exactly
//   as the main compiler does, so parse_type_record() works normally.
// - A temporary Compiler is initialised with init_compiler() for each sub-pass.
// - After both sub-passes are complete, the collected types are merged into the
//   main compiler's type_table via type_table_add_all.
// - The scanner is rewound with init_scanner(source) after pre-scanning so the
//   main pass starts from the beginning.

// Advance the pre-scanner, skipping error tokens silently.
static void pre_advance(void)
{
	parser.prev_previous = parser.previous;
	parser.previous = parser.current;
	for (;;) {
		parser.current = scan_token();
		if (parser.current.type != TOKEN_ERROR)
			break;
	}
}

// Skip a balanced brace block { ... } starting at the current token (which
// must be TOKEN_LEFT_BRACE). Handles nesting.
static void pre_skip_block(void)
{
	if (parser.current.type != TOKEN_LEFT_BRACE)
		return;
	pre_advance(); // consume '{'
	int depth = 1;
	while (depth > 0 && parser.current.type != TOKEN_EOF) {
		if (parser.current.type == TOKEN_LEFT_BRACE)
			depth++;
		if (parser.current.type == TOKEN_RIGHT_BRACE)
			depth--;
		pre_advance();
	}
}

// Skip a balanced parenthesis group ( ... ) starting at current token.
static void pre_skip_parens(void)
{
	if (parser.current.type != TOKEN_LEFT_PAREN)
		return;
	pre_advance(); // consume '('
	int depth = 1;
	while (depth > 0 && parser.current.type != TOKEN_EOF) {
		if (parser.current.type == TOKEN_LEFT_PAREN)
			depth++;
		if (parser.current.type == TOKEN_RIGHT_PAREN)
			depth--;
		pre_advance();
	}
}

// Advance past a type annotation starting at the current token.
// Handles nested brackets and parentheses (e.g. Array[Int], (Int)->Bool,
// Table[String:Int], union types separated by '|').
static void pre_skip_type(void)
{
	// A type may be: a keyword type, an identifier, a function type
	// (parens), followed by optional '[...]' subscript, followed by
	// optional '|' union chains.
	for (;;) {
		CruxTokenType t = parser.current.type;

		if (t == TOKEN_LEFT_PAREN) {
			// Function type: (T, T) -> T
			pre_skip_parens();
			// consume '->'
			if (parser.current.type == TOKEN_ARROW)
				pre_advance();
			pre_skip_type(); // return type
		} else if (t == TOKEN_INT_TYPE || t == TOKEN_FLOAT_TYPE ||
			   t == TOKEN_BOOL_TYPE || t == TOKEN_STRING_TYPE ||
			   t == TOKEN_NIL_TYPE || t == TOKEN_ANY_TYPE ||
			   t == TOKEN_ARRAY_TYPE || t == TOKEN_TABLE_TYPE ||
			   t == TOKEN_VECTOR_TYPE || t == TOKEN_MATRIX_TYPE ||
			   t == TOKEN_BUFFER_TYPE || t == TOKEN_ERROR_TYPE ||
			   t == TOKEN_RESULT_TYPE || t == TOKEN_RANGE_TYPE ||
			   t == TOKEN_TUPLE_TYPE || t == TOKEN_COMPLEX_TYPE ||
			   t == TOKEN_SET_TYPE || t == TOKEN_RANDOM_TYPE ||
			   t == TOKEN_FILE_TYPE || t == TOKEN_IDENTIFIER) {
			pre_advance(); // consume the base type token
			// Optional subscript: Array[Int], Table[K:V], etc.
			if (parser.current.type == TOKEN_LEFT_SQUARE) {
				pre_advance(); // consume '['
				int depth = 1;
				while (depth > 0 &&
				       parser.current.type != TOKEN_EOF) {
					if (parser.current.type ==
					    TOKEN_LEFT_SQUARE)
						depth++;
					if (parser.current.type ==
					    TOKEN_RIGHT_SQUARE)
						depth--;
					pre_advance();
				}
			}
		} else {
			// Unknown/unsupported — stop.
			break;
		}

		// Union continuation: T | T | ...
		if (parser.current.type == TOKEN_PIPE) {
			pre_advance(); // consume '|'
			continue; // parse next variant
		}
		break;
	}
}

// Collect a single top-level struct declaration into pre_compiler's type_table.
// On entry parser.current is TOKEN_STRUCT (already consumed by caller).
static void pre_collect_struct(void)
{
	// Consume struct name.
	if (parser.current.type != TOKEN_IDENTIFIER)
		return;
	Token name_token = parser.current;
	pre_advance();

	// Expect '{' to start the struct body.
	if (parser.current.type != TOKEN_LEFT_BRACE)
		return;
	pre_advance(); // consume '{'

	// Build a field_types TypeTable.
	TypeTable *field_types = ALLOCATE(current->owner, TypeTable, 1);
	init_type_table(field_types);
	int field_count = 0;

	// Create a lightweight ObjectStruct so new_struct_type_rec works.
	ObjectString *struct_name = copy_string(current->owner,
						name_token.start,
						name_token.length);
	ObjectStruct *struct_obj = new_struct_type(current->owner, struct_name);

	while (parser.current.type != TOKEN_RIGHT_BRACE &&
	       parser.current.type != TOKEN_EOF) {
		// Field name
		if (parser.current.type != TOKEN_IDENTIFIER)
			break;
		Token field_tok = parser.current;
		ObjectString *field_name = copy_string(current->owner,
						       field_tok.start,
						       field_tok.length);
		pre_advance();

		TypeRecord *field_type = NULL;
		if (parser.current.type == TOKEN_COLON) {
			pre_advance(); // consume ':'
			// parse_type_record uses the global current compiler
			// and parser — this works because we're in a properly
			// initialised pre_compiler context.
			field_type = parse_type_record();
		} else {
			field_type = new_type_rec(&current->type_arena,
						  ANY_TYPE);
		}

		type_table_set(field_types, field_name, field_type);
		table_set(current->owner, &struct_obj->fields, field_name,
			  INT_VAL(field_count));
		field_count++;

		// Allow trailing comma between fields.
		if (parser.current.type == TOKEN_COMMA)
			pre_advance();
	}

	// Consume '}'.
	if (parser.current.type == TOKEN_RIGHT_BRACE)
		pre_advance();

	// Build and register the struct TypeRecord.
	TypeRecord *struct_type = new_struct_type_rec(&current->type_arena,
						      struct_obj, field_types,
						      field_count);

	type_table_set(&current->type_table, struct_name, struct_type);
}

// Collect a single top-level function signature into pre_compiler's
// type_table. On entry parser.current is TOKEN_FN (already consumed by caller).
static void pre_collect_function(void)
{
	// Consume function name.
	if (parser.current.type != TOKEN_IDENTIFIER)
		return;
	Token fn_name_token = parser.current;
	pre_advance();

	// Expect '(' to start parameter list.
	if (parser.current.type != TOKEN_LEFT_PAREN)
		return;
	pre_advance(); // consume '('

	int param_cap = 4;
	int param_count = 0;
	TypeRecord **param_types = malloc(sizeof(TypeRecord *) * param_cap);
	if (!param_types)
		return;

	while (parser.current.type != TOKEN_RIGHT_PAREN &&
	       parser.current.type != TOKEN_EOF) {
		// Parameter name (identifier).
		if (parser.current.type != TOKEN_IDENTIFIER) {
			free(param_types);
			return;
		}
		pre_advance(); // consume param name

		TypeRecord *param_type = NULL;
		if (parser.current.type == TOKEN_COLON) {
			pre_advance(); // consume ':'
			param_type = parse_type_record();
		} else {
			param_type = new_type_rec(&current->type_arena,
						  ANY_TYPE);
		}

		if (param_count == param_cap) {
			param_cap *= 2;
			TypeRecord **grown = realloc(param_types,
						     sizeof(TypeRecord *) *
							     param_cap);
			if (!grown) {
				free(param_types);
				return;
			}
			param_types = grown;
		}
		param_types[param_count++] = param_type;

		if (parser.current.type == TOKEN_COMMA)
			pre_advance();
	}

	// Consume ')'.
	if (parser.current.type == TOKEN_RIGHT_PAREN)
		pre_advance();

	// Optional return type annotation: -> Type
	TypeRecord *return_type = NULL;
	if (parser.current.type == TOKEN_ARROW) {
		pre_advance(); // consume '->'
		return_type = parse_type_record();
	} else {
		return_type = new_type_rec(&current->type_arena, ANY_TYPE);
	}

	// Register the function TypeRecord.
	TypeRecord *fn_type = new_function_type_rec(&current->type_arena,
						    param_types, param_count,
						    return_type);

	ObjectString *fn_name = copy_string(current->owner, fn_name_token.start,
					    fn_name_token.length);
	type_table_set(&current->type_table, fn_name, fn_type);

	// Skip the function body so we don't accidentally parse its tokens
	// as top-level declarations.
	pre_skip_block();
}

// Run a single forward-scan sub-pass over the token stream.
// `collect_structs`: when true, collect struct declarations; when false,
// collect function declarations (fn / pub fn).
static void pre_scan_pass(bool collect_structs)
{
	// Reset parser to a clean state (advance() will prime the first token
	// after this is called from compile() with a fresh init_scanner).
	parser.had_error = false;
	parser.panic_mode = false;

	// Prime the first token.
	pre_advance();

	while (parser.current.type != TOKEN_EOF) {
		CruxTokenType t = parser.current.type;

		if (t == TOKEN_STRUCT) {
			pre_advance(); // consume 'struct'
			if (collect_structs) {
				pre_collect_struct();
			} else {
				// Skip: name + block
				if (parser.current.type == TOKEN_IDENTIFIER)
					pre_advance();
				pre_skip_block();
			}

		} else if (t == TOKEN_FN) {
			pre_advance(); // consume 'fn'
			if (!collect_structs) {
				pre_collect_function();
			} else {
				// Skip: name + parens + optional ->T + block
				if (parser.current.type == TOKEN_IDENTIFIER)
					pre_advance();
				pre_skip_parens();
				if (parser.current.type == TOKEN_ARROW) {
					pre_advance();
					pre_skip_type();
				}
				pre_skip_block();
			}

		} else if (t == TOKEN_PUB) {
			pre_advance(); // consume 'pub'
			// pub fn ... or pub struct ...
			if (parser.current.type == TOKEN_FN) {
				pre_advance(); // consume 'fn'
				if (!collect_structs) {
					pre_collect_function();
				} else {
					if (parser.current.type ==
					    TOKEN_IDENTIFIER)
						pre_advance();
					pre_skip_parens();
					if (parser.current.type ==
					    TOKEN_ARROW) {
						pre_advance();
						pre_skip_type();
					}
					pre_skip_block();
				}
			} else if (parser.current.type == TOKEN_STRUCT) {
				pre_advance(); // consume 'struct'
				if (collect_structs) {
					pre_collect_struct();
				} else {
					if (parser.current.type ==
					    TOKEN_IDENTIFIER)
						pre_advance();
					pre_skip_block();
				}
			} else {
				pre_advance(); // skip unknown pub token
			}

		} else {
			// Not a top-level fn/struct — skip the token.
			pre_advance();
		}
	}
}

// Run both pre-scan sub-passes and merge results into `dest`.
// The scanner must be initialised with init_scanner(source) before calling.
static void pre_scan(VM *vm, char *source, TypeTable *dest)
{
	// ── Sub-pass 1: collect struct declarations ──────────────────────────
	init_scanner(source);
	Compiler pre_compiler_structs;
	init_compiler(&pre_compiler_structs, TYPE_SCRIPT, vm);
	// Clear parser error state — the pre-pass is allowed to have syntax
	// it doesn't understand; errors here are non-fatal.
	parser.had_error = false;
	parser.panic_mode = false;
	parser.source = source;

	pre_scan_pass(true /* collect_structs */);

	// ── Sub-pass 2: collect function signatures ──────────────────────────
	// Re-initialise the scanner AND a new Compiler that already has the
	// struct types from sub-pass 1, so struct-typed parameters resolve.
	init_scanner(source);
	Compiler pre_compiler_fns;
	init_compiler(&pre_compiler_fns, TYPE_SCRIPT, vm);
	parser.had_error = false;
	parser.panic_mode = false;

	// Seed the fn-pass compiler with struct types from the first pass so
	// parse_type_record() can resolve struct names in parameter
	// annotations.
	type_table_add_all(&pre_compiler_structs.type_table,
			   &pre_compiler_fns.type_table);

	pre_scan_pass(false /* collect functions */);

	// ── Merge into the destination table ─────────────────────────────────
	// Structs first so that struct types are available when the main pass
	// processes function signatures.
	type_table_add_all(&pre_compiler_structs.type_table, dest);
	type_table_add_all(&pre_compiler_fns.type_table, dest);

	// Free the pre-compiler type_table entries arrays to avoid
	// LeakSanitizer reports. The TypeRecord pointers they reference live in
	// the stack- allocated arenas (valid until pre_scan returns); the
	// caller will deep-copy them into the main compiler's arena before
	// returning.
	free_type_table(&pre_compiler_structs.type_table);
	free_type_table(&pre_compiler_fns.type_table);

	// Note: pre_compiler_structs and pre_compiler_fns are stack-allocated;
	// their type_arenas go out of scope here. The TypeRecords they contain
	// are pointed to by dest — those TypeRecords live in the arenas inside
	// the pre-compiler stack frames and will become dangling once this
	// function returns.
	//
	// To avoid dangling pointers we deep-copy every entry in dest into the
	// main compiler's arena. The caller (compile()) does this immediately
	// after pre_scan() returns, before the pre-compiler frames are gone.
	// (The arenas are still valid while we're inside pre_scan().)
}

// ── Main compilation entry point ─────────────────────────────────────────────

ObjectFunction *compile(VM *vm, char *source)
{
	// ── Pre-scan pass ────────────────────────────────────────────────────
	// Collect top-level struct and function signatures so the main pass
	// can resolve forward references.

	// Initialise the main compiler first so we can use its arena for
	// deep-copying pre-scanned type records.
	init_scanner(source);
	Compiler compiler;
	init_compiler(&compiler, TYPE_SCRIPT, vm);

	parser.had_error = false;
	parser.panic_mode = false;
	parser.source = source;

	// Run pre-scan, merging into a temporary staging table.
	TypeTable staging;
	init_type_table(&staging);
	pre_scan(vm, source, &staging);

	// Deep-copy each pre-scanned TypeRecord into the main compiler's arena
	// and register it in the main compiler's type_table.
	// Also free any heap-allocated arg_types arrays stored in the staging
	// TypeRecords (copy_type_rec_to_arena creates fresh copies, so the
	// originals are no longer needed after the copy).
	for (int i = 0; i < staging.capacity; i++) {
		TypeEntry *entry = &staging.entries[i];
		if (entry->key == NULL)
			continue;
		TypeRecord *src = entry->value;
		TypeRecord *copied =
			copy_type_rec_to_arena(&compiler.type_arena, src);
		type_table_set(&compiler.type_table, entry->key, copied);
		// Free the heap-allocated arg_types array from
		// pre_collect_function — copy_type_rec_to_arena has already
		// created its own copy.
		if (src && src->base_type == FUNCTION_TYPE &&
		    src->as.function_type.arg_types != NULL) {
			free(src->as.function_type.arg_types);
			src->as.function_type.arg_types = NULL;
		}
	}
	free_type_table(&staging);

	// ── Main compile pass
	// ───────────────────────────────────────────────── Rewind the scanner
	// for the real compilation pass.
	init_scanner(source);
	parser.had_error = false;
	parser.panic_mode = false;
	parser.source = source;

	// current was set to &compiler inside init_compiler(); re-confirm it
	// is still pointing at our main compiler (it should be, since
	// pre_scan's end_compiler calls restore current to NULL and then
	// init_compiler for the main compiler set it to &compiler).
	// Because init_compiler sets current = compiler, and the pre-scan
	// compilers were initialised after the main one, current may be
	// pointing at the last pre-scan compiler. Reset it explicitly.
	current = &compiler;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	ObjectFunction *function = end_compiler();
	if (function != NULL) {
		function->module_record = vm->current_module_record;
	}
	return parser.had_error ? NULL : function;
}

void mark_compiler_roots(VM *vm)
{
	const Compiler *compiler = current;
	while (compiler != NULL) {
		mark_object(vm, (CruxObject *)compiler->function);
		compiler = (Compiler *)compiler->enclosing;
	}
}
