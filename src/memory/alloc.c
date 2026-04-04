#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "slab_allocator.h"
#include "vm.h"

void *alloc_memory(VM *vm, const size_t size)
{
	SlabAllocator *slab = NULL;

	if (size <= 16) {
		slab = vm->slab_16;
	} else if (size <= 24) {
		slab = vm->slab_24;
	} else if (size <= 32) {
		slab = vm->slab_32;
	} else if (size <= 48) {
		slab = vm->slab_48;
	} else if (size <= 64) {
		slab = vm->slab_64;
	}

	if (!slab) {
		// Allocate from heap
		void *ptr = malloc(size);
		if (!ptr) {
			return NULL;
		}
		return ptr;
	}

	// Allocate from slab
	void *ptr = allocate_from_slab(slab);
	if (!ptr) {
		return NULL;
	}
	return ptr;
}

void free_memory(VM *vm, void *ptr, const size_t size)
{
	if (!ptr) {
		fprintf(stderr, "Error: NULL pointer given for memory to free.\n");
		return;
	}

	vm->bytes_allocated -= size;

	if (size <= 16) {
		free_from_slab(vm->slab_16, ptr);
	} else if (size <= 24) {
		free_from_slab(vm->slab_24, ptr);
	} else if (size <= 32) {
		free_from_slab(vm->slab_32, ptr);
	} else if (size <= 48) {
		free_from_slab(vm->slab_48, ptr);
	} else if (size <= 64) {
		free_from_slab(vm->slab_64, ptr);
	} else {
		free(ptr);
	}
}

void *allocate_object_with_gc(VM *vm, const size_t size)
{
	vm->bytes_allocated += size;
	if (vm->bytes_allocated > vm->next_gc) {
		collect_garbage(vm);
	}
	void *result = alloc_memory(vm, size);
	if (result == NULL) {
		collect_garbage(vm);
		result = alloc_memory(vm, size);
		if (result == NULL) {
			if (vm->current_module_record) {
				runtime_panic(vm->current_module_record, MEMORY, "Failed to allocate %zu bytes.", size);
			} else {
				fprintf(stderr, "Fatal error - Out of Memory: Failed to allocate %zu bytes.\n", size);
			}
			free_vm(vm);
			exit(1);
		}
	}
	return result;
}

void *allocate_object_without_gc(VM *vm, const size_t size)
{
	void *result = malloc(size);
	if (result == NULL) {
		if (vm)
			collect_garbage(vm);
		result = malloc(size);
		if (result == NULL) {
			if (vm && vm->current_module_record) {
				runtime_panic(vm->current_module_record, MEMORY, "Failed to allocate %zu bytes.", size);
			} else {
				fprintf(stderr, "Fatal error - Out of Memory: Failed to allocate %zu bytes.\n", size);
			}
			if (vm)
				free_vm(vm);
			exit(1);
		}
	}
	return result;
}

void free_slab_object(VM *vm, void *ptr, const size_t curr_size)
{
	if (!ptr)
		return;

	vm->bytes_allocated -= curr_size;

	if (curr_size <= 16) {
		free_from_slab(vm->slab_16, ptr);
	} else if (curr_size <= 24) {
		free_from_slab(vm->slab_24, ptr);
	} else if (curr_size <= 32) {
		free_from_slab(vm->slab_32, ptr);
	} else if (curr_size <= 48) {
		free_from_slab(vm->slab_48, ptr);
	} else {
		free(ptr);
	}
}

void *reallocate(VM *vm, void *pointer, const size_t oldSize, const size_t newSize)
{
	vm->bytes_allocated += newSize - oldSize;
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collect_garbage(vm);
#endif
		if (vm->bytes_allocated > vm->next_gc) {
			collect_garbage(vm);
		}
	}

	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, newSize);
	if (result == NULL) {
		if (vm)
			collect_garbage(vm);
		result = realloc(pointer, newSize);
		if (result == NULL) {
			if (vm && vm->current_module_record) {
				runtime_panic(vm->current_module_record, MEMORY, "Failed to reallocate %zu bytes.", newSize);
			} else {
				fprintf(stderr, "Fatal error - Out of Memory: Failed to reallocate %zu bytes.\n", newSize);
			}
			if (vm)
				free_vm(vm);
			exit(1);
		}
	}

	return result;
}
