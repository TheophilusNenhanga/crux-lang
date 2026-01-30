#ifndef CRUX_LANG_SLAB_ALLOCATOR_H
#define CRUX_LANG_SLAB_ALLOCATOR_H
#include <stdint.h>
#include <stdbool.h>

typedef struct SlabAllocator SlabAllocator;

typedef struct
{
	uint16_t* values;
	uint16_t* values_top;
	uint16_t count;
	uint16_t capacity;
} SlabFreeList;

struct SlabAllocator
{
	void* slots;
	uint16_t count;
	uint16_t capacity;
	uint16_t slot_size;
	SlabAllocator* next;
	SlabFreeList freelist;
};

SlabAllocator * init_slab_allocator(uint16_t slot_size, uint16_t capacity);
bool is_slab_full(const SlabAllocator * slab_allocator);
void* slab_allocate(SlabAllocator * slab_allocator);
void slab_free(SlabAllocator * slab_allocator, void* ptr);



#endif // CRUX_LANG_SLAB_ALLOCATOR_H
