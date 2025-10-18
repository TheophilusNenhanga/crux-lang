#include "scanner.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
	const char *start;
	const char *current;
	int line;
} Scanner;

Scanner scanner;

/**
 * Advances the scanner to the next character.
 * @return The character that was just consumed
 */
static char advance(void)
{
	scanner.current++;
	return scanner.current[-1];
}

/**
 * Returns the current character without consuming it.
 * @return The current character
 */
static char peek(void)
{
	return *scanner.current;
}

/**
 * Checks if the scanner has reached the end of the source.
 * @return true if at the end of source, false otherwise
 */
static bool is_at_end(void)
{
	return *scanner.current == '\0';
}

/**
 * Returns the character after the current one without consuming any.
 * @return The next character, or '\0' if at the end of source
 */
static char peek_next(void)
{
	if (is_at_end())
		return '\0';
	return scanner.current[1];
}

/**
 * Determines if a character can start an identifier.
 * @param c The character to check
 * @return true if the character can start an identifier, false otherwise
 */
static bool is_identifier_starter(const char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * Checks if a character is alphabetic or underscore.
 * @param c The character to check
 * @return true if the character is alphabetic or underscore, false otherwise
 */
static bool is_alpha(const char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * Skips whitespace and comments in the source code.
 */
static void skip_whitespace(void)
{
	for (;;) {
		const char c = peek();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;
		case '\n': {
			scanner.line++;
			advance();
			break;
		}
		case '/': {
			if (peek_next() == '/') {
				// Comments are only on one line
				while (peek() != '\n' && !is_at_end())
					advance();
			} else {
				return;
			}
			break;
		}
		default:
			return;
		}
	}
}

static CruxTokenType check_keyword(const int start, const int length,
				   const char *rest, const CruxTokenType type)
{
	if (scanner.current - scanner.start == start + length &&
	    memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}
	return TOKEN_IDENTIFIER;
}

/**
 * Determines the token type of identifier.
 * @return The token type (keyword or identifier)
 */
static CruxTokenType identifier_type(void)
{
	switch (scanner.start[0]) {
	case 'a': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 's': {
				return check_keyword(2, 0, "", TOKEN_AS);
			}
			case 'n': {
				return check_keyword(2, 1, "d", TOKEN_AND);
			}
			default:;
			}
		}
		break;
	}
	case 'b':
		return check_keyword(1, 4, "reak", TOKEN_BREAK);
	case 'c': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'o':
				return check_keyword(2, 6, "ntinue",
						     TOKEN_CONTINUE);
			default:;
			}
		}
		break;
	}
	case 'd': {
		return check_keyword(1, 6, "efault", TOKEN_DEFAULT);
	}
	case 'e':
		return check_keyword(1, 3, "lse", TOKEN_ELSE);
	case 'g': {
		return check_keyword(1, 3, "ive", TOKEN_GIVE);
	}
	case 'i':
		return check_keyword(1, 1, "f", TOKEN_IF);
	case 'l':
		return check_keyword(1, 2, "et", TOKEN_LET);
	case 'n':
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'o': {
				return check_keyword(2, 1, "t", TOKEN_NOT);
			}
			case 'i': {
				return check_keyword(2, 1, "l", TOKEN_NIL);
			}
			case 'e': {
				return check_keyword(2, 1, "w", TOKEN_NEW);
			}
			default:;
			}
			break;
		}
	case 'o':
		return check_keyword(1, 1, "r", TOKEN_OR);
	case 'r':
		return check_keyword(1, 5, "eturn", TOKEN_RETURN);
	case 's': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 't': {
				if (scanner.current - scanner.start > 2) {
					return check_keyword(2, 4, "ruct",
							     TOKEN_STRUCT);
				}
			}
			default:;
			}
		}
		break;
	}
	case 'w':
		return check_keyword(1, 4, "hile", TOKEN_WHILE);
	case 'f': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'a':
				return check_keyword(2, 3, "lse", TOKEN_FALSE);
			case 'o':
				return check_keyword(2, 1, "r", TOKEN_FOR);
			case 'n':
				return TOKEN_FN;
			case 'r':
				return check_keyword(2, 2, "om", TOKEN_FROM);
			default:;
			}
		}
		break;
	}
	case 'm': {
		return check_keyword(1, 4, "atch", TOKEN_MATCH);
	}
	case 't': {
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'r':
				return check_keyword(2, 2, "ue", TOKEN_TRUE);
			case 'y':
				return check_keyword(2, 4, "peof",
						     TOKEN_TYPEOF);
			default:;
			}
		}
		break;
	}
	case 'u':
		return check_keyword(1, 2, "se", TOKEN_USE);
	case 'p':
		return check_keyword(1, 2, "ub", TOKEN_PUB);
	case 'E':
		return check_keyword(1, 2, "rr", TOKEN_ERR);
	case 'O':
		return check_keyword(1, 1, "k", TOKEN_OK);
	default:;
	}

	return TOKEN_IDENTIFIER;
}

void init_scanner(const char *source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line += 1;
}

/**
 * Creates a token of the given type from the current lexeme.
 * @param type The token type
 * @return The created token
 */
static Token make_token(const CruxTokenType type)
{
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

/**
 * Creates an error token with the given message.
 * @param message The error message
 * @return The error token
 */
static Token error_token(const char *message)
{
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	return token;
}

/**
 * Checks if the current character matches the expected one and advances if it
 * does.
 * @param expected The character to match
 * @return true if the character matches, false otherwise
 */
static bool match(const char expected)
{
	if (is_at_end())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}

/**
 * Scans a single-quoted string literal.
 * @return The string token or an error token
 */
static Token single_string(void)
{
	while (!is_at_end()) {
		if (peek() == '\'')
			break;
		if (peek() == '\\') {
			advance();
			if (!is_at_end()) {
				advance();
			}
			continue;
		}
		if (peek() == '\n') {
			scanner.line++;
		}
		advance();
	}

	if (is_at_end())
		return error_token("Unterminated String");
	// The closing quote
	advance();
	return make_token(TOKEN_STRING);
}

/**
 * Scans a double-quoted string literal.
 * @return The string token or an error token
 */
static Token double_string(void)
{
	while (!is_at_end()) {
		if (peek() == '"')
			break;
		if (peek() == '\\') {
			advance();
			if (!is_at_end()) {
				advance();
			}
			continue;
		}
		if (peek() == '\n') {
			scanner.line++;
		}
		advance();
	}

	if (is_at_end())
		return error_token("Unterminated String");
	// The closing quote
	advance();
	return make_token(TOKEN_STRING);
}

/**
 * Checks if a character is a digit (0-9).
 * @param c The character to check
 * @return true if the character is a digit, false otherwise
 */
static bool is_digit(const char c)
{
	return c >= '0' && c <= '9';
}

/**
 * Scans a numeric literal (integer or float).
 * @return The numeric token (TOKEN_INT or TOKEN_FLOAT)
 */
static Token number(void)
{
	while (is_digit(peek()))
		advance();
	bool fpFound = false;
	if (peek() == '.' && is_digit(peek_next())) {
		fpFound = true;
		advance();
		while (is_digit(peek()))
			advance();
	}
	if (fpFound) {
		return make_token(TOKEN_FLOAT);
	}
	return make_token(TOKEN_INT);
}

/**
 * Scans an identifier or keyword.
 * @return The identifier or keyword token
 */
static Token identifier(void)
{
	while (is_alpha(peek()) || is_digit(peek()))
		advance();
	return make_token(identifier_type());
}

Token scan_token(void)
{
	skip_whitespace();
	scanner.start = scanner.current;
	if (is_at_end())
		return make_token(TOKEN_EOF);

	const char c = advance();

	if (is_digit(c))
		return number();
	if (is_identifier_starter(c))
		return identifier();

	switch (c) {
	case ':':
		return make_token(TOKEN_COLON);
	case '(':
		return make_token(TOKEN_LEFT_PAREN);
	case ')':
		return make_token(TOKEN_RIGHT_PAREN);
	case '{':
		return make_token(TOKEN_LEFT_BRACE);
	case '}':
		return make_token(TOKEN_RIGHT_BRACE);
	case '[':
		return make_token(TOKEN_LEFT_SQUARE);
	case ']':
		return make_token(TOKEN_RIGHT_SQUARE);
	case ';':
		return make_token(TOKEN_SEMICOLON);
	case ',':
		return make_token(TOKEN_COMMA);
	case '.':
		return make_token(TOKEN_DOT);
	case '-':
		return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
	case '+':
		return make_token(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
	case '/':
		return make_token(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
	case '\\':
		if (match('=')) {
			return make_token(TOKEN_BACK_SLASH_EQUAL);
		}
		return make_token(TOKEN_BACKSLASH);
	case '*': {
		if (match('*')) {
			return make_token(TOKEN_STAR_STAR);
		}
		if (match('=')) {
			return make_token(TOKEN_STAR_EQUAL);
		}
		return make_token(TOKEN_STAR);
	}
	case '%':
		if (match('=')) {
			return make_token(TOKEN_PERCENT_EQUAL);
		}
		return make_token(TOKEN_PERCENT);
	case '!': {
		if (match('=')) {
			return make_token(TOKEN_BANG_EQUAL);
		}
		return error_token("Unexpected token");
	}
	case '=': {
		if (match('=')) {
			return make_token(TOKEN_EQUAL_EQUAL);
		}
		if (match('>')) {
			return make_token(TOKEN_EQUAL_ARROW);
		}
		return make_token(TOKEN_EQUAL);
	}
	case '<':
		if (match('<')) {
			return make_token(TOKEN_LEFT_SHIFT);
		}
		return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
	case '>':
		if (match('>')) {
			return make_token(TOKEN_RIGHT_SHIFT);
		}
		return make_token(match('=') ? TOKEN_GREATER_EQUAL
					     : TOKEN_GREATER);
	case '"':
		return double_string();
	case '\'':
		return single_string();
	case '$': {
		if (match('{')) {
			return make_token(TOKEN_DOLLAR_LEFT_CURLY);
		}
		if (match('[')) {
			return make_token(TOKEN_DOLLAR_LEFT_SQUARE);
		}
		break;
	}
	case '?': {
		return make_token(TOKEN_QUESTION_MARK);
	}
	default:;
	}
	return error_token("Unexpected character.");
}
