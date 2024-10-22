#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
	OP_RETURN,
	OP_CONSTANT,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_NEGATE,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER_EQUAL,
	OP_NOT_EQUAL,
	OP_ADD,
	OP_NOT,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_PRINT,
	OP_POP,
	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_LOOP,
	OP_CALL,
	OP_CLOSURE,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_CLOSE_UPVALUE,
	OP_CLASS,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_INVOKE,
	OP_METHOD,
	OP_INHERIT,
	OP_GET_SUPER,
	OP_SUPER_INVOKE,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t *code;
	int *lines;
	ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);

void freeChunk(Chunk *chunk);

int addConstant(Chunk *chunk, Value value);

#endif // CHUNK_H
