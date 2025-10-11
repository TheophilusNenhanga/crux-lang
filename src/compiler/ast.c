#include <stdlib.h>

#include "compiler/ast.h"
#include "compiler/ast_compiler.h"

AST * new_ast(const AST ast)
{
	AST* ptr = malloc(sizeof(AST));
	if (ptr == NULL) exit(-1);
	*ptr = ast;
	return ptr;
}


AST* parse_expression(ASTParser* parser)
{

}

AST* parse_statement(ASTParser* parser)
{

}

AST* parse_declaration(ASTParser* parser)
{

}

AST* parse_program(ASTParser* parser)
{

}
