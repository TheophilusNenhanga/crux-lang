/*
*	@misc{Keleshev:2022-1,
*	title="Abstract Syntax Tree: an Example in C",
*	author="Vladimir Keleshev",
*	year=2022,
*	howpublished=
*
"\url{https://keleshev.com/abstract-syntax-tree-an-example-in-c}",
}*/

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdint.h>

#include "compiler.h"
#include "compiler/ast_compiler.h"

typedef struct AST AST;

struct AST {
	enum {
		/* Literals */
		AST_INT,
		AST_FLOAT,
		AST_BOOL,
		AST_STRING,
		AST_ARRAY_LITERAL,
		AST_TABLE_LITERAL,

		/* Variables and Identifiers */
		AST_IDENTIFIER,
		AST_VAR_DECLARATION,
		AST_ASSIGNMENT,
		AST_COMPOUND_ASSIGNMENT,

		/* Expressions */
		AST_BINARY_OP,
		AST_UNARY_OP,
		AST_GROUPING,

		/* Functions */
		AST_FUNCTION_DECLARATION,
		AST_ANON_FUNCTION_DECLARATION,
		AST_FUNCTION_CALL,
		AST_RETURN_STATEMENT,

		/* Control Flow */
		AST_IF_STATEMENT,
		AST_WHILE_LOOP,
		AST_FOR_LOOP,
		AST_BREAK_STATEMENT,
		AST_CONTINUE_STATEMENT,
		AST_BLOCK,

		/* Object Oriented Stuff */
		AST_STRUCT_DECLARATION,
		AST_STRUCT_INSTANCE,
		AST_DOT_ACCESS,
		AST_METHOD_CALL,

		/* Collections */
		AST_COLLECTION_INDEX,

		/* Pattern Matching */
		AST_MATCH_EXPRESSION,
		AST_MATCH_ARM,
		AST_GIVE_STATEMENT,

		/* Modules */
		AST_USE_STATEMENT,
		AST_PUBLIC_DECLARATION,

		/* Other */
		AST_EXPRESSION_STATEMENT,
		AST_TYPEOF,
		AST_RESULT_UNWRAP,
		AST_PROGRAM
	} tag;
	union {
		/* Literals */
		struct AST_INT {
			int32_t value;
		} AST_INT;

		struct AST_FLOAT {
			double value;
		} AST_FLOAT;

		struct AST_BOOL {
			bool value;
		} AST_BOOL;

		struct AST_STRING {
			char *value;
			uint32_t length;
		} AST_STRING;

		struct AST_ARRAY_LITERAL {
			AST **elements;
			uint32_t count;
		} AST_ARRAY_LITERAL;

		struct AST_TABLE_LITERAL {
			AST **keys;
			AST **values;
			uint32_t count;
		} AST_TABLE_LITERAL;

		/* Variables and Identifiers */
		struct AST_IDENTIFIER {
			char *name;
			uint32_t length;
		} AST_IDENTIFIER;

		struct AST_VAR_DECLARATION {
			char *name;
			uint32_t length;
			AST *initializer; // Can be NULL
		} AST_VAR_DECLARATION;

		struct AST_ASSIGNMENT {
			AST *target;
			AST *value;
		} AST_ASSIGNMENT;

		struct AST_COMPOUND_ASSIGNMENT {
			AST *target;
			CompoundOp op;
			AST *value;
		} AST_COMPOUND_ASSIGNMENT;

		/* Expressions */
		struct AST_BINARY_OP {
			AST *left;
			CruxTokenType op;
			AST *right;
		} AST_BINARY_OP;

		struct AST_UNARY_OP {
			CruxTokenType op;
			AST *operand;
		} AST_UNARY_OP;

		struct AST_GROUPING {
			AST *expression;
		} AST_GROUPING;

		struct AST_LOGICAL_AND {
			AST *left;
			AST *right;
		} AST_LOGICAL_AND;

		struct AST_LOGICAL_OR {
			AST *left;
			AST *right;
		} AST_LOGICAL_OR;

		/* Functions */
		struct AST_FUNCTION_DECLARATION {
			char *name;
			uint32_t name_length;
			AST **parameters;
			uint32_t param_count;
			AST *body;
			FunctionType function_type;
		} AST_FUNCTION_DECLARATION;

		struct AST_ANON_FUNCTION_DECLARATION {
			AST **parameters;
			uint32_t param_count;
			AST *body;
		} AST_ANON_FUNCTION_DECLARATION;

		struct AST_FUNCTION_CALL {
			AST *callee;
			AST **arguments;
			uint32_t arg_count;
		} AST_FUNCTION_CALL;

		struct AST_RETURN_STATEMENT {
			AST *value; // Can be NULL for empty return
		} AST_RETURN_STATEMENT;

		/* Control Flow */
		struct AST_IF_STATEMENT {
			AST *condition;
			AST *then_branch;
			AST *else_branch; // Can be NULL
		} AST_IF_STATEMENT;

		struct AST_WHILE_LOOP {
			AST *condition;
			AST *body;
		} AST_WHILE_LOOP;

		struct AST_FOR_LOOP {
			AST *initializer; // Can be NULL
			AST *condition; // Can be NULL
			AST *increment; // Can be NULL
			AST *body;
		} AST_FOR_LOOP;

		struct AST_BREAK_STATEMENT {
		} AST_BREAK_STATEMENT;

		struct AST_CONTINUE_STATEMENT {
		} AST_CONTINUE_STATEMENT;

		struct AST_BLOCK {
			AST **statements;
			uint32_t count;
		} AST_BLOCK;

		/* Object Oriented Stuff */
		struct AST_STRUCT_DECLARATION {
			char *name;
			uint32_t name_length;
			char **field_names;
			uint32_t *field_name_lengths;
			uint32_t field_count;
		} AST_STRUCT_DECLARATION;

		struct AST_STRUCT_INSTANCE {
			char *struct_name;
			uint32_t name_length;
			char **field_names;
			uint32_t *field_name_lengths;
			AST **field_values;
			uint32_t field_count;
		} AST_STRUCT_INSTANCE;

		struct AST_DOT_ACCESS {
			AST *object;
			char *property;
			uint32_t property_length;
		} AST_DOT_ACCESS;

		struct AST_METHOD_CALL {
			AST *object;
			char *method_name;
			uint32_t method_length;
			AST **arguments;
			uint32_t arg_count;
		} AST_METHOD_CALL;

		/* Collections */
		struct AST_COLLECTION_INDEX {
			AST *collection;
			AST *index;
		} AST_COLLECTION_INDEX;

		/* Pattern Matching */
		struct AST_MATCH_EXPRESSION {
			AST *target;
			AST **arms;
			uint32_t arm_count;
		} AST_MATCH_EXPRESSION;

		struct AST_MATCH_ARM {
			AST *pattern; // Can be NULL for default
			char *binding; // For Ok(x) or Err(x) patterns, can be
				       // NULL
			uint32_t binding_length;
			AST *body;
			bool is_default;
			bool is_ok_pattern;
			bool is_err_pattern;
		} AST_MATCH_ARM;

		struct AST_GIVE_STATEMENT {
			AST *value; // Can be NULL
		} AST_GIVE_STATEMENT;

		/* Modules */
		struct AST_USE_STATEMENT {
			char **import_names;
			uint32_t *import_name_lengths;
			char **alias_names; // Can be NULL for imports without
					    // aliases
			uint32_t *alias_name_lengths;
			uint32_t import_count;
			char *module_path;
			uint32_t module_path_length;
			bool is_native;
		} AST_USE_STATEMENT;

		struct AST_PUBLIC_DECLARATION {
			AST *declaration; // Can be fn, let, or struct
		} AST_PUBLIC_DECLARATION;

		/* Other */
		struct AST_EXPRESSION_STATEMENT {
			AST *expression;
		} AST_EXPRESSION_STATEMENT;

		struct AST_TYPEOF {
			AST *operand;
		} AST_TYPEOF;

		struct AST_RESULT_UNWRAP {
			AST *operand;
		} AST_RESULT_UNWRAP;

		struct AST_PROGRAM {
			AST **declarations;
			uint32_t count;
		} AST_PROGRAM;
	};
};

AST* parse_expression(ASTParser* parser);
AST* parse_statement(ASTParser* parser);
AST* parse_declaration(ASTParser* parser);
AST* parse_program(ASTParser* parser);

ObjectFunction* ast_compile(VM* vm, const char* source);

AST* new_ast(AST ast);

#define NEW_AST(tag, ...) \
	new_ast((AST) {tag, {.tag=(struct tag){__VA_ARGS__}}})

#endif // AST_H
