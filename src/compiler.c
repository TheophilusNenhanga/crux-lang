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
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
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
	compiler_panic(&parser, message, SYNTAX);
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
 *
 * @param offset The index of the jump instruction in the bytecode to patch.
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

/**
 * Emits OP_NIL_RETURN that signals the end of a scope.
 */
static void emit_return(void)
{
	emit_word(OP_NIL_RETURN);
}

/**
 * Creates a constant value and adds it to the current chunk's constant pool.
 *
 * @param value The value to add as a constant.
 * @return The index of the constant in the constant pool.
 */
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

/**
 * Emits an OP_CONSTANT instruction and its operand (the constant index).
 *
 * @param value The constant value to emit.
 */
static void emit_constant(const Value value)
{
	const uint16_t constant = make_constant(value);
	if (constant >= UINT16_MAX) {
		compiler_panic(&parser, "Too many constants", SYNTAX);
	}
	emit_words(OP_CONSTANT, constant);
}

/**
 * Initializes a compiler.
 *
 * Sets up the compiler state for compiling a new function (or script).
 *
 * @param compiler The compiler to initialize.
 * @param type The type of function being compiled (e.g., TYPE_FUNCTION,
 * TYPE_SCRIPT).
 * @param vm The virtual machine instance.
 */
static void init_compiler(Compiler *compiler, const FunctionType type, VM *vm)
{
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->local_count = 0;
	compiler->scope_depth = 0;
	compiler->match_depth = 0;
	compiler->loop_depth = 0;
	compiler->owner = vm;

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

/**
 * Creates a constant for an identifier token.
 *
 * @param name The token representing the identifier.
 * @return The index of the identifier constant in the constant pool.
 */
static uint16_t identifier_constant(const Token *name)
{
	return make_constant(OBJECT_VAL(
		copy_string(current->owner, name->start, name->length)));
}

/**
 * Begins a new scope.
 *
 * Increases the scope depth, indicating that variables declared subsequently
 * are in a new, inner scope.
 */
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

/**
 * Checks if two identifier tokens are equal.
 *
 * @param a The first token.
 * @param b The second token.
 * @return true if the tokens represent the same identifier, false otherwise.
 */
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
 * @return The index of the added upvalue in the current function's upvalue
 * array.
 */
static int add_upvalue(Compiler *compiler, const uint16_t index,
		       const bool isLocal)
{
	const int upvalueCount = compiler->function->upvalue_count;

	for (int i = 0; i < upvalueCount; i++) {
		const Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->is_local == isLocal) {
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
		return add_upvalue(compiler, (uint16_t)local, true);
	}

	const int upValue = resolve_upvalue(compiler->enclosing, name);
	if (upValue != -1) {
		return add_upvalue(compiler, (uint16_t)upValue, false);
	}

	return -1;
}

/**
 * Adds a local variable to the current scope.
 *
 * @param name The token representing the name of the local variable.
 */
static void add_local(const Token name)
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

	add_local(*name);
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

/**
 * Parses an argument list for a function call.
 *
 * @return The number of arguments parsed.
 */
static uint16_t argument_list(void)
{
	uint16_t arg_count = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (arg_count == UINT16_MAX) {
				compiler_panic(&parser,
					       "Cannot have more than 65535 "
					       "arguments.",
					       ARGUMENT_EXTENT);
			}
			arg_count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	return arg_count;
}

/**
 * Parses the 'and' operator. Implemented using short-circuiting.
 *
 * @param can_assign Whether the 'and' expression can be the target of an
 * assignment.
 */
static void and_(bool can_assign)
{
	(void)can_assign;
	const int endJump = emit_jump(OP_JUMP_IF_FALSE);
	emit_word(OP_POP);
	parse_precedence(PREC_AND);

	patch_jump(endJump);
}

/**
 * Parses the 'or' operator. Implemented using short-circuiting.
 *
 * @param can_assign Whether the 'or' expression can be the target of an
 * assignment.
 */
static void or_(bool can_assign)
{
	(void)can_assign;
	const int elseJump = emit_jump(OP_JUMP_IF_FALSE);
	const int endJump = emit_jump(OP_JUMP);

	patch_jump(elseJump);
	emit_word(OP_POP);
	parse_precedence(PREC_OR);
	patch_jump(endJump);
}

/**
 * Finishes compilation and returns the compiled function.
 *
 * Emits the final OP_RETURN instruction, frees the compiler state, and returns
 * the compiled function object.
 *
 * @return The compiled function object.
 */
static ObjectFunction *end_compiler(void)
{
	emit_return();
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

	switch (operatorType) {
	case TOKEN_BANG_EQUAL:
		emit_word(OP_NOT_EQUAL);
		break;
	case TOKEN_EQUAL_EQUAL:
		emit_word(OP_EQUAL);
		break;
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
	case TOKEN_PLUS:
		emit_word(OP_ADD);
		break;
	case TOKEN_MINUS:
		emit_word(OP_SUBTRACT);
		break;
	case TOKEN_STAR:
		emit_word(OP_MULTIPLY);
		break;
	case TOKEN_SLASH:
		emit_word(OP_DIVIDE);
		break;
	case TOKEN_PERCENT:
		emit_word(OP_MODULUS);
		break;
	case TOKEN_RIGHT_SHIFT:
		emit_word(OP_RIGHT_SHIFT);
		break;
	case TOKEN_LEFT_SHIFT:
		emit_word(OP_LEFT_SHIFT);
		break;
	case TOKEN_BACKSLASH:
		emit_word(OP_INT_DIVIDE);
		break;
	case TOKEN_STAR_STAR:
		emit_word(OP_POWER);
		break;

	default:
		return; // unreachable
	}
}

static void call(bool can_assign)
{
	(void)can_assign;
	const uint16_t arg_count = argument_list();
	emit_words(OP_CALL, arg_count);
}

/**
 * Parses a literal boolean or nil value.
 *
 * @param can_assign Whether the literal can be the target of an assignment
 * (always false).
 */
static void literal(bool can_assign)
{
	(void)can_assign;
	switch (parser.previous.type) {
	case TOKEN_FALSE:
		emit_word(OP_FALSE);
		break;
	case TOKEN_NIL:
		emit_word(OP_NIL);
		break;
	case TOKEN_TRUE:
		emit_word(OP_TRUE);
		break;
	default:
		return; // unreachable
	}
}

/**
 * Parses a dot (.) property access expression.
 *
 * @param can_assign Whether the dot expression can be the target of an
 * assignment.
 */
static void dot(const bool can_assign)
{
	consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
	const uint16_t name = identifier_constant(&parser.previous);

	if (name >= UINT16_MAX) {
		compiler_panic(&parser, "Too many constants.", SYNTAX);
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		emit_words(OP_SET_PROPERTY, name);
	} else if (match(TOKEN_LEFT_PAREN)) {
		const uint16_t arg_count = argument_list();
		emit_words(OP_INVOKE, name);
		emit_word(arg_count);
	} else {
		emit_words(OP_GET_PROPERTY, name);
	}
}

/**
 * Parses an expression. Entry point for expression parsing.
 */
static void expression(void)
{
	parse_precedence(PREC_ASSIGNMENT);
}

/**
 * Gets the compound opcode based on the set opcode and compound operation.
 *
 * This helper function is used for compound assignments (e.g., +=, -=, *=, /=).
 *
 * @param setOp The base set opcode (e.g., OP_SET_LOCAL, OP_SET_GLOBAL).
 * @param op The compound operation (e.g., COMPOUND_OP_PLUS, COMPOUND_OP_MINUS).
 * @return The corresponding compound opcode.
 */
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

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = resolve_upvalue(current, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		arg = identifier_constant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (can_assign) {
		if (match(TOKEN_EQUAL)) {
			expression();
			emit_words(setOp, arg);
			return;
		}

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
			const OpCode compoundOp = get_compound_opcode(setOp,
								      op);
			emit_words(compoundOp, arg);
			return;
		}
	}

	emit_words(getOp, arg);
}

void struct_instance(const bool can_assign)
{
	consume(TOKEN_IDENTIFIER,
		"Expected struct name to start initialization.");
	named_variable(parser.previous, can_assign);
	if (!match(TOKEN_LEFT_BRACE)) {
		compiler_panic(&parser,
			       "Expected '{' to start struct instance.",
			       SYNTAX);
		return;
	}
	uint16_t fieldCount = 0;
	emit_word(OP_STRUCT_INSTANCE_START);

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount == UINT16_MAX) {
				compiler_panic(
					&parser,
					"Too many fields in struct initializer",
					SYNTAX);
				return;
			}
			consume(TOKEN_IDENTIFIER, "Expected field name.");
			// field_name: value
			ObjectString *fieldName = copy_string(
				current->owner, parser.previous.start,
				parser.previous.length);
			consume(TOKEN_COLON,
				"Expected ':' after struct field name.");
			expression();
			const uint16_t fieldNameConstant = make_constant(
				OBJECT_VAL(fieldName));
			emit_words(OP_STRUCT_NAMED_FIELD, fieldNameConstant);

			fieldCount++;
		} while (match(TOKEN_COMMA));
	}

	if (fieldCount != 0) {
		consume(TOKEN_RIGHT_BRACE,
			"Expected '}' after struct field list.");
	}
	emit_word(OP_STRUCT_INSTANCE_END);
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

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > UINT8_MAX) {
				compiler_panic(
					&parser,
					"Functions cannot have more than "
					"255 arguments",
					ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(
				"Expected parameter name");
			define_variable(constant);
		} while (match(TOKEN_COMMA));
	}

	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	ObjectFunction *function = end_compiler();
	emit_words(OP_CLOSURE, make_constant(OBJECT_VAL(function)));

	for (int i = 0; i < function->upvalue_count; i++) {
		emit_word(compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler.upvalues[i].index);
	}
}

static void fn_declaration(void)
{
	const uint16_t global = parse_variable("Expected function name");
	mark_initialized();
	function(TYPE_FUNCTION);
	define_variable(global);
}

static void anonymous_function(bool can_assign)
{
	(void)can_assign;
	Compiler compiler;
	init_compiler(&compiler, TYPE_ANONYMOUS, current->owner);
	begin_scope();
	consume(TOKEN_LEFT_PAREN, "Expected '(' to start argument list");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > UINT8_MAX) {
				compiler_panic(
					&parser,
					"Functions cannot have more than "
					"255 arguments",
					ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(
				"Expected parameter name");
			define_variable(constant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	consume(TOKEN_LEFT_BRACE, "Expected '{' before function body");
	block();
	ObjectFunction *function = end_compiler();

	const uint16_t constantIndex = make_constant(OBJECT_VAL(function));
	emit_words(OP_ANON_FUNCTION, constantIndex);

	for (int i = 0; i < function->upvalue_count; i++) {
		emit_word(compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler.upvalues[i].index);
	}
}

static void create_array(const OpCode creationOpCode, const char *typeName)
{
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_SQUARE)) {
		do {
			expression();
			if (elementCount >= UINT16_MAX) {
				char buffer[64];
				snprintf(buffer, sizeof(buffer),
					 "Too many elements in %s literal.",
					 typeName);
				compiler_panic(&parser, buffer,
					       COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_SQUARE,
			"Expected ']' after array elements");
	}
	emit_word(creationOpCode);
	emit_word(elementCount);
}

static void array_literal(bool can_assign)
{
	(void)can_assign;
	create_array(OP_ARRAY, "array");
}

static void table_literal(bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			consume(TOKEN_COLON, "Expected ':' after <table> key");
			expression();
			if (elementCount >= UINT16_MAX) {
				compiler_panic(
					&parser,
					"Too many elements in 'table' literal.",
					SYNTAX);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after table elements");
	}
	emit_word(OP_TABLE);
	emit_word(elementCount);
}

/**
 * Parses a collection index access expression (e.g., array[index]).
 *
 * @param can_assign Whether the collection index expression can be the target
 * of an assignment.
 */
static void collection_index(const bool can_assign)
{
	expression();
	consume(TOKEN_RIGHT_SQUARE, "Expected ']' after index");

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		emit_word(OP_SET_COLLECTION);
	} else {
		emit_word(OP_GET_COLLECTION);
	}
}

static void var_declaration(void)
{
	const uint16_t global = parse_variable("Expected Variable Name.");

	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emit_word(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
	define_variable(global);
}

static void expression_statement(void)
{
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emit_word(OP_POP);
}

static void while_statement(void)
{
	begin_scope();
	const int loopStart = current_chunk()->count;

	push_loop_context(LOOP_WHILE, loopStart);

	expression();
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
		consume(TOKEN_SEMICOLON, "Expected ';' after loop condition");

		// Jump out of the loop if the condition is false
		exitJump = emit_jump(OP_JUMP_IF_FALSE);
		emit_word(OP_POP); // condition
	}

	const int bodyJump = emit_jump(OP_JUMP);
	const int incrementStart = current_chunk()->count;

	push_loop_context(LOOP_FOR, incrementStart);

	expression();
	emit_word(OP_POP);

	emit_loop(loopStart); // main loop that takes us back to the top of the
			      // for loop
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
			       "Cannot use <return> outside of a function",
			       SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		emit_return();
	} else {
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after return value");
		emit_word(OP_RETURN);
	}
}

static void use_statement(void)
{
	bool hasParen = false;
	if (parser.current.type == TOKEN_LEFT_PAREN) {
		consume(TOKEN_LEFT_PAREN, "Expected '(' after use statement");
		hasParen = true;
	}

	uint16_t nameCount = 0;
	uint16_t names[UINT8_MAX];
	uint16_t aliases[UINT8_MAX];
	bool aliasPresence[UINT8_MAX];

	for (int i = 0; i < UINT8_MAX; i++) {
		names[i] = 0;
		aliases[i] = 0;
		aliasPresence[i] = false;
	}

	do {
		if (nameCount >= UINT8_MAX) {
			compiler_panic(&parser,
				       "Cannot import more than 255 names from "
				       "another module.",
				       IMPORT_EXTENT);
		}
		consume(TOKEN_IDENTIFIER,
			"Expected name to import from module");

		uint16_t name;
		if (parser.current.type == TOKEN_AS) {
			name = identifier_constant(&parser.previous);
			consume(TOKEN_AS, "Expected 'as' keyword.");
			consume(TOKEN_IDENTIFIER,
				"Expected name to alias import from external "
				"module.");
			const uint16_t alias = identifier_constant(
				&parser.previous);
			aliases[nameCount] = alias;
			aliasPresence[nameCount] = true;
		} else {
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
	consume(TOKEN_STRING, "Expected string literal for module name");

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

	consume(TOKEN_SEMICOLON, "Expected semicolon after import statement.");
}

static void struct_declaration(void)
{
	consume(TOKEN_IDENTIFIER, "Expected class name");
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

	declare_variable();

	const uint16_t structConstant = make_constant(OBJECT_VAL(structObject));
	emit_words(OP_STRUCT, structConstant);

	define_variable(nameConstant);

	consume(TOKEN_LEFT_BRACE, "Expected '{' before struct body");
	int fieldCount = 0;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount >= UINT16_MAX) {
				compiler_panic(&parser,
					       "Too many fields in struct",
					       SYNTAX);
				break;
			}

			consume(TOKEN_IDENTIFIER, "Expected field name");
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
					       "declaration",
					       SYNTAX);
				break;
			}

			table_set(current->owner, &structObject->fields,
				  fieldName, INT_VAL(fieldCount));
			fieldCount++;
		} while (match(TOKEN_COMMA));
	}
	if (fieldCount != 0) {
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after struct body");
	}
	GC_PROTECT_END(current->owner->current_module_record);
}

static void result_unwrap(bool can_assign)
{
	(void)can_assign;
	emit_word(OP_UNWRAP);
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
	} else {
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after give statement.");
	}

	emit_word(OP_GIVE);
}

/**
 * Parses a match expression.
 */
static void match_expression(bool can_assign)
{
	(void)can_assign;
	begin_match_scope();
	expression(); // compile match target
	consume(TOKEN_LEFT_BRACE, "Expected '{' after match target.");

	int *endJumps = ALLOCATE(current->owner, int, 8);
	int jumpCount = 0;
	int jumpCapacity = 8;

	emit_word(OP_MATCH);
	bool hasDefault = false;
	bool hasOkPattern = false;
	bool hasErrPattern = false;
	uint16_t bindingSlot = UINT16_MAX;
	bool hasBinding = false;

	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		int jumpIfNotMatch = -1;
		bindingSlot = UINT16_MAX;
		hasBinding = false;

		if (match(TOKEN_DEFAULT)) {
			if (hasDefault) {
				compiler_panic(&parser,
					       "Cannot have multiple default "
					       "patterns.",
					       SYNTAX);
			}
			hasDefault = true;
		} else if (match(TOKEN_OK)) {
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
				mark_initialized();
				consume(TOKEN_RIGHT_PAREN,
					"Expected ')' after identifier.");
			}
		} else if (match(TOKEN_ERR)) {
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
				mark_initialized();
				consume(TOKEN_RIGHT_PAREN,
					"Expected ')' after identifier.");
			}
		} else {
			expression();
			jumpIfNotMatch = emit_jump(OP_MATCH_JUMP);
		}

		consume(TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

		if (bindingSlot != UINT16_MAX) {
			emit_words(OP_RESULT_BIND, bindingSlot);
		}

		// Compile match arm body

		if (match(TOKEN_LEFT_BRACE)) {
			block();
		} else if (match(TOKEN_GIVE)) {
			if (match(TOKEN_SEMICOLON)) {
				emit_word(OP_NIL);
			} else {
				expression();
				consume(TOKEN_SEMICOLON,
					"Expected ';' after give expression.");
			}
			emit_word(OP_GIVE);
		} else {
			expression();
			consume(TOKEN_SEMICOLON,
				"Expected ';' after expression.");
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
				       "'Err' patterns, or "
				       "include a default case.",
				       SYNTAX);
		}
	} else if (!hasDefault) {
		compiler_panic(
			&parser,
			"'match' expression must have default case 'default'.",
			SYNTAX);
	}

	for (int i = 0; i < jumpCount; i++) {
		patch_jump(endJumps[i]);
	}

	emit_word(OP_MATCH_END);

	FREE_ARRAY(current->owner, int, endJumps, jumpCapacity);
	consume(TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
	end_match_scope();
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

/**
 * Parses a declaration, which can be a variable, function, or class
 * declaration.
 */
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

/**
 * Parses a statement.
 */
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
	} else {
		expression_statement();
	}
}

/**
 * Parses a grouping expression.
 *
 * @param can_assign Whether the grouping expression can be the target of an
 * assignment.
 */
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
	} else {
		const int32_t integer = (int32_t)number;
		if ((double)integer == number) {
			emit_constant(INT_VAL(integer));
		} else {
			emit_constant(FLOAT_VAL(number));
		}
	}
}

/**
 * Processes an escape sequence within a string literal.
 *
 * @param escape The escape character (e.g., 'n', 't', '\\').
 * @param hasError A pointer to a boolean to indicate if an error occurred
 * during processing.
 * @return The processed escaped character, or '\0' if an error occurred.
 */
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

/**
 * Parses a string literal.
 *
 * @param can_assign Whether the string literal can be the target of an
 * assignment.
 */
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
	emit_constant(OBJECT_VAL(string));
}

/**
 * Parses a unary operator expression.
 *
 * @param can_assign Whether the unary expression can be the target of an
 * assignment.
 */
static void unary(bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = parser.previous.type;

	// compile the operand
	parse_precedence(PREC_UNARY);

	switch (operatorType) {
	case TOKEN_NOT:
		emit_word(OP_NOT);
		break;
	case TOKEN_MINUS:
		emit_word(OP_NEGATE);
		break;
	default:
		return; // unreachable
	}
}

static void typeof_expression(bool can_assign)
{
	(void)can_assign;
	parse_precedence(PREC_UNARY);
	emit_word(OP_TYPEOF);
}

/**
 * @brief Parse rules for each token type.
 *
 * Defines prefix and infix parsing functions and precedence for each token
 * type.
 */
ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, call, NULL, PREC_CALL},
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
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parse_precedence(const Precedence precedence)
{
	advance();
	const ParseFn prefixRule = get_rule(parser.previous.type)->prefix;
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

/**
 * Retrieves the parse rule for a given token type.
 *
 * @param type The token type to get the rule for.
 * @return A pointer to the ParseRule struct for the given token type.
 */
static ParseRule *get_rule(const CruxTokenType type)
{
	return &rules[type]; // Returns the rule at the given index
}

/**
 * Compiles source code into bytecode.
 *
 * @param vm The virtual machine instance.
 * @param source The source code string to compile.
 * @return A pointer to the compiled function object if compilation succeeds,
 * NULL otherwise.
 */
ObjectFunction *compile(VM *vm, char *source)
{
	init_scanner(source);
	Compiler compiler;
	init_compiler(&compiler, TYPE_SCRIPT, vm);

	parser.had_error = false;
	parser.panic_mode = false;
	parser.source = source;

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

/**
 * Marks compiler-related objects as reachable for garbage collection.
 *
 * Traverses the compiler chain and marks the function objects associated with
 * each compiler.
 *
 * @param vm The virtual machine instance.
 */
void mark_compiler_roots(VM *vm)
{
	const Compiler *compiler = current;
	while (compiler != NULL) {
		mark_object(vm, (CruxObject *)compiler->function);
		compiler = (Compiler *)compiler->enclosing;
	}
}
