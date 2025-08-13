#include "debug.h"
#include "chunk.h"
#include "value.h"

#include <stdio.h>

#include "object.h"

void disassembleChunk(const Chunk *chunk, const char *name)
{
	printf("======= %s =======\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
		// This changes the size of offset, because instructions can
		// have different sizes
	}
}

/**
 * @brief Formats and prints a simple instruction (with no operands)
 *
 * Prints the instruction name and returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 1)
 */
static int simpleInstruction(const char *name, const int offset)
{
	printf(" %s\n", name);
	return offset + 1;
}

/**
 * @brief Formats and prints an instruction with a single byte operand
 *
 * Prints the instruction name and its byte operand, then returns
 * the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 2)
 */
static int byteInstruction(const char *name, const Chunk *chunk,
			   const int offset)
{
	const uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

/**
 * @brief Formats and prints a jump instruction with a two-byte operand
 *
 * Decodes the two-byte jump offset, calculates and prints the target offset,
 * then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param sign Direction of the jump (1 for forward, -1 for backward)
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 3)
 */
static int jumpInstruction(const char *name, const int sign, const Chunk *chunk,
			   const int offset)
{
	uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
	jump |= chunk->code[offset + 2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

/**
 * @brief Formats and prints an instruction with a constant operand
 *
 * Prints the instruction name, the constant index, and the constant's value,
 * then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 2)
 */
static int constantInstruction(const char *name, const Chunk *chunk,
			       const int offset)
{
	const uint8_t constant =
		chunk->code[offset + 1]; // Get the constant index
	printf("%-16s %4d '", name, constant); // Print the name of the opcode
	printValue(chunk->constants.values[constant],
		   false); // print the constant's value
	printf("'\n");
	return offset + 2; // +2 because OP_CONSTANT is two bytes
}

/**
 * @brief Formats and prints a method invocation instruction
 *
 * Prints the instruction name, argument count, constant index, and the method
 * name, then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 3)
 */
static int invokeInstruction(const char *name, const Chunk *chunk,
			     const int offset)
{
	const uint8_t constant = chunk->code[offset + 1];
	const uint8_t argCount = chunk->code[offset + 2];
	printf("%-16s (%d args) %4d '", name, argCount, constant);
	printValue(chunk->constants.values[constant], false);
	printf("'\n");
	return offset + 3;
}

int disassembleInstruction(const Chunk *chunk, int offset)
{
	printf("%04d",
	       offset); // Prints the byte offset of the given instruction

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	} else {
		printf("%4d ", chunk->lines[offset]);
	}

	const uint8_t instruction =
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
	case OP_NOT_EQUAL: {
		return simpleInstruction("OP_NOT_EQUAL", offset);
	}
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
	case OP_MODULUS:
		return simpleInstruction("OP_MODULUS", offset);
	case OP_LEFT_SHIFT:
		return simpleInstruction("OP_LEFT_SHIFT", offset);
	case OP_RIGHT_SHIFT:
		return simpleInstruction("OP_RIGHT_SHIFT", offset);
	case OP_POP:
		return simpleInstruction("OP_POP", offset);
	case OP_DEFINE_GLOBAL:
		return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
	case OP_GET_GLOBAL:
		return constantInstruction("OP_GET_GLOBAL", chunk, offset);
	case OP_SET_GLOBAL:
		return constantInstruction("OP_SET_GLOBAL", chunk, offset);
	case OP_GET_LOCAL:
		return byteInstruction("OP_GET_LOCAL", chunk, offset);
	case OP_SET_LOCAL:
		return byteInstruction("OP_SET_LOCAL", chunk, offset);
	case OP_JUMP:
		return jumpInstruction("OP_JUMP", 1, chunk, offset);
	case OP_JUMP_IF_FALSE:
		return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
	case OP_LOOP:
		return jumpInstruction("OP_LOOP", -1, chunk, offset);
	case OP_CALL:
		return byteInstruction("OP_CALL", chunk, offset);
	case OP_GET_UPVALUE:
		return byteInstruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byteInstruction("OP_SET_UPVALUE", chunk, offset);
	case OP_CLOSURE: {
		offset++;
		const uint8_t constant = chunk->code[offset++];
		printf("%-16s %4d ", "OP_CLOSURE", constant);
		printValue(chunk->constants.values[constant], false);
		printf("\n");

		const ObjectFunction *function = AS_CRUX_FUNCTION(
			chunk->constants.values[constant]);
		for (int j = 0; j < function->upvalueCount; j++) {
			const int isLocal = chunk->code[offset++];
			const int index = chunk->code[offset++];
			printf("%04d        |                %s %d\n",
			       offset - 2, isLocal ? "local" : "upvalue",
			       index);
		}
		return offset;
	}
	case OP_CLOSE_UPVALUE:
		return simpleInstruction("OP_CLOSE_UPVALUE", offset);
	case OP_GET_PROPERTY:
		return constantInstruction("OP_GET_PROPERTY", chunk, offset);
	case OP_SET_PROPERTY:
		return constantInstruction("OP_SET_PROPERTY", chunk, offset);
	case OP_ARRAY:
		return constantInstruction("OP_ARRAY", chunk, offset);
	case OP_TABLE:
		return constantInstruction("OP_TABLE", chunk, offset);
	case OP_GET_COLLECTION:
		return constantInstruction("OP_GET_COLLECTION", chunk, offset);
	case OP_SET_COLLECTION:
		return constantInstruction("OP_SET_COLLECTION", chunk, offset);
	case OP_INVOKE:
		return invokeInstruction("OP_INVOKE", chunk, offset);
	case OP_SET_LOCAL_SLASH: {
		return simpleInstruction("OP_SET_LOCAL_SLASH", offset);
	}
	case OP_SET_LOCAL_STAR: {
		return simpleInstruction("OP_SET_LOCAL_STAR", offset);
	}
	case OP_SET_LOCAL_PLUS: {
		return simpleInstruction("OP_SET_LOCAL_PLUS", offset);
	}
	case OP_SET_LOCAL_MINUS: {
		return simpleInstruction("OP_SET_LOCAL_MINUS", offset);
	}
	case OP_SET_UPVALUE_SLASH: {
		return simpleInstruction("OP_SET_UPVALUE_SLASH", offset);
	}
	case OP_SET_UPVALUE_STAR: {
		return simpleInstruction("OP_SET_UPVALUE_STAR", offset);
	}
	case OP_SET_UPVALUE_PLUS: {
		return simpleInstruction("OP_SET_UPVALUE_PLUS", offset);
	}
	case OP_SET_UPVALUE_MINUS: {
		return simpleInstruction("OP_SET_UPVALUE_MINUS", offset);
	}
	case OP_SET_GLOBAL_SLASH: {
		return simpleInstruction("OP_SET_GLOBAL_SLASH", offset);
	}
	case OP_SET_GLOBAL_STAR: {
		return simpleInstruction("OP_SET_GLOBAL_STAR", offset);
	}
	case OP_SET_GLOBAL_PLUS: {
		return simpleInstruction("OP_SET_GLOBAL_PLUS", offset);
	}
	case OP_SET_GLOBAL_MINUS: {
		return simpleInstruction("OP_SET_GLOBAL_MINUS", offset);
	}
	case OP_SET_GLOBAL_INT_DIVIDE: {
		return simpleInstruction("OP_SET_GLOBAL_INT_DIVIDE", offset);
	}
	case OP_SET_GLOBAL_MODULUS: {
		return simpleInstruction("OP_SET_GLOBAL_MODULUS", offset);
	}
	case OP_SET_LOCAL_INT_DIVIDE: {
		return simpleInstruction("OP_SET_LOCAL_INT_DIVIDE", offset);
	}
	case OP_SET_LOCAL_MODULUS: {
		return simpleInstruction("OP_SET_LOCAL_MODULUS", offset);
	}
	case OP_SET_UPVALUE_INT_DIVIDE: {
		return simpleInstruction("OP_SET_UPVALUE_INT_DIVIDE", offset);
	}
	case OP_SET_UPVALUE_MODULUS: {
		return simpleInstruction("OP_SET_UPVALUE_MODULUS", offset);
	}
	case OP_ANON_FUNCTION: {
		return constantInstruction("OP_ANON_FUNCTION", chunk, offset);
	}
	case OP_USE_MODULE: {
		const uint8_t nameCount = chunk->code[offset + 1];
		printf("%-16s %4d name(s) from ", "OP_USE_MODULE", nameCount);
		printValue(chunk->constants
				   .values[chunk->code[offset + nameCount + 2]],
			   false);
		printf("\n");
		return offset + nameCount + 3;
	}
	case OP_PUB: {
		return simpleInstruction("OP_PUB", offset);
	}
	case OP_MATCH: {
		return simpleInstruction("OP_MATCH", offset);
	}
	case OP_MATCH_JUMP: {
		return jumpInstruction("OP_MATCH_JUMP", 1, chunk, offset);
	}
	case OP_MATCH_END: {
		return simpleInstruction("OP_MATCH_END", offset);
	}
	case OP_RESULT_MATCH_OK: {
		return simpleInstruction("OP_RESULT_MATCH_OK", offset);
	}
	case OP_RESULT_MATCH_ERR: {
		return simpleInstruction("OP_RESULT_MATCH_ERR", offset);
	}
	case OP_RESULT_BIND: {
		return constantInstruction("OP_RESULT_BIND", chunk, offset);
	}
	case OP_GIVE: {
		return simpleInstruction("OP_GIVE", offset);
	}
	case OP_INT_DIVIDE: {
		return simpleInstruction("OP_INT_DIVIDE", offset);
	}
	case OP_POWER: {
		return simpleInstruction("OP_POWER", offset);
	}
	case OP_USE_NATIVE: {
		return simpleInstruction("OP_USE_NATIVE", offset);
	}
	case OP_FINISH_USE: {
		return simpleInstruction("OP_FINISH_USE", offset);
	}
	case OP_STRUCT: {
		return simpleInstruction("OP_STRUCT", offset);
	}
	case OP_STRUCT_16: {
		return simpleInstruction("OP_STRUCT_16", offset);
	}
	case OP_STATIC_ARRAY: {
		return simpleInstruction("OP_STATIC_ARRAY", offset);
	}
	case OP_STATIC_TABLE: {
		return simpleInstruction("OP_STATIC_TABLE", offset);
	}
	case OP_STRUCT_INSTANCE_START: {
		return simpleInstruction("OP_STRUCT_INSTANCE_START", offset);
	}
	case OP_STRUCT_NAMED_FIELD: {
		return simpleInstruction("OP_STRUCT_NAMED_FIELD", offset);
	}
	case OP_STRUCT_NAMED_FIELD_16: {
		return simpleInstruction("OP_STRUCT_NAMED_FIELD_16", offset);
	}
	case OP_STRUCT_INSTANCE_END: {
		return simpleInstruction("OP_STRUCT_INSTANCE_END", offset);
	}
	case OP_NIL_RETURN: {
		return simpleInstruction("OP_NIL_RETURN", offset);
	}

	case OP_TYPEOF: {
		return simpleInstruction("OP_TYPEOF", offset);
	}

	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
