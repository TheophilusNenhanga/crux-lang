#include "debug.h"
#include "chunk.h"
#include "type_system.h"
#include "value.h"

#include <stdio.h>

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
static int byte_instruction(const char *name, const Chunk *chunk, const int offset)
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
static int jump_instruction(const char *name, const int sign, const Chunk *chunk, const int offset)
{
	uint16_t jump = chunk->code[offset + 1];
	printf("%-16s %4d -> %d\n", name, offset, offset + 2 + sign * jump);
	return offset + 2;
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
static int constant_instruction(const char *name, const Chunk *chunk, const int offset)
{
	const uint16_t constant = chunk->code[offset + 1]; // Get the constant index
	printf("%-16s %4d '", name, constant); // Print the name of the opcode
	print_value(chunk->constants.values[constant],
				false); // print the constant's value
	printf("'\n");
	return offset + 2; // +2 because OP_CONSTANT is two bytes
}

static int inline_arg_instruction(const char *name, const Chunk *chunk, const int offset)
{
	const uint16_t arg = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, arg);
	return offset + 2;
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
static int invoke_instruction(const char *name, const Chunk *chunk, const int offset)
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

	const OpCode instruction = chunk->code[offset]; // read a single byte, that is the opcode

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
	case OP_ITER_INIT:
		return simple_instruction("OP_ITER_INIT", offset);
	case OP_ITER_NEXT:
		return jump_instruction("OP_ITER_NEXT", 1, chunk, offset);
	case OP_LOOP:
		return jump_instruction("OP_LOOP", -1, chunk, offset);
	case OP_CALL:
		return byte_instruction("OP_CALL", chunk, offset);
	case OP_GET_UPVALUE:
		return byte_instruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byte_instruction("OP_SET_UPVALUE", chunk, offset);
	case OP_CLOSURE: {
		offset++;
		const uint16_t constant = chunk->code[offset++];
		printf("%-16s %4d ", "OP_CLOSURE", constant);
		print_value(chunk->constants.values[constant], false);
		printf("\n");

		const ObjectFunction *function = AS_CRUX_FUNCTION(chunk->constants.values[constant]);
		for (int j = 0; j < function->upvalue_count; j++) {
			const int isLocal = chunk->code[offset++];
			const int index = chunk->code[offset++];
			printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
		}
		return offset;
	}
	case OP_CLOSE_UPVALUE:
		return simple_instruction("OP_CLOSE_UPVALUE", offset);
	case OP_GET_PROPERTY:
		return constant_instruction("OP_GET_PROPERTY", chunk, offset);
	case OP_SET_PROPERTY:
		return constant_instruction("OP_SET_PROPERTY", chunk, offset);
	case OP_ARRAY:
		return inline_arg_instruction("OP_ARRAY", chunk, offset);
	case OP_TABLE:
		return inline_arg_instruction("OP_TABLE", chunk, offset);
	case OP_SET:
		return inline_arg_instruction("OP_SET", chunk, offset);
	case OP_TUPLE:
		return inline_arg_instruction("OP_TUPLE", chunk, offset);
	case OP_RANGE:
		return simple_instruction("OP_RANGE", offset);
	case OP_GET_COLLECTION:
		return simple_instruction("OP_GET_COLLECTION", offset);
	case OP_SET_COLLECTION:
		return simple_instruction("OP_SET_COLLECTION", offset);
	case OP_INVOKE:
		return invoke_instruction("OP_INVOKE", chunk, offset);
	case OP_SET_LOCAL_SLASH: {
		return inline_arg_instruction("OP_SET_LOCAL_SLASH", chunk, offset);
	}
	case OP_SET_LOCAL_STAR: {
		return inline_arg_instruction("OP_SET_LOCAL_STAR", chunk, offset);
	}
	case OP_SET_LOCAL_PLUS: {
		return inline_arg_instruction("OP_SET_LOCAL_PLUS", chunk, offset);
	}
	case OP_SET_LOCAL_MINUS: {
		return inline_arg_instruction("OP_SET_LOCAL_MINUS", chunk, offset);
	}
	case OP_SET_UPVALUE_SLASH: {
		return inline_arg_instruction("OP_SET_UPVALUE_SLASH", chunk, offset);
	}
	case OP_SET_UPVALUE_STAR: {
		return inline_arg_instruction("OP_SET_UPVALUE_STAR", chunk, offset);
	}
	case OP_SET_UPVALUE_PLUS: {
		return inline_arg_instruction("OP_SET_UPVALUE_PLUS", chunk, offset);
	}
	case OP_SET_UPVALUE_MINUS: {
		return inline_arg_instruction("OP_SET_UPVALUE_MINUS", chunk, offset);
	}
	case OP_SET_GLOBAL_SLASH: {
		return constant_instruction("OP_SET_GLOBAL_SLASH", chunk, offset);
	}
	case OP_SET_GLOBAL_STAR: {
		return constant_instruction("OP_SET_GLOBAL_STAR", chunk, offset);
	}
	case OP_SET_GLOBAL_PLUS: {
		return constant_instruction("OP_SET_GLOBAL_PLUS", chunk, offset);
	}
	case OP_SET_GLOBAL_MINUS: {
		return constant_instruction("OP_SET_GLOBAL_MINUS", chunk, offset);
	}
	case OP_SET_GLOBAL_INT_DIVIDE: {
		return constant_instruction("OP_SET_GLOBAL_INT_DIVIDE", chunk, offset);
	}
	case OP_SET_GLOBAL_MODULUS: {
		return constant_instruction("OP_SET_GLOBAL_MODULUS", chunk, offset);
	}
	case OP_SET_LOCAL_INT_DIVIDE: {
		return inline_arg_instruction("OP_SET_LOCAL_INT_DIVIDE", chunk, offset);
	}
	case OP_SET_LOCAL_MODULUS: {
		return inline_arg_instruction("OP_SET_LOCAL_MODULUS", chunk, offset);
	}
	case OP_SET_UPVALUE_INT_DIVIDE: {
		return inline_arg_instruction("OP_SET_UPVALUE_INT_DIVIDE", chunk, offset);
	}
	case OP_SET_UPVALUE_MODULUS: {
		return inline_arg_instruction("OP_SET_UPVALUE_MODULUS", chunk, offset);
	}
	case OP_ANON_FUNCTION: {
		return constant_instruction("OP_ANON_FUNCTION", chunk, offset);
	}
	case OP_USE_MODULE: {
		return constant_instruction("OP_USE_MODULE", chunk, offset);
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
		return jump_instruction("OP_RESULT_MATCH_OK", 1, chunk, offset);
	}
	case OP_RESULT_MATCH_ERR: {
		return jump_instruction("OP_RESULT_MATCH_ERR", 1, chunk, offset);
	}
	case OP_RESULT_BIND: {
		return byte_instruction("OP_RESULT_BIND", chunk, offset);
	}
	case OP_OPTION_MATCH_SOME: {
		return jump_instruction("OP_OPTION_MATCH_SOME", 1, chunk, offset);
	}
	case OP_OPTION_MATCH_NONE: {
		return jump_instruction("OP_OPTION_MATCH_NONE", 1, chunk, offset);
	}
	case OP_TYPE_MATCH: {
		const uint16_t match_type = chunk->code[offset + 1];
		const uint16_t jump = chunk->code[offset + 2];
		char type_name[64];
		type_mask_name((TypeMask)match_type, type_name, sizeof(type_name));
		printf("%-16s %-10s %4d -> %d\n", "OP_TYPE_MATCH", type_name, offset, offset + 3 + jump);
		return offset + 3;
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
		const uint16_t nameCount = chunk->code[offset + 1];
		printf("%-16s %4d name(s) from '", "OP_USE_NATIVE", nameCount);
		print_value(chunk->constants.values[chunk->code[offset + 2 + 2 * nameCount]], false);
		printf("'\n");
		return offset + 3 + 2 * nameCount;
	}
	case OP_FINISH_USE: {
		const uint16_t nameCount = chunk->code[offset + 1];
		printf("%-16s %4d name(s)\n", "OP_FINISH_USE", nameCount);
		return offset + 2 + 2 * nameCount;
	}
	case OP_STRUCT: {
		return constant_instruction("OP_STRUCT", chunk, offset);
	}
	case OP_STRUCT_INSTANCE_START: {
		return simple_instruction("OP_STRUCT_INSTANCE_START", offset);
	}
	case OP_STRUCT_NAMED_FIELD: {
		return constant_instruction("OP_STRUCT_NAMED_FIELD", chunk, offset);
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
	case OP_PANIC: {
		return simple_instruction("OP_PANIC", offset);
	}
	case OP_BITWISE_AND: {
		return simple_instruction("OP_BITWISE_AND", offset);
	}
	case OP_BITWISE_OR: {
		return simple_instruction("OP_BITWISE_OR", offset);
	}
	case OP_BITWISE_XOR: {
		return simple_instruction("OP_BITWISE_XOR", offset);
	}
	case OP_METHOD: {
		return constant_instruction("OP_METHOD", chunk, offset);
	}
	case OP_SET_PROPERTY_PLUS: {
		return constant_instruction("OP_SET_PROPERTY_PLUS", chunk, offset);
	}
	case OP_SET_PROPERTY_MINUS: {
		return constant_instruction("OP_SET_PROPERTY_MINUS", chunk, offset);
	}
	case OP_SET_PROPERTY_STAR: {
		return constant_instruction("OP_SET_PROPERTY_STAR", chunk, offset);
	}
	case OP_SET_PROPERTY_SLASH: {
		return constant_instruction("OP_SET_PROPERTY_SLASH", chunk, offset);
	}
	case OP_SET_PROPERTY_INT_DIVIDE: {
		return constant_instruction("OP_SET_PROPERTY_INT_DIVIDE", chunk, offset);
	}
	case OP_SET_PROPERTY_MODULUS: {
		return constant_instruction("OP_SET_PROPERTY_MODULUS", chunk, offset);
	}
	case OP_GET_PROPERTY_INDEX: {
		return inline_arg_instruction("OP_GET_PROPERTY_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_PLUS_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_PLUS_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_MINUS_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_MINUS_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_STAR_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_STAR_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_SLASH_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_SLASH_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_INT_DIVIDE_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_INT_DIVIDE_INDEX", chunk, offset);
	}
	case OP_SET_PROPERTY_MODULUS_INDEX: {
		return inline_arg_instruction("OP_SET_PROPERTY_MODULUS_INDEX", chunk, offset);
	}
	case OP_BITWISE_NOT: {
		return simple_instruction("OP_BITWISE_NOT", offset);
	}
	case OP_TYPE_COERCE: {
		return constant_instruction("OP_TYPE_COERCE", chunk, offset);
	}
	case OP_GET_SLICE: {
		return simple_instruction("OP_GET_SLICE", offset);
	}
	case OP_IN: {
		return simple_instruction("OP_IN", offset);
	}
	case OP_OK: {
		return simple_instruction("OP_OK", offset);
	}
	case OP_ERR: {
		return simple_instruction("OP_ERROR", offset);
	}
	case OP_SOME: {
		return simple_instruction("OP_SOME", offset);
	}
	case OP_NONE: {
		return simple_instruction("OP_NONE", offset);
	}
	case OP_ADD_INT: {
		return simple_instruction("OP_ADD_INT", offset);
	}
	case OP_ADD_NUM: {
		return simple_instruction("OP_ADD_NUM", offset);
	}
	case OP_SUBTRACT_INT: {
		return simple_instruction("OP_SUBTRACT_INT", offset);
	}
	case OP_SUBTRACT_NUM: {
		return simple_instruction("OP_SUBTRACT_NUM", offset);
	}
	case OP_MULTIPLY_INT: {
		return simple_instruction("OP_MULTIPLY_INT", offset);
	}
	case OP_MULTIPLY_NUM: {
		return simple_instruction("OP_MULTIPLY_NUM", offset);
	}
	case OP_DIVIDE_NUM: {
		return simple_instruction("OP_DIVIDE_NUM", offset);
	}
	case OP_INT_DIVIDE_INT: {
		return simple_instruction("OP_INT_DIVIDE_INT", offset);
	}
	case OP_MODULUS_INT: {
		return simple_instruction("OP_MODULUS_INT", offset);
	}
	case OP_POWER_INT: {
		return simple_instruction("OP_POWER_INT", offset);
	}
	case OP_POWER_NUM: {
		return simple_instruction("OP_POWER_NUM", offset);
	}
	case OP_ADD_VECTOR_VECTOR: {
		return simple_instruction("OP_ADD_VECTOR_VECTOR", offset);
	}
	case OP_SUBTRACT_VECTOR_VECTOR: {
		return simple_instruction("OP_SUBTRACT_VECTOR_VECTOR", offset);
	}
	case OP_MULTIPLY_VECTOR_VECTOR: {
		return simple_instruction("OP_MULTIPLY_VECTOR_VECTOR", offset);
	}
	case OP_DIVIDE_VECTOR_VECTOR: {
		return simple_instruction("OP_DIVIDE_VECTOR_VECTOR", offset);
	}
	case OP_MULTIPLY_VECTOR_SCALAR: {
		return simple_instruction("OP_MULTIPLY_VECTOR_SCALAR", offset);
	}
	case OP_MULTIPLY_SCALAR_VECTOR: {
		return simple_instruction("OP_MULTIPLY_SCALAR_VECTOR", offset);
	}
	case OP_DIVIDE_VECTOR_SCALAR: {
		return simple_instruction("OP_DIVIDE_VECTOR_SCALAR", offset);
	}
	case OP_ADD_COMPLEX_COMPLEX: {
		return simple_instruction("OP_ADD_COMPLEX_COMPLEX", offset);
	}
	case OP_SUBTRACT_COMPLEX_COMPLEX: {
		return simple_instruction("OP_SUBTRACT_COMPLEX_COMPLEX", offset);
	}
	case OP_MULTIPLY_COMPLEX_COMPLEX: {
		return simple_instruction("OP_MULTIPLY_COMPLEX_COMPLEX", offset);
	}
	case OP_DIVIDE_COMPLEX_COMPLEX: {
		return simple_instruction("OP_DIVIDE_COMPLEX_COMPLEX", offset);
	}
	case OP_MULTIPLY_COMPLEX_SCALAR: {
		return simple_instruction("OP_MULTIPLY_COMPLEX_SCALAR", offset);
	}
	case OP_MULTIPLY_SCALAR_COMPLEX: {
		return simple_instruction("OP_MULTIPLY_SCALAR_COMPLEX", offset);
	}
	case OP_DIVIDE_COMPLEX_SCALAR: {
		return simple_instruction("OP_DIVIDE_COMPLEX_SCALAR", offset);
	}
	case OP_ADD_MATRIX_MATRIX: {
		return simple_instruction("OP_ADD_MATRIX_MATRIX", offset);
	}
	case OP_SUBTRACT_MATRIX_MATRIX: {
		return simple_instruction("OP_SUBTRACT_MATRIX_MATRIX", offset);
	}
	case OP_ADD_MATRIX_SCALAR: {
		return simple_instruction("OP_ADD_MATRIX_SCALAR", offset);
	}
	case OP_ADD_SCALAR_MATRIX: {
		return simple_instruction("OP_ADD_SCALAR_MATRIX", offset);
	}
	case OP_SUBTRACT_MATRIX_SCALAR: {
		return simple_instruction("OP_SUBTRACT_MATRIX_SCALAR", offset);
	}
	case OP_SUBTRACT_SCALAR_MATRIX: {
		return simple_instruction("OP_SUBTRACT_SCALAR_MATRIX", offset);
	}
	case OP_MULTIPLY_MATRIX_MATRIX: {
		return simple_instruction("OP_MULTIPLY_MATRIX_MATRIX", offset);
	}
	case OP_MULTIPLY_MATRIX_SCALAR: {
		return simple_instruction("OP_MULTIPLY_MATRIX_SCALAR", offset);
	}
	case OP_MULTIPLY_SCALAR_MATRIX: {
		return simple_instruction("OP_MULTIPLY_SCALAR_MATRIX", offset);
	}
	case OP_DIVIDE_MATRIX_SCALAR: {
		return simple_instruction("OP_DIVIDE_MATRIX_SCALAR", offset);
	}
	case OP_INVOKE_STDLIB: {
		return invoke_instruction("OP_INVOKE_STDLIB", chunk, offset);
	}
	case OP_INVOKE_STDLIB_UNWRAP: {
		return invoke_instruction("OP_INVOKE_STDLIB_UNWRAP", chunk, offset);
	}
	case OP_POP_N: {
		return inline_arg_instruction("OP_POP_N", chunk, offset);
	}
	case OP_DEFINE_PUB_GLOBAL: {
		return constant_instruction("OP_DEFINE_PUB_GLOBAL", chunk, offset);
	}
	case OP_0_INT: {
		return simple_instruction("OP_0_INT", offset);
	}
	case OP_1_INT: {
		return simple_instruction("OP_1_INT", offset);
	}
	case OP_2_INT: {
		return simple_instruction("OP_2_INT", offset);
	}
	case OP_0_FLOAT: {
		return simple_instruction("OP_0_FLOAT", offset);
	}
	case OP_1_FLOAT: {
		return simple_instruction("OP_1_FLOAT", offset);
	}
	case OP_2_FLOAT: {
		return simple_instruction("OP_2_FLOAT", offset);
	}
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
