#include "slab_allocator.h"
#include <stdio.h>
#include <stdlib.h>

SlabAllocator * init_slab_allocator(const uint16_t slot_size, const uint16_t capacity)
{
    SlabAllocator* slab = (SlabAllocator*)calloc(1, sizeof(SlabAllocator));
    if (!slab) return NULL;

    slab->slot_size = slot_size;
    slab->capacity = capacity;
    slab->used_count = 0;
    slab->next = NULL;

    slab->memory = malloc((size_t)slot_size * capacity);
    slab->next_indexes = malloc(sizeof(int16_t) * capacity);

    if (!slab->memory || !slab->next_indexes) {
        if(slab->memory) free(slab->memory);
        if(slab->next_indexes) free(slab->next_indexes);
        free(slab);
        fprintf(stderr, "Error: Failed to allocate slab memory.\n");
        return NULL;
    }

    for (int16_t i = 0; i < capacity - 1; i++) {
        slab->next_indexes[i] = i + 1;
    }
    slab->next_indexes[capacity - 1] = -1;
    slab->free_head = 0;

    return slab;
}

bool is_slab_full(const SlabAllocator* slab_allocator) {
    return slab_allocator->free_head == -1;
}

void destroy_slab_allocator(SlabAllocator * slab_allocator)
{
    while (slab_allocator) {
        SlabAllocator* next = slab_allocator->next;
        free(slab_allocator->memory);
        free(slab_allocator->next_indexes);
        free(slab_allocator);
        slab_allocator = next;
    }
}

void* allocate_from_slab(SlabAllocator * slab_allocator)
{
    if (!slab_allocator) return NULL;

    SlabAllocator* curr = slab_allocator;

    while (curr && is_slab_full(curr)) {
        if (!curr->next) {
            // create new slab
            curr->next = init_slab_allocator(curr->slot_size, curr->capacity);
            if (!curr->next) return NULL;
        }
        curr = curr->next;
    }

    const int16_t slot_index = curr->free_head;
    // move free head to next slot
    curr->free_head = curr->next_indexes[slot_index];
    curr->used_count++;

    return curr->memory + ((size_t)slot_index * curr->slot_size);
}

void free_from_slab(SlabAllocator * slab_allocator, void* ptr)
{
    if (!slab_allocator || !ptr) return;

    SlabAllocator* curr = slab_allocator;
    while (curr) {
	    const uint8_t * start = curr->memory;
	    const uint8_t * end = curr->memory + ((size_t)curr->capacity * curr->slot_size);

        if ((uint8_t*)ptr >= start && (uint8_t*)ptr < end) {
            // page found

		const size_t offset = (uint8_t*)ptr - start;
	    const int16_t index = (int16_t)(offset / curr->slot_size);

            // add this slot to free list
            curr->next_indexes[index] = curr->free_head;
            curr->free_head = index;

            curr->used_count--;

            return;
        }
        curr = curr->next;
    }

    fprintf(stderr, "Error: Pointer not found in any slab page.\n");
}
