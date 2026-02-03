#ifndef CRUX_LANG_SLAB_ALLOCATOR_H
#define CRUX_LANG_SLAB_ALLOCATOR_H
#include <stdint.h>
#include <stdbool.h>

#include "vm.h"

typedef struct SlabSlot SlabSlot;

struct SlabSlot
{
	void* memory;
	SlabSlot* next;
};

struct SlabAllocator
{
	SlabSlot* slots;
	uint16_t count;
	uint16_t capacity;
	uint16_t slot_size;
	SlabSlot* free_slots;
	SlabAllocator* next;
};

SlabAllocator * init_slab_allocator(uint16_t slot_size, uint16_t capacity);
void destroy_slab_allocator(SlabAllocator * slab_allocator);
bool is_slab_full(const SlabAllocator * slab_allocator);
void* allocate_from_slab(SlabAllocator * slab_allocator);
void free_from_slab(SlabAllocator * slab_allocator, void* ptr);

#define FREE_OBJECT(vm, type, pointer) free_slab_object((vm), (pointer), sizeof(type))

#endif // CRUX_LANG_SLAB_ALLOCATOR_H
