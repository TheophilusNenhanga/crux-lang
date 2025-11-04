#include <stdio.h>

#include "debug.h"
#include "chunk.h"
#include "value.h"
#include "object.h"

void disassemble_chunk(const Chunk *chunk, const char *name)
{
	printf("======= %s =======\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = disassemble_instruction(chunk, offset);
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
static int simple_instruction(const char *name, const int offset)
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
static int byte_instruction(const char *name, const Chunk *chunk,
			    const int offset)
{
	const uint16_t slot = chunk->code[offset + 1];
	printf("%-16s %6d\n", name, slot);
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
static int jump_instruction(const char *name, const int sign,
			    const Chunk *chunk, const int offset)
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
static int constant_instruction(const char *name, const Chunk *chunk,
				const int offset)
{
	const uint16_t constant =
		chunk->code[offset + 1]; // Get the constant index
	printf("%-16s %4d '", name, constant); // Print the name of the opcode
	print_value(chunk->constants.values[constant],
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
static int invoke_instruction(const char *name, const Chunk *chunk,
			      const int offset)
{
	const uint16_t constant = chunk->code[offset + 1];
	const uint16_t arg_count = chunk->code[offset + 2];
	printf("%-16s (%d args) %4d '", name, arg_count, constant);
	print_value(chunk->constants.values[constant], false);
	printf("'\n");
	return offset + 3;
}

int disassemble_instruction(const Chunk *chunk, int offset)
{
	printf("%04d",
	       offset); // Prints the byte offset of the given instruction

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	} else {
		printf("%4d ", chunk->lines[offset]);
	}

	const OpCode instruction =
		chunk->code[offset]; // read a single byte, that is the opcode

	switch (instruction) {
	case OP_RETURN:
		return simple_instruction("OP_RETURN", offset);
	case OP_CONSTANT:
		return constant_instruction("OP_CONSTANT", chunk, offset);
	case OP_NEGATE:
		return simple_instruction("OP_NEGATE", offset);
	case OP_NIL:
		return simple_instruction("OP_NIL", offset);
	case OP_TRUE:
		return simple_instruction("OP_TRUE", offset);
	case OP_FALSE:
		return simple_instruction("OP_FALSE", offset);
	case OP_ADD:
		return simple_instruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simple_instruction("OP_SUBTRACT", offset);
	case OP_MULTIPLY:
		return simple_instruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simple_instruction("OP_DIVIDE", offset);
	case OP_NOT:
		return simple_instruction("OP_NOT", offset);
	case OP_NOT_EQUAL: {
		return simple_instruction("OP_NOT_EQUAL", offset);
	}
	case OP_EQUAL:
		return simple_instruction("OP_EQUAL", offset);
	case OP_GREATER:
		return simple_instruction("OP_GREATER", offset);
	case OP_LESS:
		return simple_instruction("OP_LESS", offset);
	case OP_LESS_EQUAL:
		return simple_instruction("OP_LESS_EQUAL", offset);
	case OP_GREATER_EQUAL:
		return simple_instruction("OP_GREATER_EQUAL", offset);
	case OP_MODULUS:
		return simple_instruction("OP_MODULUS", offset);
	case OP_LEFT_SHIFT:
		return simple_instruction("OP_LEFT_SHIFT", offset);
	case OP_RIGHT_SHIFT:
		return simple_instruction("OP_RIGHT_SHIFT", offset);
	case OP_POP:
		return simple_instruction("OP_POP", offset);
	case OP_DEFINE_GLOBAL:
		return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
	case OP_GET_GLOBAL:
		return constant_instruction("OP_GET_GLOBAL", chunk, offset);
	case OP_SET_GLOBAL:
		return constant_instruction("OP_SET_GLOBAL", chunk, offset);
	case OP_GET_LOCAL:
		return byte_instruction("OP_GET_LOCAL", chunk, offset);
	case OP_SET_LOCAL:
		return byte_instruction("OP_SET_LOCAL", chunk, offset);
	case OP_JUMP:
		return jump_instruction("OP_JUMP", 1, chunk, offset);
	case OP_JUMP_IF_FALSE:
		return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
	case OP_LOOP:
		return jump_instruction("OP_LOOP", -1, chunk, offset);
	case OP_CALL:
		return byte_instruction("OP_CALL", chunk, offset);
	case OP_GET_UPVALUE:
		return byte_instruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byte_instruction("OP_SET_UPVALUE", chunk, offset);
	case OP_CLOSURE: {
		/* TODO: FIX SEG FAULT */
		offset++;
		return offset;
		// const uint16_t constant = chunk->code[offset++];
		// printf("%-16s %4d ", "OP_CLOSURE", constant);
		// print_value(chunk->constants.values[constant], false);
		// printf("\n");
		//
		// const ObjectFunction *function = AS_CRUX_FUNCTION(
		// 	chunk->constants.values[constant]);
		// for (int j = 0; j < function->upvalue_count; j++) {
		// 	const int isLocal = chunk->code[offset++];
		// 	const int index = chunk->code[offset++];
		// 	printf("%04d        |                %s %d\n",
		// 	       offset - 2, isLocal ? "local" : "upvalue",
		// 	       index);
		// }
		// return offset;
	}
	case OP_CLOSE_UPVALUE:
		return simple_instruction("OP_CLOSE_UPVALUE", offset);
	case OP_GET_PROPERTY:
		return constant_instruction("OP_GET_PROPERTY", chunk, offset);
	case OP_SET_PROPERTY:
		return constant_instruction("OP_SET_PROPERTY", chunk, offset);
	case OP_ARRAY:
		return constant_instruction("OP_ARRAY", chunk, offset);
	case OP_TABLE:
		return constant_instruction("OP_TABLE", chunk, offset);
	case OP_GET_COLLECTION:
		return constant_instruction("OP_GET_COLLECTION", chunk, offset);
	case OP_SET_COLLECTION:
		return constant_instruction("OP_SET_COLLECTION", chunk, offset);
	case OP_INVOKE:
		return invoke_instruction("OP_INVOKE", chunk, offset);
	case OP_SET_LOCAL_SLASH: {
		return simple_instruction("OP_SET_LOCAL_SLASH", offset);
	}
	case OP_SET_LOCAL_STAR: {
		return simple_instruction("OP_SET_LOCAL_STAR", offset);
	}
	case OP_SET_LOCAL_PLUS: {
		return simple_instruction("OP_SET_LOCAL_PLUS", offset);
	}
	case OP_SET_LOCAL_MINUS: {
		return simple_instruction("OP_SET_LOCAL_MINUS", offset);
	}
	case OP_SET_UPVALUE_SLASH: {
		return simple_instruction("OP_SET_UPVALUE_SLASH", offset);
	}
	case OP_SET_UPVALUE_STAR: {
		return simple_instruction("OP_SET_UPVALUE_STAR", offset);
	}
	case OP_SET_UPVALUE_PLUS: {
		return simple_instruction("OP_SET_UPVALUE_PLUS", offset);
	}
	case OP_SET_UPVALUE_MINUS: {
		return simple_instruction("OP_SET_UPVALUE_MINUS", offset);
	}
	case OP_SET_GLOBAL_SLASH: {
		return simple_instruction("OP_SET_GLOBAL_SLASH", offset);
	}
	case OP_SET_GLOBAL_STAR: {
		return simple_instruction("OP_SET_GLOBAL_STAR", offset);
	}
	case OP_SET_GLOBAL_PLUS: {
		return simple_instruction("OP_SET_GLOBAL_PLUS", offset);
	}
	case OP_SET_GLOBAL_MINUS: {
		return simple_instruction("OP_SET_GLOBAL_MINUS", offset);
	}
	case OP_SET_GLOBAL_INT_DIVIDE: {
		return simple_instruction("OP_SET_GLOBAL_INT_DIVIDE", offset);
	}
	case OP_SET_GLOBAL_MODULUS: {
		return simple_instruction("OP_SET_GLOBAL_MODULUS", offset);
	}
	case OP_SET_LOCAL_INT_DIVIDE: {
		return simple_instruction("OP_SET_LOCAL_INT_DIVIDE", offset);
	}
	case OP_SET_LOCAL_MODULUS: {
		return simple_instruction("OP_SET_LOCAL_MODULUS", offset);
	}
	case OP_SET_UPVALUE_INT_DIVIDE: {
		return simple_instruction("OP_SET_UPVALUE_INT_DIVIDE", offset);
	}
	case OP_SET_UPVALUE_MODULUS: {
		return simple_instruction("OP_SET_UPVALUE_MODULUS", offset);
	}
	case OP_ANON_FUNCTION: {
		return constant_instruction("OP_ANON_FUNCTION", chunk, offset);
	}
	case OP_USE_MODULE: {
		const uint16_t nameCount = chunk->code[offset + 1];
		printf("%-16s %4d name(s) from ", "OP_USE_MODULE", nameCount);
		print_value(
			chunk->constants
				.values[chunk->code[offset + nameCount + 2]],
			false);
		printf("\n");
		return offset + nameCount + 3;
	}
	case OP_PUB: {
		return simple_instruction("OP_PUB", offset);
	}
	case OP_MATCH: {
		return simple_instruction("OP_MATCH", offset);
	}
	case OP_MATCH_JUMP: {
		return jump_instruction("OP_MATCH_JUMP", 1, chunk, offset);
	}
	case OP_MATCH_END: {
		return simple_instruction("OP_MATCH_END", offset);
	}
	case OP_RESULT_MATCH_OK: {
		return simple_instruction("OP_RESULT_MATCH_OK", offset);
	}
	case OP_RESULT_MATCH_ERR: {
		return simple_instruction("OP_RESULT_MATCH_ERR", offset);
	}
	case OP_RESULT_BIND: {
		return constant_instruction("OP_RESULT_BIND", chunk, offset);
	}
	case OP_GIVE: {
		return simple_instruction("OP_GIVE", offset);
	}
	case OP_INT_DIVIDE: {
		return simple_instruction("OP_INT_DIVIDE", offset);
	}
	case OP_POWER: {
		return simple_instruction("OP_POWER", offset);
	}
	case OP_USE_NATIVE: {
		return simple_instruction("OP_USE_NATIVE", offset);
	}
	case OP_FINISH_USE: {
		return simple_instruction("OP_FINISH_USE", offset);
	}
	case OP_STRUCT: {
		return simple_instruction("OP_STRUCT", offset);
	}
	case OP_STRUCT_INSTANCE_START: {
		return simple_instruction("OP_STRUCT_INSTANCE_START", offset);
	}
	case OP_STRUCT_NAMED_FIELD: {
		return simple_instruction("OP_STRUCT_NAMED_FIELD", offset);
	}
	case OP_STRUCT_INSTANCE_END: {
		return simple_instruction("OP_STRUCT_INSTANCE_END", offset);
	}
	case OP_NIL_RETURN: {
		return simple_instruction("OP_NIL_RETURN", offset);
	}

	case OP_TYPEOF: {
		return simple_instruction("OP_TYPEOF", offset);
	}
	case OP_UNWRAP: {
		return simple_instruction("OP_UNWRAP", offset);
	}
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
