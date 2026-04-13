#include "slab_allocator.h"
#include <stdlib.h>

// Helper to allocate a new underlying chunk of memory
static bool append_new_slab(SlabAllocator *allocator)
{
	// Allocate node + memory in one contiguous block
	size_t header_size = sizeof(SlabNode);
	size_t memory_size = (size_t)allocator->slot_size * allocator->capacity;

	SlabNode *new_slab = (SlabNode *)malloc(header_size + memory_size);
	if (!new_slab)
		return false;

	// Prepend to slab tracking list (for destruction)
	new_slab->next = allocator->slab_head;
	allocator->slab_head = new_slab;

	// Format new memory block into a linked free list
	uint8_t *memory = (uint8_t *)(new_slab + 1);
	for (uint16_t i = 0; i < allocator->capacity - 1; i++) {
		void **current_slot = (void **)(memory + (i * allocator->slot_size));
		void *next_slot = memory + ((i + 1) * allocator->slot_size);
		*current_slot = next_slot;
	}

	// Last slot points to previous global free list
	void **last_slot = (void **)(memory + ((allocator->capacity - 1) * allocator->slot_size));
	*last_slot = allocator->free_list;

	// Update global free list head to start of new slab
	allocator->free_list = memory;
	return true;
}

SlabAllocator *init_slab_allocator(uint16_t slot_size, uint16_t capacity)
{
	// Ensure slot is big enough to hold a pointer
	if (slot_size < sizeof(void *)) {
		slot_size = sizeof(void *);
	}

	SlabAllocator *allocator = (SlabAllocator *)malloc(sizeof(SlabAllocator));
	if (!allocator)
		return NULL;

	allocator->slot_size = slot_size;
	allocator->capacity = capacity;
	allocator->free_list = NULL;
	allocator->slab_head = NULL;

	return allocator;
}

void destroy_slab_allocator(SlabAllocator *allocator)
{
	if (!allocator)
		return;

	SlabNode *curr = allocator->slab_head;
	while (curr) {
		SlabNode *next = curr->next;
		free(curr); // Frees node and slot memory at once
		curr = next;
	}
	free(allocator);
}

void *allocate_from_slab(SlabAllocator *allocator)
{
	if (!allocator)
		return NULL;

	// If out of free slots, allocate a new slab
	if (!allocator->free_list) {
		if (!append_new_slab(allocator)) {
			return NULL;
		}
	}

	// Pop head of free list
	void *ptr = allocator->free_list;
	allocator->free_list = *(void **)ptr;

	return ptr;
}

void free_from_slab(SlabAllocator *allocator, void *ptr)
{
	if (!allocator || !ptr)
		return;

	// Push freed pointer to head of free list
	*(void **)ptr = allocator->free_list;
	allocator->free_list = ptr;
}
