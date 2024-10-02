//
// Created by theon on 08/09/2024.
//

#include "debug.h"
#include "value.h"

#include <stdio.h>

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name); // So we can tell which chunk we are looking at

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
    // This changes the size of offset, because instructions can have different
    // sizes
  }
}

static int simpleInstruction(const char *name, const int offset) {
  printf(" %s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, const Chunk *chunk,
                               const int offset) {
  uint8_t constant = chunk->code[offset + 1];    // Get the constant index
  printf("%-16s %4d '", name, constant);         // Print the name of the opcode
  printValue(chunk->constants.values[constant]); // print the constant's value
  printf("'\n");
  return offset + 2; // +2 because OP_CONSTANT is two bytes
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d", offset); // Prints the byte offset of the given instruction

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruction =
      chunk->code[offset]; // read a single byte, that is the opcode

  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_LESS_EQUAL:
    return simpleInstruction("OP_LESS_EQUAL", offset);
  case OP_GREATER_EQUAL:
    return simpleInstruction("OP_GREATER_EQUAL", offset);
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_DEFINE_GLOBAL:
    return simpleInstruction("OP_DEFINE_GLOBAL", offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL_CONSTANT:
    return simpleInstruction("OP_DEFINE_GLOBAL_CONSTANT", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
