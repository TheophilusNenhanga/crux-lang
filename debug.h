#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk *chunk, const char *name);

int disassembleInstruction(Chunk *chunk, int offset);
bool verifyNumbers(Value a, Value b, const char *operation);
#endif // DEBUG_H
