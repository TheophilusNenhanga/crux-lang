#ifndef SCANNER_H
#define SCANNER_H

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN,   // (
  TOKEN_RIGHT_PAREN,  // )
  TOKEN_LEFT_BRACE,   // {
  TOKEN_RIGHT_BRACE,  // }
  TOKEN_LEFT_SQUARE,  // [
  TOKEN_RIGHT_SQUARE, // ]
  TOKEN_COMMA,        // ,
  TOKEN_DOT,          // .
  TOKEN_MINUS,        // -
  TOKEN_PLUS,         // +
  TOKEN_SEMICOLON,    // ;
  TOKEN_SLASH,        // /
  TOKEN_BACKSLASH,    // "\"
  TOKEN_STAR,         // *
  TOKEN_STAR_STAR,    // **
  TOKEN_PERCENT,      // %
  TOKEN_COLON,        // :
  TOKEN_SINGLE_QUOTE, // '
  TOKEN_DOUBLE_QUOTE, // "
  // One or two character tokens. //
  TOKEN_BANG_EQUAL,       // !=
  TOKEN_EQUAL,            // =
  TOKEN_EQUAL_EQUAL,      // ==
  TOKEN_GREATER,          // >
  TOKEN_GREATER_EQUAL,    // >=
  TOKEN_LESS,             // <
  TOKEN_LESS_EQUAL,       // <=
  TOKEN_LEFT_SHIFT,       // <<
  TOKEN_RIGHT_SHIFT,      // >>
  TOKEN_PLUS_EQUAL,       // +=
  TOKEN_MINUS_EQUAL,      // -=
  TOKEN_STAR_EQUAL,       // *=
  TOKEN_SLASH_EQUAL,      // /=
  TOKEN_BACK_SLASH_EQUAL, // \=
  TOKEN_PERCENT_EQUAL,    // %=
  // Literals. //
  TOKEN_IDENTIFIER, //
  TOKEN_STRING,     //
  TOKEN_INT,        //
  TOKEN_FLOAT,      //
  // Keywords. //
  TOKEN_AND,         // and
  TOKEN_NOT,         // not
  TOKEN_ELSE,        // else
  TOKEN_FALSE,       // false
  TOKEN_FOR,         // for
  TOKEN_FN,          // fn
  TOKEN_IF,          // if
  TOKEN_NIL,         // nil
  TOKEN_OR,          // or
  TOKEN_RETURN,      // return
  TOKEN_TRUE,        // true
  TOKEN_LET,         // let
  TOKEN_WHILE,       // while
  TOKEN_ERROR,       //
  TOKEN_BREAK,       // break
  TOKEN_CONTINUE,    // continue
  TOKEN_USE,         // use
  TOKEN_FROM,        // from
  TOKEN_PUB,         // pub
  TOKEN_AS,          // as
  TOKEN_EOF,         //
  TOKEN_MATCH,       // match
  TOKEN_EQUAL_ARROW, // =>
  TOKEN_OK,          // Ok
  TOKEN_ERR,         // Err
  TOKEN_DEFAULT,     // default
  TOKEN_GIVE,        // give
  TOKEN_TYPEOF,      // typeof
  TOKEN_NEW,         // new

  TOKEN_DOLLAR_LEFT_CURLY,  // ${
  TOKEN_DOLLAR_LEFT_SQUARE, // $[
  TOKEN_STRUCT, // struct

  TOKEN_QUESTION_MARK,
} CruxTokenType;

typedef struct {
  CruxTokenType type;
  int length;
  const char *start;
  int line;
} Token;

/**
 * Initializes the scanner with the given source code.
 * @param source Pointer to the source code string
 */
void initScanner(const char *source);

/**
 * Scans the next token from the source code.
 * @return The scanned token
 */
Token scanToken();

#endif // SCANNER_H
