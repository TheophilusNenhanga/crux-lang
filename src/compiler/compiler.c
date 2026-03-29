#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "file_handler.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
#include "value.h"

static void expression(Compiler *compiler);

static void parse_precedence(Compiler *compiler, Precedence precedence);

static ParseRule *get_rule(CruxTokenType type);

static void binary(Compiler *compiler, bool can_assign);

static void unary(Compiler *compiler, bool can_assign);

static void grouping(Compiler *compiler, bool can_assign);

static void number(Compiler *compiler, bool can_assign);

static bool parse_signed_int_literal(Compiler *compiler, int32_t *value, const char *message);

static void set_literal(Compiler *compiler, bool can_assign);

static void tuple_literal(Compiler *compiler, bool can_assign);

static bool parse_signed_int_literal(Compiler *compiler, int32_t *value, const char *message)
{
	bool is_negative = false;
	if (match(compiler, CRUX_TOKEN_MINUS)) {
		is_negative = true;
	}

	if (!match(compiler, CRUX_TOKEN_INT)) {
		compiler_panic(compiler->parser, message, SYNTAX);
		return false;
	}

	char *end = NULL;
	long parsed = strtol(compiler->parser->previous.start, &end, 10);
	if (end == compiler->parser->previous.start) {
		compiler_panic(compiler->parser, "Failed to parse integer literal in range.", SYNTAX);
		return false;
	}

	if (is_negative) {
		parsed = -parsed;
	}

	*value = (int32_t)parsed;
	return true;
}

static void statement(Compiler *compiler);

static void declaration(Compiler *compiler);

static ObjectModuleRecord *compile_module_statically(Compiler *compiler, ObjectString *path);

ObjectTypeRecord *parse_type_record(Compiler *compiler)
{
	ObjectTypeRecord *type_record = NULL;

	if (match(compiler, CRUX_TOKEN_INT_TYPE)) {
		type_record = T_INT;
	} else if (match(compiler, CRUX_TOKEN_FLOAT_TYPE)) {
		type_record = T_FLOAT;
	} else if (match(compiler, CRUX_TOKEN_BOOL_TYPE)) {
		type_record = T_BOOL;
	} else if (match(compiler, CRUX_TOKEN_STRING_TYPE)) {
		type_record = T_STRING;
	} else if (match(compiler, CRUX_TOKEN_NIL_TYPE)) {
		type_record = T_NIL;
	} else if (match(compiler, CRUX_TOKEN_ANY_TYPE)) {
		type_record = T_ANY;
	} else if (match(compiler, CRUX_TOKEN_SHAPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_BRACE)) {
			int field_count = 0;
			ObjectTypeTable *field_types = new_type_table(compiler->owner, 8);
			push(compiler->owner->current_module_record, OBJECT_VAL(field_types));
			if (!check(compiler, CRUX_TOKEN_RIGHT_BRACE)) {
				do {
					consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected field name.");
					ObjectString *fieldName = copy_string(compiler->owner, compiler->parser->previous.start,
														  compiler->parser->previous.length);
					push(compiler->owner->current_module_record, OBJECT_VAL(fieldName));
					consume(compiler, CRUX_TOKEN_COLON, "Expected ':' after field name.");
					ObjectTypeRecord *field_type = parse_type_record(compiler);
					push(compiler->owner->current_module_record, OBJECT_VAL(field_type));
					type_table_set(field_types, fieldName, field_type);
					field_count++;
				} while (match(compiler, CRUX_TOKEN_COMMA));
			}
			consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after shape definition.");
			type_record = new_shape_type_rec(compiler->owner, field_types, field_count);

			for (int i = 0; i < field_count; i++) {
				pop(compiler->owner->current_module_record); // field type
				pop(compiler->owner->current_module_record); // field name
			}
			pop(compiler->owner->current_module_record); // field_types

		} else {
			compiler_panic(compiler->parser, "Expected '{' for shape type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, CRUX_TOKEN_ARRAY_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, ARRAY_TYPE);
			push(compiler->owner->current_module_record, OBJECT_VAL(type_record));
			ObjectTypeRecord *parsed_type = parse_type_record(compiler);
			type_record->as.array_type.element_type = parsed_type;
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after array element type.");
			pop(compiler->owner->current_module_record); // type_record
		} else {
			compiler_panic(compiler->parser, "Expected '[' for array type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, CRUX_TOKEN_TABLE_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, TABLE_TYPE);
			push(compiler->owner->current_module_record, OBJECT_VAL(type_record));
			ObjectTypeRecord *key_type = parse_type_record(compiler);
			push(compiler->owner->current_module_record, OBJECT_VAL(key_type));

			if (!is_valid_table_key_type(key_type)) {
				pop(compiler->owner->current_module_record); // key_type
				pop(compiler->owner->current_module_record); // type_record
				char got[128];
				type_record_name(key_type, got, sizeof(got));
				compiler_panicf(
					compiler->parser, TYPE,
					"Table key type '%s' is not hashable. Keys must be 'Int | Float | String | Bool | Nil'.", got);
				return T_ANY;
			}

			type_record->as.table_type.key_type = key_type;
			consume(compiler, CRUX_TOKEN_COMMA, "Expected ',' after key type.");
			type_record->as.table_type.value_type = parse_type_record(compiler);
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after table element type.");
			pop(compiler->owner->current_module_record); // key_type
			pop(compiler->owner->current_module_record); // type_record
		} else {
			compiler_panic(compiler->parser, "Expected '[' for table type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, CRUX_TOKEN_VECTOR_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, VECTOR_TYPE);
			push(compiler->owner->current_module_record, OBJECT_VAL(type_record));

			if (match(compiler, CRUX_TOKEN_RIGHT_SQUARE)) {
				pop(compiler->owner->current_module_record); // type_record
				type_record->as.vector_type.dimensions = -1;
				return type_record;
			}

			consume(compiler, CRUX_TOKEN_INT, "Expected 'int' for vector dimensions.");
			const int dimensions = (int)strtol(compiler->parser->previous.start, NULL, 10);
			type_record->as.vector_type.dimensions = dimensions;
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after vector element type.");
			pop(compiler->owner->current_module_record); // type_record
		} else {
			compiler_panic(compiler->parser, "Expected '[' for vector type definition.", TYPE);
			type_record = new_type_rec(compiler->owner, ANY_TYPE);
		}
	} else if (match(compiler, CRUX_TOKEN_MATRIX_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			type_record = new_type_rec(compiler->owner, MATRIX_TYPE);
			push(compiler->owner->current_module_record, OBJECT_VAL(type_record));

			if (match(compiler, CRUX_TOKEN_COMMA)) {
				if (match(compiler, CRUX_TOKEN_RIGHT_SQUARE)) {
					type_record->as.matrix_type.cols = -1;
					type_record->as.matrix_type.rows = -1;
					pop(compiler->owner->current_module_record); // type_record
					return type_record;
				} else {
					pop(compiler->owner->current_module_record); // type_record
					compiler_panic(compiler->parser, "Expected ']' to end definition of generic matrix type", SYNTAX);
					return T_ANY;
				}
			}

			consume(compiler, CRUX_TOKEN_INT, "Expected 'int' for matrix dimensions.");
			const int row_dim = (int)strtol(compiler->parser->previous.start, NULL, 10);
			consume(compiler, CRUX_TOKEN_COMMA, "Expected ',' after row dimension.");
			const int col_dim = (int)strtol(compiler->parser->previous.start, NULL, 10);
			type_record->as.matrix_type.rows = row_dim;
			type_record->as.matrix_type.cols = col_dim;
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after matrix element type.");
			pop(compiler->owner->current_module_record); // type_record
		} else {
			compiler_panic(compiler->parser, "Expected '[' for matrix type definition.", TYPE);
			type_record = T_ANY;
		}
	} else if (match(compiler, CRUX_TOKEN_BUFFER_TYPE)) {
		type_record = new_type_rec(compiler->owner, BUFFER_TYPE);
	} else if (match(compiler, CRUX_TOKEN_ERROR_TYPE)) {
		type_record = new_type_rec(compiler->owner, ERROR_TYPE);
	} else if (match(compiler, CRUX_TOKEN_RESULT_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			ObjectTypeRecord *value_type = parse_type_record(compiler);
			push(compiler->owner->current_module_record, OBJECT_VAL(value_type));
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after result value type.");
			type_record = new_result_type_rec(compiler->owner, value_type);
			pop(compiler->owner->current_module_record); // value_type
		} else {
			compiler_panic(compiler->parser, "Expected '[' for result type definition.", TYPE);
			type_record = T_ANY;
		}
	} else if (match(compiler, CRUX_TOKEN_RANGE_TYPE)) {
		type_record = new_type_rec(compiler->owner, RANGE_TYPE);
	} else if (match(compiler, CRUX_TOKEN_TUPLE_TYPE)) {
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			int param_capacity = 4;
			int param_count = 0;
			ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
			if (!param_types) {
				compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
				return T_ANY;
			}

			if (!check(compiler, CRUX_TOKEN_RIGHT_SQUARE)) {
				do {
					if (param_count == param_capacity) {
						int old_capacity = param_capacity;
						param_capacity = GROW_CAPACITY(param_capacity);
						ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types,
															  old_capacity, param_capacity);
						if (!grown) {
							FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_capacity);
							for (int i = 0; i < param_count; i++)
								pop(compiler->owner->current_module_record);
							compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
							return T_ANY;
						}
						param_types = grown;
					}
					ObjectTypeRecord *param_type = parse_type_record(compiler);
					push(compiler->owner->current_module_record, OBJECT_VAL(param_type));
					param_types[param_count++] = param_type;
				} while (match(compiler, CRUX_TOKEN_COMMA));
			}
			param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);
			type_record = new_tuple_type_rec(compiler->owner, param_types, param_count);
			for (int i = 0; i < param_count; i++) {
				pop(compiler->owner->current_module_record); // param_types[i]
			}
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after tuple element types.");
		} else {
			compiler_panic(compiler->parser, "Expected '[' for tuple type definition.", TYPE);
			type_record = T_ANY;
		}
	} else if (match(compiler, CRUX_TOKEN_COMPLEX_TYPE)) {
		type_record = new_type_rec(compiler->owner, COMPLEX_TYPE);
	} else if (match(compiler, CRUX_TOKEN_LEFT_PAREN)) {
		int param_capacity = 4;
		int param_count = 0;
		ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
		if (!param_types) {
			compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
			return T_ANY;
		}

		if (!check(compiler, CRUX_TOKEN_RIGHT_PAREN)) {
			do {
				ObjectTypeRecord *inner = parse_type_record(compiler);
				if (!inner) {
					compiler_panic(compiler->parser, "Expected type.", TYPE);
					inner = T_ANY;
				}

				// protect inner before array grows
				push(compiler->owner->current_module_record, OBJECT_VAL(inner));

				if (param_count == param_capacity) {
					param_capacity = GROW_CAPACITY(param_capacity);
					ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count,
														  param_capacity);
					if (!grown) {
						FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count);
						// +1: account for inner
						for (int i = 0; i < param_count + 1; i++)
							pop(compiler->owner->current_module_record);
						compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
						return T_ANY;
					}
					param_types = grown;
				}
				param_types[param_count++] = inner;
			} while (match(compiler, CRUX_TOKEN_COMMA));
		}
		consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' to end function argument types.");

		consume(compiler, CRUX_TOKEN_ARROW, "Expected '->' to separate function argument types from return type.");
		ObjectTypeRecord *return_type = parse_type_record(compiler);
		if (!return_type) {
			for (int i = 0; i < param_count; i++) {
				pop(compiler->owner->current_module_record); // param_types[i]
			}
			compiler_panic(compiler->parser, "Expected type.", TYPE);
			FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity);
			return T_ANY;
		}
		push(compiler->owner->current_module_record, OBJECT_VAL(return_type));

		if (param_count < param_capacity) {
			param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);
		}

		type_record = new_type_rec(compiler->owner, FUNCTION_TYPE);
		type_record->as.function_type.arg_types = param_types;
		type_record->as.function_type.arg_count = param_count;
		type_record->as.function_type.return_type = return_type;

		pop(compiler->owner->current_module_record); // return_type
		for (int i = 0; i < param_count; i++) {
			pop(compiler->owner->current_module_record); // param_types[i]
		}

	} else if (match(compiler, CRUX_TOKEN_SET_TYPE)) {
		type_record = new_type_rec(compiler->owner, SET_TYPE);
		push(compiler->owner->current_module_record, OBJECT_VAL(type_record));
		if (match(compiler, CRUX_TOKEN_LEFT_SQUARE)) {
			ObjectTypeRecord *element_type = parse_type_record(compiler);
			pop(compiler->owner->current_module_record); // type_record
			type_record->as.set_type.element_type = element_type;
			consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after set element type.");
		} else {
			pop(compiler->owner->current_module_record); // type_record
			type_record->as.set_type.element_type = T_ANY;
		}
	} else if (match(compiler, CRUX_TOKEN_RANDOM_TYPE)) {
		type_record = new_type_rec(compiler->owner, RANDOM_TYPE);
	} else if (match(compiler, CRUX_TOKEN_FILE_TYPE)) {
		type_record = new_type_rec(compiler->owner, FILE_TYPE);
	} else if (check(compiler, CRUX_TOKEN_IDENTIFIER)) {
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
			compiler_panicf(compiler->parser, TYPE, "Unknown type: %s", name_str->chars);
			type_record = T_ANY;
		} else {
			type_record = found;
		}
	} else {
		compiler_panic(compiler->parser, "Expected type.", TYPE);
		type_record = T_ANY;
	}

	if (type_record && match(compiler, CRUX_TOKEN_PIPE)) {
		push(compiler->owner->current_module_record, OBJECT_VAL(type_record));
		int capacity = 4;
		int count = 1;
		ObjectTypeRecord **variants = ALLOCATE(compiler->owner, ObjectTypeRecord *, capacity);

		if (!variants) {
			compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
			pop(compiler->owner->current_module_record);
			return type_record;
		}
		variants[0] = type_record;

		do {
			if (count == capacity) {
				capacity = GROW_CAPACITY(capacity);
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, variants, count, capacity);
				if (!grown) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, variants, count);
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					for (int i = 0; i < count; i++)
						pop(compiler->owner->current_module_record);
					return type_record;
				}
				variants = grown;
			}
			ObjectTypeRecord *variant = parse_type_record(compiler);
			push(compiler->owner->current_module_record, OBJECT_VAL(variant));
			variants[count++] = variant;
		} while (match(compiler, CRUX_TOKEN_PIPE));

		if (count < capacity) {
			variants = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, variants, capacity, count);
		}

		type_record = new_union_type_rec(compiler->owner, variants, NULL, count);

		for (int i = 0; i < count; i++) {
			pop(compiler->owner->current_module_record);
		}
	}

	return type_record;
}

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

	memset(compiler->locals, 0, sizeof(compiler->locals));
	memset(compiler->upvalues, 0, sizeof(compiler->upvalues));
	memset(compiler->type_stack, 0, sizeof(compiler->type_stack));
	memset(compiler->loop_stack, 0, sizeof(compiler->loop_stack));

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
	compiler->last_give_type = NULL;
	compiler->type_table = NULL;
	compiler->type_table = new_type_table(vm, INITIAL_TYPE_TABLE_SIZE);

	// add core fn types for top-level scripts
	if (enclosing == NULL) {
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
	local->type = T_ANY;

	if (type == TYPE_METHOD) {
		local->name.start = "self";
		local->name.length = 4;
	}

	compiler->current_narrowing.tracked_local_index = -1;
	compiler->current_narrowing.tracked_global_name = NULL;
	compiler->current_narrowing.tracked_is_typeof = false;
	compiler->current_narrowing.tracked_literal_type = NULL;
	compiler->current_narrowing.local_index = -1;
	compiler->current_narrowing.global_name = NULL;
	compiler->current_narrowing.narrowed_to = NULL;
	compiler->current_narrowing.stripped_down = NULL;

	return true;
}

/**
 * Parses a named variable (local, upvalue, or global).
 * pushes the type of the variable onto the type stack.
 *
 * @param name The token representing the variable name.
 * @param can_assign Whether the variable expression can be the target of an
 * assignment.
 */
static void named_variable(Compiler *compiler, Token name, const bool can_assign)
{
	ObjectString *name_str = copy_string(compiler->owner, name.start, name.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(name_str));

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
		const Compiler *comp = compiler;
		while (comp != NULL) {
			if (type_table_get(comp->type_table, name_str, &var_type))
				break;
			comp = comp->enclosing;
		}

		if (!var_type) {
			compiler_panicf(compiler->parser, TYPE,
							"Undeclared variable '%.*s'. Did you forget to declare or import it?", name.length,
							name.start);
			var_type = T_ANY; // This allocates!
		}
	}
	push(compiler->owner->current_module_record, OBJECT_VAL(var_type));

	if (can_assign) {
		if (match(compiler, CRUX_TOKEN_EQUAL)) {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (var_type && value_type && var_type->base_type != ANY_TYPE && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(var_type, value_type)) {
					char exp[128], got[128];
					type_record_name(var_type, exp, sizeof(exp));
					type_record_name(value_type, got, sizeof(got));
					compiler_panicf(compiler->parser, TYPE, "Cannot assign '%s' to variable of type '%s'.", got, exp);
				}
			}
			emit_words(compiler, setOp, arg);
			push_type_record(compiler, T_NIL);

			pop(compiler->owner->current_module_record); // var_type
			pop(compiler->owner->current_module_record); // name_str
			return;
		}

		const int op = match_compound_op(compiler);
		if (op != -1) {
			expression(compiler);
			ObjectTypeRecord *rhs_type = pop_type_record(compiler);
			check_compound_type_math(compiler, var_type, rhs_type, op);

			emit_words(compiler, get_compound_opcode(compiler, setOp, op), arg);
			push_type_record(compiler, T_NIL);

			pop(compiler->owner->current_module_record); // var_type
			pop(compiler->owner->current_module_record); // name_str
			return;
		}
	}

	if (getOp == OP_GET_LOCAL) {
		compiler->current_narrowing.tracked_local_index = arg;
		compiler->current_narrowing.tracked_global_name = NULL;
	} else if (getOp == OP_GET_GLOBAL) {
		compiler->current_narrowing.tracked_global_name = name_str;
		compiler->current_narrowing.tracked_local_index = -1;
	} else {
		compiler->current_narrowing.tracked_local_index = -1;
		compiler->current_narrowing.tracked_global_name = NULL;
	}

	emit_words(compiler, getOp, arg);
	push_type_record(compiler, var_type);

	pop(compiler->owner->current_module_record); // var_type
	pop(compiler->owner->current_module_record); // name_str
}

static void and_(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;

	const ObjectTypeRecord *left_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(left_type));

	if (left_type && left_type->base_type != BOOL_TYPE && left_type->base_type != ANY_TYPE) {
		char got[128];
		type_record_name(left_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "Left operand of 'and' must be of type 'Bool', got '%s'.", got);
	}

	const NarrowingInfo left_narrowing = compiler->current_narrowing;

	// Reset tracking for the right side
	compiler->current_narrowing.tracked_local_index = -1;
	compiler->current_narrowing.tracked_is_typeof = false;
	compiler->current_narrowing.tracked_literal_type = NULL;
	compiler->current_narrowing.local_index = -1;
	compiler->current_narrowing.narrowed_to = NULL;
	compiler->current_narrowing.stripped_down = NULL;
	compiler->current_narrowing.tracked_global_name = NULL;
	compiler->current_narrowing.global_name = NULL;

	const int endJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	emit_word(compiler, OP_POP);

	// If the left side narrowed a variable, apply it temporarily while compiling the right side
	ObjectTypeRecord *original_type = NULL;
	if (left_narrowing.narrowed_to) {
		if (left_narrowing.local_index != -1) {
			original_type = compiler->locals[left_narrowing.local_index].type;
			compiler->locals[left_narrowing.local_index].type = left_narrowing.narrowed_to;
		} else if (left_narrowing.global_name) {
			type_table_get(compiler->type_table, left_narrowing.global_name, &original_type);
			type_table_set(compiler->type_table, left_narrowing.global_name, left_narrowing.narrowed_to);
		}
	}

	push(compiler->owner->current_module_record, original_type ? OBJECT_VAL(original_type) : NIL_VAL);

	parse_precedence(compiler, PREC_AND);

	pop(compiler->owner->current_module_record); // original_type

	// Restore original type if temporarily narrowed it
	if (left_narrowing.narrowed_to) {
		if (left_narrowing.local_index != -1) {
			compiler->locals[left_narrowing.local_index].type = original_type;
		} else if (left_narrowing.global_name) {
			type_table_set(compiler->type_table, left_narrowing.global_name, original_type);
		}
	}

	const ObjectTypeRecord *right_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(right_type));

	if (right_type && right_type->base_type != BOOL_TYPE && right_type->base_type != ANY_TYPE) {
		char got[128];
		type_record_name(right_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "Right operand of 'and' must be of type 'Bool', got '%s'.", got);
	}

	patch_jump(compiler, endJump);

	push_type_record(compiler, T_BOOL);

	// for 'and' if the right side didn't narrow anything preserve the left side's narrowing
	if (compiler->current_narrowing.local_index == -1 && compiler->current_narrowing.global_name == NULL) {
		compiler->current_narrowing = left_narrowing;
	}

	pop(compiler->owner->current_module_record); // right_type
	pop(compiler->owner->current_module_record); // left_type
}

static void or_(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;

	const ObjectTypeRecord *left_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(left_type));

	if (left_type && left_type->base_type != BOOL_TYPE && left_type->base_type != ANY_TYPE) {
		char got[128];
		type_record_name(left_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "Left operand of 'or' must be of type 'Bool', got '%s'.", got);
	}

	// For 'or' we can't guarantee either side is true in the 'then' block, so discard narrowing.
	compiler->current_narrowing.tracked_local_index = -1;
	compiler->current_narrowing.tracked_is_typeof = false;
	compiler->current_narrowing.tracked_literal_type = NULL;
	compiler->current_narrowing.local_index = -1;
	compiler->current_narrowing.narrowed_to = NULL;
	compiler->current_narrowing.stripped_down = NULL;
	compiler->current_narrowing.tracked_global_name = NULL;
	compiler->current_narrowing.global_name = NULL;

	const int elseJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	const int endJump = emit_jump(compiler, OP_JUMP);
	patch_jump(compiler, elseJump);
	emit_word(compiler, OP_POP);
	parse_precedence(compiler, PREC_OR);

	ObjectTypeRecord *right_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(right_type));

	if (right_type && right_type->base_type != BOOL_TYPE && right_type->base_type != ANY_TYPE) {
		char got[128];
		type_record_name(right_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "Right operand of 'or' must be of type 'Bool', got '%s'.", got);
	}

	patch_jump(compiler, endJump);

	// Discard right side narrowing for 'or'
	compiler->current_narrowing.tracked_local_index = -1;
	compiler->current_narrowing.tracked_is_typeof = false;
	compiler->current_narrowing.tracked_literal_type = NULL;
	compiler->current_narrowing.local_index = -1;
	compiler->current_narrowing.narrowed_to = NULL;
	compiler->current_narrowing.stripped_down = NULL;
	compiler->current_narrowing.tracked_global_name = NULL;
	compiler->current_narrowing.global_name = NULL;

	push_type_record(compiler, T_BOOL);

	pop(compiler->owner->current_module_record); // right_type
	pop(compiler->owner->current_module_record); // left_type
}

/**
 * Returns a compiled function. (top level script is also a function)
 * Caller must root the returned function
 * @param compiler The current compiler
 * @return The compiled function
 */
static ObjectFunction *end_compiler(Compiler *compiler)
{
	emit_return(compiler);
	push_type_record(compiler, compiler->return_type);
	ObjectFunction *function = compiler->function;
#ifdef DEBUG_PRINT_CODE
	if (!compiler->parser->had_error) {
		disassemble_chunk(current_chunk(compiler), function->name != NULL ? function->name->chars : "<script>");
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
	push(compiler->owner->current_module_record, OBJECT_VAL(left_type));
	push(compiler->owner->current_module_record, OBJECT_VAL(right_type));

	ObjectTypeRecord *result_type = NULL;
	// this slot will be used to protect the result
	push(compiler->owner->current_module_record, NIL_VAL);
	Value *result_slot = compiler->owner->current_module_record->stack_top - 1;

	const bool either_any = (left_type && left_type->base_type == ANY_TYPE) ||
							(right_type && right_type->base_type == ANY_TYPE) || !left_type || !right_type;

	switch (operatorType) {
	case CRUX_TOKEN_EQUAL_EQUAL:
	case CRUX_TOKEN_BANG_EQUAL:
		if (operatorType == CRUX_TOKEN_EQUAL_EQUAL) {
			emit_word(compiler, OP_EQUAL);
		} else {
			emit_word(compiler, OP_NOT_EQUAL);
		}

		result_type = T_BOOL;
		*result_slot = OBJECT_VAL(result_type);

		if ((compiler->current_narrowing.tracked_local_index != -1 ||
			 compiler->current_narrowing.tracked_global_name != NULL) &&
			compiler->current_narrowing.tracked_literal_type != NULL) {
			compiler->current_narrowing.local_index = compiler->current_narrowing.tracked_local_index;
			compiler->current_narrowing.global_name = compiler->current_narrowing.tracked_global_name;

			ObjectTypeRecord *var_type = NULL;
			if (compiler->current_narrowing.local_index != -1) {
				var_type = compiler->locals[compiler->current_narrowing.local_index].type;
			} else {
				type_table_get(compiler->type_table, compiler->current_narrowing.global_name, &var_type);
			}

			ObjectTypeRecord *narrow = NULL;
			ObjectTypeRecord *stripped = NULL;

			if (compiler->current_narrowing.tracked_is_typeof ||
				compiler->current_narrowing.tracked_literal_type->base_type == NIL_TYPE) {
				narrow = compiler->current_narrowing.tracked_literal_type;
				stripped = strip_type(compiler->owner, var_type, narrow);
			}

			if (operatorType == CRUX_TOKEN_EQUAL_EQUAL) {
				compiler->current_narrowing.narrowed_to = narrow;
				compiler->current_narrowing.stripped_down = stripped;
			} else {
				compiler->current_narrowing.narrowed_to = stripped;
				compiler->current_narrowing.stripped_down = narrow;
			}
		}
		break;

	case CRUX_TOKEN_GREATER:
	case CRUX_TOKEN_GREATER_EQUAL:
	case CRUX_TOKEN_LESS:
	case CRUX_TOKEN_LESS_EQUAL: {
		if (!either_any) {
			const bool left_num = is_numeric_type(left_type);
			const bool right_num = is_numeric_type(right_type);

			if (!(left_num && right_num)) {
				compiler_panic(compiler->parser, "Comparison operator requires numeric operands.", TYPE);
			}
		}

		switch (operatorType) {
		case CRUX_TOKEN_GREATER:
			emit_word(compiler, OP_GREATER);
			break;
		case CRUX_TOKEN_GREATER_EQUAL:
			emit_word(compiler, OP_GREATER_EQUAL);
			break;
		case CRUX_TOKEN_LESS:
			emit_word(compiler, OP_LESS);
			break;
		case CRUX_TOKEN_LESS_EQUAL:
			emit_word(compiler, OP_LESS_EQUAL);
			break;
		default:
			break;
		}

		result_type = T_BOOL;
		break;
	}

	case CRUX_TOKEN_PLUS: {
		emit_word(compiler, OP_ADD);

		if (either_any) {
			result_type = new_type_rec(compiler->owner, ANY_TYPE);
			break;
		}

		const bool left_str = left_type->base_type == STRING_TYPE;
		const bool right_str = right_type->base_type == STRING_TYPE;

		if (left_str || right_str) {
			if (!left_str || !right_str) {
				compiler_panic(compiler->parser, "Cannot use '+' between String and non-String.", TYPE);
			}
			result_type = T_STRING;
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

		if (left_type->base_type == FLOAT_TYPE || right_type->base_type == FLOAT_TYPE) {
			result_type = T_FLOAT;
		} else {
			result_type = T_INT;
		}
		break;
	}

	case CRUX_TOKEN_MINUS:
	case CRUX_TOKEN_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;

			if (!left_num || !right_num) {
				char left_name[128], right_name[128];
				type_record_name(left_type, left_name, sizeof(left_name));
				type_record_name(right_type, right_name, sizeof(right_name));
				compiler_panicf(compiler->parser, TYPE, "%s requires numeric operands, got '%s' and '%s'.",
								operatorType == CRUX_TOKEN_MINUS ? "'-'" : "'*'", left_name, right_name);
			}
		}

		emit_word(compiler, operatorType == CRUX_TOKEN_MINUS ? OP_SUBTRACT : OP_MULTIPLY);

		if (either_any) {
			result_type = new_type_rec(compiler->owner, ANY_TYPE);
		} else if (left_type->base_type == FLOAT_TYPE || right_type->base_type == FLOAT_TYPE) {
			result_type = T_FLOAT;
		} else {
			result_type = T_INT;
		}
		break;
	}

	case CRUX_TOKEN_SLASH: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;
			if (!left_num || !right_num) {
				char left_name[128], right_name[128];
				type_record_name(left_type, left_name, sizeof(left_name));
				type_record_name(right_type, right_name, sizeof(right_name));
				compiler_panicf(compiler->parser, TYPE, "'/' requires numeric operands, got '%s' and '%s'.", left_name,
								right_name);
			}
		}
		emit_word(compiler, OP_DIVIDE);
		result_type = T_FLOAT;
		break;
	}

	case CRUX_TOKEN_PERCENT:
	case CRUX_TOKEN_BACKSLASH: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE || right_type->base_type != INT_TYPE) {
				char left_name[128], right_name[128];
				type_record_name(left_type, left_name, sizeof(left_name));
				type_record_name(right_type, right_name, sizeof(right_name));
				compiler_panicf(compiler->parser, TYPE, "%s requires Int operands, got '%s' and '%s'.",
								operatorType == CRUX_TOKEN_PERCENT ? "'%'" : "'\\'", left_name, right_name);
			}
		}
		emit_word(compiler, operatorType == CRUX_TOKEN_PERCENT ? OP_MODULUS : OP_INT_DIVIDE);
		result_type = T_INT;
		break;
	}

	case CRUX_TOKEN_RIGHT_SHIFT:
	case CRUX_TOKEN_LEFT_SHIFT:
	case CRUX_TOKEN_AMPERSAND:
	case CRUX_TOKEN_CARET:
	case CRUX_TOKEN_PIPE: {
		if (!either_any) {
			if (left_type->base_type != INT_TYPE || right_type->base_type != INT_TYPE) {
				compiler_panic(compiler->parser, "Bitwise operators require Int operands.", TYPE);
			}
		}

		switch (operatorType) {
		case CRUX_TOKEN_RIGHT_SHIFT:
			emit_word(compiler, OP_RIGHT_SHIFT);
			break;
		case CRUX_TOKEN_LEFT_SHIFT:
			emit_word(compiler, OP_LEFT_SHIFT);
			break;
		case CRUX_TOKEN_AMPERSAND:
			emit_word(compiler, OP_BITWISE_AND);
			break;
		case CRUX_TOKEN_CARET:
			emit_word(compiler, OP_BITWISE_XOR);
			break;
		case CRUX_TOKEN_PIPE:
			emit_word(compiler, OP_BITWISE_OR);
			break;
		default:
			break;
		}

		result_type = T_INT;
		break;
	}

	case CRUX_TOKEN_STAR_STAR: {
		if (!either_any) {
			const bool left_num = left_type->base_type == INT_TYPE || left_type->base_type == FLOAT_TYPE;
			const bool right_num = right_type->base_type == INT_TYPE || right_type->base_type == FLOAT_TYPE;
			if (!left_num || !right_num) {
				char left_name[128], right_name[128];
				type_record_name(left_type, left_name, sizeof(left_name));
				type_record_name(right_type, right_name, sizeof(right_name));
				compiler_panicf(compiler->parser, TYPE, "'**' requires numeric operands, got '%s' and '%s'.", left_name,
								right_name);
			}
		}
		emit_word(compiler, OP_POWER);
		result_type = T_FLOAT;
		break;
	}

	default:
		result_type = T_ANY;
		break;
	}

	// Update the slot
	*result_slot = result_type ? OBJECT_VAL(result_type) : NIL_VAL;
	push_type_record(compiler, result_type);

	pop(compiler->owner->current_module_record); // result_slot
	pop(compiler->owner->current_module_record); // right_type
	pop(compiler->owner->current_module_record); // left_type
}

static void infix_call(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;

	const ObjectTypeRecord *func_type = peek_type_record(compiler);
	uint16_t arg_count = 0;
	ObjectTypeRecord *arg_types[UINT8_COUNT] = {0};

	if (!check(compiler, CRUX_TOKEN_RIGHT_PAREN)) {
		do {
			if (arg_count == UINT16_MAX) {
				// Prevent stack leak if we panic inside this loop
				for (int i = 0; i < arg_count; i++)
					pop(compiler->owner->current_module_record);
				compiler_panic(compiler->parser, "Cannot have more than 65535 arguments.", ARGUMENT_EXTENT);
				return;
			}
			expression(compiler);
			arg_types[arg_count] = pop_type_record(compiler);
			push(compiler->owner->current_module_record, OBJECT_VAL(arg_types[arg_count]));
			arg_count++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
	}
	consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");

	emit_words(compiler, OP_CALL, arg_count);

	// Type-check when the callee is statically known.
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
						compiler_panicf(compiler->parser, TYPE, "Argument %d type mismatch: expected '%s', got '%s'.",
										i + 1, exp_name, got_name);
					}
				}
			}
		}

		ObjectTypeRecord *ret = func_type->as.function_type.return_type;
		pop_type_record(compiler);
		push_type_record(compiler, ret ? ret : T_ANY);
	} else {
		// unknown callee type
		pop_type_record(compiler);
		push_type_record(compiler, T_ANY);
	}
	for (int i = 0; i < arg_count; i++) {
		pop(compiler->owner->current_module_record);
	}
}

static void literal(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	switch (compiler->parser->previous.type) {
	case CRUX_TOKEN_FALSE:
		emit_word(compiler, OP_FALSE);
		push_type_record(compiler, T_BOOL);
		break;
	case CRUX_TOKEN_NIL:
		compiler->current_narrowing.tracked_literal_type = T_NIL;
		emit_word(compiler, OP_NIL);
		push_type_record(compiler, T_NIL);
		break;
	case CRUX_TOKEN_TRUE:
		emit_word(compiler, OP_TRUE);
		push_type_record(compiler, T_BOOL);
		break;
	default:
		return; // unreachable
	}
}

static void dot(Compiler *compiler, const bool can_assign)
{
	consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected property name after '.'.");
	const uint16_t name_constant = identifier_constant(compiler, &compiler->parser->previous);
	const Token method_name_token = compiler->parser->previous;

	if (name_constant >= UINT16_MAX) {
		compiler_panic(compiler->parser, "Too many constants.", SYNTAX);
	}

	ObjectTypeRecord *object_type = peek_type_record(compiler);
	if (!object_type) {
		object_type = T_ANY;
	}
	push(compiler->owner->current_module_record, OBJECT_VAL(object_type));

	// OP_SET_PROPERTY - this only works for structs
	if (can_assign) {
		const ObjectString *field_name = copy_string(compiler->owner, method_name_token.start,
													 method_name_token.length);
		push(compiler->owner->current_module_record, OBJECT_VAL(field_name));
		ObjectTypeRecord *field_type = NULL;

		if (object_type->base_type == STRUCT_TYPE) {
			const ObjectTypeTable *field_types = object_type->as.struct_type.field_types;
			type_table_get(field_types, field_name, &field_type);
		}

		if (match(compiler, CRUX_TOKEN_EQUAL)) {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (object_type->base_type == STRUCT_TYPE && !field_type) {
				compiler_panicf(compiler->parser, NAME, "Struct has no field '%.*s'.", (int)method_name_token.length,
								method_name_token.start);
			}

			if (field_type && field_type->base_type != ANY_TYPE && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(field_type, value_type)) {
					char exp[128], got[128];
					type_record_name(field_type, exp, sizeof(exp));
					type_record_name(value_type, got, sizeof(got));
					compiler_panicf(compiler->parser, TYPE, "Cannot assign '%s' to field of type '%s'.", got, exp);
				}
			}

			emit_words(compiler, OP_SET_PROPERTY, name_constant);
			push_type_record(compiler, T_NIL);
			pop_type_record(compiler);

			pop(compiler->owner->current_module_record); // field_name
			pop(compiler->owner->current_module_record); // object_type
			return;
		}

		// Handle properties with compound op
		const int op = match_compound_op(compiler);
		if (op != -1) {
			if (object_type->base_type == STRUCT_TYPE && !field_type) {
				compiler_panicf(compiler->parser, NAME, "Struct has no field '%.*s'.", (int)method_name_token.length,
								method_name_token.start);
			}

			expression(compiler);
			ObjectTypeRecord *rhs_type = pop_type_record(compiler);
			check_compound_type_math(compiler, field_type ? field_type : T_ANY, rhs_type, op);

			emit_words(compiler, get_compound_opcode(compiler, OP_SET_PROPERTY, op), name_constant);
			pop_type_record(compiler);
			push_type_record(compiler, rhs_type); // assignment leaves the value on the stack

			pop(compiler->owner->current_module_record); // field_name
			pop(compiler->owner->current_module_record); // object_type
			return;
		}
		pop(compiler->owner->current_module_record); // field_name
	}

	// TODO: Add get property

	// OP_INVOKE
	if (match(compiler, CRUX_TOKEN_LEFT_PAREN)) {
		uint16_t arg_count = 0;
		ObjectTypeRecord *arg_types[UINT8_COUNT] = {0};

		if (!check(compiler, CRUX_TOKEN_RIGHT_PAREN)) {
			do {
				if (arg_count >= UINT8_COUNT) {
					for (int i = 0; i < arg_count; i++)
						pop(compiler->owner->current_module_record);
					pop(compiler->owner->current_module_record); // object_type
					compiler_panic(compiler->parser, "Cannot have more than 255 arguments.", ARGUMENT_EXTENT);
					return;
				}

				expression(compiler);
				arg_types[arg_count] = pop_type_record(compiler);
				push(compiler->owner->current_module_record, OBJECT_VAL(arg_types[arg_count]));
				arg_count++;
			} while (match(compiler, CRUX_TOKEN_COMMA));
		}
		consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");

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
				compiler_panicf(compiler->parser, TYPE, "Struct field '%.*s' is not callable.",
								(int)method_name_token.length, method_name_token.start);
			}

		} else if (object_type->base_type != ANY_TYPE) {
			const VM *vm = compiler->owner;
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
					char type_name[128];
					type_record_name(object_type, type_name, sizeof(type_name));
					compiler_panicf(compiler->parser, NAME, "'%s' has no method '%.*s'.", type_name,
									(int)method_name_token.length, method_name_token.start);
				}
			}
		}

		if (method_found && method_arg_types) {
			int param_offset = (object_type->base_type == STRUCT_TYPE) ? 0 : 1;
			int user_params = method_arity - param_offset;
			if (user_params < 0)
				user_params = 0;

			if ((int)arg_count != user_params) {
				compiler_panicf(compiler->parser, ARGUMENT_MISMATCH, "Method '%.*s' expects %d argument(s), got %d.",
								(int)method_name_token.length, method_name_token.start, user_params, (int)arg_count);
			} else {
				for (int i = 0; i < (int)arg_count; i++) {
					ObjectTypeRecord *expected = method_arg_types[i + param_offset];
					ObjectTypeRecord *got_type = arg_types[i];
					if (expected && got_type && expected->base_type != ANY_TYPE && got_type->base_type != ANY_TYPE &&
						!types_compatible(expected, got_type)) {
						char exp_name[128], got_name[128];
						type_record_name(expected, exp_name, sizeof(exp_name));
						type_record_name(got_type, got_name, sizeof(got_name));
						compiler_panicf(compiler->parser, TYPE, "Argument %d type mismatch: expected '%s', got '%s'.",
										i + 1, exp_name, got_name);
					}
				}
			}
		}

		pop_type_record(compiler);
		push_type_record(compiler, method_return ? method_return : T_ANY);

		for (int i = 0; i < (int)arg_count; i++) {
			pop(compiler->owner->current_module_record);
		}
		pop(compiler->owner->current_module_record); // object_type
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
			compiler_panicf(compiler->parser, NAME, "Struct has no field '%.*s'.", (int)method_name_token.length,
							method_name_token.start);
		}
	}

	pop_type_record(compiler);
	push_type_record(compiler, result_type ? result_type : T_ANY);

	pop(compiler->owner->current_module_record); // object_type
}

void struct_instance(Compiler *compiler, const bool can_assign)
{
	consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected struct name to start initialization.");

	named_variable(compiler, compiler->parser->previous, can_assign);

	ObjectTypeRecord *struct_type = peek_type_record(compiler);

	const bool type_known = struct_type && struct_type->base_type != ANY_TYPE;
	if (type_known && struct_type->base_type != STRUCT_TYPE) {
		char got[128];
		type_record_name(struct_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "'new' requires a struct type name, got '%s'.", got);
		pop_type_record(compiler);
		push_type_record(compiler, T_ANY);
		return;
	}

	if (!match(compiler, CRUX_TOKEN_LEFT_BRACE)) {
		compiler_panic(compiler->parser, "Expected '{' to start struct instance.", SYNTAX);
		pop_type_record(compiler);
		push_type_record(compiler, T_ANY);
		return;
	}

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

	if (!match(compiler, CRUX_TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount == UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many fields in struct initializer.", SYNTAX);
				if (field_seen)
					FREE_ARRAY(compiler->owner, bool, field_seen, declared_field_count);
				pop_type_record(compiler);
				push_type_record(compiler, new_type_rec(compiler->owner, ANY_TYPE));
				return;
			}

			consume(compiler, CRUX_TOKEN_IDENTIFIER,
					"Expected field name. Trailing commas after final field are not allowed.");
			ObjectString *fieldName = copy_string(compiler->owner, compiler->parser->previous.start,
												  compiler->parser->previous.length);
			push(compiler->owner->current_module_record, OBJECT_VAL(fieldName));

			consume(compiler, CRUX_TOKEN_EQUAL, "Expected '=' after struct field name.");

			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (type_known) {
				const ObjectTypeTable *field_types = struct_type->as.struct_type.field_types;
				const ObjectStruct *definition = struct_type->as.struct_type.definition;

				// ensure field exists on the struct
				Value field_index_val;
				if (!table_get(&definition->fields, fieldName, &field_index_val)) {
					compiler_panicf(compiler->parser, NAME, "Struct has no field '%.*s'.", (int)fieldName->byte_length,
									fieldName->chars);
				} else {
					// mark field as seen
					const int field_index = AS_INT(field_index_val);
					if (field_index >= 0 && field_index < declared_field_count) {
						if (field_seen[field_index]) {
							compiler_panicf(compiler->parser, NAME, "Field '%.*s' specified more than once.",
											(int)fieldName->byte_length, fieldName->chars);
						}
						field_seen[field_index] = true;
					}

					// Validate the value type against the declared field type.
					ObjectTypeRecord *declared_field_type = NULL;
					type_table_get(field_types, fieldName, &declared_field_type);

					if (declared_field_type && value_type && declared_field_type->base_type != ANY_TYPE &&
						value_type->base_type != ANY_TYPE) {
						if (!types_compatible(declared_field_type, value_type)) {
							char expected[128], got[128];
							type_record_name(declared_field_type, expected, sizeof(expected));
							type_record_name(value_type, got, sizeof(got));
							compiler_panicf(compiler->parser, TYPE, "Field '%.*s' expects type '%s', got '%s'.",
											(int)fieldName->byte_length, fieldName->chars, expected, got);
						}
					}
				}
			}

			const uint16_t fieldNameConstant = make_constant(compiler, OBJECT_VAL(fieldName));
			emit_words(compiler, OP_STRUCT_NAMED_FIELD, fieldNameConstant);

			pop(compiler->owner->current_module_record); // unroot fieldName
			fieldCount++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
	}

	// Check for missing fields — every declared field must be provided.
	if (type_known && field_seen) {
		const ObjectStruct *definition = struct_type->as.struct_type.definition;

		for (int i = 0; i < declared_field_count; i++) {
			if (!field_seen[i]) {
				const char *missing = "<unknown>";
				int missing_len = 0;
				for (int e = 0; e < definition->fields.capacity; e++) {
					const Entry *entry = &definition->fields.entries[e];
					if (entry->key != NULL && AS_INT(entry->value) == i) {
						missing = entry->key->chars;
						missing_len = (int)entry->key->byte_length;
						break;
					}
				}
				compiler_panicf(compiler->parser, NAME, "Missing required field '%.*s' in struct initializer.",
								missing_len, missing);
				break; // Don't report all missing fields at once
			}
		}
		FREE_ARRAY(compiler->owner, bool, field_seen, declared_field_count);
	}

	if (fieldCount != 0) {
		consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after struct field list.");
	}

	emit_word(compiler, OP_STRUCT_INSTANCE_END);

	pop_type_record(compiler);
	push_type_record(compiler, struct_type ? struct_type : T_ANY);
}

/**
 * Parses an expression.
 * Allocates - so protect before
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
	while (!check(compiler, CRUX_TOKEN_RIGHT_BRACE) && !check(compiler, CRUX_TOKEN_EOF)) {
		declaration(compiler);
	}

	consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(Compiler *compiler, const FunctionType type, ObjectTypeRecord *self_type)
{
	Compiler function_compiler = {0};
	if (!init_compiler(compiler->owner, &function_compiler, compiler, type)) {
		fprintf(stderr, "Fatal error: Memory allocation failed. Shutting down!\n");
		exit(EXIT_FAILURE);
	}
	if (type == TYPE_METHOD && self_type) {
		function_compiler.locals[0].type = self_type;
	}
	begin_scope(&function_compiler);

	int param_capacity = 4;
	int param_count = 0;
	ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_capacity);
	if (!param_types) {
		compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
		return;
	}

	consume(compiler, CRUX_TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(&function_compiler, CRUX_TOKEN_RIGHT_PAREN)) {
		do {
			function_compiler.function->arity++;
			if (function_compiler.function->arity > UINT8_MAX) {
				compiler_panic(compiler->parser,
							   "Functions cannot have more than "
							   "255 arguments.",
							   ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(&function_compiler, "Expected parameter name.");

			ObjectTypeRecord *param_type = NULL;
			if (match(&function_compiler, CRUX_TOKEN_COLON)) {
				param_type = parse_type_record(&function_compiler);
			} else {
				param_type = T_ANY;
			}

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
		} while (match(compiler, CRUX_TOKEN_COMMA));
	}

	param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_capacity, param_count);

	consume(&function_compiler, CRUX_TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	ObjectTypeRecord *annotated_return_type = NULL;
	if (match(&function_compiler, CRUX_TOKEN_ARROW)) {
		annotated_return_type = parse_type_record(&function_compiler);
	} else {
		annotated_return_type = T_ANY;
	}
	function_compiler.return_type = annotated_return_type;

	consume(&function_compiler, CRUX_TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block(&function_compiler);

	if (!function_compiler.has_return && annotated_return_type && annotated_return_type->base_type != NIL_TYPE &&
		annotated_return_type->base_type != ANY_TYPE) {
		char expected[128];
		type_record_name(annotated_return_type, expected, sizeof(expected));
		compiler_panicf(compiler->parser, TYPE, "Function expects to return '%s' but has no return statement.",
						expected);
	}

	ObjectFunction *fn = end_compiler(&function_compiler);

	push(compiler->owner->current_module_record, OBJECT_VAL(fn));
	push(compiler->owner->current_module_record, OBJECT_VAL(annotated_return_type));
	for (int i = 0; i < param_count; i++) {
		push(compiler->owner->current_module_record, OBJECT_VAL(param_types[i]));
	}

	emit_words(compiler, OP_CLOSURE, make_constant(compiler, OBJECT_VAL(fn)));

	for (int i = 0; i < fn->upvalue_count; i++) {
		emit_word(compiler, function_compiler.upvalues[i].is_local ? 1 : 0);
		emit_word(compiler, function_compiler.upvalues[i].index);
	}

	ObjectTypeRecord *func_type = new_function_type_rec(compiler->owner, param_types, param_count,
														annotated_return_type);
	push_type_record(compiler, func_type);

	for (int i = 0; i < param_count; i++) {
		pop(compiler->owner->current_module_record); // param_types[i]
	}
	pop(compiler->owner->current_module_record); // annotated_return_type
	pop(compiler->owner->current_module_record); // fn
}

static void fn_declaration(Compiler *compiler, const bool is_public)
{
	const uint16_t global = parse_variable(compiler, "Expected function name.");

	const Token fn_name_token = compiler->parser->previous;
	ObjectString *name_str = copy_string(compiler->owner, fn_name_token.start, fn_name_token.length);

	push(compiler->owner->current_module_record, OBJECT_VAL(name_str));

	const int local_index = (compiler->scope_depth > 0) ? compiler->local_count - 1 : -1;

	mark_initialized(compiler);
	function(compiler, TYPE_FUNCTION, NULL);

	ObjectTypeRecord *fn_type = pop_type_record(compiler);

	push(compiler->owner->current_module_record, OBJECT_VAL(fn_type));

	if (is_public || (compiler->owner->current_module_record && compiler->owner->current_module_record->is_repl)) {
		type_table_set(compiler->owner->current_module_record->types, name_str, fn_type);
	}

	if (fn_type) {
		if (compiler->scope_depth == 0) {
			type_table_set(compiler->type_table, name_str, fn_type);
		} else {
			compiler->locals[local_index].type = fn_type;
		}
	}

	define_variable(compiler, global);

	pop(compiler->owner->current_module_record); // fn_type
	pop(compiler->owner->current_module_record); // name_str
}

static void anonymous_function(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	Compiler function_compiler = {0};
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
	consume(&function_compiler, CRUX_TOKEN_LEFT_PAREN, "Expected '(' to start argument list.");

	if (!check(&function_compiler, CRUX_TOKEN_RIGHT_PAREN)) {
		do {
			function_compiler.function->arity++;
			if (function_compiler.function->arity > UINT8_MAX) {
				compiler_panic(compiler->parser,
							   "Functions cannot have more than "
							   "255 arguments.",
							   ARGUMENT_EXTENT);
			}
			const uint16_t constant = parse_variable(&function_compiler, "Expected parameter name.");

			ObjectTypeRecord *param_type = NULL;
			if (match(&function_compiler, CRUX_TOKEN_COLON)) {
				param_type = parse_type_record(&function_compiler);
			} else {
				param_type = T_ANY;
			}

			// Store on the local slot that parse_variable created
			function_compiler.locals[function_compiler.local_count - 1].type = param_type;

			if (param_count == param_capacity) {
				const int old_cap = param_capacity;
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
		} while (match(compiler, CRUX_TOKEN_COMMA));
	}

	consume(&function_compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");

	ObjectTypeRecord *annotated_return_type = NULL;
	if (match(&function_compiler, CRUX_TOKEN_ARROW)) {
		annotated_return_type = parse_type_record(&function_compiler);
	} else {
		annotated_return_type = T_ANY;
	}

	function_compiler.return_type = annotated_return_type;

	consume(&function_compiler, CRUX_TOKEN_LEFT_BRACE, "Expected '{' before function body.");
	block(&function_compiler);

	if (!function_compiler.has_return && annotated_return_type && annotated_return_type->base_type != NIL_TYPE &&
		annotated_return_type->base_type != ANY_TYPE) {
		char expected[128];
		type_record_name(annotated_return_type, expected, sizeof(expected));
		compiler_panicf(function_compiler.parser, TYPE, "Function expects to return '%s' but has no return statement.",
						expected);
	}

	ObjectFunction *fn = end_compiler(&function_compiler);

	push(compiler->owner->current_module_record, OBJECT_VAL(fn));
	push(compiler->owner->current_module_record, OBJECT_VAL(annotated_return_type));
	for (int i = 0; i < param_count; i++) {
		push(compiler->owner->current_module_record, OBJECT_VAL(param_types[i]));
	}

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

	for (int i = 0; i < param_count; i++) {
		pop(compiler->owner->current_module_record); // param_types[i]
	}
	pop(compiler->owner->current_module_record); // annotated_return_type
	pop(compiler->owner->current_module_record); // fn
}

static void array_literal(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	ObjectTypeRecord *element_type = NULL;

	push(compiler->owner->current_module_record, NIL_VAL);
	const int type_root_stack_index = (int)(compiler->owner->current_module_record->stack_top -
											compiler->owner->current_module_record->stack - 1);

	if (!match(compiler, CRUX_TOKEN_RIGHT_SQUARE)) {
		do {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (!element_type) {
				element_type = value_type;
			} else if (element_type->base_type != ANY_TYPE && value_type && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(element_type, value_type)) {
					if ((element_type->base_type == INT_TYPE && value_type->base_type == FLOAT_TYPE) ||
						(element_type->base_type == FLOAT_TYPE && value_type->base_type == INT_TYPE)) {
						element_type = T_FLOAT;
					} else {
						element_type = T_ANY;
					}
				}
			}

			compiler->owner->current_module_record->stack[type_root_stack_index] = element_type
																					   ? OBJECT_VAL(element_type)
																					   : NIL_VAL;

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many elements in array literal.", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
		consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after array elements.");
	}

	if (!element_type) {
		element_type = T_ANY;
		compiler->owner->current_module_record->stack[type_root_stack_index] = OBJECT_VAL(element_type);
	}

	emit_word(compiler, OP_ARRAY);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *array_type = new_array_type_rec(compiler->owner, element_type);
	push_type_record(compiler, array_type);

	pop(compiler->owner->current_module_record); // element_type
}

static void set_literal(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	ObjectTypeRecord *element_type = NULL;

	push(compiler->owner->current_module_record, NIL_VAL);
	const int type_root_stack_index = (int)(compiler->owner->current_module_record->stack_top -
											compiler->owner->current_module_record->stack - 1);

	if (!match(compiler, CRUX_TOKEN_RIGHT_BRACE)) {
		do {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);

			if (value_type && !is_valid_table_key_type(value_type)) {
				char got[128];
				type_record_name(value_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE, "Set elements must be hashable, got '%s'.", got);
			}

			if (!element_type) {
				element_type = value_type;
			} else if (element_type->base_type != ANY_TYPE && value_type && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(element_type, value_type)) {
					if ((element_type->base_type == INT_TYPE && value_type->base_type == FLOAT_TYPE) ||
						(element_type->base_type == FLOAT_TYPE && value_type->base_type == INT_TYPE)) {
						element_type = T_FLOAT;
					} else {
						element_type = T_ANY;
					}
				}
			}

			compiler->owner->current_module_record->stack[type_root_stack_index] = element_type
																					   ? OBJECT_VAL(element_type)
																					   : NIL_VAL;

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many elements in set literal.", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
		consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after set elements.");
	}

	if (!element_type) {
		element_type = T_ANY;
		compiler->owner->current_module_record->stack[type_root_stack_index] = OBJECT_VAL(element_type);
	}

	emit_word(compiler, OP_SET);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *set_type = new_set_type_rec(compiler->owner, element_type);
	push_type_record(compiler, set_type);

	pop(compiler->owner->current_module_record); // element_type
}

static void tuple_literal(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	int element_capacity = 4;
	ObjectTypeRecord **element_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, element_capacity);

	if (element_types == NULL) {
		compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
		push_type_record(compiler, T_ANY);
		return;
	}

	if (!match(compiler, CRUX_TOKEN_RIGHT_SQUARE)) {
		do {
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);
			push(compiler->owner->current_module_record, value_type ? OBJECT_VAL(value_type) : NIL_VAL);

			if (elementCount == element_capacity) {
				const int old_capacity = element_capacity;
				element_capacity = GROW_CAPACITY(element_capacity);
				ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, element_types, old_capacity,
													  element_capacity);
				if (grown == NULL) {
					FREE_ARRAY(compiler->owner, ObjectTypeRecord *, element_types, old_capacity);
					for (uint16_t i = 0; i < elementCount; i++) {
						pop(compiler->owner->current_module_record);
					}
					compiler_panic(compiler->parser, "Memory allocation failed.", MEMORY);
					push_type_record(compiler, T_ANY);
					return;
				}
				element_types = grown;
			}

			element_types[elementCount++] = value_type;

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many elements in tuple literal.", COLLECTION_EXTENT);
			}
		} while (match(compiler, CRUX_TOKEN_COMMA));
		consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after tuple elements.");
	}

	if (elementCount < element_capacity) {
		ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, element_types, element_capacity,
											  elementCount);
		if (grown != NULL || elementCount == 0) {
			element_types = grown;
		}
	}

	emit_word(compiler, OP_TUPLE);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *tuple_type = new_tuple_type_rec(compiler->owner, element_types, elementCount);
	push_type_record(compiler, tuple_type);

	for (uint16_t i = 0; i < elementCount; i++) {
		pop(compiler->owner->current_module_record);
	}
}

static void table_literal(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	uint16_t elementCount = 0;
	ObjectTypeRecord *table_key_type = NULL;
	ObjectTypeRecord *table_value_type = NULL;

	push(compiler->owner->current_module_record, NIL_VAL); // key
	push(compiler->owner->current_module_record, NIL_VAL); // val
	const int val_idx = (int)(compiler->owner->current_module_record->stack_top -
							  compiler->owner->current_module_record->stack - 1);
	const int key_idx = val_idx - 1;

	if (!match(compiler, CRUX_TOKEN_RIGHT_BRACE)) {
		do {
			expression(compiler);
			ObjectTypeRecord *key_type = pop_type_record(compiler);
			consume(compiler, CRUX_TOKEN_COLON, "Expected ':' after table key.");

			push(compiler->owner->current_module_record, OBJECT_VAL(key_type));
			expression(compiler);
			ObjectTypeRecord *value_type = pop_type_record(compiler);
			pop(compiler->owner->current_module_record);

			if (!table_key_type) {
				table_key_type = key_type;
			} else if (table_key_type->base_type != ANY_TYPE && key_type && key_type->base_type != ANY_TYPE) {
				if (!types_equal(table_key_type, key_type)) {
					char expected[128], got[128];
					type_record_name(table_key_type, expected, sizeof(expected));
					type_record_name(key_type, got, sizeof(got));
					compiler_panicf(compiler->parser, TYPE,
									"Inconsistent key types in table literal: expected '%s', got '%s'.", expected, got);
				}
			}

			if (!table_value_type) {
				table_value_type = value_type;
			} else if (table_value_type->base_type != ANY_TYPE && value_type && value_type->base_type != ANY_TYPE) {
				if (!types_compatible(table_value_type, value_type)) {
					if ((table_value_type->base_type == INT_TYPE && value_type->base_type == FLOAT_TYPE) ||
						(table_value_type->base_type == FLOAT_TYPE && value_type->base_type == INT_TYPE)) {
						table_value_type = T_FLOAT;
					} else {
						table_value_type = T_ANY;
					}
				}
			}

			// update roots
			compiler->owner->current_module_record->stack[key_idx] = table_key_type ? OBJECT_VAL(table_key_type)
																					: NIL_VAL;
			compiler->owner->current_module_record->stack[val_idx] = table_value_type ? OBJECT_VAL(table_value_type)
																					  : NIL_VAL;

			if (elementCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many elements in table literal.", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
		consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after table elements.");
	}

	if (!table_key_type)
		table_key_type = T_ANY;
	if (!table_value_type)
		table_value_type = T_ANY;
	compiler->owner->current_module_record->stack[key_idx] = OBJECT_VAL(table_key_type);
	compiler->owner->current_module_record->stack[val_idx] = OBJECT_VAL(table_value_type);

	emit_word(compiler, OP_TABLE);
	emit_word(compiler, elementCount);

	ObjectTypeRecord *table_type = new_table_type_rec(compiler->owner, table_key_type, table_value_type);
	push_type_record(compiler, table_type);

	pop(compiler->owner->current_module_record); // val
	pop(compiler->owner->current_module_record); // key
}

static void collection_index(Compiler *compiler, const bool can_assign)
{
	ObjectTypeRecord *collection_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(collection_type));

	expression(compiler);
	ObjectTypeRecord *index_type = pop_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(index_type));

	if (collection_type && index_type && index_type->base_type != ANY_TYPE && collection_type->base_type != ANY_TYPE) {
		if (collection_type->base_type == ARRAY_TYPE || collection_type->base_type == STRING_TYPE ||
			collection_type->base_type == TUPLE_TYPE || collection_type->base_type == BUFFER_TYPE) {
			if (index_type->base_type != INT_TYPE && index_type->base_type != RANGE_TYPE) {
				char got[128];
				type_record_name(index_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE, "Collection index must be of type 'Int' | 'Range', got '%s'.",
								got);
			}
		} else if (collection_type->base_type == TABLE_TYPE) {
			ObjectTypeRecord *key_type = collection_type->as.table_type.key_type;
			if (key_type && key_type->base_type != ANY_TYPE && !types_compatible(key_type, index_type)) {
				char expected[128], got[128];
				type_record_name(key_type, expected, sizeof(expected));
				type_record_name(index_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE, "Table key type mismatch: expected '%s', got '%s'.", expected,
								got);
			}
		}
	}

	bool is_slice = index_type && index_type->base_type == RANGE_TYPE;

	consume(compiler, CRUX_TOKEN_RIGHT_SQUARE, "Expected ']' after index.");

	if (can_assign && match(compiler, CRUX_TOKEN_EQUAL)) {
		if (is_slice) {
			compiler_panicf(compiler->parser, TYPE, "Cannot assign to a range slice of a collection.");
		}

		expression(compiler);
		ObjectTypeRecord *value_type = pop_type_record(compiler);

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
				char expected[128], got[128];
				type_record_name(expected_value, expected, sizeof(expected));
				type_record_name(value_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE, "Cannot assign '%s' to collection of element type '%s'.", got,
								expected);
			}
		}

		emit_word(compiler, OP_SET_COLLECTION);
		push_type_record(compiler, T_NIL);
	} else {
		if (is_slice) {
			emit_word(compiler, OP_GET_SLICE);
			ObjectTypeRecord *result_type = T_ANY;
			if (collection_type) {
				switch (collection_type->base_type) {
				case TABLE_TYPE: {
					compiler_panicf(compiler->parser, TYPE, "Cannot index a table with a range slice.");
					break;
				}
				case ARRAY_TYPE: {
					result_type = new_array_type_rec(compiler->owner, collection_type->as.array_type.element_type);
					break;
				}
				case STRING_TYPE: {
					result_type = T_STRING;
					break;
				}
				case TUPLE_TYPE: {
					result_type = new_tuple_type_rec(compiler->owner, NULL, -1);
					break;
				}
				case BUFFER_TYPE: {
					ObjectTypeRecord *element_type = T_INT;
					push(compiler->owner->current_module_record, OBJECT_VAL(element_type));
					result_type = new_array_type_rec(compiler->owner, element_type);
					pop(compiler->owner->current_module_record);
					break;
				}
				default: {
					break;
				}
				}
			}
			push_type_record(compiler, result_type);
		} else {
			emit_word(compiler, OP_GET_COLLECTION);
			ObjectTypeRecord *result_type = NULL;
			if (collection_type) {
				if (collection_type->base_type == ARRAY_TYPE) {
					result_type = collection_type->as.array_type.element_type;
				} else if (collection_type->base_type == TABLE_TYPE) {
					result_type = collection_type->as.table_type.value_type;
				} else if (collection_type->base_type == TUPLE_TYPE) {
					result_type = T_ANY; // cannot determine element type
				} else if (collection_type->base_type == STRING_TYPE) {
					result_type = T_STRING;
				} else if (collection_type->base_type == BUFFER_TYPE) {
					result_type = T_INT;
				}
			}
			push_type_record(compiler, result_type ? result_type : T_ANY);
		}
	}

	pop(compiler->owner->current_module_record); // index_type
	pop(compiler->owner->current_module_record); // collection_type
}

static void var_declaration(Compiler *compiler, const bool is_public)
{
	const uint16_t global = parse_variable(compiler, "Expected Variable Name.");
	const Token var_name = compiler->parser->previous;
	ObjectString *name_str = copy_string(compiler->owner, var_name.start, var_name.length);

	push(compiler->owner->current_module_record, OBJECT_VAL(name_str));

	ObjectTypeRecord *annotated_type = NULL;
	if (match(compiler, CRUX_TOKEN_COLON)) {
		annotated_type = parse_type_record(compiler);
	}

	push(compiler->owner->current_module_record, annotated_type ? OBJECT_VAL(annotated_type) : NIL_VAL);

	ObjectTypeRecord *value_type = NULL;
	if (match(compiler, CRUX_TOKEN_EQUAL)) {
		expression(compiler);
		value_type = pop_type_record(compiler);

		if (annotated_type && value_type && annotated_type->base_type != ANY_TYPE &&
			value_type->base_type != ANY_TYPE && !types_compatible(annotated_type, value_type)) {
			char expected[128], got[128];
			type_record_name(annotated_type, expected, sizeof(expected));
			type_record_name(value_type, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "Type mismatch: expected '%s', got '%s'.", expected, got);
		}
	} else {
		if (annotated_type && annotated_type->base_type != NIL_TYPE && annotated_type->base_type != ANY_TYPE) {
			compiler_panic(compiler->parser, "Variable with non-Nil type must be initialized.", TYPE);
		}
		emit_word(compiler, OP_NIL);
		value_type = T_NIL;
	}

	ObjectTypeRecord *resolved_type = annotated_type ? annotated_type : value_type;
	if (!resolved_type)
		resolved_type = T_ANY;

	if (is_public || (compiler->owner->current_module_record && compiler->owner->current_module_record->is_repl)) {
		type_table_set(compiler->owner->current_module_record->types, name_str, resolved_type);
	}

	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

	if (compiler->scope_depth > 0) {
		compiler->locals[compiler->local_count - 1].type = resolved_type;
	} else {
		type_table_set(compiler->type_table, name_str, resolved_type);
	}
	define_variable(compiler, global);

	pop(compiler->owner->current_module_record); // annotated_type
	pop(compiler->owner->current_module_record); // name_str
}

static void expression_statement(Compiler *compiler)
{
	expression(compiler);
	pop_type_record(compiler); // discard — value is unused
	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after expression.");
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
		char got[128];
		type_record_name(condition_type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "'while' condition must be of type 'Bool', got '%s'.", got);
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

	if (match(compiler, CRUX_TOKEN_SEMICOLON)) {
		// no initializer
	} else if (match(compiler, CRUX_TOKEN_LET)) {
		var_declaration(compiler, false);
	} else {
		expression_statement(compiler);
	}

	int loopStart = current_chunk(compiler)->count;
	int exitJump = -1;

	if (!match(compiler, CRUX_TOKEN_SEMICOLON)) {
		expression(compiler);

		// Condition must be Bool
		const ObjectTypeRecord *condition_type = pop_type_record(compiler);
		if (condition_type && condition_type->base_type != BOOL_TYPE && condition_type->base_type != ANY_TYPE) {
			char got[128];
			type_record_name(condition_type, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "'for' condition must be of type 'Bool', got '%s'.", got);
		}

		consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after loop condition");
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
	compiler->current_narrowing.tracked_local_index = -1;
	compiler->current_narrowing.tracked_global_name = NULL;
	compiler->current_narrowing.tracked_is_typeof = false;
	compiler->current_narrowing.tracked_literal_type = NULL;
	compiler->current_narrowing.local_index = -1;
	compiler->current_narrowing.global_name = NULL;
	compiler->current_narrowing.narrowed_to = NULL;
	compiler->current_narrowing.stripped_down = NULL;

	expression(compiler);

	const NarrowingInfo narrow_state = compiler->current_narrowing;
	ObjectTypeRecord *original_type = NULL;

	// Condition must be Bool
	const ObjectTypeRecord *condition_type = pop_type_record(compiler);
	if (condition_type && condition_type->base_type != BOOL_TYPE && condition_type->base_type != ANY_TYPE) {
		compiler_panic(compiler->parser, "'if' condition must be of type 'Bool'.", TYPE);
	}

	const int thenJump = emit_jump(compiler, OP_JUMP_IF_FALSE);
	emit_word(compiler, OP_POP);

	// then block narrowing
	if (narrow_state.narrowed_to) {
		if (narrow_state.local_index != -1) {
			original_type = compiler->locals[narrow_state.local_index].type;
			compiler->locals[narrow_state.local_index].type = narrow_state.narrowed_to;
		} else if (narrow_state.global_name) {
			type_table_get(compiler->type_table, narrow_state.global_name, &original_type);
			type_table_set(compiler->type_table, narrow_state.global_name, narrow_state.narrowed_to);
		}
	}

	push(compiler->owner->current_module_record, original_type ? OBJECT_VAL(original_type) : NIL_VAL);

	statement(compiler);

	const int elseJump = emit_jump(compiler, OP_JUMP);
	patch_jump(compiler, thenJump);
	emit_word(compiler, OP_POP);

	// apply stripped down type for else
	if (narrow_state.stripped_down) {
		if (narrow_state.local_index != -1) {
			compiler->locals[narrow_state.local_index].type = narrow_state.stripped_down;
		} else if (narrow_state.global_name) {
			type_table_set(compiler->type_table, narrow_state.global_name, narrow_state.stripped_down);
		}
	} else {
		if (narrow_state.local_index != -1) {
			compiler->locals[narrow_state.local_index].type = original_type;
		} else if (narrow_state.global_name) {
			type_table_set(compiler->type_table, narrow_state.global_name, original_type);
		}
	}

	if (match(compiler, CRUX_TOKEN_ELSE)) {
		statement(compiler);
	}

	patch_jump(compiler, elseJump);

	pop(compiler->owner->current_module_record); // original_type

	// restore original type
	if (narrow_state.local_index != -1) {
		compiler->locals[narrow_state.local_index].type = original_type;
	} else if (narrow_state.global_name) {
		type_table_set(compiler->type_table, narrow_state.global_name, original_type);
	}
}

static void return_statement(Compiler *compiler)
{
	if (compiler->type == TYPE_SCRIPT) {
		compiler_panic(compiler->parser, "Cannot use <return> outside of a function.", SYNTAX);
	}

	compiler->has_return = true;

	if (match(compiler, CRUX_TOKEN_SEMICOLON)) {
		// check that the function expects Nil
		if (compiler->return_type && compiler->return_type->base_type != NIL_TYPE &&
			compiler->return_type->base_type != ANY_TYPE) {
			compiler_panic(compiler->parser, "Non-Nil return type requires a return value.", TYPE);
		}
		emit_return(compiler);
	} else {
		expression(compiler);
		consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after return value.");

		ObjectTypeRecord *value_type = pop_type_record(compiler);

		// Only validate if both sides are known
		if (value_type && compiler->return_type && compiler->return_type->base_type != ANY_TYPE &&
			value_type->base_type != ANY_TYPE) {
			if (!types_compatible(compiler->return_type, value_type)) {
				char expected[128];
				char got[128];
				type_record_name(compiler->return_type, expected, sizeof(expected));
				type_record_name(value_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE, "Return type mismatch: expected '%s', got '%s'.", expected,
								got);
			}
		}
		emit_word(compiler, OP_RETURN);
	}
	compiler->last_give_type = new_type_rec(compiler->owner, NEVER_TYPE);
}

static void import_statement(Compiler *compiler, bool is_dynamic)
{
	bool hasParen = false;
	if (compiler->parser->current.type == CRUX_TOKEN_LEFT_PAREN) {
		consume(compiler, CRUX_TOKEN_LEFT_PAREN, "Expected '(' after use statement.");
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

		if (compiler->parser->current.type == CRUX_TOKEN_AS) {
			consume(compiler, CRUX_TOKEN_AS, "Expected 'as' keyword.");
			consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected alias name after 'as'.");
			aliasTokens[nameCount] = compiler->parser->previous;
			aliases[nameCount] = identifier_constant(compiler, &compiler->parser->previous);
			aliasPresence[nameCount] = true;
		}

		nameCount++;
	} while (match(compiler, CRUX_TOKEN_COMMA));

	if (hasParen) {
		consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after last imported name.");
	}

	consume(compiler, CRUX_TOKEN_FROM, "Expected 'from' after import statement.");
	consume(compiler, CRUX_TOKEN_STRING, "Expected string literal for module name.");

	const bool isNative = memcmp(compiler->parser->previous.start, "\"crux:", 6) == 0;

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
		ObjectString *raw_path_str = copy_string(compiler->owner, compiler->parser->previous.start + 1,
												 compiler->parser->previous.length - 2);

		push(compiler->owner->current_module_record, OBJECT_VAL(raw_path_str)); // raw_path_str

		const char *base_path = NULL;
		if (compiler->owner->current_module_record && compiler->owner->current_module_record->path) {
			base_path = compiler->owner->current_module_record->path->chars;
		} else {
			base_path = ".";
		}

		char *resolved_chars = resolve_path(base_path, raw_path_str->chars);
		if (resolved_chars == NULL) {
			compiler_panicf(compiler->parser, IMPORT, "Failed to resolve import path: '%s'", raw_path_str->chars);
			pop(compiler->owner->current_module_record);
			return;
		}

		ObjectString *path_str = copy_string(compiler->owner, resolved_chars, strlen(resolved_chars));
		free(resolved_chars);

		// FIXED: Swap roots
		pop(compiler->owner->current_module_record); // raw_path_str
		push(compiler->owner->current_module_record, OBJECT_VAL(path_str)); // path_str

		module_const = make_constant(compiler, OBJECT_VAL(path_str));

		if (!is_dynamic) {
			statically_imported_mod = compile_module_statically(compiler, path_str);
			if (!statically_imported_mod || statically_imported_mod->state == STATE_ERROR) {
				compiler_panicf(compiler->parser, IMPORT, "Failed to compile module '%s'.", path_str->chars);
				pop(compiler->owner->current_module_record); // path_str
				return;
			}
		}

		emit_words(compiler, OP_USE_MODULE, module_const);
		emit_words(compiler, OP_FINISH_USE, nameCount);

		pop(compiler->owner->current_module_record); // path_str
	}

	// vm will bind names/aliases at runtime
	for (uint16_t i = 0; i < nameCount; i++)
		emit_word(compiler, names[i]);
	for (uint16_t i = 0; i < nameCount; i++)
		emit_word(compiler, aliasPresence[i] ? aliases[i] : names[i]);

	if (isNative) {
		emit_word(compiler, module_const);
	}

	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after import statement.");

	// type resolution
	for (uint16_t i = 0; i < nameCount; i++) {
		const Token visible = aliasPresence[i] ? aliasTokens[i] : nameTokens[i];
		ObjectString *name_str = copy_string(compiler->owner, visible.start, visible.length);

		push(compiler->owner->current_module_record, OBJECT_VAL(name_str)); // name_str

		ObjectTypeRecord *resolved_type = NULL;

		if (compiler->scope_depth == 0) {
			if (isNative) {
				// Native Stdlib Module
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
				// Static User Module
				if (!type_table_get(statically_imported_mod->types, name_str, &resolved_type)) {
					compiler_panicf(compiler->parser, NAME, "Module does not export '%s'", name_str->chars);
					resolved_type = T_ANY; // Fallback to prevent crashes during panic
				}
			}
		}

		// Dynamic fallback
		if (!resolved_type) {
			resolved_type = T_ANY;
		}

		push(compiler->owner->current_module_record, OBJECT_VAL(resolved_type)); // resolved_type

		if (compiler->scope_depth == 0 && compiler->owner->current_module_record &&
			compiler->owner->current_module_record->is_repl) {
			type_table_set(compiler->owner->current_module_record->types, name_str, resolved_type);
		}

		type_table_set(compiler->type_table, name_str, resolved_type);

		pop(compiler->owner->current_module_record); // resolved_type
		pop(compiler->owner->current_module_record); // name_str
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
	consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected struct name.");
	const Token structName = compiler->parser->previous;

	ObjectString *struct_name_str = copy_string(compiler->owner, structName.start, structName.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(struct_name_str));

	const uint16_t nameConstant = identifier_constant(compiler, &structName);
	ObjectStruct *structObject = new_struct_type(compiler->owner, struct_name_str);
	push(compiler->owner->current_module_record, OBJECT_VAL(structObject));

	declare_variable(compiler);

	const int local_index = (compiler->scope_depth > 0) ? compiler->local_count - 1 : -1;

	const uint16_t structConstant = make_constant(compiler, OBJECT_VAL(structObject));
	emit_words(compiler, OP_STRUCT, structConstant);
	define_variable(compiler, nameConstant);

	consume(compiler, CRUX_TOKEN_LEFT_BRACE, "Expected '{' before struct body.");

	ObjectTypeTable *field_types = new_type_table(compiler->owner, INITIAL_TYPE_TABLE_SIZE);
	push(compiler->owner->current_module_record, OBJECT_VAL(field_types));
	int fieldCount = 0;

	if (!match(compiler, CRUX_TOKEN_RIGHT_BRACE)) {
		do {
			if (fieldCount >= UINT16_MAX) {
				compiler_panic(compiler->parser, "Too many fields in struct.", SYNTAX);
				break;
			}

			consume(compiler, CRUX_TOKEN_IDENTIFIER,
					"Expected field name. Trailing comma after last field is not allowed.");
			ObjectString *fieldName = copy_string(compiler->owner, compiler->parser->previous.start,
												  compiler->parser->previous.length);
			push(compiler->owner->current_module_record, OBJECT_VAL(fieldName));

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
			if (match(compiler, CRUX_TOKEN_COLON)) {
				field_type = parse_type_record(compiler);
			} else {
				field_type = T_ANY;
			}
			push(compiler->owner->current_module_record, OBJECT_VAL(field_type));

			type_table_set(field_types, fieldName, field_type);

			table_set(compiler->owner, &structObject->fields, fieldName, INT_VAL(fieldCount));
			fieldCount++;
		} while (match(compiler, CRUX_TOKEN_COMMA));
	}

	if (fieldCount != 0) {
		consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after struct body.");
	}

	ObjectTypeRecord *struct_type = new_struct_type_rec(compiler->owner, structObject, field_types, fieldCount);
	push(compiler->owner->current_module_record, OBJECT_VAL(struct_type));

	// type registration
	if (compiler->scope_depth == 0) {
		type_table_set(compiler->type_table, struct_name_str, struct_type);
	} else {
		compiler->locals[local_index].type = struct_type;
	}
	if (is_public || (compiler->owner->current_module_record && compiler->owner->current_module_record->is_repl)) {
		type_table_set(compiler->owner->current_module_record->types, struct_name_str, struct_type);
	}

	pop(compiler->owner->current_module_record); // struct_type
	for (int i = 0; i < fieldCount; i++) {
		pop(compiler->owner->current_module_record); // field_type
		pop(compiler->owner->current_module_record); // fieldName
	}
	pop(compiler->owner->current_module_record); // field_types
	pop(compiler->owner->current_module_record); // structObject
	pop(compiler->owner->current_module_record); // struct_name_str
}

static void impl_declaration(Compiler *compiler)
{
	consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected struct name after 'impl'.");
	const Token struct_name_token = compiler->parser->previous;
	const ObjectString *struct_name_str = copy_string(compiler->owner, struct_name_token.start,
													  struct_name_token.length);

	ObjectTypeRecord *struct_type = NULL;
	if (!type_table_get(compiler->type_table, struct_name_str, &struct_type) || struct_type->base_type != STRUCT_TYPE) {
		if (struct_type) {
			char got[128];
			type_record_name(struct_type, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "Cannot implement methods for a non-struct type, got '%s'.", got);
		} else {
			compiler_panicf(compiler->parser, TYPE, "Cannot implement methods for a non-struct type '%s'.",
							struct_name_str->chars);
		}
		return;
	}

	named_variable(compiler, struct_name_token, false);
	pop_type_record(compiler);

	consume(compiler, CRUX_TOKEN_LEFT_BRACE, "Expected '{' before impl body.");

	while (!check(compiler, CRUX_TOKEN_RIGHT_BRACE) && !check(compiler, CRUX_TOKEN_EOF)) {
		consume(compiler, CRUX_TOKEN_FN, "Expected 'fn' inside impl block.");
		consume(compiler, CRUX_TOKEN_IDENTIFIER, "Expected method name.");

		const Token method_name_tok = compiler->parser->previous;
		ObjectString *method_name_str = copy_string(compiler->owner, method_name_tok.start, method_name_tok.length);
		push(compiler->owner->current_module_record, OBJECT_VAL(method_name_str));
		const uint16_t method_name_const = make_constant(compiler, OBJECT_VAL(method_name_str));

		// slot 0 is preserved for self
		function(compiler, TYPE_METHOD, struct_type);

		ObjectTypeRecord *method_type = pop_type_record(compiler);
		push(compiler->owner->current_module_record, OBJECT_VAL(method_type));
		type_table_set(struct_type->as.struct_type.field_types, method_name_str, method_type);

		emit_words(compiler, OP_METHOD, method_name_const);
		pop(compiler->owner->current_module_record); // method_type
		pop(compiler->owner->current_module_record); // method_name_str
	}

	consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after impl body.");

	emit_word(compiler, OP_POP);
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
		char got[128];
		type_record_name(type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "'?' operator requires a 'Result' type, got '%s'.", got);
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
static void synchronize(const Compiler *compiler)
{
	compiler->parser->panic_mode = false;

	while (compiler->parser->current.type != CRUX_TOKEN_EOF) {
		if (compiler->parser->previous.type == CRUX_TOKEN_SEMICOLON)
			return;
		switch (compiler->parser->current.type) {
		case CRUX_TOKEN_STRUCT:
		case CRUX_TOKEN_PUB:
		case CRUX_TOKEN_FN:
		case CRUX_TOKEN_LET:
		case CRUX_TOKEN_FOR:
		case CRUX_TOKEN_IF:
		case CRUX_TOKEN_WHILE:
		case CRUX_TOKEN_RETURN:
		case CRUX_TOKEN_PANIC:
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

	if (match(compiler, CRUX_TOKEN_SEMICOLON)) {
		emit_word(compiler, OP_NIL);
		compiler->last_give_type = T_NIL;
	} else {
		expression(compiler);
		// Record for match_expression to check arm consistency.
		compiler->last_give_type = pop_type_record(compiler);
		consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after give statement.");
	}

	emit_word(compiler, OP_GIVE);
}

static void match_expression(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	begin_match_scope(compiler);

	expression(compiler);

	// The target's type tells us what patterns are valid.
	ObjectTypeRecord *target_type = pop_type_record(compiler);
	if (!target_type)
		target_type = T_ANY;

	push(compiler->owner->current_module_record, OBJECT_VAL(target_type));

	consume(compiler, CRUX_TOKEN_LEFT_BRACE, "Expected '{' after match target.");

	int *endJumps = ALLOCATE(compiler->owner, int, 8);
	int jumpCount = 0;
	int jumpCapacity = 8;

	emit_word(compiler, OP_MATCH);

	bool hasDefault = false;
	bool hasOkPattern = false;
	bool hasErrPattern = false;

	// type that the arm produces
	ObjectTypeRecord *arm_type = NULL;

	push(compiler->owner->current_module_record, NIL_VAL);
	const int arm_idx = (int)(compiler->owner->current_module_record->stack_top -
							  compiler->owner->current_module_record->stack - 1);

	while (!check(compiler, CRUX_TOKEN_RIGHT_BRACE) && !check(compiler, CRUX_TOKEN_EOF)) {
		int jumpIfNotMatch = -1;
		uint16_t bindingSlot = UINT16_MAX;
		bool hasBinding = false;
		compiler->last_give_type = NULL;

		if (match(compiler, CRUX_TOKEN_DEFAULT)) {
			if (hasDefault) {
				compiler_panic(compiler->parser, "Cannot have multiple default patterns.", SYNTAX);
			}
			hasDefault = true;

		} else if (match(compiler, CRUX_TOKEN_OK)) {
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

			if (match(compiler, CRUX_TOKEN_LEFT_PAREN)) {
				begin_scope(compiler);
				hasBinding = true;
				consume(compiler, CRUX_TOKEN_IDENTIFIER,
						"Expected identifier after 'Ok' "
						"pattern.");
				declare_variable(compiler);
				bindingSlot = compiler->local_count - 1;

				ObjectTypeRecord *ok_type = (target_type->base_type == RESULT_TYPE)
												? target_type->as.result_type.ok_type
												: NULL;
				compiler->locals[bindingSlot].type = ok_type ? ok_type : new_type_rec(compiler->owner, ANY_TYPE);

				mark_initialized(compiler);
				consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}

		} else if (match(compiler, CRUX_TOKEN_ERR)) {
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

			if (match(compiler, CRUX_TOKEN_LEFT_PAREN)) {
				begin_scope(compiler);
				hasBinding = true;
				consume(compiler, CRUX_TOKEN_IDENTIFIER,
						"Expected identifier after 'Err' "
						"pattern.");
				declare_variable(compiler);
				bindingSlot = compiler->local_count - 1;

				compiler->locals[bindingSlot].type = new_type_rec(compiler->owner, ERROR_TYPE);

				mark_initialized(compiler);
				consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}

		} else {
			expression(compiler);
			ObjectTypeRecord *pattern_type = pop_type_record(compiler);

			if (pattern_type && target_type && pattern_type->base_type != ANY_TYPE &&
				target_type->base_type != ANY_TYPE) {
				if (!types_compatible(target_type, pattern_type)) {
					char expected[128], got[128];
					type_record_name(target_type, expected, sizeof(expected));
					type_record_name(pattern_type, got, sizeof(got));
					compiler_panicf(compiler->parser, TYPE,
									"Pattern type '%s' is not compatible with match target type '%s'.", got, expected);
				}
			}

			jumpIfNotMatch = emit_jump(compiler, OP_MATCH_JUMP);
		}

		consume(compiler, CRUX_TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

		if (bindingSlot != UINT16_MAX) {
			emit_words(compiler, OP_RESULT_BIND, bindingSlot);
		}

		ObjectTypeRecord *this_arm_type = NULL;

		if (match(compiler, CRUX_TOKEN_LEFT_BRACE)) {
			block(compiler);
			// Blocks produce values with give and statements (usually the never type e.g. return, break)
			this_arm_type = compiler->last_give_type ? compiler->last_give_type : T_NIL;
		} else if (match(compiler, CRUX_TOKEN_GIVE)) {
			if (match(compiler, CRUX_TOKEN_SEMICOLON)) {
				emit_word(compiler, OP_NIL);
				this_arm_type = T_NIL;
			} else {
				expression(compiler);
				this_arm_type = pop_type_record(compiler);
				consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after give expression.");
			}
			emit_word(compiler, OP_GIVE);
		} else {
			expression(compiler);
			this_arm_type = pop_type_record(compiler);
			consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after expression.");
		}

		// Check arm type consistency.
		if (!this_arm_type)
			this_arm_type = new_type_rec(compiler->owner, ANY_TYPE);

		if (arm_type == NULL || arm_type->base_type == NEVER_TYPE) {
			arm_type = this_arm_type;
		} else if (this_arm_type->base_type == NEVER_TYPE) {
			// This arm produces never, so it doesn't affect the overall type.
		} else if (arm_type->base_type != ANY_TYPE && this_arm_type->base_type != ANY_TYPE) {
			if (!types_compatible(arm_type, this_arm_type)) {
				char expected[128], got[128];
				type_record_name(arm_type, expected, sizeof(expected));
				type_record_name(this_arm_type, got, sizeof(got));
				compiler_panicf(compiler->parser, TYPE,
								"Match arms produce inconsistent "
								"types: expected '%s', got '%s'.",
								expected, got);
			}
		}

		compiler->owner->current_module_record->stack[arm_idx] = arm_type ? OBJECT_VAL(arm_type) : NIL_VAL;

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
						   "Result 'match' must have both 'Ok' and 'Err' patterns, or a default case.", SYNTAX);
		}
	} else if (!hasDefault) {
		compiler_panic(compiler->parser, "'match' expression must have a 'default' case.", SYNTAX);
	}

	for (int i = 0; i < jumpCount; i++) {
		patch_jump(compiler, endJumps[i]);
	}

	emit_word(compiler, OP_MATCH_END);

	FREE_ARRAY(compiler->owner, int, endJumps, jumpCapacity);
	consume(compiler, CRUX_TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
	end_match_scope(compiler);

	push_type_record(compiler, arm_type ? arm_type : T_ANY);

	pop(compiler->owner->current_module_record); // arm_type
	pop(compiler->owner->current_module_record); // target_type
}

static void continue_statement(Compiler *compiler)
{
	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after 'continue',");
	const int continueTarget = get_current_continue_target(compiler);
	if (continueTarget == -1) {
		return;
	}
	const LoopContext *loopContext = &compiler->loop_stack[compiler->loop_depth - 1];
	emit_cleanup_for_jump(compiler, loopContext->scope_depth);
	emit_loop(compiler, continueTarget);
	compiler->last_give_type = new_type_rec(compiler->owner, NEVER_TYPE);
}

static void break_statement(Compiler *compiler)
{
	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after 'break'.");
	if (compiler->loop_depth <= 0) {
		compiler_panic(compiler->parser, "Cannot use 'break' outside of a loop.", SYNTAX);
		return;
	}
	const LoopContext *loopContext = &compiler->loop_stack[compiler->loop_depth - 1];
	emit_cleanup_for_jump(compiler, loopContext->scope_depth);
	add_break_jump(compiler, emit_jump(compiler, OP_JUMP));
	compiler->last_give_type = new_type_rec(compiler->owner, NEVER_TYPE);
}

static void panic_statement(Compiler *compiler)
{
	expression(compiler);
	const ObjectTypeRecord *type = pop_type_record(compiler);

	if (type && type->base_type != STRING_TYPE && type->base_type != ANY_TYPE) {
		char got[128];
		type_record_name(type, got, sizeof(got));
		compiler_panicf(compiler->parser, TYPE, "'panic' requires a 'String', got '%s'.", got);
	}
	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after 'panic'.");
	emit_word(compiler, OP_PANIC);

	compiler->last_give_type = new_type_rec(compiler->owner, NEVER_TYPE);
}

static void type_declaration(Compiler *compiler, bool is_public)
{
	const uint16_t global = parse_variable(compiler, "Expected type name.");

	const Token type_name_token = compiler->parser->previous;
	ObjectString *type_name_str = copy_string(compiler->owner, type_name_token.start, type_name_token.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(type_name_str));
	consume(compiler, CRUX_TOKEN_EQUAL, "Expected '=' after type name.");

	ObjectTypeRecord *aliased_type = parse_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(aliased_type));
	consume(compiler, CRUX_TOKEN_SEMICOLON, "Expected ';' after type declaration.");

	if (compiler->scope_depth == 0) {
		type_table_set(compiler->type_table, type_name_str, aliased_type);
	} else {
		compiler->locals[compiler->local_count - 1].type = aliased_type;
	}

	emit_constant(compiler, OBJECT_VAL(aliased_type));
	define_variable(compiler, global);

	if (is_public || (compiler->owner->current_module_record && compiler->owner->current_module_record->is_repl)) {
		type_table_set(compiler->owner->current_module_record->types, type_name_str, aliased_type);
	}
	pop(compiler->owner->current_module_record);
	pop(compiler->owner->current_module_record);
}

static void public_declaration(Compiler *compiler)
{
	if (compiler->scope_depth > 0) {
		compiler_panic(compiler->parser, "Cannot declare public members in a local scope.", SYNTAX);
	}
	emit_word(compiler, OP_PUB);
	if (match(compiler, CRUX_TOKEN_FN)) {
		fn_declaration(compiler, true);
	} else if (match(compiler, CRUX_TOKEN_LET)) {
		var_declaration(compiler, true);
	} else if (match(compiler, CRUX_TOKEN_STRUCT)) {
		struct_declaration(compiler, true);
	} else if (match(compiler, CRUX_TOKEN_TYPE)) {
		type_declaration(compiler, true);
	} else {
		compiler_panic(compiler->parser, "Expected 'fn', 'let', 'struct', or 'type' after 'pub'.", SYNTAX);
	}
}

static void declaration(Compiler *compiler)
{
	if (match(compiler, CRUX_TOKEN_LET)) {
		var_declaration(compiler, false);
	} else if (match(compiler, CRUX_TOKEN_FN)) {
		fn_declaration(compiler, false);
	} else if (match(compiler, CRUX_TOKEN_STRUCT)) {
		struct_declaration(compiler, false);
	} else if (match(compiler, CRUX_TOKEN_TYPE)) {
		type_declaration(compiler, false);
	} else if (match(compiler, CRUX_TOKEN_PUB)) {
		public_declaration(compiler);
	} else if (match(compiler, CRUX_TOKEN_IMPL)) {
		impl_declaration(compiler);
	} else {
		statement(compiler);
	}

	if (compiler->parser->panic_mode)
		synchronize(compiler);
}

static void statement(Compiler *compiler)
{
	if (match(compiler, CRUX_TOKEN_IF)) {
		if_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_LEFT_BRACE)) {
		begin_scope(compiler);
		block(compiler);
		end_scope(compiler);
	} else if (match(compiler, CRUX_TOKEN_WHILE)) {
		while_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_FOR)) {
		for_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_RETURN)) {
		return_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_DYN_USE)) {
		dynuse_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_USE)) {
		use_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_GIVE)) {
		give_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_BREAK)) {
		break_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_CONTINUE)) {
		continue_statement(compiler);
	} else if (match(compiler, CRUX_TOKEN_PANIC)) {
		panic_statement(compiler);
	} else {
		expression_statement(compiler);
	}
}

static void grouping(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	expression(compiler);
	consume(compiler, CRUX_TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void binary_number(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	char *end = NULL;
	// +2 to skip the "0b" prefix
	long n = strtol(compiler->parser->previous.start + 2, &end, 2);
	if (end == compiler->parser->previous.start) {
		compiler_panic(compiler->parser, "Failed to parse binary literal.", SYNTAX);
		push_type_record(compiler, T_ANY);
		return;
	}
	emit_constant(compiler, INT_VAL((int32_t)n));
	push_type_record(compiler, T_INT);
}

static void hex_number(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	char *end = NULL;
	// +2 to skip the "0x" prefix
	long n = strtol(compiler->parser->previous.start + 2, &end, 16);
	if (end == compiler->parser->previous.start) {
		compiler_panic(compiler->parser, "Failed to parse hex literal.", SYNTAX);
		push_type_record(compiler, T_ANY);
		return;
	}
	emit_constant(compiler, INT_VAL((int32_t)n));
	push_type_record(compiler, T_INT);
}

static void number(Compiler *compiler, bool can_assign)
{
	(void)can_assign;
	if (check(compiler, CRUX_TOKEN_DOT_DOT)) {
		if (compiler->parser->previous.type != CRUX_TOKEN_INT) {
			compiler_panic(compiler->parser, "Range literals require an Int start value.", TYPE);
			push_type_record(compiler, T_ANY);
			return;
		}

		char *start_end = NULL;
		long start_long = strtol(compiler->parser->previous.start, &start_end, 10);
		if (start_end == compiler->parser->previous.start) {
			compiler_panic(compiler->parser, "Failed to parse range start.", SYNTAX);
			push_type_record(compiler, T_ANY);
			return;
		}

		const int32_t start = (int32_t)start_long;
		int32_t step = 1;
		int32_t end_value = 0;

		advance(compiler); // consume '..'
		if (!parse_signed_int_literal(compiler, &end_value, "Expected Int literal after '..' in range literal.")) {
			push_type_record(compiler, T_ANY);
			return;
		}

		if (match(compiler, CRUX_TOKEN_DOT_DOT)) {
			step = end_value;
			if (!parse_signed_int_literal(compiler, &end_value,
										  "Expected Int literal after second '..' in range literal.")) {
				push_type_record(compiler, T_ANY);
				return;
			}
		}

		if (step == 0) {
			compiler_panic(compiler->parser, "Range literal step cannot be zero.", VALUE);
			push_type_record(compiler, T_ANY);
			return;
		}
		if (step > 0 && start > end_value) {
			compiler_panic(compiler->parser, "Range literal start cannot be greater than end when step is positive.",
						   VALUE);
			push_type_record(compiler, T_ANY);
			return;
		}
		if (step < 0 && start < end_value) {
			compiler_panic(compiler->parser, "Range literal start cannot be less than end when step is negative.",
						   VALUE);
			push_type_record(compiler, T_ANY);
			return;
		}

		emit_constant(compiler, INT_VAL(start));
		emit_constant(compiler, INT_VAL(step));
		emit_constant(compiler, INT_VAL(end_value));
		emit_word(compiler, OP_RANGE);
		push_type_record(compiler, new_type_rec(compiler->owner, RANGE_TYPE));
		return;
	}

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
		push_type_record(compiler, T_FLOAT);
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
		push_type_record(compiler, T_FLOAT);
	} else {
		const int32_t integer = (int32_t)number;
		if ((double)integer == number) {
			emit_constant(compiler, INT_VAL(integer));
			push_type_record(compiler, T_INT);
		} else {
			emit_constant(compiler, FLOAT_VAL(number));
			push_type_record(compiler, T_FLOAT);
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

static void string(Compiler *compiler, const bool can_assign)
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
		push_type_record(compiler, T_STRING);
		emit_constant(compiler, OBJECT_VAL(string));
		FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
		return;
	}

	for (int i = 0; i < srcLength; i++) {
		if (src[i] == '\\') {
			if (i + 1 >= srcLength) {
				compiler_panic(compiler->parser, "Unterminated escape sequence at end of string", SYNTAX);
				FREE_ARRAY(compiler->owner, char, processed, compiler->parser->previous.length);
				return;
			}

			bool error;
			const char escaped = process_escape_sequence(src[i + 1], &error);
			if (error) {
				compiler_panicf(compiler->parser, SYNTAX, "Unexpected escape sequence '\\%c'", src[i + 1]);
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

	compiler->current_narrowing.tracked_literal_type = type_from_string(compiler->owner, compiler->type_table,
																		string->chars);

	push_type_record(compiler, T_STRING);
	emit_constant(compiler, OBJECT_VAL(string));
}

static void unary(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	const CruxTokenType operatorType = compiler->parser->previous.type;

	// compile the operand
	parse_precedence(compiler, PREC_UNARY);

	switch (operatorType) {
	case CRUX_TOKEN_NOT: {
		// check if this is a boolean type
		ObjectTypeRecord *bool_expected = pop_type_record(compiler);
		if (!bool_expected || (bool_expected->base_type != ANY_TYPE && bool_expected->base_type != BOOL_TYPE)) {
			char got[128];
			type_record_name(bool_expected, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "Expected 'Bool' type for 'not' operator, got '%s'.", got);
		}
		push_type_record(compiler, bool_expected);
		emit_word(compiler, OP_NOT);
		break;
	}
	case CRUX_TOKEN_MINUS: {
		// check if this is a negatable type
		ObjectTypeRecord *num_expected = pop_type_record(compiler);
		if (!num_expected ||
			(num_expected->base_type != ANY_TYPE && !(num_expected->base_type & (INT_TYPE | FLOAT_TYPE)))) {
			char got[128];
			type_record_name(num_expected, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "Expected 'Int | Float' type for '-' operator, got '%s'.", got);
		}
		push_type_record(compiler, num_expected);
		emit_word(compiler, OP_NEGATE);
		break;
	}
	case CRUX_TOKEN_TILDE: {
		ObjectTypeRecord *int_expected = pop_type_record(compiler);
		if (!int_expected || (int_expected->base_type != ANY_TYPE && int_expected->base_type != INT_TYPE)) {
			compiler_panicf(compiler->parser, TYPE, "Expected 'Int' type for '~' operator.");
		}
		push_type_record(compiler, int_expected);
		emit_word(compiler, OP_BITWISE_NOT);
		break;
	}
	default:
		return; // unreachable
	}
}

static void typeof_expression(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	parse_precedence(compiler, PREC_UNARY);
	compiler->current_narrowing.tracked_is_typeof = true;
	emit_word(compiler, OP_TYPEOF); // emits string representation of the type at runtime
	push_type_record(compiler, T_STRING);
}

static void type_coerce(Compiler *compiler, const bool can_assign)
{
	(void)can_assign;
	ObjectTypeRecord *got_type = pop_type_record(compiler);
	ObjectTypeRecord *type_record = parse_type_record(compiler);

	if (got_type && type_record && got_type->base_type != ANY_TYPE && type_record->base_type != ANY_TYPE) {
		if (!types_compatible(type_record, got_type) && !types_compatible(got_type, type_record)) {
			char expected[128], got[128];
			type_record_name(type_record, expected, sizeof(expected));
			type_record_name(got_type, got, sizeof(got));
			compiler_panicf(compiler->parser, TYPE, "Invalid cast: cannot cast '%s' to '%s'.", got, expected);
		}
	}

	push_type_record(compiler, type_record);
	const uint16_t type_const = make_constant(compiler, OBJECT_VAL(type_record));
	emit_words(compiler, OP_TYPE_COERCE, type_const);
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

	const FileResult result = read_file(path->chars);
	if (result.error) {
		compiler_panicf(compiler->parser, IMPORT, "Could not read file '%s': %s", path->chars, result.error);
		return NULL;
	}
	char *source = result.content;

	ObjectModuleRecord *new_module = new_object_module_record(compiler->owner, path, false, false);
	table_set(compiler->owner, &compiler->owner->module_cache, path, OBJECT_VAL(new_module));

	ObjectModuleRecord *previous_module = compiler->owner->current_module_record;
	compiler->owner->current_module_record = new_module;

	Compiler imported_compiler = {0};
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
	[CRUX_TOKEN_LEFT_PAREN] = {grouping, infix_call, NULL, PREC_CALL},
	[CRUX_TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_LEFT_BRACE] = {table_literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_LEFT_SQUARE] = {array_literal, collection_index, NULL, PREC_CALL},
	[CRUX_TOKEN_RIGHT_SQUARE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_DOLLAR_LEFT_BRACE] = {set_literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_DOLLAR_LEFT_SQUARE] = {tuple_literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_DOT] = {NULL, dot, NULL, PREC_CALL},
	[CRUX_TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
	[CRUX_TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
	[CRUX_TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[CRUX_TOKEN_BACKSLASH] = {NULL, binary, NULL, PREC_FACTOR},
	[CRUX_TOKEN_STAR] = {NULL, binary, NULL, PREC_FACTOR},
	[CRUX_TOKEN_STAR_STAR] = {NULL, binary, NULL, PREC_FACTOR},
	[CRUX_TOKEN_PERCENT] = {NULL, binary, NULL, PREC_FACTOR},
	[CRUX_TOKEN_LEFT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
	[CRUX_TOKEN_RIGHT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
	[CRUX_TOKEN_AMPERSAND] = {NULL, binary, NULL, PREC_BITWISE_AND},
	[CRUX_TOKEN_CARET] = {NULL, binary, NULL, PREC_BITWISE_XOR},
	[CRUX_TOKEN_PIPE] = {NULL, binary, NULL, PREC_BITWISE_OR},
	[CRUX_TOKEN_NOT] = {unary, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_BANG_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[CRUX_TOKEN_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
	[CRUX_TOKEN_GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
	[CRUX_TOKEN_GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[CRUX_TOKEN_LESS] = {NULL, binary, NULL, PREC_COMPARISON},
	[CRUX_TOKEN_LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
	[CRUX_TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_INT] = {number, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FLOAT] = {number, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_BINARY_INT] = {binary_number, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_HEX_INT] = {hex_number, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_CONTINUE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_BREAK] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_AND] = {NULL, and_, NULL, PREC_AND},
	[CRUX_TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FN] = {anonymous_function, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_NIL] = {literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_OR] = {NULL, or_, NULL, PREC_OR},
	[CRUX_TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_LET] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_USE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FROM] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_PUB] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_ERROR] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_DEFAULT] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_EQUAL_ARROW] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_MATCH] = {match_expression, NULL, NULL, PREC_PRIMARY},
	[CRUX_TOKEN_TYPEOF] = {typeof_expression, NULL, NULL, PREC_UNARY},
	[CRUX_TOKEN_STRUCT] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_NEW] = {struct_instance, NULL, NULL, PREC_UNARY},
	[CRUX_TOKEN_EOF] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_QUESTION_MARK] = {NULL, NULL, result_unwrap, PREC_CALL},
	[CRUX_TOKEN_AS] = {NULL, type_coerce, NULL, PREC_COERCE},
	[CRUX_TOKEN_TILDE] = {unary, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_PANIC] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_NIL_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_BOOL_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_INT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FLOAT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_STRING_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_ARRAY_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_TABLE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_ERROR_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_RESULT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_RANDOM_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_FILE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_STRUCT_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_VECTOR_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_COMPLEX_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_MATRIX_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_SET_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_TUPLE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_BUFFER_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_RANGE_TYPE] = {NULL, NULL, NULL, PREC_NONE},
	[CRUX_TOKEN_ANY_TYPE] = {NULL, NULL, NULL, PREC_NONE},
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
		prefixRule = get_rule(CRUX_TOKEN_IDENTIFIER)->prefix;
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

	if (can_assign && match(compiler, CRUX_TOKEN_EQUAL)) {
		compiler_panic(compiler->parser, "Invalid Assignment Target", SYNTAX);
	}
}

static ParseRule *get_rule(const CruxTokenType type)
{
	return &rules[type]; // Returns the rule at the given index
}

/**Pre-scanner
 * A two-sub-pass scanner that collects top-level struct and
 * function signatures before the main compilation pass begins.
 * This gives the compiler knowledge of forward-referenced names
 *
 * First pass collects struct declarations
 * Second pass collects function signatures
 */

// Advance the pre-scanner, skipping error tokens silently.
static void pre_advance(const Compiler *compiler)
{
	compiler->parser->previous = compiler->parser->current;
	for (;;) {
		compiler->parser->current = scan_token(compiler->parser->scanner);
		if (compiler->parser->current.type != CRUX_TOKEN_ERROR)
			break;
	}
}

/**
 * Skip a balanced brace block { ... } starting at the current token (which
 * must be CRUX_TOKEN_LEFT_BRACE). Handles nesting.
 */
static void pre_skip_block(const Compiler *compiler)
{
	if (compiler->parser->current.type != CRUX_TOKEN_LEFT_BRACE)
		return;
	pre_advance(compiler); // consume '{'
	int depth = 1;
	while (depth > 0 && compiler->parser->current.type != CRUX_TOKEN_EOF) {
		if (compiler->parser->current.type == CRUX_TOKEN_LEFT_BRACE)
			depth++;
		if (compiler->parser->current.type == CRUX_TOKEN_RIGHT_BRACE)
			depth--;
		pre_advance(compiler);
	}
}

// Skip a balanced parenthesis group ( ... ) starting at current token.
static void pre_skip_parens(const Compiler *compiler)
{
	if (compiler->parser->current.type != CRUX_TOKEN_LEFT_PAREN)
		return;
	pre_advance(compiler); // consume '('
	int depth = 1;
	while (depth > 0 && compiler->parser->current.type != CRUX_TOKEN_EOF) {
		if (compiler->parser->current.type == CRUX_TOKEN_LEFT_PAREN)
			depth++;
		if (compiler->parser->current.type == CRUX_TOKEN_RIGHT_PAREN)
			depth--;
		pre_advance(compiler);
	}
}

/**
 * Advance past a type annotation starting at the current token.
 * Handles nested brackets and parentheses (e.g. Array[Int], (Int)->Bool,
 * Table[String:Int], union types separated by '|').
 */
static void pre_skip_type(Compiler *compiler)
{
	for (;;) {
		const CruxTokenType t = compiler->parser->current.type;

		if (t == CRUX_TOKEN_LEFT_PAREN) {
			// Function type: (T, T) -> T
			pre_skip_parens(compiler);
			// consume '->'
			if (compiler->parser->current.type == CRUX_TOKEN_ARROW)
				pre_advance(compiler);
			pre_skip_type(compiler); // return type
		} else if (t == CRUX_TOKEN_SHAPE) {
			pre_advance(compiler); // consume 'shape'
			pre_skip_block(compiler); // skip '{ ... }'
		} else if (t == CRUX_TOKEN_INT_TYPE || t == CRUX_TOKEN_FLOAT_TYPE || t == CRUX_TOKEN_BOOL_TYPE ||
				   t == CRUX_TOKEN_STRING_TYPE || t == CRUX_TOKEN_NIL_TYPE || t == CRUX_TOKEN_ANY_TYPE ||
				   t == CRUX_TOKEN_ARRAY_TYPE || t == CRUX_TOKEN_TABLE_TYPE || t == CRUX_TOKEN_VECTOR_TYPE ||
				   t == CRUX_TOKEN_MATRIX_TYPE || t == CRUX_TOKEN_BUFFER_TYPE || t == CRUX_TOKEN_ERROR_TYPE ||
				   t == CRUX_TOKEN_RESULT_TYPE || t == CRUX_TOKEN_RANGE_TYPE || t == CRUX_TOKEN_TUPLE_TYPE ||
				   t == CRUX_TOKEN_COMPLEX_TYPE || t == CRUX_TOKEN_SET_TYPE || t == CRUX_TOKEN_RANDOM_TYPE ||
				   t == CRUX_TOKEN_FILE_TYPE || t == CRUX_TOKEN_IDENTIFIER || t == CRUX_TOKEN_NEVER_TYPE) {
			pre_advance(compiler); // consume the base type token
			// Optional subscript: Array[Int], Table[K,V], etc.
			if (compiler->parser->current.type == CRUX_TOKEN_LEFT_SQUARE) {
				pre_advance(compiler); // consume '['
				int depth = 1;
				while (depth > 0 && compiler->parser->current.type != CRUX_TOKEN_EOF) {
					if (compiler->parser->current.type == CRUX_TOKEN_LEFT_SQUARE)
						depth++;
					if (compiler->parser->current.type == CRUX_TOKEN_RIGHT_SQUARE)
						depth--;
					pre_advance(compiler);
				}
			}
		} else {
			// Unknown/unsupported — stop.
			break;
		}

		// Union continuation: T | T | ...
		if (compiler->parser->current.type == CRUX_TOKEN_PIPE) {
			pre_advance(compiler); // consume '|'
			continue; // parse next variant
		}
		break;
	}
}

// Collect a single top-level type alias declaration.
// On entry, parser.current is CRUX_TOKEN_TYPE (already consumed by caller).
static void pre_collect_type(Compiler *compiler)
{
	if (compiler->parser->current.type != CRUX_TOKEN_IDENTIFIER)
		return;
	Token name_token = compiler->parser->current;
	pre_advance(compiler);

	if (compiler->parser->current.type != CRUX_TOKEN_EQUAL)
		return;
	pre_advance(compiler); // consume '='

	// parse_type_record uses the global current compiler and parser.
	ObjectTypeRecord *resolved_type = parse_type_record(compiler);
	push(compiler->owner->current_module_record, OBJECT_VAL(resolved_type));

	// consume ';'
	if (compiler->parser->current.type == CRUX_TOKEN_SEMICOLON)
		pre_advance(compiler);

	ObjectString *type_name = copy_string(compiler->owner, name_token.start, name_token.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(type_name));
	type_table_set(compiler->type_table, type_name, resolved_type);
	pop(compiler->owner->current_module_record); // type_name
	pop(compiler->owner->current_module_record); // resolved_type
}

// Collect a single top-level struct declaration into pre_compiler's type_table.
// On entry parser.current is CRUX_TOKEN_STRUCT (already consumed by caller).
static void pre_collect_struct(Compiler *compiler)
{
	// Consume struct name.
	if (compiler->parser->current.type != CRUX_TOKEN_IDENTIFIER)
		return;
	const Token name_token = compiler->parser->current;
	pre_advance(compiler);

	// Expect '{' to start the struct body.
	if (compiler->parser->current.type != CRUX_TOKEN_LEFT_BRACE)
		return;
	pre_advance(compiler); // consume '{'

	ObjectTypeTable *field_types = new_type_table(compiler->owner, INITIAL_TYPE_TABLE_SIZE);
	push(compiler->owner->current_module_record, OBJECT_VAL(field_types));
	int field_count = 0;

	ObjectString *struct_name = copy_string(compiler->owner, name_token.start, name_token.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(struct_name));

	ObjectStruct *struct_obj = new_struct_type(compiler->owner, struct_name);
	push(compiler->owner->current_module_record, OBJECT_VAL(struct_obj));

	// Register the struct type before parsing fields so self-referential fields can resolve during the pre-pass.
	ObjectTypeRecord *struct_type = new_struct_type_rec(compiler->owner, struct_obj, field_types, 0);
	push(compiler->owner->current_module_record, OBJECT_VAL(struct_type));
	type_table_set(compiler->type_table, struct_name, struct_type);

	while (compiler->parser->current.type != CRUX_TOKEN_RIGHT_BRACE &&
		   compiler->parser->current.type != CRUX_TOKEN_EOF) {
		// Field name
		if (compiler->parser->current.type != CRUX_TOKEN_IDENTIFIER)
			break;
		const Token field_tok = compiler->parser->current;
		ObjectString *field_name = copy_string(compiler->owner, field_tok.start, field_tok.length);
		push(compiler->owner->current_module_record, OBJECT_VAL(field_name));
		pre_advance(compiler);

		ObjectTypeRecord *field_type = NULL;
		if (compiler->parser->current.type == CRUX_TOKEN_COLON) {
			pre_advance(compiler); // consume ':'
			field_type = parse_type_record(compiler);
		} else {
			field_type = new_type_rec(compiler->owner, ANY_TYPE);
		}
		push(compiler->owner->current_module_record, OBJECT_VAL(field_type));
		type_table_set(field_types, field_name, field_type);
		table_set(compiler->owner, &struct_obj->fields, field_name, INT_VAL(field_count));
		field_count++;

		// Allow trailing comma between fields. Min compiler will catch this later.
		if (compiler->parser->current.type == CRUX_TOKEN_COMMA)
			pre_advance(compiler);
	}

	// Consume '}'.
	if (compiler->parser->current.type == CRUX_TOKEN_RIGHT_BRACE)
		pre_advance(compiler);

	struct_type->as.struct_type.field_count = field_count;

	pop(compiler->owner->current_module_record); // struct type
	for (int i = 0; i < field_count; i++) {
		pop(compiler->owner->current_module_record); // field type
		pop(compiler->owner->current_module_record); // field name
	}
	pop(compiler->owner->current_module_record); // struct name
	pop(compiler->owner->current_module_record); // struct_obj
	pop(compiler->owner->current_module_record); // field_types
}

// Collect a single top-level function signature into pre_compiler's type_table.
static void pre_collect_function(Compiler *compiler)
{
	if (compiler->parser->current.type != CRUX_TOKEN_IDENTIFIER)
		return;
	const Token fn_name_token = compiler->parser->current;
	pre_advance(compiler);

	if (compiler->parser->current.type != CRUX_TOKEN_LEFT_PAREN)
		return;
	pre_advance(compiler); // consume '('

	int param_cap = 4;
	int param_count = 0;
	ObjectTypeRecord **param_types = ALLOCATE(compiler->owner, ObjectTypeRecord *, param_cap);
	if (!param_types) {
		return;
	}

	while (compiler->parser->current.type != CRUX_TOKEN_RIGHT_PAREN &&
		   compiler->parser->current.type != CRUX_TOKEN_EOF) {
		// Parameter name (identifier).
		if (compiler->parser->current.type != CRUX_TOKEN_IDENTIFIER) {
			FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_count);
			return;
		}
		pre_advance(compiler); // consume param name

		ObjectTypeRecord *param_type = NULL;
		if (compiler->parser->current.type == CRUX_TOKEN_COLON) {
			pre_advance(compiler); // consume ':'
			param_type = parse_type_record(compiler);
		} else {
			param_type = new_type_rec(compiler->owner, ANY_TYPE);
		}
		push(compiler->owner->current_module_record, OBJECT_VAL(param_type));

		if (param_count == param_cap) {
			const int old_cap = param_cap;
			param_cap = GROW_CAPACITY(param_cap);
			ObjectTypeRecord **grown = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap, param_cap);
			if (!grown) {
				pop(compiler->owner->current_module_record);
				FREE_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, old_cap);
				return;
			}
			param_types = grown;
		}
		param_types[param_count++] = param_type;

		if (compiler->parser->current.type == CRUX_TOKEN_COMMA)
			pre_advance(compiler);
	}

	// shrink to actual size
	param_types = GROW_ARRAY(compiler->owner, ObjectTypeRecord *, param_types, param_cap, param_count);

	if (compiler->parser->current.type == CRUX_TOKEN_RIGHT_PAREN)
		pre_advance(compiler);

	ObjectTypeRecord *return_type = NULL;
	if (compiler->parser->current.type == CRUX_TOKEN_ARROW) {
		pre_advance(compiler); // consume '->'
		return_type = parse_type_record(compiler);
	} else {
		return_type = T_ANY;
	}
	push(compiler->owner->current_module_record, OBJECT_VAL(return_type));

	ObjectTypeRecord *fn_type = new_function_type_rec(compiler->owner, param_types, param_count, return_type);
	push(compiler->owner->current_module_record, OBJECT_VAL(fn_type));
	ObjectString *fn_name = copy_string(compiler->owner, fn_name_token.start, fn_name_token.length);
	push(compiler->owner->current_module_record, OBJECT_VAL(fn_name));
	type_table_set(compiler->type_table, fn_name, fn_type);

	// Skip the function body
	pre_skip_block(compiler);
	pop(compiler->owner->current_module_record); // fn_name
	pop(compiler->owner->current_module_record); // fn_type
	pop(compiler->owner->current_module_record); // return type
	for (int i = 0; i < param_count; i++) {
		pop(compiler->owner->current_module_record); // param_type
	}
}

/**
 * Run a forward-scan sub-pass.
 * collects structs or functions based on the collect_structs flag.
 */
static void pre_scan_pass(Compiler *compiler, const bool collect_structs)
{
	// Reset parser to a clean state
	compiler->parser->had_error = false;
	compiler->parser->panic_mode = false;

	pre_advance(compiler);

	while (compiler->parser->current.type != CRUX_TOKEN_EOF) {
		const CruxTokenType t = compiler->parser->current.type;

		if (t == CRUX_TOKEN_STRUCT) {
			pre_advance(compiler); // consume 'struct'
			if (collect_structs) {
				pre_collect_struct(compiler);
			} else {
				// Skip: name + block
				if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
					pre_advance(compiler);
				pre_skip_block(compiler);
			}

		} else if (t == CRUX_TOKEN_TYPE) {
			pre_advance(compiler); // consume 'type'
			if (collect_structs) {
				pre_collect_type(compiler);
			} else {
				// Skip: name + '=' + type + ';'
				if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
					pre_advance(compiler);
				if (compiler->parser->current.type == CRUX_TOKEN_EQUAL)
					pre_advance(compiler);
				pre_skip_type(compiler);
				if (compiler->parser->current.type == CRUX_TOKEN_SEMICOLON)
					pre_advance(compiler);
			}

		} else if (t == CRUX_TOKEN_FN) {
			pre_advance(compiler); // consume 'fn'
			if (!collect_structs) {
				pre_collect_function(compiler);
			} else {
				// Skip: name + parens + optional ->T + block
				if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
					pre_advance(compiler);
				pre_skip_parens(compiler);
				if (compiler->parser->current.type == CRUX_TOKEN_ARROW) {
					pre_advance(compiler);
					pre_skip_type(compiler);
				}
				pre_skip_block(compiler);
			}

		} else if (t == CRUX_TOKEN_PUB) {
			pre_advance(compiler); // consume 'pub'
			// pub fn ... or pub struct ...
			if (compiler->parser->current.type == CRUX_TOKEN_FN) {
				pre_advance(compiler); // consume 'fn'
				if (!collect_structs) {
					pre_collect_function(compiler);
				} else {
					if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
						pre_advance(compiler);
					pre_skip_parens(compiler);
					if (compiler->parser->current.type == CRUX_TOKEN_ARROW) {
						pre_advance(compiler);
						pre_skip_type(compiler);
					}
					pre_skip_block(compiler);
				}
			} else if (compiler->parser->current.type == CRUX_TOKEN_STRUCT) {
				pre_advance(compiler); // consume 'struct'
				if (collect_structs) {
					pre_collect_struct(compiler);
				} else {
					if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
						pre_advance(compiler);
					pre_skip_block(compiler);
				}
			} else if (compiler->parser->current.type == CRUX_TOKEN_TYPE) {
				pre_advance(compiler); // consume 'type'
				if (collect_structs) {
					pre_collect_type(compiler);
				} else {
					if (compiler->parser->current.type == CRUX_TOKEN_IDENTIFIER)
						pre_advance(compiler);
					if (compiler->parser->current.type == CRUX_TOKEN_EQUAL)
						pre_advance(compiler);
					pre_skip_type(compiler);
					if (compiler->parser->current.type == CRUX_TOKEN_SEMICOLON)
						pre_advance(compiler);
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
// The scanner must be initialized before calling.
static void pre_scan(Compiler *compiler, char *source, ObjectTypeTable *dest)
{
	// Sub-pass 1: collect struct declarations
	Compiler pre_compiler_structs = {0};
	init_compiler(compiler->owner, &pre_compiler_structs, compiler, TYPE_SCRIPT);
	init_scanner(pre_compiler_structs.parser->scanner, source);

	pre_compiler_structs.parser->had_error = false;
	pre_compiler_structs.parser->panic_mode = false;
	pre_compiler_structs.parser->source = source;

	pre_scan_pass(&pre_compiler_structs, true);

	push(compiler->owner->current_module_record, OBJECT_VAL(pre_compiler_structs.type_table));
	compiler->enclosed = NULL;
	free(pre_compiler_structs.parser->scanner);
	free(pre_compiler_structs.parser);

	// Sub-pass 2: collect function signatures
	Compiler pre_compiler_fns = {0};
	init_compiler(compiler->owner, &pre_compiler_fns, compiler, TYPE_SCRIPT);
	init_scanner(pre_compiler_fns.parser->scanner, source);
	pre_compiler_fns.parser->had_error = false;
	pre_compiler_fns.parser->panic_mode = false;

	type_table_add_all(pre_compiler_structs.type_table, pre_compiler_fns.type_table);

	pre_scan_pass(&pre_compiler_fns, false);

	push(compiler->owner->current_module_record, OBJECT_VAL(pre_compiler_fns.type_table));
	compiler->enclosed = NULL;
	free(pre_compiler_fns.parser->scanner);
	free(pre_compiler_fns.parser);

	type_table_add_all(pre_compiler_structs.type_table, dest);
	type_table_add_all(pre_compiler_fns.type_table, dest);

	pop(compiler->owner->current_module_record); // pre_compiler_fns.type_table
	pop(compiler->owner->current_module_record); // pre_compiler_structs.type_table
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
	push(vm->current_module_record, OBJECT_VAL(staging));
	pre_scan(compiler, source, staging);

	for (int i = 0; i < staging->capacity; i++) {
		const TypeEntry *entry = &staging->entries[i];
		if (entry->key == NULL)
			continue;
		type_table_set(compiler->type_table, entry->key, entry->value);
	}
	pop(vm->current_module_record); // staging table

	// Main compiler pass
	init_scanner(compiler->parser->scanner, source);
	compiler->parser->had_error = false;
	compiler->parser->panic_mode = false;
	compiler->parser->source = source;

	advance(compiler);

	while (!match(compiler, CRUX_TOKEN_EOF)) {
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

void mark_compiler_roots(VM *vm, const Compiler *compiler)
{
	if (compiler == NULL)
		return;
	const Compiler *current = compiler;
	while (current != NULL) {
		mark_object(vm, (CruxObject *)current->function);
		mark_object(vm, (CruxObject *)current->return_type);
		mark_object(vm, (CruxObject *)current->last_give_type);
		mark_object(vm, (CruxObject *)current->type_table);

		if (current->current_narrowing.tracked_global_name != NULL)
			mark_object(vm, (CruxObject *)current->current_narrowing.tracked_global_name);
		if (current->current_narrowing.tracked_literal_type != NULL)
			mark_object(vm, (CruxObject *)current->current_narrowing.tracked_literal_type);
		if (current->current_narrowing.global_name != NULL)
			mark_object(vm, (CruxObject *)current->current_narrowing.global_name);
		if (current->current_narrowing.narrowed_to != NULL)
			mark_object(vm, (CruxObject *)current->current_narrowing.narrowed_to);
		if (current->current_narrowing.stripped_down != NULL)
			mark_object(vm, (CruxObject *)current->current_narrowing.stripped_down);

		for (int i = 0; i < current->type_stack_count; i++) {
			mark_object(vm, (CruxObject *)current->type_stack[i]);
		}
		for (int i = 0; i < current->local_count; i++) {
			mark_object(vm, (CruxObject *)current->locals[i].type);
		}
		current = (Compiler *)current->enclosed;
	}
}
