#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"

/**
 * @brief Disassembles and prints all instructions in a chunk
 *
 * Prints a header with the chunk name and then iteratively disassembles
 * each instruction in the chunk, printing human-readable representations.
 *
 * @param chunk Pointer to the Chunk to disassemble
 * @param name Name of the chunk for identification in output
 */
void disassembleChunk(const Chunk *chunk, const char *name);

/**
 * @brief Disassembles and prints a single bytecode instruction
 *
 * Prints the byte offset, source line number, and a human-readable
 * representation of the instruction at the given offset in the chunk.
 * Line numbers are displayed as "|" if they match the previous instruction's
 * line.
 *
 * The function dispatches to specialized formatting functions based on the
 * instruction type (simple, constant, byte, jump, or invoke instructions).
 *
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The byte offset of the instruction to disassemble
 * @return The byte offset of the next instruction
 */
int disassembleInstruction(const Chunk *chunk, int offset);

#endif // DEBUG_H
