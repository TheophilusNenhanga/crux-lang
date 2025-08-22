#include "chunk.h"
#include <stdlib.h>
#include "memory.h"
#include "panic.h"
#include "vm/vm_helpers.h"

void init_chunk(Chunk *chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	init_value_array(&chunk->constants);
}

void write_chunk(VM *vm, Chunk *chunk, const uint8_t byte, const int line)
{
	if (chunk->capacity < chunk->count + 1) {
		const int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(vm, uint8_t, chunk->code, oldCapacity,
					 chunk->capacity);
		chunk->lines = GROW_ARRAY(vm, int, chunk->lines, oldCapacity,
					  chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

void free_chunk(VM *vm, Chunk *chunk)
{
	FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
	free_value_array(vm, &chunk->constants);
	init_chunk(chunk);
}

int add_constant(VM *vm, Chunk *chunk, const Value value)
{
	push(vm->current_module_record, value);
	write_value_array(vm, &chunk->constants, value);
	pop(vm->current_module_record);
	return chunk->constants.count - 1;
}
