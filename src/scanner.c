#include "scanner.h"

#include <stdbool.h>
#include <string.h>

/**
 * Advances the scanner to the next character.
 * @return The character that was just consumed
 */
static char advance(Scanner *scanner)
{
	scanner->current++;
	return scanner->current[-1];
}

/**
 * Returns the current character without consuming it.
 * @return The current character
 */
static char peek(Scanner *scanner)
{
	return *scanner->current;
}

/**
 * Checks if the scanner has reached the end of the source.
 * @return true if at the end of source, false otherwise
 */
static bool is_at_end(Scanner *scanner)
{
	return *scanner->current == '\0';
}

/**
 * Returns the character after the current one without consuming any.
 * @return The next character, or '\0' if at the end of source
 */
static char peek_next(Scanner *scanner)
{
	if (is_at_end(scanner))
		return '\0';
	return scanner->current[1];
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
static void skip_whitespace(Scanner *scanner)
{
	for (;;) {
		const char c = peek(scanner);
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			advance(scanner);
			break;
		case '\n': {
			scanner->line++;
			advance(scanner);
			break;
		}
		case '/': {
			if (peek_next(scanner) == '/') {
				// Comments are only on one line
				while (peek(scanner) != '\n' && !is_at_end(scanner))
					advance(scanner);
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

static CruxTokenType check_keyword(Scanner *scanner, const int start, const int length, const char *rest,
								   const CruxTokenType type)
{
	if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0) {
		return type;
	}
	return CRUX_TOKEN_IDENTIFIER;
}

/**
 * Determines the token type of identifier.
 * @return The token type (keyword or identifier)
 */
static CruxTokenType identifier_type(Scanner *scanner)
{
	switch (scanner->start[0]) {
		// type annotations
	case 'N': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'i':
				return check_keyword(scanner, 2, 1, "l", CRUX_TOKEN_NIL_TYPE);
			case 'e':
				return check_keyword(scanner, 2, 3, "ver", CRUX_TOKEN_NEVER_TYPE);
			}
		}
		break;
	}
	case 'B': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 'o':
				return check_keyword(scanner, 2, 2, "ol", CRUX_TOKEN_BOOL_TYPE);
			case 'u':
				return check_keyword(scanner, 2, 4, "ffer", CRUX_TOKEN_BUFFER_TYPE);
			}
		}
		break;
	}
	case 'I': {
		return check_keyword(scanner, 1, 2, "nt", CRUX_TOKEN_INT_TYPE);
	}
	case 'F': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 'i':
				return check_keyword(scanner, 2, 2, "le", CRUX_TOKEN_FILE_TYPE);
			case 'l':
				return check_keyword(scanner, 2, 3, "oat", CRUX_TOKEN_FLOAT_TYPE);
			}
		}
		break;
	}
	case 'C': {
		return check_keyword(scanner, 1, 6, "omplex", CRUX_TOKEN_COMPLEX_TYPE);
	}
	case 'S': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 't':
				return check_keyword(scanner, 2, 4, "ring", CRUX_TOKEN_STRING_TYPE);
			case 'e':
				return check_keyword(scanner, 2, 1, "t", CRUX_TOKEN_SET_TYPE);
			}
		}
		break;
	}
	case 'T': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 'u':
				return check_keyword(scanner, 2, 3, "ple", CRUX_TOKEN_TUPLE_TYPE);
			case 'a':
				return check_keyword(scanner, 2, 3, "ble", CRUX_TOKEN_TABLE_TYPE);
			}
		}
		break;
	}
	case 'R': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 'a': {
				if (scanner->current - scanner->start > 3) {
					switch (scanner->start[2]) {
					case 'n':
						if (scanner->current - scanner->start > 4) {
							switch (scanner->start[3]) {
							case 'g':
								return check_keyword(scanner, 4, 1, "e", CRUX_TOKEN_RANGE_TYPE);
							case 'd':
								return check_keyword(scanner, 4, 2, "om", CRUX_TOKEN_RANDOM_TYPE);
							}
						}
					}
				}
				break;
			}
			case 'e': {
				return check_keyword(scanner, 2, 4, "sult", CRUX_TOKEN_RESULT_TYPE);
			}
			}
		}
		break;
	}
	case 'A': {
		if (scanner->current - scanner->start > 2) {
			switch (scanner->start[1]) {
			case 'n':
				return check_keyword(scanner, 2, 1, "y", CRUX_TOKEN_ANY_TYPE);
			case 'r':
				return check_keyword(scanner, 2, 3, "ray", CRUX_TOKEN_ARRAY_TYPE);
			}
		}
		break;
	}
	case 'V': {
		return check_keyword(scanner, 1, 5, "ector", CRUX_TOKEN_VECTOR_TYPE);
	}
	case 'M': {
		return check_keyword(scanner, 1, 5, "atrix", CRUX_TOKEN_MATRIX_TYPE);
	}
	// language keywords
	case 'a': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 's': {
				return check_keyword(scanner, 2, 0, "", CRUX_TOKEN_AS);
			}
			case 'n': {
				return check_keyword(scanner, 2, 1, "d", CRUX_TOKEN_AND);
			}
			default:;
			}
		}
		break;
	}
	case 'b':
		return check_keyword(scanner, 1, 4, "reak", CRUX_TOKEN_BREAK);
	case 'c': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'o':
				return check_keyword(scanner, 2, 6, "ntinue", CRUX_TOKEN_CONTINUE);
			default:;
			}
		}
		break;
	}
	case 'd': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'y': {
				return check_keyword(scanner, 2, 4, "nuse", CRUX_TOKEN_DYN_USE);
			}
			case 'e': {
				return check_keyword(scanner, 2, 5, "fault", CRUX_TOKEN_DEFAULT);
			}
			default:;
			}
		}
		break;
	}
	case 'e':
		return check_keyword(scanner, 1, 3, "lse", CRUX_TOKEN_ELSE);
	case 'g': {
		return check_keyword(scanner, 1, 3, "ive", CRUX_TOKEN_GIVE);
	}
	case 'i':
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'm': {
				return check_keyword(scanner, 2, 2, "pl", CRUX_TOKEN_IMPL);
			}
			case 'f': {
				return CRUX_TOKEN_IF;
			}
			default:;
			}
		}
		return check_keyword(scanner, 1, 1, "f", CRUX_TOKEN_IF);
	case 'l':
		return check_keyword(scanner, 1, 2, "et", CRUX_TOKEN_LET);
	case 'n':
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'o': {
				return check_keyword(scanner, 2, 1, "t", CRUX_TOKEN_NOT);
			}
			case 'i': {
				return check_keyword(scanner, 2, 1, "l", CRUX_TOKEN_NIL);
			}
			case 'e': {
				return check_keyword(scanner, 2, 1, "w", CRUX_TOKEN_NEW);
			}
			default:;
			}
			break;
		}
		break;
	case 'o':
		return check_keyword(scanner, 1, 1, "r", CRUX_TOKEN_OR);
	case 'r':
		return check_keyword(scanner, 1, 5, "eturn", CRUX_TOKEN_RETURN);
	case 's': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 't': {
				return check_keyword(scanner, 2, 4, "ruct", CRUX_TOKEN_STRUCT);
			}
			case 'h': {
				return check_keyword(scanner, 2, 3, "ape", CRUX_TOKEN_SHAPE);
			}
			default:;
			}
		}
		break;
	}
	case 'w':
		return check_keyword(scanner, 1, 4, "hile", CRUX_TOKEN_WHILE);
	case 'f': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'a':
				return check_keyword(scanner, 2, 3, "lse", CRUX_TOKEN_FALSE);
			case 'o':
				return check_keyword(scanner, 2, 1, "r", CRUX_TOKEN_FOR);
			case 'n':
				return CRUX_TOKEN_FN;
			case 'r':
				return check_keyword(scanner, 2, 2, "om", CRUX_TOKEN_FROM);
			default:;
			}
		}
		break;
	}
	case 'm': {
		return check_keyword(scanner, 1, 4, "atch", CRUX_TOKEN_MATCH);
	}
	case 't': {
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'r':
				return check_keyword(scanner, 2, 2, "ue", CRUX_TOKEN_TRUE);
			case 'y':
				if (scanner->current - scanner->start > 2) {
					switch (scanner->start[2]) {
					case 'p':
						if (scanner->current - scanner->start > 3) {
							if (scanner->start[3] == 'e') {
								if (scanner->current - scanner->start == 4)
									return CRUX_TOKEN_TYPE;
								return check_keyword(scanner, 4, 2, "of", CRUX_TOKEN_TYPEOF);
							}
						}
						break;
					}
				}
				break;
			}
		}
		break;
	}
	case 'u':
		return check_keyword(scanner, 1, 2, "se", CRUX_TOKEN_USE);
	case 'p':
		if (scanner->current - scanner->start > 1) {
			switch (scanner->start[1]) {
			case 'a':
				return check_keyword(scanner, 2, 3, "nic", CRUX_TOKEN_PANIC);
			case 'u':
				return check_keyword(scanner, 2, 1, "b", CRUX_TOKEN_PUB);
			default:;
			}
		}
		break;
	case 'E':
		return check_keyword(scanner, 1, 2, "rr", CRUX_TOKEN_ERR);
	case 'O':
		return check_keyword(scanner, 1, 1, "k", CRUX_TOKEN_OK);
	default:;
	}

	return CRUX_TOKEN_IDENTIFIER;
}

void init_scanner(Scanner *scanner, const char *source)
{
	scanner->start = source;
	scanner->current = source;
	scanner->line = 1;
}

/**
 * Creates a token of the given type from the current lexeme.
 * @param type The token type
 * @return The created token
 */
static Token make_token(Scanner *scanner, const CruxTokenType type)
{
	Token token;
	token.type = type;
	token.start = scanner->start;
	token.length = (int)(scanner->current - scanner->start);
	token.line = scanner->line;
	return token;
}

/**
 * Creates an error token with the given message.
 * @param message The error message
 * @return The error token
 */
static Token error_token(Scanner *scanner, const char *message)
{
	Token token;
	token.type = CRUX_TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner->line;
	return token;
}

/**
 * Checks if the current character matches the expected one and
 * advances if it does.
 * @param expected The character to match
 * @return true if the character matches, false otherwise
 */
static bool match(Scanner *scanner, const char expected)
{
	if (is_at_end(scanner))
		return false;
	if (*scanner->current != expected)
		return false;
	scanner->current++;
	return true;
}

/**
 * Scans a single-quoted string literal.
 * @return The string token or an error token
 */
static Token single_string(Scanner *scanner)
{
	while (!is_at_end(scanner)) {
		if (peek(scanner) == '\'')
			break;
		if (peek(scanner) == '\\') {
			advance(scanner);
			if (!is_at_end(scanner)) {
				advance(scanner);
			}
			continue;
		}
		if (peek(scanner) == '\n') {
			scanner->line++;
		}
		advance(scanner);
	}

	if (is_at_end(scanner))
		return error_token(scanner, "Unterminated String");
	// The closing quote
	advance(scanner);
	return make_token(scanner, CRUX_TOKEN_STRING);
}

/**
 * Scans a double-quoted string literal.
 * @return The string token or an error token
 */
static Token double_string(Scanner *scanner)
{
	while (!is_at_end(scanner)) {
		if (peek(scanner) == '"')
			break;
		if (peek(scanner) == '\\') {
			advance(scanner);
			if (!is_at_end(scanner)) {
				advance(scanner);
			}
			continue;
		}
		if (peek(scanner) == '\n') {
			scanner->line++;
		}
		advance(scanner);
	}

	if (is_at_end(scanner))
		return error_token(scanner, "Unterminated String");
	// The closing quote
	advance(scanner);
	return make_token(scanner, CRUX_TOKEN_STRING);
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
 * @return The numeric token (CRUX_TOKEN_INT or CRUX_TOKEN_FLOAT)
 */
static Token number(Scanner *scanner)
{
	while (is_digit(peek(scanner)))
		advance(scanner);
	bool fpFound = false;
	if (peek(scanner) == '.' && is_digit(peek_next(scanner))) {
		fpFound = true;
		advance(scanner);
		while (is_digit(peek(scanner)))
			advance(scanner);
	}
	if (fpFound) {
		return make_token(scanner, CRUX_TOKEN_FLOAT);
	}
	return make_token(scanner, CRUX_TOKEN_INT);
}

/**
 * Scans an identifier or keyword.
 * @return The identifier or keyword token
 */
static Token identifier(Scanner *scanner)
{
	while (is_alpha(peek(scanner)) || is_digit(peek(scanner)))
		advance(scanner);
	return make_token(scanner, identifier_type(scanner));
}

Token scan_token(Scanner *scanner)
{
	skip_whitespace(scanner);
	scanner->start = scanner->current;
	if (is_at_end(scanner))
		return make_token(scanner, CRUX_TOKEN_EOF);

	const char c = advance(scanner);

	if (is_digit(c))
		return number(scanner);
	if (is_identifier_starter(c))
		return identifier(scanner);

	switch (c) {
	case ':':
		return make_token(scanner, CRUX_TOKEN_COLON);
	case '(':
		return make_token(scanner, CRUX_TOKEN_LEFT_PAREN);
	case ')':
		return make_token(scanner, CRUX_TOKEN_RIGHT_PAREN);
	case '{':
		return make_token(scanner, CRUX_TOKEN_LEFT_BRACE);
	case '}':
		return make_token(scanner, CRUX_TOKEN_RIGHT_BRACE);
	case '[':
		return make_token(scanner, CRUX_TOKEN_LEFT_SQUARE);
	case ']':
		return make_token(scanner, CRUX_TOKEN_RIGHT_SQUARE);
	case '$':
		if (match(scanner, '{')) {
			return make_token(scanner, CRUX_TOKEN_DOLLAR_LEFT_BRACE);
		}
		if (match(scanner, '[')) {
			return make_token(scanner, CRUX_TOKEN_DOLLAR_LEFT_SQUARE);
		}
		return error_token(scanner, "Unexpected token");
	case ';':
		return make_token(scanner, CRUX_TOKEN_SEMICOLON);
	case ',':
		return make_token(scanner, CRUX_TOKEN_COMMA);
	case '.':
		if (match(scanner, '.')) {
			return make_token(scanner, CRUX_TOKEN_DOT_DOT);
		}
		return make_token(scanner, CRUX_TOKEN_DOT);
	case '-':
		if (match(scanner, '>'))
			return make_token(scanner, CRUX_TOKEN_ARROW);
		return make_token(scanner, match(scanner, '=') ? CRUX_TOKEN_MINUS_EQUAL : CRUX_TOKEN_MINUS);
	case '+':
		return make_token(scanner, match(scanner, '=') ? CRUX_TOKEN_PLUS_EQUAL : CRUX_TOKEN_PLUS);
	case '/':
		return make_token(scanner, match(scanner, '=') ? CRUX_TOKEN_SLASH_EQUAL : CRUX_TOKEN_SLASH);
	case '\\':
		if (match(scanner, '=')) {
			return make_token(scanner, CRUX_TOKEN_BACK_SLASH_EQUAL);
		}
		return make_token(scanner, CRUX_TOKEN_BACKSLASH);
	case '*': {
		if (match(scanner, '*')) {
			return make_token(scanner, CRUX_TOKEN_STAR_STAR);
		}
		if (match(scanner, '=')) {
			return make_token(scanner, CRUX_TOKEN_STAR_EQUAL);
		}
		return make_token(scanner, CRUX_TOKEN_STAR);
	}
	case '%':
		if (match(scanner, '=')) {
			return make_token(scanner, CRUX_TOKEN_PERCENT_EQUAL);
		}
		return make_token(scanner, CRUX_TOKEN_PERCENT);
	case '!': {
		if (match(scanner, '=')) {
			return make_token(scanner, CRUX_TOKEN_BANG_EQUAL);
		}
		return error_token(scanner, "Unexpected token");
	}
	case '=': {
		if (match(scanner, '=')) {
			return make_token(scanner, CRUX_TOKEN_EQUAL_EQUAL);
		}
		if (match(scanner, '>')) {
			return make_token(scanner, CRUX_TOKEN_EQUAL_ARROW);
		}
		return make_token(scanner, CRUX_TOKEN_EQUAL);
	}
	case '<':
		if (match(scanner, '<')) {
			return make_token(scanner, CRUX_TOKEN_LEFT_SHIFT);
		}
		return make_token(scanner, match(scanner, '=') ? CRUX_TOKEN_LESS_EQUAL : CRUX_TOKEN_LESS);
	case '>':
		if (match(scanner, '>')) {
			return make_token(scanner, CRUX_TOKEN_RIGHT_SHIFT);
		}
		return make_token(scanner, match(scanner, '=') ? CRUX_TOKEN_GREATER_EQUAL : CRUX_TOKEN_GREATER);
	case '"':
		return double_string(scanner);
	case '\'':
		return single_string(scanner);
	case '?':
		return make_token(scanner, CRUX_TOKEN_QUESTION_MARK);
	case '&':
		return make_token(scanner, CRUX_TOKEN_AMPERSAND);
	case '|':
		return make_token(scanner, CRUX_TOKEN_PIPE);
	case '^':
		return make_token(scanner, CRUX_TOKEN_CARET);
	case '~':
		return make_token(scanner, CRUX_TOKEN_TILDE);
	default:;
	}
	return error_token(scanner, "Unexpected character.");
}
