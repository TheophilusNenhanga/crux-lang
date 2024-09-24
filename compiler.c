//
// Created by theon on 11/09/2024.
//

#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif


typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

// Precedence in order from lowest to highest
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser; // So we do not need to pass this around from function to function
Chunk *compilingChunk;

// Forward declarations
static void expression();

static void parsePrecedence(Precedence precedence);

static ParseRule *getRule(TokenType type);

static void binary();

static void unary();

static void grouping();

static void number();


static Chunk *currentChunk() {
    return compilingChunk;
}

static void errorAt(Token *token, const char *message) {
    if (parser.panicMode) return; // if in panic mode do not show more errors (avoid cascading errors)
    parser.panicMode = true; // if we see an error, enter panic mode
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message); // Parser.current is used to tell the user where the error was found
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}


/**
 * Reads the next token. Validates that the token has the expected type. If not reports an error.
 */
static void consume(const TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static void emitByte(const uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(const uint8_t byte1, const uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(const Value value) {
    const int constant = addConstant(currentChunk(), value);
    // Add constant adds the given value to the end of the constant table and returns the index.
    if (constant > UINT8_MAX) {
        // Only 256 constants can be stored and loaded in a chunk.
        // TODO: Add new instruction 'OP_CONSTANT_16' to store two byte operands that can handle more constants when needed

        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t) constant;
}

static void emitConstant(const Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}


static void binary() {
    const TokenType operatorType = parser.previous.type;
    const ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS: emitByte(OP_ADD);
            break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE);
            break;
        default: return; // unreachable
    }
}


static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number() {
    const double value = strtod(parser.previous.start, NULL);

    // TODO: Try this
    //    Check if the value is an integer
    //    if (value == (int64_t)value) {
    //        // It's an integer
    //        int64_t intValue = (int64_t)value;
    //        emitConstant(NUMBER_VAL(intValue));
    //    } else {
    //        // It's a floating-point number
    //        emitConstant(NUMBER_VAL(value));
    //    }


    emitConstant(value);
}


static void unary() {
    const TokenType operatorType = parser.previous.type;

    // compile the operand
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE);
            break;
        default: return; // unreachable
    }
}

// Prefix Infix Precedence
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_INT] = {number, NULL, PREC_NONE},
    [TOKEN_FLOAT] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_SELF] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};


/**
starts at the current token and parses any expression at the given precedence or higher
*/
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expected expression.");
        return;
    }
    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static ParseRule *getRule(TokenType type) {
    return &rules[type]; // Returns the rule at the given index
}

bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expected end of expression.");
    endCompiler();
    return !parser.hadError;
}
