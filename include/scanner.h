#ifndef SCANNER_H
#define SCANNER_H

typedef struct {
	const char *start;
	const char *current;
	int line;
} Scanner;

typedef enum {
	// Single-character tokens.
	TOKEN_LEFT_PAREN, // (
	TOKEN_RIGHT_PAREN, // )
	TOKEN_LEFT_BRACE, // {
	TOKEN_RIGHT_BRACE, // }
	TOKEN_LEFT_SQUARE, // [
	TOKEN_RIGHT_SQUARE, // ]
	TOKEN_COMMA, // ,
	TOKEN_DOT, // .
	TOKEN_MINUS, // -
	TOKEN_PLUS, // +
	TOKEN_SEMICOLON, // ;
	TOKEN_SLASH, // /
	TOKEN_BACKSLASH, // "\"
	TOKEN_STAR, // *
	TOKEN_STAR_STAR, // **
	TOKEN_PERCENT, // %
	TOKEN_COLON, // :
	TOKEN_SINGLE_QUOTE, // '
	TOKEN_DOUBLE_QUOTE, // "
	// One or two character tokens. //
	TOKEN_ARROW, // ->
	TOKEN_BANG_EQUAL, // !=
	TOKEN_EQUAL, // =
	TOKEN_EQUAL_EQUAL, // ==
	TOKEN_GREATER, // >
	TOKEN_GREATER_EQUAL, // >=
	TOKEN_LESS, // <
	TOKEN_LESS_EQUAL, // <=
	TOKEN_LEFT_SHIFT, // <<
	TOKEN_RIGHT_SHIFT, // >>
	TOKEN_AMPERSAND, // &
	TOKEN_CARET, // ^
	TOKEN_PIPE, // |
	TOKEN_TILDE, // ~
	TOKEN_PLUS_EQUAL, // +=
	TOKEN_MINUS_EQUAL, // -=
	TOKEN_STAR_EQUAL, // *=
	TOKEN_SLASH_EQUAL, // /=
	TOKEN_BACK_SLASH_EQUAL, // \=
	TOKEN_PERCENT_EQUAL, // %=
	TOKEN_QUESTION_MARK, // ?
	// Literals. //
	TOKEN_IDENTIFIER, //
	TOKEN_STRING, //
	TOKEN_INT, //
	TOKEN_FLOAT, //
	// Keywords. //
	TOKEN_AND, // and
	TOKEN_NOT, // not
	TOKEN_ELSE, // else
	TOKEN_FALSE, // false
	TOKEN_FOR, // for
	TOKEN_FN, // fn
	TOKEN_IF, // if
	TOKEN_NIL, // nil
	TOKEN_OR, // or
	TOKEN_RETURN, // return
	TOKEN_TRUE, // true
	TOKEN_LET, // let
	TOKEN_WHILE, // while
	TOKEN_ERROR, //
	TOKEN_BREAK, // break
	TOKEN_CONTINUE, // continue
	TOKEN_USE, // use
	TOKEN_FROM, // from
	TOKEN_PUB, // pub
	TOKEN_AS, // as
	TOKEN_EOF, //
	TOKEN_MATCH, // match
	TOKEN_EQUAL_ARROW, // =>
	TOKEN_OK, // Ok
	TOKEN_ERR, // Err
	TOKEN_DEFAULT, // default
	TOKEN_GIVE, // give
	TOKEN_TYPEOF, // typeof
	TOKEN_NEW, // new
	TOKEN_PANIC, // panic
	TOKEN_STRUCT, // struct
	TOKEN_SHAPE, // shape
	TOKEN_DYN_USE, // dynuse
	TOKEN_IMPL, // impl
	TOKEN_TYPE, // type

	TOKEN_NIL_TYPE, // Nil
	TOKEN_BOOL_TYPE, // Bool
	TOKEN_INT_TYPE, // Int
	TOKEN_FLOAT_TYPE, // Float
	TOKEN_STRING_TYPE, // String
	TOKEN_ARRAY_TYPE, // Array
	TOKEN_TABLE_TYPE, // Table
	TOKEN_ERROR_TYPE, // Error
	TOKEN_RESULT_TYPE, // Result
	TOKEN_RANDOM_TYPE, // Random
	TOKEN_FILE_TYPE, // File
	TOKEN_STRUCT_TYPE, // Struct
	TOKEN_VECTOR_TYPE, // Vector
	TOKEN_COMPLEX_TYPE, // Complex
	TOKEN_MATRIX_TYPE, // Matrix
	TOKEN_SET_TYPE, // Set
	TOKEN_TUPLE_TYPE, // Tuple
	TOKEN_BUFFER_TYPE, // Buffer
	TOKEN_RANGE_TYPE, // Range
	TOKEN_ANY_TYPE, // Any
	TOKEN_NEVER_TYPE, // Never
} CruxTokenType;

typedef struct {
	CruxTokenType type;
	int length;
	const char *start;
	int line;
} Token;

void init_scanner(Scanner* scanner, const char *source);
Token scan_token(Scanner* scanner);

#endif // SCANNER_H
