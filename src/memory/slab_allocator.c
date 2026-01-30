#include "slab_allocator.h"

#include <stdio.h>
#include <stdlib.h>

SlabAllocator * init_slab_allocator(const uint16_t slot_size, const uint16_t capacity)
{
	SlabAllocator* slab_allocator = malloc(sizeof(SlabAllocator));
	if (slab_allocator == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory for slab allocator");
		return NULL;
	}
	slab_allocator->capacity = capacity;
	slab_allocator->count = 0;
	slab_allocator->next = NULL;
	slab_allocator->slot_size = slot_size;
	slab_allocator->slots = malloc(slab_allocator->slot_size * slab_allocator->capacity);
	if (slab_allocator->slots == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory for slab allocator");
		free(slab_allocator);
		return NULL;
	}

	slab_allocator->freelist.capacity = capacity;
	slab_allocator->freelist.count = 0;
	slab_allocator->freelist.values = malloc(sizeof(uint16_t) * capacity);
	if (slab_allocator->freelist.values == NULL) {
		free(slab_allocator->slots);
		free(slab_allocator);
		fprintf(stderr, "Error: Failed to allocate memory for slab allocator");
		return NULL;
	}
	slab_allocator->freelist.values_top = slab_allocator->freelist.values;

	return slab_allocator;
}

static void push_free_list(SlabFreeList* list, uint16_t value)
{
	if (list == NULL) {
		fprintf(stderr, "Error: Null slab free list provided.");
		return;
	}
	if (list->count == list->capacity) {
		fprintf(stderr, "Error: Slab free list is full.");
		return;
	}
	*list->values_top++ = value;
	list->count++;
}

static uint16_t pop_free_list(SlabFreeList* list, bool* success)
{
	if (list == NULL) {
		fprintf(stderr, "Error: Null slab free list provided.");
		*success = false;
		return 0;
	}
	if (list->count == 0) {
		fprintf(stderr, "Error: Slab free list is empty.");
		*success = false;
		return 0;
	}
	*success = true;
	list->count--;
	return *--list->values_top;
}

bool is_slab_full(const SlabAllocator * slab_allocator)
{
	return slab_allocator->count == slab_allocator->capacity;
}

void* slab_allocate(SlabAllocator * slab_allocator)
{
	SlabAllocator* curr_allocator = slab_allocator;
	SlabAllocator* prev_allocator = NULL;
	while (curr_allocator != NULL && is_slab_full(curr_allocator)) {
		prev_allocator = curr_allocator;
		curr_allocator = curr_allocator->next;
	}

	if (curr_allocator == NULL && prev_allocator == NULL) {
		fprintf(stderr, "Error: Cannot allocate from NULL allocator");
		return NULL;
	}

	if (curr_allocator == NULL) {
		SlabAllocator* new_allocator = init_slab_allocator(slab_allocator->slot_size, slab_allocator->capacity);
		if (new_allocator == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for slab allocator");
			return NULL;
		}
		prev_allocator->next = new_allocator;
		new_allocator->count++;
		return new_allocator->slots + new_allocator->count - 1;

	}else {
		bool success = false;
		const uint16_t new_index = pop_free_list(&curr_allocator->freelist, &success);

		if (success) {
			curr_allocator->count++;
			return curr_allocator->slots+new_index;
		}else {
			// there is no old spot to use, just use next spot
			curr_allocator->count++;
			return curr_allocator->slots+curr_allocator->count-1;
		}
	}
}
void slab_free(SlabAllocator * slab_allocator, void* ptr)
{

}
