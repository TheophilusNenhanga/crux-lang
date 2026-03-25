#include <string.h>

#include "chunk.h"
#include "compiler.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
#include "value.h"

Chunk *current_chunk(const Compiler *compiler)
{
	return &compiler->function->chunk;
}

void advance(const Compiler *compiler)
{
	compiler->parser->previous = compiler->parser->current;
	for (;;) {
		compiler->parser->current = scan_token(compiler->parser->scanner);
		if (compiler->parser->current.type != CRUX_TOKEN_ERROR)
			break;
		compiler_panic(compiler->parser, compiler->parser->current.start, SYNTAX);
	}
}

void consume(const Compiler *compiler, const CruxTokenType type, const char *message)
{
	if (compiler->parser->current.type == type) {
		advance(compiler);
		return;
	}
	compiler_panic_at_current(compiler->parser, message, SYNTAX);
}

bool check(const Compiler *compiler, const CruxTokenType type)
{
	return compiler->parser->current.type == type;
}

bool match(const Compiler *compiler, const CruxTokenType type)
{
	if (!check(compiler, type))
		return false;
	advance(compiler);
	return true;
}

bool match_type_name(const Compiler *compiler)
{
	CruxTokenType type_tokens[] = {
		CRUX_TOKEN_NIL_TYPE,	CRUX_TOKEN_BOOL_TYPE,	  CRUX_TOKEN_INT_TYPE,	  CRUX_TOKEN_FLOAT_TYPE,
		CRUX_TOKEN_STRING_TYPE, CRUX_TOKEN_ARRAY_TYPE,	  CRUX_TOKEN_TABLE_TYPE,  CRUX_TOKEN_ERROR_TYPE,
		CRUX_TOKEN_RESULT_TYPE, CRUX_TOKEN_RANDOM_TYPE,	  CRUX_TOKEN_FILE_TYPE,	  CRUX_TOKEN_STRUCT_TYPE,
		CRUX_TOKEN_VECTOR_TYPE, CRUX_TOKEN_COMPLEX_TYPE,  CRUX_TOKEN_MATRIX_TYPE, CRUX_TOKEN_SET_TYPE,
		CRUX_TOKEN_TUPLE_TYPE,	CRUX_TOKEN_BUFFER_TYPE,	  CRUX_TOKEN_RANGE_TYPE,  CRUX_TOKEN_ANY_TYPE,
		CRUX_TOKEN_NEVER_TYPE,	CRUX_TOKEN_ITERATOR_TYPE, CRUX_TOKEN_OPTION_TYPE,
	};
	int len = (int)(sizeof(type_tokens) / sizeof(type_tokens[0]));
	for (int i = 0; i < len; i++) {
		if (match(compiler, type_tokens[i]))
			return true;
	}
	return false;
}

TypeMask type_token_type_to_mask(CruxTokenType token_type)
{
	switch (token_type) {
	case CRUX_TOKEN_NIL_TYPE: {
		return NIL_TYPE;
	}
	case CRUX_TOKEN_BOOL_TYPE: {
		return BOOL_TYPE;
	}
	case CRUX_TOKEN_INT_TYPE: {
		return INT_TYPE;
	}
	case CRUX_TOKEN_FLOAT_TYPE: {
		return FLOAT_TYPE;
	}
	case CRUX_TOKEN_STRING_TYPE: {
		return STRING_TYPE;
	}
	case CRUX_TOKEN_ARRAY_TYPE: {
		return ARRAY_TYPE;
	}
	case CRUX_TOKEN_TABLE_TYPE: {
		return TABLE_TYPE;
	}
	case CRUX_TOKEN_ERROR_TYPE: {
		return ERROR_TYPE;
	}
	case CRUX_TOKEN_RESULT_TYPE: {
		return RESULT_TYPE;
	}
	case CRUX_TOKEN_RANDOM_TYPE: {
		return RANDOM_TYPE;
	}
	case CRUX_TOKEN_FILE_TYPE: {
		return FILE_TYPE;
	}
	case CRUX_TOKEN_STRUCT_TYPE: {
		return STRUCT_TYPE;
	}
	case CRUX_TOKEN_VECTOR_TYPE: {
		return VECTOR_TYPE;
	}
	case CRUX_TOKEN_COMPLEX_TYPE: {
		return COMPLEX_TYPE;
	}
	case CRUX_TOKEN_MATRIX_TYPE: {
		return MATRIX_TYPE;
	}
	case CRUX_TOKEN_SET_TYPE: {
		return SET_TYPE;
	}
	case CRUX_TOKEN_TUPLE_TYPE: {
		return TUPLE_TYPE;
	}
	case CRUX_TOKEN_BUFFER_TYPE: {
		return BUFFER_TYPE;
	}
	case CRUX_TOKEN_RANGE_TYPE: {
		return RANGE_TYPE;
	}
	case CRUX_TOKEN_ANY_TYPE: {
		return ANY_TYPE;
	}
	case CRUX_TOKEN_NEVER_TYPE: {
		return NEVER_TYPE;
	}
	case CRUX_TOKEN_ITERATOR_TYPE: {
		return ITERATOR_TYPE;
	}
	case CRUX_TOKEN_OPTION_TYPE: {
		return OPTION_TYPE;
	}
	default: {
		return ANY_TYPE;
	}
	}
}

bool is_identifier_like(const CruxTokenType type)
{
	switch (type) {
	case CRUX_TOKEN_IDENTIFIER:
	// Type names that are also valid constructor/import names:
	case CRUX_TOKEN_RANDOM_TYPE:
	case CRUX_TOKEN_BUFFER_TYPE:
	case CRUX_TOKEN_RANGE_TYPE:
	case CRUX_TOKEN_VECTOR_TYPE:
	case CRUX_TOKEN_MATRIX_TYPE:
	case CRUX_TOKEN_COMPLEX_TYPE:
	case CRUX_TOKEN_SET_TYPE:
	case CRUX_TOKEN_TUPLE_TYPE:
	case CRUX_TOKEN_FILE_TYPE:
		return true;
	default:
		return false;
	}
}

bool check_identifier_like(const Compiler *compiler)
{
	return is_identifier_like(compiler->parser->current.type);
}

void consume_identifier_like(const Compiler *compiler, const char *message)
{
	if (check_identifier_like(compiler)) {
		advance(compiler);
		return;
	}
	compiler_panic(compiler->parser, message, SYNTAX);
}

const ObjectNativeCallable *lookup_stdlib_method(const Compiler *compiler, const Table *type_table,
												 const Token *name_token)
{
	const ObjectString *name = copy_string(compiler->owner, name_token->start, name_token->length);
	Value value;
	if (!table_get(type_table, name, &value))
		return NULL;
	if (!IS_CRUX_NATIVE_CALLABLE(value))
		return NULL;
	return AS_CRUX_NATIVE_CALLABLE(value);
}

bool check_previous_op_code(const Compiler *compiler, const OpCode op, const int distance)
{
	const Chunk *chunk = current_chunk(compiler);
	if (chunk->count == 0)
		return false;
	if (chunk->count - distance < 0)
		return false;
	return chunk->code[chunk->count - distance] == op;
}

bool set_previous_op_code(const Compiler *compiler, const OpCode op, const int distance)
{
	const Chunk *chunk = current_chunk(compiler);
	if (chunk->code == 0)
		return false;
	if (chunk->code - distance < 0)
		return false;
	chunk->code[chunk->count - distance] = op;
	return true;
}

void emit_word(const Compiler *compiler, const uint16_t word)
{
	write_chunk(compiler->owner, current_chunk(compiler), word, compiler->parser->previous.line);
}

void emit_words(const Compiler *compiler, const uint16_t word1, const uint16_t word2)
{
	emit_word(compiler, word1);
	emit_word(compiler, word2);
}

bool is_valid_table_key_type(ObjectTypeRecord *type)
{
	if (!type)
		return false;
	if (type->base_type == ANY_TYPE)
		return true;
	return (type->base_type & HASHABLE_TYPE) != 0;
}

void emit_loop(const Compiler *compiler, const int loopStart)
{
	emit_word(compiler, OP_LOOP);
	// +1 takes into account the size of the OP_LOOP
	const int offset = current_chunk(compiler)->count - loopStart + 1;
	if (offset > UINT16_MAX) {
		compiler_panic(compiler->parser, "Loop body too large.", LOOP_EXTENT);
	}
	emit_word(compiler, offset);
}

int emit_jump(const Compiler *compiler, const uint16_t instruction)
{
	emit_word(compiler, instruction);
	emit_word(compiler, 0xffff);
	return current_chunk(compiler)->count - 1;
}

void patch_jump(const Compiler *compiler, const int offset)
{
	// -1 to adjust for the bytecode for the jump offset itself
	const int jump = current_chunk(compiler)->count - offset - 1;
	if (jump > UINT16_MAX) {
		compiler_panic(compiler->parser, "Too much code to jump over.", BRANCH_EXTENT);
	}
	current_chunk(compiler)->code[offset] = (uint16_t)jump;
}

void emit_return(const Compiler *compiler)
{
	emit_word(compiler, OP_NIL_RETURN);
}

uint16_t make_constant(const Compiler *compiler, const Value value)
{
	const int constant = add_constant(compiler->owner, current_chunk(compiler), value);
	if (constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants in one chunk.", LIMIT);
		return 0;
	}
	return (uint16_t)constant;
}

void emit_constant(const Compiler *compiler, const Value value)
{
	const uint16_t constant = make_constant(compiler, value);
	if (constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants", SYNTAX);
	}
	emit_words(compiler, OP_CONSTANT, constant);
}

void push_loop_context(Compiler *compiler, const LoopType type, const int continueTarget)
{
	if (compiler->loop_depth >= UINT8_MAX) {
		compiler_panic(compiler->parser, "Too many nested loops.", LOOP_EXTENT);
		return;
	}

	LoopContext *context = &compiler->loop_stack[compiler->loop_depth++];
	context->type = type;
	context->continue_target = continueTarget;
	context->break_jumps = NULL;
	context->scope_depth = compiler->scope_depth;
}

void pop_loop_context(Compiler *compiler)
{
	if (compiler->loop_depth <= 0) {
		return;
	}

	const LoopContext *context = &compiler->loop_stack[--compiler->loop_depth];

	// Patch all break jumps to jump to current position
	BreakJump *breakJump = context->break_jumps;
	while (breakJump != NULL) {
		patch_jump(compiler, breakJump->jumpOffset);
		BreakJump *next = breakJump->next;
		FREE(compiler->owner, BreakJump, breakJump);
		breakJump = next;
	}
}

void add_break_jump(Compiler *compiler, const int jumpOffset)
{
	if (compiler->loop_depth <= 0) {
		compiler_panic(compiler->parser, "Cannot use 'break' outside of a loop.", SYNTAX);
		return;
	}

	LoopContext *context = &compiler->loop_stack[compiler->loop_depth - 1];

	BreakJump *breakJump = ALLOCATE(compiler->owner, BreakJump, 1);
	breakJump->jumpOffset = jumpOffset;
	breakJump->next = context->break_jumps;
	context->break_jumps = breakJump;
}

void push_type_record(Compiler *compiler, ObjectTypeRecord *type_record)
{
	compiler->type_stack[compiler->type_stack_count++] = type_record;
}

ObjectTypeRecord *pop_type_record(Compiler *compiler)
{
	if (compiler->type_stack_count <= 0) {
		return T_ANY; // return Any type on underflow
	}
	return compiler->type_stack[--compiler->type_stack_count];
}

ObjectTypeRecord *peek_type_record(const Compiler *compiler)
{
	if (compiler->type_stack_count <= 0)
		return NULL;
	return compiler->type_stack[compiler->type_stack_count - 1];
}

uint16_t identifier_constant(const Compiler *compiler, const Token *name)
{
	return make_constant(compiler, OBJECT_VAL(copy_string(compiler->owner, name->start, name->length)));
}

void begin_scope(Compiler *compiler)
{
	compiler->scope_depth++;
}

void cleanupLocalsToDepth(Compiler *compiler, const int targetDepth)
{
	uint16_t to_pop_count = 0;
	while (compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > targetDepth) {
		if (compiler->locals[compiler->local_count - 1].is_captured) {
			if (to_pop_count > 0) {
				if (to_pop_count == 1) {
					emit_word(compiler, OP_POP);
				} else {
					emit_words(compiler, OP_POP_N, to_pop_count);
				}
			}
			emit_word(compiler, OP_CLOSE_UPVALUE);
		} else {
			to_pop_count++;
		}
		compiler->local_count--;
	}
	if (to_pop_count > 0) {
		if (to_pop_count == 1) {
			emit_word(compiler, OP_POP);
		} else {
			emit_words(compiler, OP_POP_N, to_pop_count);
		}
	}
}

void emit_cleanup_for_jump(const Compiler *compiler, const int targetDepth)
{
	uint16_t to_pop_count = 0;
	int i = compiler->local_count - 1;
	while (i >= 0 && compiler->locals[i].depth > targetDepth) {
		if (compiler->locals[i].is_captured) {
			if (to_pop_count > 0) {
				if (to_pop_count == 1) {
					emit_word(compiler, OP_POP);
				} else {
					emit_words(compiler, OP_POP_N, to_pop_count);
				}
			}
			to_pop_count = 0;
			emit_word(compiler, OP_CLOSE_UPVALUE);
		} else {
			to_pop_count++;
		}
		i--;
	}
	if (to_pop_count > 0) {
		if (to_pop_count == 1) {
			emit_word(compiler, OP_POP);
		} else {
			emit_words(compiler, OP_POP_N, to_pop_count);
		}
	}
}

void end_scope(Compiler *compiler)
{
	compiler->scope_depth--;
	cleanupLocalsToDepth(compiler, compiler->scope_depth);
}

bool identifiers_equal(const Token *a, const Token *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

int resolve_local(const Compiler *compiler, const Token *name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		const Local *local = &compiler->locals[i];
		if (identifiers_equal(name, &local->name)) {
			if (local->depth == -1) {
				compiler_panic(compiler->parser, "Cannot read local variable in its own initializer", NAME);
			}
			return i;
		}
	}
	return -1;
}

int get_current_continue_target(const Compiler *compiler)
{
	if (compiler->loop_depth <= 0) {
		compiler_panic(compiler->parser, "Cannot use 'continue' outside of a loop.", SYNTAX);
		return -1;
	}

	return compiler->loop_stack[compiler->loop_depth - 1].continue_target;
}

int add_upvalue(Compiler *compiler, const uint16_t index, const bool isLocal, ObjectTypeRecord *type)
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
		compiler_panic(compiler->parser, "Too many closure variables in function.", CLOSURE_EXTENT);
		return 0;
	}

	compiler->upvalues[upvalueCount].is_local = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	compiler->upvalues[upvalueCount].type = type;
	return compiler->function->upvalue_count++;
}

int resolve_upvalue(Compiler *compiler, Token *name)
{
	if (compiler->enclosing == NULL)
		return -1;

	const int local = resolve_local(compiler->enclosing, name);
	if (local != -1) {
		compiler->enclosing->locals[local].is_captured = true;
		ObjectTypeRecord *type = compiler->enclosing->locals[local].type;
		return add_upvalue(compiler, (uint16_t)local, true, type);
	}

	const int upValue = resolve_upvalue(compiler->enclosing, name);
	if (upValue != -1) {
		ObjectTypeRecord *type = compiler->enclosing->upvalues[upValue].type;
		return add_upvalue(compiler, (uint16_t)upValue, false, type);
	}

	return -1;
}

void add_local(Compiler *compiler, const Token name, ObjectTypeRecord *type)
{
	if (compiler->local_count == UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many local variables in function.", LOCAL_EXTENT);
		return;
	}

	Local *local = &compiler->locals[compiler->local_count++];
	local->name = name;
	local->depth = -1;
	local->is_captured = false;
	local->type = type; // NULL until the initializer is complete
}

/**
 * Ensures that the given local name is available in the current scope.
 */
void ensure_local_name_available(const Compiler *compiler, const Token name)
{
	for (int i = compiler->local_count - 1; i >= 0; i--) {
		const Local *local = &compiler->locals[i];
		if (local->depth != -1 && local->depth < compiler->scope_depth) {
			break;
		}
		if (identifiers_equal(&name, &local->name)) {
			compiler_panic(compiler->parser, "Cannot redefine variable in the same scope", NAME);
			return;
		}
	}
}

void declare_named_variable(Compiler *compiler, Token name, ObjectTypeRecord *type)
{
	ensure_local_name_available(compiler, name);
	add_local(compiler, name, type);
}

void declare_variable(Compiler *compiler)
{
	if (compiler->scope_depth == 0)
		return;
	const Token *name = &compiler->parser->previous;
	declare_named_variable(compiler, *name, NULL);
}

void mark_initialized(Compiler *compiler)
{
	if (compiler->scope_depth == 0)
		return;
	compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

uint16_t parse_variable(Compiler *compiler, const char *errorMessage)
{
	consume(compiler, CRUX_TOKEN_IDENTIFIER, errorMessage);
	declare_variable(compiler);
	if (compiler->scope_depth > 0)
		return 0;
	return identifier_constant(compiler, &compiler->parser->previous);
}

int match_compound_op(const Compiler *compiler)
{
	if (match(compiler, CRUX_TOKEN_PLUS_EQUAL))
		return COMPOUND_OP_PLUS;
	if (match(compiler, CRUX_TOKEN_MINUS_EQUAL))
		return COMPOUND_OP_MINUS;
	if (match(compiler, CRUX_TOKEN_STAR_EQUAL))
		return COMPOUND_OP_STAR;
	if (match(compiler, CRUX_TOKEN_SLASH_EQUAL))
		return COMPOUND_OP_SLASH;
	if (match(compiler, CRUX_TOKEN_BACK_SLASH_EQUAL))
		return COMPOUND_OP_BACK_SLASH;
	if (match(compiler, CRUX_TOKEN_PERCENT_EQUAL))
		return COMPOUND_OP_PERCENT;
	return -1;
}

void check_compound_type_math(const Compiler *compiler, ObjectTypeRecord *lhs_type, ObjectTypeRecord *rhs_type,
							  const int op)
{
	if (!lhs_type || !rhs_type || lhs_type->base_type == ANY_TYPE || rhs_type->base_type == ANY_TYPE)
		return;

	const bool lhs_num = lhs_type->base_type == INT_TYPE || lhs_type->base_type == FLOAT_TYPE;
	const bool rhs_num = rhs_type->base_type == INT_TYPE || rhs_type->base_type == FLOAT_TYPE;

	if (op == COMPOUND_OP_BACK_SLASH || op == COMPOUND_OP_PERCENT) {
		if (lhs_type->base_type != INT_TYPE || rhs_type->base_type != INT_TYPE) {
			char left_name[128], right_name[128];
			type_record_name(lhs_type, left_name, sizeof(left_name));
			type_record_name(rhs_type, right_name, sizeof(right_name));
			compiler_panicf(compiler->parser, TYPE, "This compound operator requires Int operands, got '%s' and '%s'.",
							left_name, right_name);
		}
	} else if (!lhs_num || !rhs_num) {
		char left_name[128], right_name[128];
		type_record_name(lhs_type, left_name, sizeof(left_name));
		type_record_name(rhs_type, right_name, sizeof(right_name));
		compiler_panicf(compiler->parser, TYPE, "Compound assignment requires numeric operands, got '%s' and '%s'.",
						left_name, right_name);
	}
}

void define_variable(Compiler *compiler, const uint16_t global, bool is_public)
{
	if (compiler->scope_depth > 0) {
		mark_initialized(compiler);
		return;
	}
	if (global >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many variables.", SYNTAX);
	}
	is_public ? emit_words(compiler, OP_DEFINE_PUB_GLOBAL, global) : emit_words(compiler, OP_DEFINE_GLOBAL, global);
}

OpCode get_compound_opcode(const Compiler *compiler, const OpCode setOp, const int op)
{
	const OpCode local_ops[] = {OP_SET_LOCAL_PLUS,	OP_SET_LOCAL_MINUS,		 OP_SET_LOCAL_STAR,
								OP_SET_LOCAL_SLASH, OP_SET_LOCAL_INT_DIVIDE, OP_SET_LOCAL_MODULUS};
	const OpCode upvalue_ops[] = {OP_SET_UPVALUE_PLUS,	OP_SET_UPVALUE_MINUS,	   OP_SET_UPVALUE_STAR,
								  OP_SET_UPVALUE_SLASH, OP_SET_UPVALUE_INT_DIVIDE, OP_SET_UPVALUE_MODULUS};
	const OpCode global_ops[] = {OP_SET_GLOBAL_PLUS,  OP_SET_GLOBAL_MINUS,		OP_SET_GLOBAL_STAR,
								 OP_SET_GLOBAL_SLASH, OP_SET_GLOBAL_INT_DIVIDE, OP_SET_GLOBAL_MODULUS};
	const OpCode property_ops[] = {OP_SET_PROPERTY_PLUS,  OP_SET_PROPERTY_MINUS,	  OP_SET_PROPERTY_STAR,
								   OP_SET_PROPERTY_SLASH, OP_SET_PROPERTY_INT_DIVIDE, OP_SET_PROPERTY_MODULUS};

	if (setOp == OP_SET_LOCAL)
		return local_ops[op];
	if (setOp == OP_SET_UPVALUE)
		return upvalue_ops[op];
	if (setOp == OP_SET_GLOBAL)
		return global_ops[op];
	if (setOp == OP_SET_PROPERTY)
		return property_ops[op];

	compiler_panic(compiler->parser, "Compiler Error: Failed to create bytecode for compound operation.", RUNTIME);
	return setOp;
}
