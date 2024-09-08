//
// Created by theon on 08/09/2024.
//

#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_RETURN, // Return from the current function
    OP_CONSTANT,
} OpCode;


typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
}Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line); // Can write opcodes or operands
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value value);

#endif //CHUNK_H
