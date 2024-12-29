#include "chunk.h"
#include <stdlib.h>
#include "memory.h"

void initChunk(Chunk *chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	initValueArray(&chunk->constants);
}

void writeChunk(VM *vm, Chunk *chunk, uint8_t byte, int line) {
	if (chunk->capacity < chunk->count + 1) {
		// check if the current array already has the capacity for the new byte
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity); // Growing the array to make room for the next byte
		chunk->code = GROW_ARRAY(vm, uint8_t, chunk->code, oldCapacity, chunk->capacity);
		chunk->lines = GROW_ARRAY(vm, int, chunk->lines, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

void freeChunk(VM *vm, Chunk *chunk) {
	FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
	freeValueArray(vm, &chunk->constants);
	initChunk(chunk);
}

int addConstant(VM *vm,Chunk *chunk, Value value) {
	push(vm, value);
	writeValueArray(vm, &chunk->constants, value);
	pop(vm);
	return chunk->constants.count - 1;
	// After we add the constant we return the index where the constant was
	// appended so that we can locate the same constant later
}
