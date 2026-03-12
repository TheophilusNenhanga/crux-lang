#include "compiler.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "file_handler.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
#include "type_system.h"
#include "value.h"

static void expression(Compiler *compiler);

static void parse_precedence(Compiler *compiler, Precedence precedence);

static ParseRule *get_rule(CruxTokenType type);

static void binary(Compiler *compiler, bool can_assign);

static void unary(Compiler *compiler, bool can_assign);

static void grouping(Compiler *compiler, bool can_assign);

static void number(Compiler *compiler, bool can_assign);

static void statement(Compiler *compiler);

static void declaration(Compiler *compiler);

static ObjectModuleRecord *compile_module_statically(Compiler *compiler, ObjectString *path);

static Chunk *current_chunk(const Compiler *compiler)
{
	return &compiler->function->chunk;
}

static void advance(const Compiler *compiler)
{
	compiler->parser->previous = compiler->parser->current;
	for (;;) {
		compiler->parser->current = scan_token(compiler->parser->scanner);
		if (compiler->parser->current.type != TOKEN_ERROR)
			break;
		compiler_panic(compiler->parser, compiler->parser->current.start, SYNTAX);
	}
}

static void consume(const Compiler *compiler, const CruxTokenType type, const char *message)
{
	if (compiler->parser->current.type == type) {
		advance(compiler);
		return;
	}
	compiler_panic_at_current(compiler->parser, message, SYNTAX);
}

static bool check(const Compiler *compiler, const CruxTokenType type)
{
	return compiler->parser->current.type == type;
}

static bool match(const Compiler *compiler, const CruxTokenType type)
{
	if (!check(compiler, type))
		return false;
	advance(compiler);
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

static bool check_identifier_like(const Compiler *compiler)
{
	return is_identifier_like(compiler->parser->current.type);
}

static void consume_identifier_like(const Compiler *compiler, const char *message)
{
	if (check_identifier_like(compiler)) {
		advance(compiler);
		return;
	}
	compiler_panic(compiler->parser, message, SYNTAX);
}

/**
 * lookup a callable in a vm stdlib table
 * @param compiler The current compiler
 * @param type_table The vm owned type table to look for a callable from
 * @param name_token The name of the callable
 * @return the callable otherwise NULL if no callable found
 */
static const ObjectNativeCallable *lookup_stdlib_method(const Compiler *compiler, const Table *type_table,
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

static void emit_word(const Compiler *compiler, const uint16_t word)
{
	write_chunk(compiler->owner, current_chunk(compiler), word, compiler->parser->previous.line);
}

static void emit_words(const Compiler *compiler, const uint16_t word1, const uint16_t word2)
{
	emit_word(compiler, word1);
	emit_word(compiler, word2);
}

static bool is_valid_table_key_type(ObjectTypeRecord *type)
{
	if (!type)
		return false;
	if (type->base_type == ANY_TYPE)
		return true;
	return (type->base_type & HASHABLE_TYPE) != 0;
}

#define T_ANY new_type_rec(compiler->owner, ANY_TYPE)

ObjectTypeRecord *parse_type_record(Compiler *compiler)
{
	ObjectTypeRecord *type_record = NULL;

	if (match(compiler, TOKEN_INT_TYPE)) {
		type_record = new_type_rec(compiler->owner, INT_TYPE);
	} else if (match(compiler, TOKEN_FLOAT_TYPE)) {
		type_record = new_type_rec(compiler->owner, FLOAT_TYPE);
	} else if (match(compiler, TOKEN_BOOL_TYPE)) {
		type_record = new_type_rec(compiler->owner, BOOL_TYPE);
	} else if (match(compiler, TOKEN_STRING_TYPE)) {
		type_record = new_type_rec(compiler->owner, STRING_TYPE);
	} else if (match(compiler, TOKEN_NIL_TYPE)) {
		type_record = new_type_rec(compiler->owner, NIL_TYPE);
	} else if (match(compiler, TOKEN_ANY_TYPE)) {
		type_record = T_ANY;
	} else if (match(compiler, TOKEN_ARRAY_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, ARRAY_TYPE);
			type_record->as.array_type.element_type = parse_type_record(compiler);
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after array element type.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for array type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_TABLE_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, TABLE_TYPE);
			ObjectTypeRecord *key_type = parse_type_record(compiler);
			if (!is_valid_table_key_type(key_type)) {
				char got[64];
				type_record_name(key_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE,
								"Table key type '%s' is not hashable. Keys must be Int, Float, String, Bool, or Nil.",
								got);
			}
			type_record->as.table_type.key_type = key_type;
			consume(compiler, TOKEN_COMMA, "Expected ',' after key type.");
			type_record->as.table_type.value_type = parse_type_record(compiler);
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after table element type.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for table type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_VECTOR_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, VECTOR_TYPE);

			// consuming a generic vector type
			if (match(compiler, TOKEN_RIGHT_SQUARE)) {
				type_record->as.vector_type.dimensions = -1;
				return type_record;
			}

			consume(compiler, TOKEN_INT, "Expected 'int' for vector dimensions.");
			const int dimensions = (int)strtol(compiler->parser->previous.start, NULL, 10);
			type_record->as.vector_type.dimensions = dimensions;
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after vector element type.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for vector type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_MATRIX_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, MATRIX_TYPE);
			if (match(compiler, TOKEN_COMMA)) {
				if (match(compiler, TOKEN_RIGHT_SQUARE)) {
					type_record->as.matrix_type.cols = -1;
					type_record->as.matrix_type.rows = -1;
					return type_record;
				} else {
					compiler_panic(compiler->parser, "Expected ']' to end definition of generic matrix type", SYNTAX);
					type_record = new_type_rec(compiler->owner, ANY_TYPE);
				}
			}

			consume(compiler, TOKEN_INT, "Expected 'int' for matrix dimensions.");
			const int row_dim = (int)strtol(compiler->parser->previous.start, NULL, 10);
			consume(compiler, TOKEN_COMMA, "Expected ',' after row dimension.");
			const int col_dim = (int)strtol(compiler->parser->previous.start, NULL, 10);
			type_record->as.matrix_type.rows = row_dim;
			type_record->as.matrix_type.cols = col_dim;
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after matrix element type.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for matrix type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_BUFFER_TYPE)) {
		type_record = new_type_rec(compiler->owner, BUFFER_TYPE);
	} else if (match(compiler, TOKEN_ERROR_TYPE)) {
		type_record = new_type_rec(compiler->owner, ERROR_TYPE);
	} else if (match(compiler, TOKEN_RESULT_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			ObjectTypeRecord *value_type = parse_type_record(compiler);
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after result value type.");
			type_record = new_result_type_rec(compiler->owner, value_type);
		} else {
			compiler_panic(compiler->parser, "Expected '[' for result type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_RANGE_TYPE)) {
		type_record = new_type_rec(compiler->owner, RANGE_TYPE);
	} else if (match(compiler, TOKEN_TUPLE_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, TUPLE_TYPE);
			type_record->as.tuple_type.element_type = parse_type_record(compiler);
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after tuple element type.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for tuple type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, TOKEN_COMPLEX_TYPE)) {
		type_record = new_type_rec(compiler->owner, COMPLEX_TYPE);
	} else if (match(compiler, TOKEN_LEFT_PAREN)) {
		int param_capacity = 4;
		int param_count = 0;
		// TODO: make this use GC allocation functions
		ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
		if (!param_types) {
			compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
			return T_ANY;
		}

		do {
			ObjectTypeRecord *inner = parse_type_record(compiler);
			if (!inner) {
				compiler_panic(compiler->parser, "Expected type.", TYPE);
				inner = new_type_rec(compiler->owner, ANY_TYPE);
			}
			if (param_count == param_capacity) {
				param_capacity = GROW_CAPACITY(param_capacity);
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count,
													  param_capacity);
				if (!grown) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count);
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					return new_type_rec(compiler->owner, ANY_TYPE);
				}
				param_types = grown;
			}
			param_types[param_count++] = inner;
		} while (match(compiler, TOKEN_COMMA));
		consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' to end function argument types.");

		consume(compiler, TOKEN_ARROW, "Expected '->' to separate function argument types from return type.");
		ObjectTypeRecord *return_type = parse_type_record(compiler);
		if (!return_type) {
			compiler_panic(compiler->parser, "Expected type.", TYPE);
			FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity);
			return T_ANY;
		}

		// Shrink to exact size
		if (param_count < param_capacity) {
			param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);
		}

		type_record = new_type_rec(compiler->owner, FUNCTION_TYPE);
		type_record->as.function_type.arg_types = param_types;
		type_record->as.function_type.arg_count = param_count;
		type_record->as.function_type.return_type = return_type;
	} else if (match(compiler, TOKEN_SET_TYPE)) {
		if (match(compiler, TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, SET_TYPE);
			type_record->as.set_type.element_type = parse_type_record(compiler);
			consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after set element type.");
		} else {
			type_record = new_type_rec(compiler->owner, SET_TYPE);
		}
	} else if (match(compiler, TOKEN_RANDOM_TYPE)) {
		type_record = new_type_rec(compiler->owner, RANDOM_TYPE);
	} else if (match(compiler, TOKEN_FILE_TYPE)) {
		type_record = new_type_rec(compiler->owner, FILE_TYPE);
	} else if (check(compiler, TOKEN_IDENTIFIER)) {
		// Task 3: look up a user-defined struct (or other named) type.
		// Walk the compiler chain so inner functions see enclosing
		// struct declarations.
		advance(compiler);
		ObjectString *name_str = copy_string(compiler->owner, compiler->parser->previous.start,
											 compiler->parser->previous.length);
		ObjectTypeRecord *found = NULL;
		Compiler *comp = compiler;
		while (comp != NULL) {
			if (type_table_get(comp->type_table, name_str, &found))
				break;
			comp = comp->enclosing;
		}
		if (!found) {
			// unknown name — degrade to ANY_TYPE.
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		} else {
			type_record = found;
		}
	} else {
		compiler_panic(compiler->parser, "Expected type.", TYPE);
		type_record = T_ANY;
	}

	if (type_record && match(compiler, TOKEN_PIPE)) {
		int cap = 4;
		int count = 1;
		ObjectTypeRecord **variants = ALLOCATE(compiler->owner, ObjectTypeRecord *, cap);
		if (!variants) {
			compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
			return type_record; // return first variant as fallback
		}
		variants[0] = type_record;

		do {
			if (count == cap) {
				cap *= 2;
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, variants, count, cap);
				if (!grown) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, variants, count);
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					return type_record;
				}
				variants = grown;
			}
			variants[count++] = parse_type_record(compiler);
		} while (match(compiler, TOKEN_PIPE));

		if (count < cap) {
			variants = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, variants, cap, count);
		}
		// element_names can be NULL for anonymous unions.
		type_record = new_union_type_rec(compiler->owner, variants, NULL, count);
	}

	return type_record;
}

/**
 * emits an OP_LOOP instruction
 * @param loopStart The starting point of the loop
 */
static void emit_loop(Compiler *compiler, const int loopStart)
{
	emit_word(compiler, OP_LOOP);
	// +1 takes into account the size of the OP_LOOP
	const int offset = current_chunk(compiler)->count - loopStart + 1;
	if (offset > UINT16_MAX) {
		compiler_panic(compiler->parser, "Loop body too large.", LOOP_EXTENT);
	}
	emit_word(compiler, offset);
}

/**
 * Emits a jump instruction and placeholders for the jump offset.
 *
 * @param instruction The opcode for the jump instruction (e.g., OP_JUMP,
 * OP_JUMP_IF_FALSE).
 * @return The index of the jump instruction in the bytecode, used for patching
 * later.
 */
static int emit_jump(Compiler *compiler, const uint16_t instruction)
{
	emit_word(compiler, instruction);
	emit_word(compiler, 0xffff);
	return current_chunk(compiler)->count - 1;
}

/**
 * Patches a jump instruction with the calculated offset.
 */
static void patch_jump(Compiler *compiler, const int offset)
{
	// -1 to adjust for the bytecode for the jump offset itself
	const int jump = current_chunk(compiler)->count - offset - 1;
	if (jump > UINT16_MAX) {
		compiler_panic(compiler->parser, "Too much code to jump over.", BRANCH_EXTENT);
	}
	current_chunk(compiler)->code[offset] = (uint16_t)jump;
}

static void emit_return(Compiler *compiler)
{
	emit_word(compiler, OP_NIL_RETURN);
}

static uint16_t make_constant(Compiler *compiler, const Value value)
{
	const int constant = add_constant(compiler->owner, current_chunk(compiler), value);
	if (constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants in one chunk.", LIMIT);
		return 0;
	}
	return (uint16_t)constant;
}

static void emit_constant(Compiler *compiler, const Value value)
{
	const uint16_t constant = make_constant(compiler, value);
	if (constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants", SYNTAX);
	}
	emit_words(compiler, OP_CONSTANT, constant);
}

static void push_loop_context(Compiler *compiler, const LoopType type, const int continueTarget)
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

static void pop_loop_context(Compiler *compiler)
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

static void add_break_jump(Compiler *compiler, const int jumpOffset)
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

ObjectTypeRecord *peek_type_record(Compiler *compiler)
{
	if (compiler->type_stack_count <= 0)
		return NULL;
	return compiler->type_stack[compiler->type_stack_count - 1];
}

// Used to set current to the new compiler
bool init_compiler(VM *vm, Compiler *compiler, Compiler *enclosing, const FunctionType type)
{
	if (vm == NULL)
		return false;
	if (compiler == NULL)
		return false;

	compiler->enclosed = NULL;
	compiler->enclosing = enclosing;

	if (enclosing) {
		enclosing->enclosed = compiler;
	}

	// if we have a parent compiler, and the type is not TYPE_SCRIPT, share the parser
	if (enclosing && type != TYPE_SCRIPT) {
		compiler->parser = enclosing->parser;
	} else { // otherwise, allocate a new parser
		compiler->parser = malloc(sizeof(Parser));
		if (compiler->parser == NULL)
			return false;
		compiler->parser->source = NULL;
		compiler->parser->scanner = malloc(sizeof(Scanner));
		if (compiler->parser->scanner == NULL) {
			free(compiler->parser);
			return false;
		}
		compiler->parser->scanner->start = NULL;
		compiler->parser->scanner->current = NULL;
		compiler->parser->scanner->line = 1;

		compiler->parser->current = (Token){0};
		compiler->parser->previous = (Token){0};
		compiler->parser->had_error = false;
		compiler->parser->panic_mode = false;
	}

	compiler->type_stack_count = 0;
	compiler->function = NULL;
	compiler->type = type;
	compiler->local_count = 0;
	compiler->scope_depth = 0;
	compiler->match_depth = 0;
	compiler->loop_depth = 0;
	compiler->owner = vm;
	compiler->has_return = false;
	compiler->return_type = NULL;
	compiler->type_table = new_type_table(vm, INITIAL_TYPE_TABLE_SIZE);

	// only add core fn types for top-level scripts
	if (enclosing == NULL) {
		// add core fn types
		for (int i = 0; i < vm->core_fns.capacity; i++) {
			if (vm->core_fns.entries[i].key != NULL) {
				const Value val = vm->core_fns.entries[i].value;
				if (IS_CRUX_NATIVE_CALLABLE(val)) {
					const ObjectNativeCallable *callable = AS_CRUX_NATIVE_CALLABLE(val);

					ObjectTypeRecord **args_copy = NULL;
					if (callable->arity > 0) {
						args_copy = ALLOCATE(vm, ObjectTypeRecord *, callable->arity);
						for (int j = 0; j < callable->arity; j++) {
							args_copy[j] = callable->arg_types[j];
						}
					}

					ObjectTypeRecord *fn_type = new_function_type_rec(vm, args_copy, callable->arity,
																	  callable->return_type);
					type_table_set(compiler->type_table, vm->core_fns.entries[i].key, fn_type);
				}
			}
		}
	}

	// Load existing types from the module record
	if (type == TYPE_SCRIPT && vm->current_module_record) {
		type_table_add_all(vm->current_module_record->types, compiler->type_table);
	}

	compiler->function = new_function(compiler->owner);

	if (type == TYPE_ANONYMOUS) {
		compiler->function->name = copy_string(compiler->owner, "anonymous", 9);
	} else if (type != TYPE_SCRIPT) {
		compiler->function->name = copy_string(compiler->owner, compiler->parser->previous.start,
											   compiler->parser->previous.length);
	}

	Local *local = &compiler->locals[compiler->local_count++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
	local->is_captured = false;

	if (type == TYPE_METHOD) {
		local->name.start = "self";
		local->name.length = 4;
	}

	return true;
}

static uint16_t identifier_constant(Compiler *compiler, const Token *name)
{
	return make_constant(compiler, OBJECT_VAL(copy_string(compiler->owner, name->start, name->length)));
}

static void begin_scope(Compiler *compiler)
{
	compiler->scope_depth++;
}

static void cleanupLocalsToDepth(Compiler *compiler, const int targetDepth)
{
	while (compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > targetDepth) {
		if (compiler->locals[compiler->local_count - 1].is_captured) {
			emit_word(compiler, OP_CLOSE_UPVALUE);
		} else {
			emit_word(compiler, OP_POP);
		}
		compiler->local_count--;
	}
}

/**
 * Ends the current scope.
 * Decreases the scope depth and emits OP_POP instructions to remove local
 * variables that go out of scope.
 */
static void end_scope(Compiler *compiler)
{
	compiler->scope_depth--;
	cleanupLocalsToDepth(compiler, compiler->scope_depth);
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
				compiler_panic(compiler->parser,
							   "Cannot read local variable in "
							   "its own initializer",
							   NAME);
			}
			return i;
		}
	}
	return -1;
}

static int get_current_continue_target(Compiler *compiler)
{
	if (compiler->loop_depth <= 0) {
		compiler_panic(compiler->parser, "Cannot use 'continue' outside of a loop.", SYNTAX);
		return -1;
	}

	return compiler->loop_stack[compiler->loop_depth - 1].continue_target;
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
static int add_upvalue(Compiler *compiler, const uint16_t index, const bool isLocal, ObjectTypeRecord *type)
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

/**
 * Adds a local variable to the current scope.
 *
 * @param name The token representing the name of the local variable.
 */
static void add_local(Compiler *compiler, const Token name, ObjectTypeRecord *type)
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
 * Declares a variable in the current scope.
 *
 * Adds the variable name to the list of local variables, but does not mark it
 * as initialized yet.
 */
static void declare_variable(Compiler *compiler)
{
	if (compiler->scope_depth == 0)
		return;

	const Token *name = &compiler->parser->previous;

	for (int i = compiler->local_count - 1; i >= 0; i--) {
		const Local *local = &compiler->locals[i];
		if (local->depth != -1 && local->depth < compiler->scope_depth) {
			break;
		}
		if (identifiers_equal(name, &local->name)) {
			compiler_panic(compiler->parser, "Cannot redefine variable in the same scope", NAME);
		}
	}

	add_local(compiler, *name, NULL);
}

/**
 * Marks the most recently declared local variable as initialized.
 *
 * This prevents reading a local variable before it has been assigned a value.
 */
static void mark_initialized(Compiler *compiler)
{
	if (compiler->scope_depth == 0)
		return;
	compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

/**
 * Parses a variable name and returns its constant pool index.
 *
 * @param errorMessage The error message to display if an identifier is not
 * found.
 * @return The index of the variable name constant in the constant pool.
 */
static uint16_t parse_variable(Compiler *compiler, const char *errorMessage)
{
	consume(compiler, TOKEN_IDENTIFIER, errorMessage);
	declare_variable(compiler);
	if (compiler->scope_depth > 0)
		return 0;
	return identifier_constant(compiler, &compiler->parser->previous);
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
static void define_variable(Compiler *compiler, const uint16_t global)
{
	if (compiler->scope_depth > 0) {
		mark_initialized(compiler);
		return;
	}
	if (global >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many variables.", SYNTAX);
	}
	emit_words(compiler, OP_DEFINE_GLOBAL, global);
}

static OpCode get_compound_opcode(Compiler *compiler, const OpCode setOp, const CompoundOp op)
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
			compiler_panic(compiler->parser,
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
			compiler_panic(compiler->parser,
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
			compiler_panic(compiler->parser,
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
static void named_variable(Compiler *compiler, Token name, const bool can_assign)
{
	uint16_t getOp, setOp;
	int arg = resolve_local(compiler, &name);
	ObjectTypeRecord *var_type = NULL;

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
		var_type = compiler->locals[arg].type;
	} else if ((arg = resolve_upvalue(compiler, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
		var_type = compiler->upvalues[arg].type;
	} else {
		arg = identifier_constant(compiler, &name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;

		// Walk the compiler chain so inner functions can see globals
		ObjectString *name_str = copy_string(compiler->owner, name.start, name.length);
		Compiler *comp = compiler;
		while (comp != NULL) {
			if (type_table_get(comp->type_table, name_str, &var_type))
				break;
			comp = comp->enclosing;
		}

		if (!var_type) {
			compiler_panicf(compiler->parser, TYPE,
							"Undeclared variable '%.*s'. Did you forget to declare or import it?", name.length,
							name.start);
			var_type = T_ANY;
		}
	}

	if (can_assign) {
		// assignment: x = expression
		if (match(compiler, TOKEN_EQUAL)) {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (var_type && value_type && var_type->base_type != ANY_TYPE && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(var_type, value_type)) {
					char expected[128], got[128];
					type_record_name(var_type, expected, sizeof(expected));
					type_record_name(value_type, got, sizeof(got));
					compiler_panicf(compiler->parser, TYPE, "Cannot assign '%s' to variable of type '%s'.", got,
									expected);
				}
			}

			emit_words(compiler, setOp, arg);
			return;
		}

		// compound assignment: x += expr
		CompoundOp op;
		bool isCompoundAssignment = true;

		if (match(compiler, TOKEN_PLUS_EQUAL)) {
			op = COMPOUND_OP_PLUS;
		} else if (match(compiler, TOKEN_MINUS_EQUAL)) {
			op = COMPOUND_OP_MINUS;
		} else if (match(compiler, TOKEN_STAR_EQUAL)) {
			op = COMPOUND_OP_STAR;
		} else if (match(compiler, TOKEN_SLASH_EQUAL)) {
			op = COMPOUND_OP_SLASH;
		} else if (match(compiler, TOKEN_BACK_SLASH_EQUAL)) {
			op = COMPOUND_OP_BACK_SLASH;
		} else if (match(compiler, TOKEN_PERCENT_EQUAL)) {
			op = COMPOUND_OP_PERCENT;
		} else {
			isCompoundAssignment = false;
		}

		if (isCompoundAssignment) {
			expression(compiler);
			ObjectTypeRecord *rhs_type = pop_type_record(compiler);

			if (var_type && rhs_type && var_type->base_type != ANY_TYPE && rhs_type->base_type != ANY_TYPE) {
				const bool var_num = var_type->base_type == INT_TYPE || var_type->base_type == FLOAT_TYPE;
				const bool rhs_num = rhs_type->base_type == INT_TYPE || rhs_type->base_type == FLOAT_TYPE;

				if (op == COMPOUND_OP_PLUS && var_type->base_type == STRING_TYPE) {
					// String += String only
					if (rhs_type->base_type != STRING_TYPE) {
						compiler_panic(compiler->parser,
									   "'+=' on a String "
									   "requires a String "
									   "right-hand side.",
									   TYPE);
					}
				} else if (op == COMPOUND_OP_BACK_SLASH || op == COMPOUND_OP_PERCENT) {
					// Integer-only operators
					if (var_type->base_type != INT_TYPE || rhs_type->base_type != INT_TYPE) {
						compiler_panic(compiler->parser,
									   "This compound "
									   "operator "
									   "requires Int "
									   "operands.",
									   TYPE);
					}
				} else if (!var_num || !rhs_num) {
					compiler_panic(compiler->parser,
								   "Compound assignment requires "
								   "numeric operands.",
								   TYPE);
				}
			}

			const OpCode compoundOp = get_compound_opcode(compiler, setOp, op);
			emit_words(compiler, compoundOp, arg);
			return;
		}
	}

	emit_words(compiler, getOp, arg);
	push_type_record(compiler, var_type);
}

static void and_(Compiler *compiler, bool can_assign)
{
	(void)can_assign;

	const ObjectTypeRecord *left_type = pop_type_record(compiler);
	if (left_type && left_type->base_type != BOOL_TYPE && left_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "Left operand of 'and' must be of type 'Bool'.", TYPE);
	}

	const int endJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	emit_word(compiler, OP_POP);
	parse_precedence(compiler, PREC_AND);

	const ObjectTypeRecord *right_type = pop_type_record(compiler);
	if (right_type && right_type->base_type != BOOL_TYPE && right_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "Right operand of 'and' must be of type 'Bool'.", TYPE);
	}

	patch_jump(compiler, endJump);

	push_type_record(compiler, new_type_rec(compiler->owner, BOOL_TYPE));
}

static void or_(Compiler *compiler, bool can_assign)
{
	(void)can_assign;

	ObjectTypeRecord *left_type = pop_type_record(compiler);
	if (left_type && left_type->base_type != BOOL_TYPE && left_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "Left operand of 'or' must be of type 'Bool'.", TYPE);
	}

	const int elseJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	const int endJump = emit_jump(compiler, OP_JUMP);
	patch_jump(compiler, elseJump);
	emit_word(compiler, OP_POP);
	parse_precedence(compiler, PREC_OR);

	ObjectTypeRecord *right_type = pop_type_record(compiler);
	if (right_type && right_type->base_type != BOOL_TYPE && right_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "Right operand of 'or' must be of type 'Bool'.", TYPE);
	}

	patch_jump(compiler, endJump);

	push_type_record(compiler, new_type_rec(compiler->owner, BOOL_TYPE));
}

// TODO: When called must change the compiler to its enclosing
static ObjectFunction *end_compiler(Compiler *compiler)
{
	emit_return(compiler);
	push_type_record(compiler, compiler->return_type);
	ObjectFunction *function = compiler->function;
#ifdef DEBUG_PRINT_CODE
	if (!compiler->parser->had_error) {
		disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif

	function->module_record = compiler->owner->current_module_record;
	if (compiler->enclosing) {
		compiler->enclosing->enclosed = NULL;
	}
	return function;
}

static void binary(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = compiler->parser->previous.type;
	const ParseRule *rule = get_rule(operatorType);
	parse_precedence(compiler, rule->precedence + 1);

	ObjectTypeRecord *right_type = pop_type_record(compiler);
	ObjectTypeRecord *left_type = pop_type_record(compiler);
	ObjectTypeRecord *result_type = NULL;

	const bool either_any = (left_type && left_type->base_type == ANY_TYPE) ||
							(right_type && right_type->base_type == ANY_TYPE) || !left_type || !right_type;

	switch (operatorType) {
	case TOKEN_EQUAL_EQUAL:
		emit_word(compiler, OP_EQUAL);
		result_type = new_type_rec(compiler->owner, BOOL_TYPE);
		break;

	case TOKEN_BANG_EQUAL:
		emit_word(compiler, OP_NOT_EQUAL);
		result_type = new_type_rec(compiler->owner, BOOL_TYPE);
		break;

	case TOKEN_GREATER:
	case TOKEN_GREATER_EQUAL:
	case TOKEN_LESS:
	case TOKEN_LESS_EQUAL: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;
			const bool both_str = left_type->base_type == STRING_TYPE && right_type->base_type == STRING_TYPE;

			if (!((left_num && right_num) || both_str)) {
				compiler_panic(compiler->parser,
							   "Comparison operator requires numeric "
							   "or String operands.",
							   TYPE);
			}
		}

		switch (operatorType) {
		case TOKEN_GREATER:
			emit_word(compiler, OP_GREATER);
			break;
		case TOKEN_GREATER_EQUAL:
			emit_word(compiler, OP_GREATER_EQUAL);
			break;
		case TOKEN_LESS:
			emit_word(compiler, OP_LESS);
			break;
		case TOKEN_LESS_EQUAL:
			emit_word(compiler, OP_LESS_EQUAL);
			break;
		default:
			break;
		}

		result_type = new_type_rec(compiler->owner, BOOL_TYPE);
		break;
	}

	case TOKEN_PLUS: {
		emit_word(compiler, OP_ADD);

		if (either_any) {
			result_type = new_type_rec(compiler->owner, ANY_TYPE);
			break;
		}

		const bool left_str = left_type->base_type == STRING_TYPE;
		const bool right_str = right_type->base_type == STRING_TYPE;

		if (left_str || right_str) {
			if (!left_str || !right_str) {
				compiler_panic(compiler->parser,
							   "Cannot use '+' between String and "
							   "non-String.",
							   TYPE);
			}
			result_type = new_type_rec(compiler->owner, STRING_TYPE);
			break;
		}

		const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
		const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;

		if (!left_num || !right_num) {
			compiler_panic(compiler->parser,
						   "'+' requires numeric or String "
						   "operands.",
						   TYPE);
		}

		// Int + Float => Float
		if (left_type->base_type == FLOAT_TYPE || right_type->base_type == FLOAT_TYPE) {
			result_type = new_type_rec(compiler->owner, FLOAT_TYPE);
		} else {
			result_type = new_type_rec(compiler->owner, INT_TYPE);
		}
		break;
	}

	case TOKEN_MINUS:
	case TOKEN_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;

			if (!left_num || !right_num) {
				compiler_panic(compiler->parser,
							   operatorType == TOKEN_MINUS ? "'-' requires numeric "
															 "operands."
														   : "'*' requires numeric "
															 "operands.",
							   TYPE);
			}
		}

		emit_word(compiler, operatorType == TOKEN_MINUS ? OP_SUBTRACT : OP_MULTIPLY);

		if (either_any) {
			result_type = new_type_rec(compiler->owner, ANY_TYPE);
		} else if (left_type->base_type == FLOAT_TYPE || right_type->base_type == FLOAT_TYPE) {
			result_type = new_type_rec(compiler->owner, FLOAT_TYPE);
		} else {
			result_type = new_type_rec(compiler->owner, INT_TYPE);
		}
		break;
	}

	case TOKEN_SLASH: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;
			if (!left_num || !right_num) {
				compiler_panic(compiler->parser, "'/' requires numeric operands.", TYPE);
			}
		}
		emit_word(compiler, OP_DIVIDE);
		result_type = new_type_rec(compiler->owner, FLOAT_TYPE);
		break;
	}

	case TOKEN_PERCENT:
	case TOKEN_BACKSLASH: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE || right_type->base_type != INT_TYPE) {
				compiler_panic(compiler->parser,
							   operatorType == TOKEN_PERCENT ? "'%' requires Int operands."
															 : "'\\' requires Int operands.",
							   TYPE);
			}
		}
		emit_word(compiler, operatorType == TOKEN_PERCENT ? OP_MODULUS : OP_INT_DIVIDE);
		result_type = new_type_rec(compiler->owner, INT_TYPE);
		break;
	}

	case TOKEN_RIGHT_SHIFT:
	case TOKEN_LEFT_SHIFT:
	case TOKEN_AMPERSAND:
	case TOKEN_CARET:
	case TOKEN_PIPE: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE || right_type->base_type != INT_TYPE) {
				compiler_panic(compiler->parser,
							   "Bitwise operators require Int "
							   "operands.",
							   TYPE);
			}
		}

		switch (operatorType) {
		case TOKEN_RIGHT_SHIFT:
			emit_word(compiler, OP_RIGHT_SHIFT);
			break;
		case TOKEN_LEFT_SHIFT:
			emit_word(compiler, OP_LEFT_SHIFT);
			break;
		case TOKEN_AMPERSAND:
			emit_word(compiler, OP_BITWISE_AND);
			break;
		case TOKEN_CARET:
			emit_word(compiler, OP_BITWISE_XOR);
			break;
		case TOKEN_PIPE:
			emit_word(compiler, OP_BITWISE_OR);
			break;
		default:
			break;
		}

		result_type = new_type_rec(compiler->owner, INT_TYPE);
		break;
	}

	case TOKEN_STAR_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;
			if (!left_num || !right_num) {
				compiler_panic(compiler->parser, "'**' requires numeric operands.", TYPE);
			}
		}
		emit_word(compiler, OP_POWER);
		result_type = new_type_rec(compiler->owner, FLOAT_TYPE);
		break;
	}

	default:
		// Should be unreachable.
		result_type = T_ANY;
		break;
	}

	push_type_record(compiler, result_type);
}

static void infix_call(Compiler *compiler, bool can_assign)
{
	(void)can_assign;

	const ObjectTypeRecord *func_type = pop_type_record(compiler);
	uint16_t arg_count = 0;
	ObjectTypeRecord *arg_types[UINT8_COUNT] = {0};

	if (!check(compiler, TOKEN_RIGHT_PAREN)) {
		do {
			if (arg_count == UINT16_MAX) {
				compiler_panic(compiler->parser,
							   "Cannot have more than 65535 "
							   "arguments.",
							   ARGUMENT_EXTENT);
			}
			expression(compiler);
			arg_types[arg_count] = pop_type_record(compiler);
			arg_count++;
		} while (match(compiler, TOKEN_COMMA));
	}
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");

	emit_words(compiler, OP_CALL, arg_count);

	// Type-check only when the callee type is statically known.
	if (func_type && func_type->base_type == FUNCTION_TYPE) {
		const int expected_count = func_type->as.function_type.arg_count;

		if ((int)arg_count != expected_count) {
			compiler_panicf(compiler->parser, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d.", expected_count,
							(int)arg_count);
		} else {
			for (int i = 0; i < (int)arg_count; i++) {
				ObjectTypeRecord *expected = func_type->as.function_type.arg_types[i];
				ObjectTypeRecord *got = arg_types[i];
				if (expected && got && expected->base_type != ANY_TYPE && got->base_type != ANY_TYPE) {
					if (!types_compatible(expected, got)) {
						char exp_name[128], got_name[128];
						type_record_name(expected, exp_name, sizeof(exp_name));
						type_record_name(got, got_name, sizeof(got_name));
						compiler_panicf(compiler->parser, TYPE,
										"Argument %d type "
										"mismatch: expected "
										"'%s', got '%s'.",
										i + 1, exp_name, got_name);
					}
				}
			}
		}

		ObjectTypeRecord *ret = func_type->as.function_type.return_type;
		push_type_record(compiler, ret ? ret : T_ANY);
	} else {
		// unknown callee type
		push_type_record(compiler, T_ANY);
	}
}

static void literal(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	switch (compiler->parser->previous.type) {
	case TOKEN_FALSE:
		emit_word(compiler, OP_FALSE);
		push_type_record(compiler, new_type_rec(compiler->owner, BOOL_TYPE));
		break;
	case TOKEN_NIL:
		emit_word(compiler, OP_NIL);
		push_type_record(compiler, new_type_rec(compiler->owner, NIL_TYPE));
		break;
	case TOKEN_TRUE:
		emit_word(compiler, OP_TRUE);
		push_type_record(compiler, new_type_rec(compiler->owner, BOOL_TYPE));
		break;
	default:
		return; // unreachable
	}
}

static void dot(Compiler *compiler, const bool can_assign)
{
	consume(compiler, TOKEN_IDENTIFIER, "Expected property name after '.'.");
	const uint16_t name_constant = identifier_constant(compiler, &compiler->parser->previous);
	const Token method_name_token = compiler->parser->previous;

	if (name_constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants.", SYNTAX);
	}

	ObjectTypeRecord *object_type = pop_type_record(compiler);
	if (!object_type)
		object_type = T_ANY;

	// OP_SET_PROPERTY - this only works for structs
	if (can_assign && match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		ObjectTypeRecord *value_type = pop_type_record(compiler);

		if (object_type->base_type == STRUCT_TYPE && value_type) {
			const ObjectTypeTable *field_types = object_type->as.struct_type.field_types;

			const ObjectString *field_name = copy_string(compiler->owner, method_name_token.start,
														 method_name_token.length);
			ObjectTypeRecord *field_type = NULL;
			if (type_table_get(field_types, field_name, &field_type)) {
				if (field_type && field_type->base_type != ANY_TYPE && value_type->base_type != ANY_TYPE &&
					!types_compatible(field_type, value_type)) {
					char exp[128], got[128], msg[300];
					type_record_name(field_type, exp, sizeof(exp));
					type_record_name(value_type, got, sizeof(got));
					snprintf(msg, sizeof(msg), "Cannot assign '%s' to field of type '%s'.", got, exp);
					compiler_panic(compiler->parser, msg, TYPE);
				}
			} else {
				char msg[160];
				snprintf(msg, sizeof(msg), "Struct has no field '%.*s'.", (int)method_name_token.length,
						 method_name_token.start);
				compiler_panic(compiler->parser, msg, NAME);
			}
		}

		emit_words(compiler, OP_SET_PROPERTY, name_constant);
		push_type_record(compiler, new_type_rec(compiler->owner, NIL_TYPE));
		return;
	}

	// OP_INVOKE
	if (match(compiler, TOKEN_LEFT_PAREN)) {
		uint16_t arg_count = 0;
		ObjectTypeRecord *arg_types[UINT8_COUNT] = {0};

		if (!check(compiler, TOKEN_RIGHT_PAREN)) {
			do {
				expression(compiler);
				arg_types[arg_count++] = pop_type_record(compiler);
			} while (match(compiler, TOKEN_COMMA));
		}
		consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

		emit_words(compiler, OP_INVOKE, name_constant);
		emit_word(compiler, arg_count);

		ObjectTypeRecord **method_arg_types = NULL;
		ObjectTypeRecord *method_return = NULL;
		int method_arity = 0; // includes self as arg[0]
		bool method_found = false;

		if (object_type->base_type == STRUCT_TYPE) {
			const ObjectTypeTable *field_types = object_type->as.struct_type.field_types;
			const ObjectString *field_name = copy_string(compiler->owner, method_name_token.start,
														 method_name_token.length);
			ObjectTypeRecord *fn_type = NULL;
			if (type_table_get(field_types, field_name, &fn_type) && fn_type && fn_type->base_type == FUNCTION_TYPE) {
				method_arg_types = fn_type->as.function_type.arg_types;
				method_arity = fn_type->as.function_type.arg_count;
				method_return = fn_type->as.function_type.return_type;
				method_found = true;
			} else {
				char msg[160];
				snprintf(msg, sizeof(msg), "Struct field '%.*s' is not callable.", (int)method_name_token.length,
						 method_name_token.start);
				compiler_panic(compiler->parser, msg, TYPE);
			}

		} else if (object_type->base_type != ANY_TYPE) {
			VM *vm = compiler->owner;
			const Table *type_table = NULL;

			switch (object_type->base_type) {
			case STRING_TYPE:
				type_table = &vm->string_type;
				break;
			case ARRAY_TYPE:
				type_table = &vm->array_type;
				break;
			case TABLE_TYPE:
				type_table = &vm->table_type;
				break;
			case ERROR_TYPE:
				type_table = &vm->error_type;
				break;
			case RESULT_TYPE:
				type_table = &vm->result_type;
				break;
			case FILE_TYPE:
				type_table = &vm->file_type;
				break;
			case RANDOM_TYPE:
				type_table = &vm->random_type;
				break;
			case VECTOR_TYPE:
				type_table = &vm->vector_type;
				break;
			case COMPLEX_TYPE:
				type_table = &vm->complex_type;
				break;
			case MATRIX_TYPE:
				type_table = &vm->matrix_type;
				break;
			case RANGE_TYPE:
				type_table = &vm->range_type;
				break;
			case TUPLE_TYPE:
				type_table = &vm->tuple_type;
				break;
			case SET_TYPE:
				type_table = &vm->set_type;
				break;
			case BUFFER_TYPE:
				type_table = &vm->buffer_type;
				break;
			default:
				break;
			}

			if (type_table) {
				const ObjectNativeCallable *callable = lookup_stdlib_method(compiler, type_table, &method_name_token);
				if (callable) {
					method_arg_types = callable->arg_types;
					method_arity = callable->arity;
					method_return = callable->return_type;
					method_found = true;
				} else {
					char type_name[128], msg[300];
					type_record_name(object_type, type_name, sizeof(type_name));
					snprintf(msg, sizeof(msg), "'%s' has no method '%.*s'.", type_name, (int)method_name_token.length,
							 method_name_token.start);
					compiler_panic(compiler->parser, msg, NAME);
				}
			}
			// If type_table is NULL the type has no methods — panic was
			// already emitted above for known primitive types; for truly
			// unknown types we fall through to push ANY_TYPE below.
		}

		// Validate user-supplied args against declared parameter types.
		// Slot 0 of method_arg_types is always the receiver (self), so
		// user args are validated against slots [1 .. method_arity-1].

		// struct methods do not have an implicit self
		if (method_found && method_arg_types) {
			int param_offset = (object_type->base_type == STRUCT_TYPE) ? 0 : 1;
			int user_params = method_arity - param_offset;
			if (user_params < 0)
				user_params = 0;

			if ((int)arg_count != user_params) {
				char msg[128];
				snprintf(msg, sizeof(msg), "Method expects %d argument(s), got %d.", user_params, (int)arg_count);
				compiler_panic(compiler->parser, msg, ARGUMENT_MISMATCH);
			} else {
				for (int i = 0; i < (int)arg_count; i++) {
					ObjectTypeRecord *expected = method_arg_types[i + param_offset];
					ObjectTypeRecord *got_type = arg_types[i];
					if (expected && got_type && expected->base_type != ANY_TYPE && got_type->base_type != ANY_TYPE &&
						!types_compatible(expected, got_type)) {
						char exp_name[128], got_name[128], msg[300];
						type_record_name(expected, exp_name, sizeof(exp_name));
						type_record_name(got_type, got_name, sizeof(got_name));
						snprintf(msg, sizeof(msg),
								 "Argument %d type mismatch: "
								 "expected '%s', got '%s'.",
								 i + 1, exp_name, got_name);
						compiler_panic(compiler->parser, msg, TYPE);
					}
				}
			}
		}

		push_type_record(compiler, method_return ? method_return : T_ANY);
		return;
	}

	// OP_GET_PROPERTY - only works on structs
	emit_words(compiler, OP_GET_PROPERTY, name_constant);

	ObjectTypeRecord *result_type = NULL;

	if (object_type->base_type == STRUCT_TYPE) {
		const ObjectTypeTable *field_types = object_type->as.struct_type.field_types;
		const ObjectString *field_name = copy_string(compiler->owner, method_name_token.start,
													 method_name_token.length);
		ObjectTypeRecord *field_type = NULL;
		if (type_table_get(field_types, field_name, &field_type)) {
			result_type = field_type;
		} else {
			char msg[160];
			snprintf(msg, sizeof(msg), "Struct has no field '%.*s'.", (int)method_name_token.length,
					 method_name_token.start);
			compiler_panic(compiler->parser, msg, NAME);
		}
	}
	// NOTE: Could add stdlib members here

	push_type_record(compiler, result_type ? result_type : T_ANY);
}

void struct_instance(Compiler *compiler, const bool can_assign)
{
	consume(compiler, TOKEN_IDENTIFIER, "Expected struct name to start initialization.");

	const Token struct_name_token = compiler->parser->previous;
	(void)struct_name_token; // used for error messages below

	named_variable(compiler, compiler->parser->previous, can_assign);

	// named_variable pushes the type of the name it resolved — for a
	// struct this should be a STRUCT_TYPE record. Pop it now so we can
	// use its field table during initializer validation.
	ObjectTypeRecord *struct_type = pop_type_record(compiler);

	// Validate that the name actually refers to a struct. ANY_TYPE means
	// the pre-scan hasn't run yet or the name came from an import, so we
	// allow it through and skip field-level checking.
	const bool type_known = struct_type && struct_type->base_type != ANY_TYPE;
	if (type_known && struct_type->base_type != STRUCT_TYPE) {
		compiler_panic(compiler->parser, "'new' requires a struct type name.", TYPE);
		// Push ANY_TYPE so the stack stays balanced.
		push_type_record(compiler, T_ANY);
		return;
	}

	if (!match(compiler, TOKEN_LEFT_BRACE)) {
		compiler_panic(compiler->parser, "Expected '{' to start struct instance.", SYNTAX);
		push_type_record(compiler, T_ANY);
		return;
	}

	// Track which field names the programmer supplied so we can report
	// missing required fields after the loop. We use a small fixed
	// bitfield keyed by field index (structs are limited to UINT16_MAX
	// fields, but in practice they're small — use a heap array of bools
	// for safety).
	int declared_field_count = type_known ? struct_type->as.struct_type.field_count : 0;
	bool *field_seen = NULL;
	if (declared_field_count > 0) {
		field_seen = ALLOCATE(compiler->owner, bool, declared_field_count);
		for (int i = 0; i < declared_field_count; i++) {
			field_seen[i] = false;
		}
	}

	uint16_t fieldCount = 0;
	emit_word(compiler, OP_STRUCT_INSTANCE_START);

	if (!match(compiler, TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount == UINT16_MAX) {
				compiler_panic(compiler->parser,
							   "Too many fields in struct "
							   "initializer.",
							   SYNTAX);
				if (field_seen)
					FREE_ARRAY(compiler->owner, bool, field_seen, declared_field_count);
				push_type_record(compiler, new_type_rec(compiler->owner, ANY_TYPE));
				return;
			}

			consume(compiler, TOKEN_IDENTIFIER, "Expected field name.");
			ObjectString *fieldName = copy_string(compiler->owner, compiler->parser->previous.start,
												  compiler->parser->previous.length);

			consume(compiler, TOKEN_COLON, "Expected ':' after struct field name.");

			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (type_known) {
				ObjectTypeTable *field_types = struct_type->as.struct_type.field_types;
				ObjectStruct *definition = struct_type->as.struct_type.definition;

				// Check the field exists on the struct.
				Value field_index_val;
				if (!table_get(&definition->fields, fieldName, &field_index_val)) {
					char msg[160];
					snprintf(msg, sizeof(msg), "Struct has no field '%.*s'.", (int)fieldName->length, fieldName->chars);
					compiler_panic(compiler->parser, msg, NAME);
				} else {
					// Mark as seen.
					const int field_index = (int)AS_INT(field_index_val);
					if (field_index >= 0 && field_index < declared_field_count) {
						if (field_seen[field_index]) {
							char msg[160];
							snprintf(msg, sizeof(msg), "Field '%.*s' specified more than once.", (int)fieldName->length,
									 fieldName->chars);
							compiler_panic(compiler->parser, msg, NAME);
						}
						field_seen[field_index] = true;
					}

					// Validate the value type against the
					// declared field type.
					ObjectTypeRecord *declared_field_type = NULL;
					type_table_get(field_types, fieldName, &declared_field_type);

					if (declared_field_type && value_type && declared_field_type->base_type != ANY_TYPE &&
						value_type->base_type != ANY_TYPE) {
						if (!types_compatible(declared_field_type, value_type)) {
							char expected[128], got[128], msg[400];
							type_record_name(declared_field_type, expected, sizeof(expected));
							type_record_name(value_type, got, sizeof(got));
							snprintf(msg, sizeof(msg), "Field '%.*s' expects type '%s', got '%s'.",
									 (int)fieldName->length, fieldName->chars, expected, got);
							compiler_panic(compiler->parser, msg, TYPE);
						}
					}
				}
			}

			const uint16_t fieldNameConstant = make_constant(compiler, OBJECT_VAL(fieldName));
			emit_words(compiler, OP_STRUCT_NAMED_FIELD, fieldNameConstant);
			fieldCount++;
		} while (match(compiler, TOKEN_COMMA));
	}

	// Check for missing fields — every declared field must be provided.
	if (type_known && field_seen) {
		ObjectStruct *definition = struct_type->as.struct_type.definition;

		for (int i = 0; i < declared_field_count; i++) {
			if (!field_seen[i]) {
				// Find the field name for the error message by
				// scanning the struct's fields table.
				// Fields table maps name -> INT index, so walk
				// it to find index i.
				const char *missing = "<unknown>";
				int missing_len = 0;
				for (int e = 0; e < definition->fields.capacity; e++) {
					Entry *entry = &definition->fields.entries[e];
					if (entry->key != NULL && AS_INT(entry->value) == i) {
						missing = entry->key->chars;
						missing_len = (int)entry->key->length;
						break;
					}
				}
				char msg[160];
				snprintf(msg, sizeof(msg), "Missing required field '%.*s' in struct initializer.", missing_len,
						 missing);
				compiler_panic(compiler->parser, msg, NAME);
				break; // Report one missing field at a time.
			}
		}
		FREE_ARRAY(compiler->owner, bool, field_seen, declared_field_count);
	}

	if (fieldCount != 0) {
		consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after struct field list.");
	}

	emit_word(compiler, OP_STRUCT_INSTANCE_END);

	// Push the struct's type so callers know what new Foo{} produced.
	push_type_record(compiler, struct_type ? struct_type : T_ANY);
}

/**
 * Parses an expression.
 */
static void expression(Compiler *compiler)
{
	parse_precedence(compiler, PREC_ASSIGNMENT);
}

static void variable(Compiler *compiler, const bool can_assign)
{
	named_variable(compiler, compiler->parser->previous, can_assign);
}

static void block(Compiler *compiler)
{
	while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
		declaration(compiler);
	}

	consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(Compiler *compiler, const FunctionType type)
{
	Compiler function_compiler;
	if (!init_compiler(compiler->owner, &function_compiler, compiler, type)) {
		fprintf(stderr, "Fatal error: Memory allocation failed. Shutting down!\n");
		exit(EXIT_FAILURE);
	}
	begin_scope(&function_compiler);

	// Collect param types in a heap array so we can copy them to the
	// enclosing arena after end_compiler() restores current.
	int param_capacity = 4;
	int param_count = 0;
	ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
	if (!param_types) {
		compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
		return;
	}

	consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(&function_compiler, TOKEN_RIGHT_PAREN)) {
		do {
			function_compiler.function->arity++;
			if (function_compiler.function->arity > UINT8_MAX) {
				compiler_panic(compiler->parser,
							   "Functions cannot have more than "
							   "255 arguments.",
							   ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(&function_compiler, "Expected parameter name.");

			consume(&function_compiler, TOKEN_COLON, "Expect ':' after parameter name.");
			ObjectTypeRecord *param_type = parse_type_record(&function_compiler);

			function_compiler.locals[function_compiler.local_count - 1].type = param_type;

			if (param_count == param_capacity) {
				param_capacity = GROW_CAPACITY(param_capacity);
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count,
													  param_capacity);
				if (!grown) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count);
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					return;
				}
				param_types = grown;
			}
			param_types[param_count++] = param_type;

			define_variable(&function_compiler, constant);
		} while (match(compiler, TOKEN_COMMA));
	}

	param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);

	consume(&function_compiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	consume(&function_compiler, TOKEN_ARROW, "Expect '->' after parameter list.");
	ObjectTypeRecord *annotated_return_type = parse_type_record(&function_compiler);
	function_compiler.return_type = annotated_return_type;

	consume(&function_compiler, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block(&function_compiler);

	if (!function_compiler.has_return && annotated_return_type && annotated_return_type->base_type != NIL_TYPE &&
		annotated_return_type->base_type != ANY_TYPE) {
		char expected[128], msg[256];
		type_record_name(annotated_return_type, expected, sizeof(expected));
		snprintf(msg, sizeof(msg), "Function expects to return '%s' but has no return statement.", expected);
		compiler_panic(compiler->parser, msg, TYPE);
	}

	ObjectFunction *fn = end_compiler(&function_compiler);
	emit_words(compiler, OP_CLOSURE, make_constant(compiler, OBJECT_VAL(fn)));

	for (int i = 0; i < fn->upvalue_count; i++) {
		emit_word(compiler, function_compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler, function_compiler.upvalues[i].index);
	}

	ObjectTypeRecord *func_type = new_function_type_rec(compiler->owner, param_types, param_count,
														annotated_return_type);
	push_type_record(compiler, func_type);
}

static void fn_declaration(Compiler *compiler, bool is_public)
{
	const uint16_t global = parse_variable(compiler, "Expected function name.");

	// compiler->parser->previous is the function name identifier right now.
	// Capture it before function() calls consume() and advances the token.
	const Token fn_name_token = compiler->parser->previous;
	ObjectString *name_str = copy_string(compiler->owner, fn_name_token.start, fn_name_token.length);

	// Save local index in case this is a local-scope function.
	const int local_index = (compiler->scope_depth > 0) ? compiler->local_count - 1 : -1;

	mark_initialized(compiler);
	function(compiler, TYPE_FUNCTION);

	ObjectTypeRecord *fn_type = pop_type_record(compiler);
	if (is_public) {
		type_table_set(compiler->owner->current_module_record->types, name_str, fn_type);
	}

	if (fn_type) {
		if (compiler->scope_depth == 0) {
			// Global function: enter into the type table so
			// named_variable can look it up from any scope.
			ObjectString *name_str = copy_string(compiler->owner, fn_name_token.start, fn_name_token.length);
			type_table_set(compiler->type_table, name_str, fn_type);
		} else {
			// Local function: stamp the type directly onto the
			// local slot that declare_variable already reserved.
			compiler->locals[local_index].type = fn_type;
		}
	}

	define_variable(compiler, global);
}

static void anonymous_function(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	Compiler function_compiler;
	if (!init_compiler(compiler->owner, &function_compiler, compiler, TYPE_ANONYMOUS)) {
		fprintf(stderr, "Fatal error: Memory allocation failed. Shutting down!\n");
		exit(EXIT_FAILURE);
	}

	int param_capacity = 4;
	int param_count = 0;
	ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
	if (!param_types) {
		compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
		return;
	}

	begin_scope(&function_compiler);
	consume(&function_compiler, TOKEN_LEFT_PAREN, "Expected '(' to start argument list.");

	if (!check(&function_compiler, TOKEN_RIGHT_PAREN)) {
		do {
			function_compiler.function->arity++;
			if (function_compiler.function->arity > UINT8_MAX) {
				compiler_panic(compiler->parser,
							   "Functions cannot have more than "
							   "255 arguments.",
							   ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(&function_compiler, "Expected parameter name.");

			// Anonymous functions require type annotations on all
			// parameters — there is no inference fallback.
			consume(&function_compiler, TOKEN_COLON, "Expected ':' after parameter name.");
			ObjectTypeRecord *param_type = parse_type_record(&function_compiler);

			// Store on the local slot that parse_variable just
			// created via declare_variable -> add_local.
			function_compiler.locals[function_compiler.local_count - 1].type = param_type;

			if (param_count == param_capacity) {
				int old_cap = param_capacity;
				param_capacity = GROW_CAPACITY(param_capacity);
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap,
													  param_capacity);
				if (!grown) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap);
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					return;
				}
				param_types = grown;
			}
			param_types[param_count++] = param_type;

			define_variable(&function_compiler, constant);
		} while (match(compiler, TOKEN_COMMA));
	}

	consume(&function_compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
	consume(&function_compiler, TOKEN_ARROW, "Expected '->' after argument list.");
	ObjectTypeRecord *annotated_return_type = parse_type_record(&function_compiler);

	// Set on the inner compiler so return_statement validates correctly.
	function_compiler.return_type = annotated_return_type;

	consume(&function_compiler, TOKEN_LEFT_BRACE, "Expected '{' before function body.");
	block(&function_compiler);

	if (!function_compiler.has_return && annotated_return_type && annotated_return_type->base_type != NIL_TYPE &&
		annotated_return_type->base_type != ANY_TYPE) {
		char expected[128], msg[256];
		type_record_name(annotated_return_type, expected, sizeof(expected));
		snprintf(msg, sizeof(msg), "Function expects to return '%s' but has no return statement.", expected);
		compiler_panic(function_compiler.parser, msg, TYPE);
	}

	ObjectFunction *fn = end_compiler(&function_compiler);
	const uint16_t constantIndex = make_constant(compiler, OBJECT_VAL(fn));
	emit_words(compiler, OP_ANON_FUNCTION, constantIndex);

	for (int i = 0; i < fn->upvalue_count; i++) {
		emit_word(compiler, function_compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler, function_compiler.upvalues[i].index);
	}

	param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);

	ObjectTypeRecord *func_type = new_function_type_rec(compiler->owner, param_types, param_count,
														annotated_return_type);
	push_type_record(compiler, func_type);
}

static void array_literal(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	ObjectTypeRecord *element_type = NULL;

	if (!match(compiler, TOKEN_RIGHT_SQUARE)) {
		do {
			expression(compiler);

			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (!element_type) {
				element_type = value_type;
			} else if (element_type->base_type != ANY_TYPE && value_type && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(element_type, value_type)) {
					// Widen to Float if mix of Int/Float
					if ((element_type->base_type == INT_TYPE && value_type->base_type == FLOAT_TYPE) ||
						(element_type->base_type == FLOAT_TYPE && value_type->base_type == INT_TYPE)) {
						element_type = new_type_rec(compiler->owner, FLOAT_TYPE);
					} else {
						// widening for mixed tables
						element_type = T_ANY;
					}
				}
			}

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser,
							   "Too many elements in array "
							   "literal.",
							   COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(compiler, TOKEN_COMMA));
		consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after array elements.");
	}

	if (!element_type) {
		// Empty array literal — element type is unknown.
		element_type = T_ANY;
	}

	emit_word(compiler, OP_ARRAY);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *array_type = new_array_type_rec(compiler->owner, element_type);
	push_type_record(compiler, array_type);
}

static void table_literal(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	ObjectTypeRecord *table_key_type = NULL;
	ObjectTypeRecord *table_value_type = NULL;

	if (!match(compiler, TOKEN_RIGHT_BRACE)) {
		do {
			expression(compiler);
			ObjectTypeRecord *key_type = pop_type_record(compiler);
			consume(compiler, TOKEN_COLON, "Expected ':' after table key.");
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (!table_key_type) {
				table_key_type = key_type;
			} else if (table_key_type->base_type != ANY_TYPE && key_type && key_type->base_type != ANY_TYPE) {
				// Keys must be exactly equal — a table can't have mixed key types
				if (!types_equal(table_key_type, key_type)) {
					char expected[128], got[128], msg[300];
					type_record_name(table_key_type, expected, sizeof(expected));
					type_record_name(key_type, got, sizeof(got));
					snprintf(msg, sizeof(msg),
							 "Inconsistent key types in "
							 "table literal: expected "
							 "'%s', got '%s'.",
							 expected, got);
					compiler_panic(compiler->parser, msg, TYPE);
				}
			}

			if (!table_value_type) {
				table_value_type = value_type;
			} else if (table_value_type->base_type != ANY_TYPE && value_type && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(table_value_type, value_type)) {
					// Widen to Float if mix of Int/Float
					if ((table_value_type->base_type == INT_TYPE && value_type->base_type == FLOAT_TYPE) ||
						(table_value_type->base_type == FLOAT_TYPE && value_type->base_type == INT_TYPE)) {
						table_value_type = new_type_rec(compiler->owner, FLOAT_TYPE);
					} else {
						// widening for mixed tables
						table_value_type = T_ANY;
					}
				}
			}

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many elements in table literal.", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(compiler, TOKEN_COMMA));
		consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after table elements.");
	}

	if (!table_key_type) {
		table_key_type = T_ANY;
	}
	if (!table_value_type) {
		table_value_type = T_ANY;
	}

	// Emit bytecode first, then push the type — the type record is
	// compiler metadata, not part of the instruction stream.
	emit_word(compiler, OP_TABLE);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *table_type = new_table_type_rec(compiler->owner, table_key_type, table_value_type);
	push_type_record(compiler, table_type);
}

static void collection_index(Compiler *compiler, const bool can_assign)
{
	// The collection's type is on the top of the type stack — pop it
	// now so we know what element type to push after the index op, and
	// so it doesn't leak.
	ObjectTypeRecord *collection_type = pop_type_record(compiler);

	expression(compiler);
	ObjectTypeRecord *index_type = pop_type_record(compiler);

	// Validate the index type against the collection type.
	if (collection_type && index_type && index_type->base_type != ANY_TYPE && collection_type->base_type != ANY_TYPE) {
		if (collection_type->base_type == ARRAY_TYPE) {
			if (index_type->base_type != INT_TYPE) {
				compiler_panic(compiler->parser, "Array index must be of type 'Int'.", TYPE);
			}
		} else if (collection_type->base_type == TABLE_TYPE) {
			ObjectTypeRecord *key_type = collection_type->as.table_type.key_type;
			if (key_type && key_type->base_type != ANY_TYPE && !types_compatible(key_type, index_type)) {
				char expected[128], got[128], msg[300];
				type_record_name(key_type, expected, sizeof(expected));
				type_record_name(index_type, got, sizeof(got));
				snprintf(msg, sizeof(msg),
						 "Table key type mismatch: expected "
						 "'%s', got '%s'.",
						 expected, got);
				compiler_panic(compiler->parser, msg, TYPE);
			}
		}
	}

	consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after index.");

	if (can_assign && match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		ObjectTypeRecord *value_type = pop_type_record(compiler);

		// On a set, validate the value type against the collection's
		// element/value type if known.
		if (collection_type && value_type && value_type->base_type != ANY_TYPE &&
			collection_type->base_type != ANY_TYPE) {
			ObjectTypeRecord *expected_value = NULL;
			if (collection_type->base_type == ARRAY_TYPE) {
				expected_value = collection_type->as.array_type.element_type;
			} else if (collection_type->base_type == TABLE_TYPE) {
				expected_value = collection_type->as.table_type.value_type;
			}
			if (expected_value && expected_value->base_type != ANY_TYPE &&
				!types_compatible(expected_value, value_type)) {
				char expected[128], got[128], msg[300];
				type_record_name(expected_value, expected, sizeof(expected));
				type_record_name(value_type, got, sizeof(got));
				snprintf(msg, sizeof(msg),
						 "Cannot assign '%s' to collection "
						 "of element type '%s'.",
						 got, expected);
				compiler_panic(compiler->parser, msg, TYPE);
			}
		}

		emit_word(compiler, OP_SET_COLLECTION);
		// A set expression leaves no useful value — push Nil.
		push_type_record(compiler, new_type_rec(compiler->owner, NIL_TYPE));
	} else {
		emit_word(compiler, OP_GET_COLLECTION);

		// Push the element type so downstream expressions know what
		// they received.
		ObjectTypeRecord *result_type = NULL;
		if (collection_type) {
			if (collection_type->base_type == ARRAY_TYPE) {
				result_type = collection_type->as.array_type.element_type;
			} else if (collection_type->base_type == TABLE_TYPE) {
				result_type = collection_type->as.table_type.value_type;
			}
		}
		if (!result_type) {
			result_type = new_type_rec(compiler->owner, ANY_TYPE);
		}
		push_type_record(compiler, result_type);
	}
}

static void var_declaration(Compiler *compiler, bool is_public)
{
	const uint16_t global = parse_variable(compiler, "Expected Variable Name.");

	const Token var_name = compiler->parser->previous;
	ObjectString *name_str = copy_string(compiler->owner, var_name.start, var_name.length);

	ObjectTypeRecord *annotated_type = NULL;
	if (match(compiler, TOKEN_COLON)) {
		annotated_type = parse_type_record(compiler);
	}

	ObjectTypeRecord *value_type = NULL;
	if (match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		value_type = pop_type_record(compiler);

		// Only validate if both sides are known (annotated_type is NULL
		// when no annotation was given; types_compatible returns false
		// for NULL, which would cause a NULL dereference below).
		if (annotated_type && value_type && annotated_type->base_type != ANY_TYPE &&
			value_type->base_type != ANY_TYPE && !types_compatible(annotated_type, value_type)) {
			char expected[128], got[128];
			type_record_name(annotated_type, expected, sizeof(expected));
			type_record_name(value_type, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE,
							"Type mismatch in variable "
							"declaration: expected '%s', got '%s'.",
							expected, got);
		}
	} else {
		// No initializer — implicit nil.
		// If an annotation was given, nil is only acceptable for
		// Nil-typed or Any-typed variables.
		if (annotated_type && annotated_type->base_type != NIL_TYPE && annotated_type->base_type != ANY_TYPE) {
			compiler_panic(compiler->parser,
						   "Variable with non-Nil type must be "
						   "initialized.",
						   TYPE);
		}
		emit_word(compiler, OP_NIL);
		value_type = new_type_rec(compiler->owner, NIL_TYPE);
	}

	ObjectTypeRecord *resolved_type = annotated_type ? annotated_type : value_type;
	if (!resolved_type) {
		resolved_type = T_ANY;
	}
	if (is_public) {
		type_table_set(compiler->owner->current_module_record->types, name_str, resolved_type);
	}

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

	// register types
	if (compiler->scope_depth > 0) {
		compiler->locals[compiler->local_count - 1].type = resolved_type;
	} else {
		type_table_set(compiler->type_table, name_str, resolved_type);
	}
	define_variable(compiler, global);
}

static void expression_statement(Compiler *compiler)
{
	expression(compiler);
	pop_type_record(compiler); // discard — value is unused
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after expression.");
	emit_word(compiler, OP_POP);
}

static void while_statement(Compiler *compiler)
{
	begin_scope(compiler);
	const int loopStart = current_chunk(compiler)->count;
	push_loop_context(compiler, LOOP_WHILE, loopStart);

	expression(compiler);

	ObjectTypeRecord *condition_type = pop_type_record(compiler);
	if (condition_type && condition_type->base_type != BOOL_TYPE && condition_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "'while' condition must be of type 'Bool'.", TYPE);
	}

	const int exitJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	emit_word(compiler, OP_POP);
	statement(compiler);
	emit_loop(compiler, loopStart);
	patch_jump(compiler, exitJump);
	emit_word(compiler, OP_POP);

	pop_loop_context(compiler);
	end_scope(compiler);
}

static void for_statement(Compiler *compiler)
{
	begin_scope(compiler);

	if (match(compiler, TOKEN_SEMICOLON)) {
		// no initializer
	} else if (match(compiler, TOKEN_LET)) {
		var_declaration(compiler, false);
	} else {
		expression_statement(compiler);
	}

	int loopStart = current_chunk(compiler)->count;
	int exitJump = -1;

	if (!match(compiler, TOKEN_SEMICOLON)) {
		expression(compiler);

		// Condition must be Bool
		ObjectTypeRecord *condition_type = pop_type_record(compiler);
		if (condition_type && condition_type->base_type != BOOL_TYPE && condition_type->base_type != ANY_TYPE) {
			compiler_panic(compiler->parser, "'for' condition must be of type 'Bool'.", TYPE);
		}

		consume(compiler, TOKEN_SEMICOLON, "Expected ';' after loop condition");
		exitJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
		emit_word(compiler, OP_POP);
	}

	const int bodyJump = emit_jump(compiler, OP_JUMP);
	const int incrementStart = current_chunk(compiler)->count;
	push_loop_context(compiler, LOOP_FOR, incrementStart);

	// no type enforcement, but pop type
	expression(compiler);
	pop_type_record(compiler);
	emit_word(compiler, OP_POP);

	emit_loop(compiler, loopStart);
	loopStart = incrementStart;
	patch_jump(compiler, bodyJump);

	statement(compiler);
	emit_loop(compiler, loopStart);

	if (exitJump != -1) {
		patch_jump(compiler, exitJump);
		emit_word(compiler, OP_POP);
	}

	pop_loop_context(compiler);
	end_scope(compiler);
}

static void if_statement(Compiler *compiler)
{
	expression(compiler);

	// Condition must be Bool
	ObjectTypeRecord *condition_type = pop_type_record(compiler);
	if (condition_type && condition_type->base_type != BOOL_TYPE && condition_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "'if' condition must be of type 'Bool'.", TYPE);
	}

	const int thenJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	emit_word(compiler, OP_POP);
	statement(compiler);

	const int elseJump = emit_jump(compiler, OP_JUMP);
	patch_jump(compiler, thenJump);
	emit_word(compiler, OP_POP);

	if (match(compiler, TOKEN_ELSE))
		statement(compiler);

	patch_jump(compiler, elseJump);
}

static void return_statement(Compiler *compiler)
{
	if (compiler->type == TYPE_SCRIPT) {
		compiler_panic(compiler->parser, "Cannot use <return> outside of a function.", SYNTAX);
	}

	compiler->has_return = true;

	if (match(compiler, TOKEN_SEMICOLON)) {
		// check that the function expects Nil
		if (compiler->return_type && compiler->return_type->base_type != NIL_TYPE &&
			compiler->return_type->base_type != ANY_TYPE) {
			compiler_panic(compiler->parser, "Non-Nil return type requires a return value.", TYPE);
		}
		emit_return(compiler);
	} else {
		expression(compiler);
		consume(compiler, TOKEN_SEMICOLON, "Expected ';' after return value.");

		ObjectTypeRecord *value_type = pop_type_record(compiler);

		// Only validate if both sides are known
		if (value_type && compiler->return_type && compiler->return_type->base_type != ANY_TYPE &&
			value_type->base_type != ANY_TYPE) {
			if (!types_compatible(compiler->return_type, value_type)) {
				char expected[128];
				char got[128];
				type_record_name(compiler->return_type, expected, sizeof(expected));
				type_record_name(value_type, got, sizeof(got));
				char msg[300];
				snprintf(msg, sizeof(msg),
						 "Return type mismatch: expected '%s', "
						 "got '%s'.",
						 expected, got);
				compiler_panic(compiler->parser, msg, TYPE);
			}
		}
		emit_word(compiler, OP_RETURN);
	}
}

static void import_statement(Compiler *compiler, bool is_dynamic)
{
	bool hasParen = false;
	if (compiler->parser->current.type == TOKEN_LEFT_PAREN) {
		consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after use statement.");
		hasParen = true;
	}

	uint16_t nameCount = 0;
	uint16_t names[UINT8_MAX] = {0};
	uint16_t aliases[UINT8_MAX] = {0};
	bool aliasPresence[UINT8_MAX] = {0};

	Token nameTokens[UINT8_MAX] = {0};
	Token aliasTokens[UINT8_MAX] = {0};

	do {
		if (nameCount >= UINT8_MAX) {
			compiler_panic(compiler->parser, "Cannot import more than 255 names.", IMPORT_EXTENT);
		}
		consume_identifier_like(compiler, "Expected name to import from module.");

		nameTokens[nameCount] = compiler->parser->previous;
		names[nameCount] = identifier_constant(compiler, &compiler->parser->previous);

		if (compiler->parser->current.type == TOKEN_AS) {
			consume(compiler, TOKEN_AS, "Expected 'as' keyword.");
			consume(compiler, TOKEN_IDENTIFIER, "Expected alias name after 'as'.");
			aliasTokens[nameCount] = compiler->parser->previous;
			aliases[nameCount] = identifier_constant(compiler, &compiler->parser->previous);
			aliasPresence[nameCount] = true;
		}

		nameCount++;
	} while (match(compiler, TOKEN_COMMA));

	if (hasParen) {
		consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after last imported name.");
	}

	consume(compiler, TOKEN_FROM, "Expected 'from' after import statement.");
	consume(compiler, TOKEN_STRING, "Expected string literal for module name.");

	bool isNative = (memcmp(compiler->parser->previous.start, "\"crux:", 6) == 0);

	if (is_dynamic && isNative) {
		compiler_panic(compiler->parser, "Cannot use dynamic import for native modules. Use 'use'.", SYNTAX);
		return;
	}

	uint16_t module_const;
	ObjectModuleRecord *statically_imported_mod = NULL;

	if (isNative) {
		module_const = make_constant(compiler,
									 OBJECT_VAL(copy_string(compiler->owner, compiler->parser->previous.start + 6,
															compiler->parser->previous.length - 7)));
		emit_words(compiler, OP_USE_NATIVE, nameCount);
	} else {
		ObjectString *path_str = copy_string(compiler->owner, compiler->parser->previous.start + 1,
											 compiler->parser->previous.length - 2);
		module_const = make_constant(compiler, OBJECT_VAL(path_str));

		// If it's a static 'use', compile it right now!
		if (!is_dynamic) {
			statically_imported_mod = compile_module_statically(compiler, path_str);
			if (!statically_imported_mod || statically_imported_mod->state == STATE_ERROR) {
				compiler_panicf(compiler->parser, IMPORT, "Failed to compile module '%s'.", path_str->chars);
				return;
			}
		}

		// Emit the standard runtime opcodes (they work for both static and dynamic!)
		emit_words(compiler, OP_USE_MODULE, module_const);
		emit_words(compiler, OP_FINISH_USE, nameCount);
	}

	// Emit names and aliases for the VM to bind at runtime
	for (uint16_t i = 0; i < nameCount; i++)
		emit_word(compiler, names[i]);
	for (uint16_t i = 0; i < nameCount; i++)
		emit_word(compiler, aliasPresence[i] ? aliases[i] : names[i]);

	if (isNative) {
		emit_word(compiler, module_const);
	}

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after import statement.");

	// --- TYPE RESOLUTION ---
	for (uint16_t i = 0; i < nameCount; i++) {
		Token visible = aliasPresence[i] ? aliasTokens[i] : nameTokens[i];
		ObjectString *name_str = copy_string(compiler->owner, visible.start, visible.length);

		ObjectTypeRecord *resolved_type = NULL;

		if (compiler->scope_depth == 0) {
			if (isNative) {
				// 1. Native Stdlib Module Type Resolution
				for (int j = 0; j < compiler->owner->native_modules.count; j++) {
					const Table *current_table = compiler->owner->native_modules.modules[j].names;
					Value value;
					if (table_get(current_table, name_str, &value) && IS_CRUX_NATIVE_CALLABLE(value)) {
						const ObjectNativeCallable *callable = AS_CRUX_NATIVE_CALLABLE(value);
						ObjectTypeRecord **args_copy = NULL;
						if (callable->arity > 0) {
							args_copy = ALLOCATE(compiler->owner, ObjectTypeRecord *, callable->arity);
							for (int k = 0; k < callable->arity; k++)
								args_copy[k] = callable->arg_types[k];
						}
						resolved_type = new_function_type_rec(compiler->owner, args_copy, callable->arity,
															  callable->return_type);
						break;
					}
				}
			} else if (!is_dynamic && statically_imported_mod) {
				// 2. Static User Module Type Resolution
				if (!type_table_get(statically_imported_mod->types, name_str, &resolved_type)) {
					char msg[128];
					snprintf(msg, sizeof(msg), "Module does not export '%s'", name_str->chars);
					compiler_panic(compiler->parser, msg, NAME);
					resolved_type = T_ANY; // Fallback to prevent crashes during panic
				}
			}
		}

		// 3. Dynamic import or Local Scope fallback
		if (!resolved_type) {
			resolved_type = T_ANY;
		}

		type_table_set(compiler->type_table, name_str, resolved_type);
	}
}

static void use_statement(Compiler *compiler)
{
	import_statement(compiler, false);
}
static void dynuse_statement(Compiler *compiler)
{
	import_statement(compiler, true);
}

static void struct_declaration(Compiler *compiler, bool is_public)
{
	consume(compiler, TOKEN_IDENTIFIER, "Expected struct name.");
	const Token structName = compiler->parser->previous;

	GC_PROTECT_START(compiler->owner->current_module_record);

	ObjectString *struct_name_str = copy_string(compiler->owner, structName.start, structName.length);
	GC_PROTECT(compiler->owner->current_module_record, OBJECT_VAL(struct_name_str));

	const uint16_t nameConstant = identifier_constant(compiler, &structName);
	ObjectStruct *structObject = new_struct_type(compiler->owner, struct_name_str);
	GC_PROTECT(compiler->owner->current_module_record, OBJECT_VAL(structObject));

	// Reserve the local slot (or note the global name) before we start
	// parsing the body so the struct can refer to itself recursively.
	declare_variable(compiler);

	const int local_index = (compiler->scope_depth > 0) ? compiler->local_count - 1 : -1;

	const uint16_t structConstant = make_constant(compiler, OBJECT_VAL(structObject));
	emit_words(compiler, OP_STRUCT, structConstant);
	define_variable(compiler, nameConstant);

	consume(compiler, TOKEN_LEFT_BRACE, "Expected '{' before struct body.");

	// Build a ObjectTypeTable mapping field name -> ObjectTypeRecord.
	// This is heap-allocated so it outlives the compiler's type_arena.
	ObjectTypeTable *field_types = new_type_table(compiler->owner, INITIAL_TYPE_TABLE_SIZE);
	int fieldCount = 0;

	if (!match(compiler, TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many fields in struct.", SYNTAX);
				break;
			}

			consume(compiler, TOKEN_IDENTIFIER, "Expected field name.");
			ObjectString *fieldName = copy_string(compiler->owner, compiler->parser->previous.start,
												  compiler->parser->previous.length);

			GC_PROTECT(compiler->owner->current_module_record, OBJECT_VAL(fieldName));

			Value fieldNameCheck;
			if (table_get(&structObject->fields, fieldName, &fieldNameCheck)) {
				compiler_panic(compiler->parser,
							   "Duplicate field name in struct "
							   "declaration.",
							   SYNTAX);
				break;
			}

			// Optional field type annotation: fieldName: Type
			ObjectTypeRecord *field_type = NULL;
			if (match(compiler, TOKEN_COLON)) {
				field_type = parse_type_record(compiler);
			} else {
				field_type = new_type_rec(compiler->owner, ANY_TYPE);
			}

			type_table_set(field_types, fieldName, field_type);

			table_set(compiler->owner, &structObject->fields, fieldName, INT_VAL(fieldCount));
			fieldCount++;
		} while (match(compiler, TOKEN_COMMA));
	}

	if (fieldCount != 0) {
		consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after struct body.");
	}

	GC_PROTECT_END(compiler->owner->current_module_record);

	ObjectTypeRecord *struct_type = new_struct_type_rec(compiler->owner, structObject, field_types, fieldCount);

	// type registration
	if (compiler->scope_depth == 0) {
		type_table_set(compiler->type_table, struct_name_str, struct_type);
	} else {
		compiler->locals[local_index].type = struct_type;
	}
	if (is_public) {
		type_table_set(compiler->type_table, struct_name_str, struct_type);
	}
}

static void result_unwrap(Compiler *compiler, bool can_assign)
{
	(void)can_assign;

	ObjectTypeRecord *type = pop_type_record(compiler);

	if (!type || type->base_type == ANY_TYPE) {
		// Unknown type — vm will catch errors.
		emit_word(compiler, OP_UNWRAP);
		push_type_record(compiler, T_ANY);
		return;
	}

	if (type->base_type != RESULT_TYPE) {
		compiler_panic(compiler->parser, "'?' operator requires a 'Result' type.", TYPE);
		push_type_record(compiler, T_ANY);
		return;
	}
	emit_word(compiler, OP_UNWRAP);

	ObjectTypeRecord *ok_type = type->as.result_type.ok_type;
	push_type_record(compiler, ok_type ? ok_type : T_ANY);
}

/**
 * Synchronizes the parser after encountering a syntax error.
 *
 * Discards tokens until a statement boundary is found to minimize cascading
 * errors.
 */
static void synchronize(Compiler *compiler)
{
	compiler->parser->panic_mode = false;

	while (compiler->parser->current.type != TOKEN_EOF) {
		if (compiler->parser->previous.type == TOKEN_SEMICOLON)
			return;
		switch (compiler->parser->current.type) {
		case TOKEN_STRUCT:
		case TOKEN_PUB:
		case TOKEN_FN:
		case TOKEN_LET:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_RETURN:
		case TOKEN_PANIC:
			return;
		default:;
		}
		advance(compiler);
	}
}

static void begin_match_scope(Compiler *compiler)
{
	if (compiler->match_depth > 0) {
		compiler_panic(compiler->parser, "Nesting match statements is not allowed.", SYNTAX);
	}
	compiler->match_depth++;
}

static void end_match_scope(Compiler *compiler)
{
	compiler->match_depth--;
}

static void give_statement(Compiler *compiler)
{
	if (compiler->match_depth == 0) {
		compiler_panic(compiler->parser, "'give' can only be used inside a match expression.", SYNTAX);
	}

	if (match(compiler, TOKEN_SEMICOLON)) {
		emit_word(compiler, OP_NIL);
		compiler->last_give_type = new_type_rec(compiler->owner, NIL_TYPE);
	} else {
		expression(compiler);
		// Record for match_expression to check arm consistency.
		compiler->last_give_type = pop_type_record(compiler);
		consume(compiler, TOKEN_SEMICOLON, "Expected ';' after give statement.");
	}

	emit_word(compiler, OP_GIVE);
}

static void match_expression(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	begin_match_scope(compiler);

	expression(compiler);

	// The target's type tells us what patterns are valid.
	ObjectTypeRecord *target_type = pop_type_record(compiler);
	if (!target_type)
		target_type = T_ANY;

	consume(compiler, TOKEN_LEFT_BRACE, "Expected '{' after match target.");

	int *endJumps = ALLOCATE(compiler->owner, int, 8);
	int jumpCount = 0;
	int jumpCapacity = 8;

	emit_word(compiler, OP_MATCH);

	bool hasDefault = false;
	bool hasOkPattern = false;
	bool hasErrPattern = false;

	// Track the type produced by each arm for consistency checking.
	// NULL means we haven't seen an arm yet.
	ObjectTypeRecord *arm_type = NULL;

	while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
		int jumpIfNotMatch = -1;
		uint16_t bindingSlot = UINT16_MAX;
		bool hasBinding = false;
		compiler->last_give_type = NULL;

		if (match(compiler, TOKEN_DEFAULT)) {
			if (hasDefault) {
				compiler_panic(compiler->parser,
							   "Cannot have multiple default "
							   "patterns.",
							   SYNTAX);
			}
			hasDefault = true;

		} else if (match(compiler, TOKEN_OK)) {
			// Ok/Err patterns require a Result target.
			if (target_type->base_type != RESULT_TYPE && target_type->base_type != ANY_TYPE) {
				compiler_panic(compiler->parser,
							   "'Ok' pattern requires a 'Result' "
							   "match target.",
							   TYPE);
			}
			if (hasOkPattern) {
				compiler_panic(compiler->parser, "Cannot have multiple 'Ok' patterns.", SYNTAX);
			}
			hasOkPattern = true;
			jumpIfNotMatch = emit_jump(compiler, OP_RESULT_MATCH_OK);

			if (match(compiler, TOKEN_LEFT_PAREN)) {
				begin_scope(compiler);
				hasBinding = true;
				consume(compiler, TOKEN_IDENTIFIER,
						"Expected identifier after 'Ok' "
						"pattern.");
				declare_variable(compiler);
				bindingSlot = compiler->local_count - 1;

				// Stamp the binding's type from the Result's
				// ok_type, falling back to ANY_TYPE.
				ObjectTypeRecord *ok_type = (target_type->base_type == RESULT_TYPE)
												? target_type->as.result_type.ok_type
												: NULL;
				compiler->locals[bindingSlot].type = ok_type ? ok_type : new_type_rec(compiler->owner, ANY_TYPE);

				mark_initialized(compiler);
				consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}

		} else if (match(compiler, TOKEN_ERR)) {
			if (target_type->base_type != RESULT_TYPE && target_type->base_type != ANY_TYPE) {
				compiler_panic(compiler->parser,
							   "'Err' pattern requires a 'Result' "
							   "match target.",
							   TYPE);
			}
			if (hasErrPattern) {
				compiler_panic(compiler->parser, "Cannot have multiple 'Err' patterns.", SYNTAX);
			}
			hasErrPattern = true;
			jumpIfNotMatch = emit_jump(compiler, OP_RESULT_MATCH_ERR);

			if (match(compiler, TOKEN_LEFT_PAREN)) {
				begin_scope(compiler);
				hasBinding = true;
				consume(compiler, TOKEN_IDENTIFIER,
						"Expected identifier after 'Err' "
						"pattern.");
				declare_variable(compiler);
				bindingSlot = compiler->local_count - 1;

				// Err bindings always hold an ObjectError.
				compiler->locals[bindingSlot].type = new_type_rec(compiler->owner, ERROR_TYPE);

				mark_initialized(compiler);
				consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}

		} else {
			// Value pattern: the expression must be compatible
			// with the target type.
			expression(compiler);
			ObjectTypeRecord *pattern_type = pop_type_record(compiler);

			if (pattern_type && target_type && pattern_type->base_type != ANY_TYPE &&
				target_type->base_type != ANY_TYPE) {
				if (!types_compatible(target_type, pattern_type)) {
					char expected[128], got[128], msg[300];
					type_record_name(target_type, expected, sizeof(expected));
					type_record_name(pattern_type, got, sizeof(got));
					snprintf(msg, sizeof(msg),
							 "Pattern type '%s' is not "
							 "compatible with match target "
							 "type '%s'.",
							 got, expected);
					compiler_panic(compiler->parser, msg, TYPE);
				}
			}

			jumpIfNotMatch = emit_jump(compiler, OP_MATCH_JUMP);
		}

		consume(compiler, TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

		if (bindingSlot != UINT16_MAX) {
			emit_words(compiler, OP_RESULT_BIND, bindingSlot);
		}

		// Compile arm body and track the type it produces.
		ObjectTypeRecord *this_arm_type = NULL;

		if (match(compiler, TOKEN_LEFT_BRACE)) {
			block(compiler);
			// Blocks produce values only via give — read it back.
			this_arm_type = compiler->last_give_type ? compiler->last_give_type
													 : new_type_rec(compiler->owner, NIL_TYPE);
		} else if (match(compiler, TOKEN_GIVE)) {
			if (match(compiler, TOKEN_SEMICOLON)) {
				emit_word(compiler, OP_NIL);
				this_arm_type = new_type_rec(compiler->owner, NIL_TYPE);
			} else {
				expression(compiler);
				this_arm_type = pop_type_record(compiler);
				consume(compiler, TOKEN_SEMICOLON, "Expected ';' after give expression.");
			}
			emit_word(compiler, OP_GIVE);
		} else {
			expression(compiler);
			this_arm_type = pop_type_record(compiler);
			consume(compiler, TOKEN_SEMICOLON, "Expected ';' after expression.");
		}

		// Check arm type consistency.
		if (!this_arm_type)
			this_arm_type = new_type_rec(compiler->owner, ANY_TYPE);

		if (arm_type == NULL) {
			arm_type = this_arm_type;
		} else if (arm_type->base_type != ANY_TYPE && this_arm_type->base_type != ANY_TYPE) {
			if (!types_compatible(arm_type, this_arm_type)) {
				char expected[128], got[128], msg[300];
				type_record_name(arm_type, expected, sizeof(expected));
				type_record_name(this_arm_type, got, sizeof(got));
				snprintf(msg, sizeof(msg),
						 "Match arms produce inconsistent "
						 "types: expected '%s', got '%s'.",
						 expected, got);
				compiler_panic(compiler->parser, msg, TYPE);
			}
		}

		if (hasBinding) {
			end_scope(compiler);
		}

		if (jumpCount + 1 > jumpCapacity) {
			const int oldCapacity = jumpCapacity;
			jumpCapacity = GROW_CAPACITY(oldCapacity);
			endJumps = GROW_ARRAY(compiler->owner, int, endJumps, oldCapacity, jumpCapacity);
		}

		endJumps[jumpCount++] = emit_jump(compiler, OP_JUMP);

		if (jumpIfNotMatch != -1) {
			patch_jump(compiler, jumpIfNotMatch);
		}
	}

	if (jumpCount == 0) {
		compiler_panic(compiler->parser, "'match' expression must have at least one arm.", SYNTAX);
	}

	if (hasOkPattern || hasErrPattern) {
		if (!hasDefault && !(hasOkPattern && hasErrPattern)) {
			compiler_panic(compiler->parser,
						   "Result 'match' must have both 'Ok' and "
						   "'Err' patterns, or a default case.",
						   SYNTAX);
		}
	} else if (!hasDefault) {
		compiler_panic(compiler->parser, "'match' expression must have a 'default' case.", SYNTAX);
	}

	for (int i = 0; i < jumpCount; i++) {
		patch_jump(compiler, endJumps[i]);
	}

	emit_word(compiler, OP_MATCH_END);

	FREE_ARRAY(compiler->owner, int, endJumps, jumpCapacity);
	consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
	end_match_scope(compiler);

	// Push the unified arm type so callers know what the match produces.
	push_type_record(compiler, arm_type ? arm_type : T_ANY);
}

static void continue_statement(Compiler *compiler)
{
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after 'continue',");
	const int continueTarget = get_current_continue_target(compiler);
	if (continueTarget == -1) {
		return;
	}
	const LoopContext *loopContext = &compiler->loop_stack[compiler->loop_depth - 1];
	cleanupLocalsToDepth(compiler, loopContext->scope_depth);
	emit_loop(compiler, continueTarget);
}

static void break_statement(Compiler *compiler)
{
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after 'break'.");
	if (compiler->loop_depth <= 0) {
		compiler_panic(compiler->parser, "Cannot use 'break' outside of a loop.", SYNTAX);
		return;
	}
	const LoopContext *loopContext = &compiler->loop_stack[compiler->loop_depth - 1];
	cleanupLocalsToDepth(compiler, loopContext->scope_depth);
	add_break_jump(compiler, emit_jump(compiler, OP_JUMP));
}

static void panic_statement(Compiler *compiler)
{
	expression(compiler);
	ObjectTypeRecord *type = pop_type_record(compiler);

	if (type && type->base_type != STRING_TYPE && type->base_type != ANY_TYPE) {
		char got[128], msg[300];
		type_record_name(type, got, sizeof(got));
		snprintf(msg, sizeof(msg), "'panic' requires a 'String', got '%s'.", got);
		compiler_panic(compiler->parser, msg, TYPE);
	}

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after 'panic'.");
	emit_word(compiler, OP_PANIC);
}

static void public_declaration(Compiler *compiler)
{
	if (compiler->scope_depth > 0) {
		compiler_panic(compiler->parser, "Cannot declare public members in a local scope.", SYNTAX);
	}
	emit_word(compiler, OP_PUB);
	if (match(compiler, TOKEN_FN)) {
		fn_declaration(compiler, true);
	} else if (match(compiler, TOKEN_LET)) {
		var_declaration(compiler, true);
	} else if (match(compiler, TOKEN_STRUCT)) {
		struct_declaration(compiler, false);
	} else {
		compiler_panic(compiler->parser, "Expected 'fn', 'let', or 'struct' after 'pub'.", SYNTAX);
	}
}

static void declaration(Compiler *compiler)
{
	if (match(compiler, TOKEN_LET)) {
		var_declaration(compiler, false);
	} else if (match(compiler, TOKEN_FN)) {
		fn_declaration(compiler, false);
	} else if (match(compiler, TOKEN_STRUCT)) {
		struct_declaration(compiler, false);
	} else if (match(compiler, TOKEN_PUB)) {
		public_declaration(compiler);
	} else {
		statement(compiler);
	}

	if (compiler->parser->panic_mode)
		synchronize(compiler);
}

static void statement(Compiler *compiler)
{
	if (match(compiler, TOKEN_IF)) {
		if_statement(compiler);
	} else if (match(compiler, TOKEN_LEFT_BRACE)) {
		begin_scope(compiler);
		block(compiler);
		end_scope(compiler);
	} else if (match(compiler, TOKEN_WHILE)) {
		while_statement(compiler);
	} else if (match(compiler, TOKEN_FOR)) {
		for_statement(compiler);
	} else if (match(compiler, TOKEN_RETURN)) {
		return_statement(compiler);
	} else if (match(compiler, TOKEN_DYN_USE)) {
		dynuse_statement(compiler);
	} else if (match(compiler, TOKEN_USE)) {
		use_statement(compiler);
	} else if (match(compiler, TOKEN_GIVE)) {
		give_statement(compiler);
	} else if (match(compiler, TOKEN_BREAK)) {
		break_statement(compiler);
	} else if (match(compiler, TOKEN_CONTINUE)) {
		continue_statement(compiler);
	} else if (match(compiler, TOKEN_PANIC)) {
		panic_statement(compiler);
	} else {
		expression_statement(compiler);
	}
}

static void grouping(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	expression(compiler);
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	char *end;
	errno = 0;

	const char *numberStart = compiler->parser->previous.start;
	const double number = strtod(numberStart, &end);

	if (end == numberStart) {
		compiler_panic(compiler->parser, "Failed to form number", SYNTAX);
		return;
	}
	if (errno == ERANGE) {
		emit_constant(compiler, FLOAT_VAL(number));
		push_type_record(compiler, new_type_rec(compiler->owner, FLOAT_TYPE));
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
		emit_constant(compiler, FLOAT_VAL(number));
		push_type_record(compiler, new_type_rec(compiler->owner, FLOAT_TYPE));
	} else {
		const int32_t integer = (int32_t)number;
		if ((double)integer == number) {
			emit_constant(compiler, INT_VAL(integer));
			push_type_record(compiler, new_type_rec(compiler->owner, INT_TYPE));
		} else {
			emit_constant(compiler, FLOAT_VAL(number));
			push_type_record(compiler, new_type_rec(compiler->owner, FLOAT_TYPE));
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

static void string(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	char *processed = ALLOCATE(compiler->owner, char, compiler->parser->previous.length);

	if (processed == NULL) {
		compiler_panic(compiler->parser, "Cannot allocate memory for string expression.", MEMORY);
		return;
	}

	int processedLength = 0;
	const char *src = (char *)compiler->parser->previous.start + 1;
	const int srcLength = compiler->parser->previous.length - 2;

	if (srcLength == 0) {
		ObjectString *string = copy_string(compiler->owner, "", 0);
		push_type_record(compiler, new_type_rec(compiler->owner, STRING_TYPE));
		emit_constant(compiler, OBJECT_VAL(string));
		FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
		return;
	}

	for (int i = 0; i < srcLength; i++) {
		if (src[i] == '\\') {
			if (i + 1 >= srcLength) {
				compiler_panic(compiler->parser,
							   "Unterminated escape sequence at "
							   "end of string",
							   SYNTAX);
				FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
				return;
			}

			bool error;
			const char escaped = process_escape_sequence(src[i + 1], &error);
			if (error) {
				char errorMessage[64];
				snprintf(errorMessage, 64, "Unexpected escape sequence '\\%c'", src[i + 1]);
				compiler_panic(compiler->parser, errorMessage, SYNTAX);
				FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
				return;
			}

			processed[processedLength++] = escaped;
			i++;
		} else {
			processed[processedLength++] = src[i];
		}
	}

	char *temp = GROW_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length, processedLength + 1);
	if (temp == NULL) {
		compiler_panic(compiler->parser, "Cannot allocate memory for string expression.", MEMORY);
		FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
		return;
	}
	processed = temp;
	processed[processedLength] = '\0';
	ObjectString *string = take_string(compiler->owner, processed, processedLength);
	push_type_record(compiler, new_type_rec(compiler->owner, STRING_TYPE));
	emit_constant(compiler, OBJECT_VAL(string));
}

static void unary(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = compiler->parser->previous.type;

	// compile the operand
	parse_precedence(compiler, PREC_UNARY);

	switch (operatorType) {
	case TOKEN_NOT: {
		// check if this is a boolean type
		ObjectTypeRecord *bool_expected = pop_type_record(compiler);
		if (!bool_expected || bool_expected->base_type != BOOL_TYPE) {
			compiler_panic(compiler->parser, "Expected 'Bool'type for 'not' operator.", TYPE);
		}
		push_type_record(compiler, bool_expected);
		emit_word(compiler, OP_NOT);
		break;
	}
	case TOKEN_MINUS: {
		// check if this is a negatable type
		ObjectTypeRecord *num_expected = pop_type_record(compiler);
		if (!num_expected || !(num_expected->base_type & (INT_TYPE | FLOAT_TYPE))) {
			compiler_panic(compiler->parser, "Expected 'Int | Float' type for '-' operator.", TYPE);
		}
		push_type_record(compiler, num_expected);
		emit_word(compiler, OP_NEGATE);
		break;
	}
	default:
		return; // unreachable
	}
}

static void typeof_expression(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	parse_precedence(compiler, PREC_UNARY);
	emit_word(compiler, OP_TYPEOF); // this will emit a string representation of the
									// type at runtime
	push_type_record(compiler, new_type_rec(compiler->owner, STRING_TYPE));
}

static ObjectModuleRecord *compile_module_statically(Compiler *compiler, ObjectString *path)
{
	Value cached_val;
	if (table_get(&compiler->owner->module_cache, path, &cached_val)) {
		ObjectModuleRecord *mod = AS_CRUX_MODULE_RECORD(cached_val);
		if (mod->state == STATE_LOADING) {
			compiler_panicf(compiler->parser, IMPORT, "Circular dependency detected while loading module '%s'.",
							path->chars);
			return mod;
		}
		return mod;
	}

	FileResult result = read_file(path->chars);
	if (result.error) {
		compiler_panicf(compiler->parser, IMPORT, "Could not read file '%s': %s", path->chars, result.error);
		return NULL;
	}
	char *source = result.content;

	ObjectModuleRecord *new_module = new_object_module_record(compiler->owner, path, false, false);
	table_set(compiler->owner, &compiler->owner->module_cache, path, OBJECT_VAL(new_module));

	ObjectModuleRecord *previous_module = compiler->owner->current_module_record;
	compiler->owner->current_module_record = new_module;

	Compiler imported_compiler;
	ObjectFunction *module_func = compile(compiler->owner, &imported_compiler, compiler, source);
	free(result.content);
	source = NULL;

	if (module_func != NULL) {
		new_module->module_closure = new_closure(compiler->owner, module_func);
		new_module->state = STATE_LOADED;
	} else {
		new_module->state = STATE_ERROR;
	}

	compiler->owner->current_module_record = previous_module;

	return new_module;
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, infix_call, NULL, PREC_CALL},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {table_literal, NULL, NULL, PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_SQUARE] = {array_literal, collection_index, NULL, PREC_CALL},
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
	[TOKEN_NIL_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_BOOL_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_INT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FLOAT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_STRING_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ARRAY_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_TABLE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ERROR_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RESULT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RANDOM_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_FILE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_STRUCT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_VECTOR_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_COMPLEX_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_MATRIX_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_SET_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_TUPLE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_BUFFER_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_RANGE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[TOKEN_ANY_TYPE] = {NULL, NULL, NULL, PREC_NONE},
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parse_precedence(Compiler *compiler, const Precedence precedence)
{
	advance(compiler);
	ParseFn prefixRule;
	if (is_identifier_like(compiler->parser->previous.type)) {
		prefixRule = get_rule(TOKEN_IDENTIFIER)->prefix;
	} else {
		prefixRule = get_rule(compiler->parser->previous.type)->prefix;
	}
	if (prefixRule == NULL) {
		compiler_panic(compiler->parser, "Expected expression.", SYNTAX);
		return;
	}

	const bool can_assign = precedence <= PREC_ASSIGNMENT;
	prefixRule(compiler, can_assign);

	while (precedence <= get_rule(compiler->parser->current.type)->precedence) {
		advance(compiler);
		const ParseRule *rule = get_rule(compiler->parser->previous.type);
		if (rule->infix != NULL) {
			rule->infix(compiler, can_assign);
		} else if (rule->postfix != NULL) {
			rule->postfix(compiler, can_assign);
		}
	}

	if (can_assign && match(compiler, TOKEN_EQUAL)) {
		compiler_panic(compiler->parser, "Invalid Assignment Target", SYNTAX);
	}
}

static ParseRule *get_rule(const CruxTokenType type)
{
	return &rules[type]; // Returns the rule at the given index
}

// ── Pre-scanner
// ───────────
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
static void pre_advance(Compiler *compiler)
{
	compiler->parser->previous = compiler->parser->current;
	for (;;) {
		compiler->parser->current = scan_token(compiler->parser->scanner);
		if (compiler->parser->current.type != TOKEN_ERROR)
			break;
	}
}

// Skip a balanced brace block { ... } starting at the current token (which
// must be TOKEN_LEFT_BRACE). Handles nesting.
static void pre_skip_block(Compiler *compiler)
{
	if (compiler->parser->current.type != TOKEN_LEFT_BRACE)
		return;
	pre_advance(compiler); // consume '{'
	int depth = 1;
	while (depth > 0 && compiler->parser->current.type != TOKEN_EOF) {
		if (compiler->parser->current.type == TOKEN_LEFT_BRACE)
			depth++;
		if (compiler->parser->current.type == TOKEN_RIGHT_BRACE)
			depth--;
		pre_advance(compiler);
	}
}

// Skip a balanced parenthesis group ( ... ) starting at current token.
static void pre_skip_parens(Compiler *compiler)
{
	if (compiler->parser->current.type != TOKEN_LEFT_PAREN)
		return;
	pre_advance(compiler); // consume '('
	int depth = 1;
	while (depth > 0 && compiler->parser->current.type != TOKEN_EOF) {
		if (compiler->parser->current.type == TOKEN_LEFT_PAREN)
			depth++;
		if (compiler->parser->current.type == TOKEN_RIGHT_PAREN)
			depth--;
		pre_advance(compiler);
	}
}

// Advance past a type annotation starting at the current token.
// Handles nested brackets and parentheses (e.g. Array[Int], (Int)->Bool,
// Table[String:Int], union types separated by '|').
static void pre_skip_type(Compiler *compiler)
{
	// A type may be: a keyword type, an identifier, a function type
	// (parens), followed by optional '[...]' subscript, followed by
	// optional '|' union chains.
	for (;;) {
		const CruxTokenType t = compiler->parser->current.type;

		if (t == TOKEN_LEFT_PAREN) {
			// Function type: (T, T) -> T
			pre_skip_parens(compiler);
			// consume '->'
			if (compiler->parser->current.type == TOKEN_ARROW)
				pre_advance(compiler);
			pre_skip_type(compiler); // return type
		} else if (t == TOKEN_INT_TYPE || t == TOKEN_FLOAT_TYPE || t == TOKEN_BOOL_TYPE || t == TOKEN_STRING_TYPE ||
				   t == TOKEN_NIL_TYPE || t == TOKEN_ANY_TYPE || t == TOKEN_ARRAY_TYPE || t == TOKEN_TABLE_TYPE ||
				   t == TOKEN_VECTOR_TYPE || t == TOKEN_MATRIX_TYPE || t == TOKEN_BUFFER_TYPE ||
				   t == TOKEN_ERROR_TYPE || t == TOKEN_RESULT_TYPE || t == TOKEN_RANGE_TYPE || t == TOKEN_TUPLE_TYPE ||
				   t == TOKEN_COMPLEX_TYPE || t == TOKEN_SET_TYPE || t == TOKEN_RANDOM_TYPE || t == TOKEN_FILE_TYPE ||
				   t == TOKEN_IDENTIFIER) {
			pre_advance(compiler); // consume the base type token
			// Optional subscript: Array[Int], Table[K:V], etc.
			if (compiler->parser->current.type == TOKEN_LEFT_SQUARE) {
				pre_advance(compiler); // consume '['
				int depth = 1;
				while (depth > 0 && compiler->parser->current.type != TOKEN_EOF) {
					if (compiler->parser->current.type == TOKEN_LEFT_SQUARE)
						depth++;
					if (compiler->parser->current.type == TOKEN_RIGHT_SQUARE)
						depth--;
					pre_advance(compiler);
				}
			}
		} else {
			// Unknown/unsupported — stop.
			break;
		}

		// Union continuation: T | T | ...
		if (compiler->parser->current.type == TOKEN_PIPE) {
			pre_advance(compiler); // consume '|'
			continue; // parse next variant
		}
		break;
	}
}

// Collect a single top-level struct declaration into pre_compiler's type_table.
// On entry parser.current is TOKEN_STRUCT (already consumed by caller).
static void pre_collect_struct(Compiler *compiler)
{
	// Consume struct name.
	if (compiler->parser->current.type != TOKEN_IDENTIFIER)
		return;
	const Token name_token = compiler->parser->current;
	pre_advance(compiler);

	// Expect '{' to start the struct body.
	if (compiler->parser->current.type != TOKEN_LEFT_BRACE)
		return;
	pre_advance(compiler); // consume '{'

	// Build a field_types ObjectTypeTable.
	ObjectTypeTable *field_types = new_type_table(compiler->owner, INITIAL_TYPE_TABLE_SIZE);
	int field_count = 0;

	// Create a lightweight ObjectStruct so new_struct_type_rec works.
	ObjectString *struct_name = copy_string(compiler->owner, name_token.start, name_token.length);
	ObjectStruct *struct_obj = new_struct_type(compiler->owner, struct_name);

	while (compiler->parser->current.type != TOKEN_RIGHT_BRACE && compiler->parser->current.type != TOKEN_EOF) {
		// Field name
		if (compiler->parser->current.type != TOKEN_IDENTIFIER)
			break;
		Token field_tok = compiler->parser->current;
		ObjectString *field_name = copy_string(compiler->owner, field_tok.start, field_tok.length);
		pre_advance(compiler);

		ObjectTypeRecord *field_type = NULL;
		if (compiler->parser->current.type == TOKEN_COLON) {
			pre_advance(compiler); // consume ':'
			// parse_type_record uses the global current compiler
			// and parser — this works because we're in a properly
			// initialised pre_compiler context.
			field_type = parse_type_record(compiler);
		} else {
			field_type = new_type_rec(compiler->owner, ANY_TYPE);
		}

		type_table_set(field_types, field_name, field_type);
		table_set(compiler->owner, &struct_obj->fields, field_name, INT_VAL(field_count));
		field_count++;

		// Allow trailing comma between fields.
		if (compiler->parser->current.type == TOKEN_COMMA)
			pre_advance(compiler);
	}

	// Consume '}'.
	if (compiler->parser->current.type == TOKEN_RIGHT_BRACE)
		pre_advance(compiler);

	// Build and register the struct ObjectTypeRecord.
	ObjectTypeRecord *struct_type = new_struct_type_rec(compiler->owner, struct_obj, field_types, field_count);

	type_table_set(compiler->type_table, struct_name, struct_type);
}

// Collect a single top-level function signature into pre_compiler's type_table.
static void pre_collect_function(Compiler *compiler)
{
	if (compiler->parser->current.type != TOKEN_IDENTIFIER)
		return;
	Token fn_name_token = compiler->parser->current;
	pre_advance(compiler);

	if (compiler->parser->current.type != TOKEN_LEFT_PAREN)
		return;
	pre_advance(compiler); // consume '('

	int param_cap = 4;
	int param_count = 0;
	ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_cap);
	if (!param_types)
		return;

	while (compiler->parser->current.type != TOKEN_RIGHT_PAREN && compiler->parser->current.type != TOKEN_EOF) {
		// Parameter name (identifier).
		if (compiler->parser->current.type != TOKEN_IDENTIFIER) {
			FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count);
			return;
		}
		pre_advance(compiler); // consume param name

		ObjectTypeRecord *param_type = NULL;
		if (compiler->parser->current.type == TOKEN_COLON) {
			pre_advance(compiler); // consume ':'
			param_type = parse_type_record(compiler);
		} else {
			param_type = new_type_rec(compiler->owner, ANY_TYPE);
		}

		if (param_count == param_cap) {
			int old_cap = param_cap;
			param_cap = GROW_CAPACITY(param_cap);
			ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap, param_cap);
			if (!grown) {
				FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap);
				return;
			}
			param_types = grown;
		}
		param_types[param_count++] = param_type;

		if (compiler->parser->current.type == TOKEN_COMMA)
			pre_advance(compiler);
	}

	param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_cap, param_count);

	if (compiler->parser->current.type == TOKEN_RIGHT_PAREN)
		pre_advance(compiler);

	ObjectTypeRecord *return_type = NULL;
	if (compiler->parser->current.type == TOKEN_ARROW) {
		pre_advance(compiler); // consume '->'
		return_type = parse_type_record(compiler);
	} else {
		return_type = T_ANY;
	}

	// Register the function ObjectTypeRecord.
	ObjectTypeRecord *fn_type = new_function_type_rec(compiler->owner, param_types, param_count, return_type);

	ObjectString *fn_name = copy_string(compiler->owner, fn_name_token.start, fn_name_token.length);
	type_table_set(compiler->type_table, fn_name, fn_type);

	// Skip the function body so we don't accidentally parse its tokens
	// as top-level declarations.
	pre_skip_block(compiler);
}

// Run a single forward-scan sub-pass over the token stream.
// `collect_structs`: when true, collect struct declarations; when false,
// collect function declarations (fn / pub fn).
static void pre_scan_pass(Compiler *compiler, bool collect_structs)
{
	// Reset parser to a clean state (advance(compiler) will prime the first token
	// after this is called from compile() with a fresh init_scanner).
	compiler->parser->had_error = false;
	compiler->parser->panic_mode = false;

	// Prime the first token.
	pre_advance(compiler);

	while (compiler->parser->current.type != TOKEN_EOF) {
		CruxTokenType t = compiler->parser->current.type;

		if (t == TOKEN_STRUCT) {
			pre_advance(compiler); // consume 'struct'
			if (collect_structs) {
				pre_collect_struct(compiler);
			} else {
				// Skip: name + block
				if (compiler->parser->current.type == TOKEN_IDENTIFIER)
					pre_advance(compiler);
				pre_skip_block(compiler);
			}

		} else if (t == TOKEN_FN) {
			pre_advance(compiler); // consume 'fn'
			if (!collect_structs) {
				pre_collect_function(compiler);
			} else {
				// Skip: name + parens + optional ->T + block
				if (compiler->parser->current.type == TOKEN_IDENTIFIER)
					pre_advance(compiler);
				pre_skip_parens(compiler);
				if (compiler->parser->current.type == TOKEN_ARROW) {
					pre_advance(compiler);
					pre_skip_type(compiler);
				}
				pre_skip_block(compiler);
			}

		} else if (t == TOKEN_PUB) {
			pre_advance(compiler); // consume 'pub'
			// pub fn ... or pub struct ...
			if (compiler->parser->current.type == TOKEN_FN) {
				pre_advance(compiler); // consume 'fn'
				if (!collect_structs) {
					pre_collect_function(compiler);
				} else {
					if (compiler->parser->current.type == TOKEN_IDENTIFIER)
						pre_advance(compiler);
					pre_skip_parens(compiler);
					if (compiler->parser->current.type == TOKEN_ARROW) {
						pre_advance(compiler);
						pre_skip_type(compiler);
					}
					pre_skip_block(compiler);
				}
			} else if (compiler->parser->current.type == TOKEN_STRUCT) {
				pre_advance(compiler); // consume 'struct'
				if (collect_structs) {
					pre_collect_struct(compiler);
				} else {
					if (compiler->parser->current.type == TOKEN_IDENTIFIER)
						pre_advance(compiler);
					pre_skip_block(compiler);
				}
			} else {
				pre_advance(compiler); // skip unknown pub token
			}

		} else {
			// Not a top-level fn/struct — skip the token.
			pre_advance(compiler);
		}
	}
}

// Run both pre-scan sub-passes and merge results into `dest`.
// The scanner must be initialised with init_scanner(source) before calling.
static void pre_scan(Compiler *compiler, char *source, ObjectTypeTable *dest)
{
	// Sub-pass 1: collect struct declarations
	Compiler pre_compiler_structs;
	init_compiler(compiler->owner, &pre_compiler_structs, compiler, TYPE_SCRIPT);
	init_scanner(pre_compiler_structs.parser->scanner, source);

	pre_compiler_structs.parser->had_error = false;
	pre_compiler_structs.parser->panic_mode = false;
	pre_compiler_structs.parser->source = source;

	pre_scan_pass(&pre_compiler_structs, true);

	// CRITICAL: Unlink before the local struct goes out of scope!
	compiler->enclosed = NULL;
	free(pre_compiler_structs.parser->scanner);
	free(pre_compiler_structs.parser);

	// Sub-pass 2: collect function signatures
	Compiler pre_compiler_fns;
	init_compiler(compiler->owner, &pre_compiler_fns, compiler, TYPE_SCRIPT);
	init_scanner(pre_compiler_fns.parser->scanner, source);
	pre_compiler_fns.parser->had_error = false;
	pre_compiler_fns.parser->panic_mode = false;

	type_table_add_all(pre_compiler_structs.type_table, pre_compiler_fns.type_table);

	pre_scan_pass(&pre_compiler_fns, false);

	// CRITICAL: Unlink!
	compiler->enclosed = NULL;
	free(pre_compiler_fns.parser->scanner);
	free(pre_compiler_fns.parser);

	type_table_add_all(pre_compiler_structs.type_table, dest);
	type_table_add_all(pre_compiler_fns.type_table, dest);
}

/**
 * Compile a source string into a function object.
 * Expects the compiler to be initialized.
 */
ObjectFunction *compile(VM *vm, Compiler *compiler, Compiler *enclosing, char *source)
{
	// Pre-scan pass
	if (!init_compiler(vm, compiler, enclosing, TYPE_SCRIPT)) {
		return NULL;
	}
	init_scanner(compiler->parser->scanner, source);

	compiler->parser->had_error = false;
	compiler->parser->panic_mode = false;
	compiler->parser->source = source;

	// Run pre-scan, merging into a temporary staging table.
	ObjectTypeTable *staging = new_type_table(vm, INITIAL_TYPE_TABLE_SIZE);
	pre_scan(compiler, source, staging);

	for (int i = 0; i < staging->capacity; i++) {
		const TypeEntry *entry = &staging->entries[i];
		if (entry->key == NULL)
			continue;
		type_table_set(compiler->type_table, entry->key, entry->value);
	}

	// Main compiler pass
	init_scanner(compiler->parser->scanner, source);
	compiler->parser->had_error = false;
	compiler->parser->panic_mode = false;
	compiler->parser->source = source;

	advance(compiler);

	while (!match(compiler, TOKEN_EOF)) {
		declaration(compiler);
	}

	ObjectFunction *function = end_compiler(compiler);
	if (function != NULL) {
		function->module_record = vm->current_module_record;
	}

	bool had_error = compiler->parser->had_error;

	free(compiler->parser->scanner);
	free(compiler->parser);

	return had_error ? NULL : function;
}

void mark_compiler_roots(VM *vm, Compiler *compiler)
{
	if (compiler == NULL)
		return;
	Compiler *current = compiler;
	while (current != NULL) {
		mark_object(vm, (CruxObject *)current->function);
		mark_object(vm, (CruxObject *)current->return_type);
		mark_object(vm, (CruxObject *)current->last_give_type);
		mark_object_type_table(vm, current->type_table);
		for (int i = 0; i < current->type_stack_count; i++) {
			mark_object(vm, (CruxObject *)current->type_stack[i]);
		}
		for (int i = 0; i < current->local_count; i++) {
			mark_object(vm, (CruxObject *)current->locals[i].type);
		}
		current = (Compiler *)current->enclosed;
	}
}
