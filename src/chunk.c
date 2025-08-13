#include "chunk.h"
#include <stdlib.h>
#include "memory.h"
#include "panic.h"
#include "vm/vm_helpers.h"

void initChunk(Chunk *chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	initValueArray(&chunk->constants);
}

void writeChunk(VM *vm, Chunk *chunk, const uint8_t byte, const int line)
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

void freeChunk(VM *vm, Chunk *chunk)
{
	FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
	freeValueArray(vm, &chunk->constants);
	initChunk(chunk);
}

int addConstant(VM *vm, Chunk *chunk, const Value value)
{
	PUSH(vm->currentModuleRecord, value);
	writeValueArray(vm, &chunk->constants, value);
	POP(vm->currentModuleRecord);
	return chunk->constants.count - 1;
}
