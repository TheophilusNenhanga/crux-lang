//
// Created by theon on 08/09/2024.
//

#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif //DEBUG_H
