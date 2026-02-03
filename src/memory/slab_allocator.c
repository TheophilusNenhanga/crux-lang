#include "slab_allocator.h"

#include <stdio.h>
#include <stdlib.h>

SlabAllocator * init_slab_allocator(const uint16_t slot_size, const uint16_t capacity)
{
    SlabAllocator* slab_allocator = (SlabAllocator*)calloc(1, sizeof(SlabAllocator));
    if (!slab_allocator) {
        fprintf(stderr, "Error: Failed to allocate memory for slab allocator.\n");
        return NULL;
    }

    slab_allocator->slot_size = slot_size;
    slab_allocator->capacity = capacity;
    slab_allocator->slots = (SlabSlot*)calloc(capacity, sizeof(SlabSlot));

    if (!slab_allocator->slots) {
        fprintf(stderr, "Error: Failed to allocate memory for slab slots.\n");
        free(slab_allocator);
        return NULL;
    }

    for (uint16_t i = 0; i < capacity; i++) {
        slab_allocator->slots[i].memory = malloc(slot_size);
        if (!slab_allocator->slots[i].memory) {
            for (uint16_t j = 0; j < i; j++) {
                free(slab_allocator->slots[j].memory);
            }
            free(slab_allocator->slots);
            free(slab_allocator);
            return NULL;
        }

        if (i < capacity - 1) {
            slab_allocator->slots[i].next = &slab_allocator->slots[i + 1];
        } else {
            slab_allocator->slots[i].next = NULL;
        }
    }

    slab_allocator->free_slots = slab_allocator->slots;
    slab_allocator->next = NULL;
    return slab_allocator;
}

bool is_slab_full(const SlabAllocator* slab_allocator) {
    return slab_allocator->free_slots == NULL;
}

void destroy_slab_allocator(SlabAllocator * slab_allocator)
{
    if (!slab_allocator) {
        return;
    }

    SlabAllocator* curr_slab = slab_allocator;
    while (curr_slab) {
        SlabAllocator* next_slab = curr_slab->next;
        for (uint16_t i = 0; i < curr_slab->capacity; i++) {
            free(curr_slab->slots[i].memory);
            curr_slab->slots[i].next = NULL;
            curr_slab->slots[i].memory = NULL;
        }
        free(curr_slab->slots);
        free(curr_slab);
        curr_slab = next_slab;
    }
}

void* allocate_from_slab(SlabAllocator * slab_allocator)
{
    if (!slab_allocator) {
        return NULL;
    }

    SlabAllocator* curr_slab = slab_allocator;
    while (curr_slab && is_slab_full(curr_slab)) {
        if (!curr_slab->next) {
            SlabAllocator* new_allocator = init_slab_allocator(curr_slab->slot_size, curr_slab->capacity);
            if (!new_allocator) {
                fprintf(stderr, "Error: Failed to allocate new slab.\n");
                return NULL;
            }
            curr_slab->next = new_allocator;
        }
        curr_slab = curr_slab->next;
    }

    SlabSlot* free_slot = curr_slab->free_slots;
    if (!free_slot) {
        fprintf(stderr, "Error: No free slots available.\n");
        return NULL;
    }

    curr_slab->free_slots = free_slot->next;
    return free_slot->memory;
}

void free_from_slab(SlabAllocator * slab_allocator, void* ptr)
{
    if (!slab_allocator || !ptr) {
        fprintf(stderr, "Error: NULL pointer given for slab allocator, or ptr.\n");
        return;
    }

    SlabAllocator* curr_slab = slab_allocator;

    while (curr_slab) {
        for (uint16_t i = 0; i < curr_slab->capacity; i++) {
            if (curr_slab->slots[i].memory == ptr) {
                SlabSlot* slot = &curr_slab->slots[i];
                slot->next = curr_slab->free_slots;
                curr_slab->free_slots = slot;
                return;
            }
        }
        curr_slab = curr_slab->next;
    }

    fprintf(stderr, "Error: Pointer not found in any slab.\n");
}
