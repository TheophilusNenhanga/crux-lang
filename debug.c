//
// Created by theon on 08/09/2024.
//

#include "debug.h"
#include "value.h"

#include <stdio.h>

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name); // So we can tell which chunk we are looking at

    for (int offset = 0; offset < chunk -> count;) {
        offset = disassembleInstruction(chunk, offset); // This changes the size of offset, because instructions can have different sizes
    }
}

static int simpleInstruction(const char* name, int offset) {
    printf(" %s\n", name);
    return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk -> code[offset+1]; // Get the constant index
    printf("%-16s %4d '", name, constant); // Print the name of the opcode
    printValue(chunk->constants.values[constant]); // print the constant's value
    printf("'\n");
    return offset + 2; // +2 because OP_CONSTANT is two bytes
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d", offset); // Prints the byte offset of the given instruction

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset -1]) {
        printf("   | ");
    }else {
        printf("%4d ", chunk ->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset]; // read a single byte, that is the opcode

    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
