#include <stdlib.h>
#include <stdio.h>

#include "alloc.h"
#include "slab_allocator.h"
#include "garbage_collector.h"


void* alloc_memory(VM* vm, const size_t size)
{
  SlabAllocator* slab = NULL;

  switch (size) {
    case 16: {
        slab = vm->slab_16;
        break;
    }
    case 24:{
      slab = vm->slab_24;
      break;
    }

    case 32: {
      slab = vm->slab_32;
      break;
    }

    case 48: {
      slab = vm->slab_48;
      break;
    }
  }

  if (!slab) {
      // Allocate from heap
      void* ptr = malloc(size);
      if (!ptr) {
          fprintf(stderr, "Error: Failed to allocate memory!\n");
          return NULL;
      }
      return ptr;
  }

  // Allocate from slab
  void* ptr = allocate_from_slab(slab);
  if (!ptr) {
      fprintf(stderr, "Error: Failed to allocate memory from slab!\n");
      return NULL;
  }
  return ptr;
}

void free_memory(VM* vm, void* ptr, const size_t size) {
    if (!ptr) {
        fprintf(stderr, "Error: NULL pointer given for memory to free.\n");
        return;
    }

    switch (size) {
        case 16: {
            free_from_slab(vm->slab_16, ptr);
            break;
        }
        case 24: {
            free_from_slab(vm->slab_24, ptr);
            break;
        }
        case 32: {
            free_from_slab(vm->slab_32, ptr);
            break;
        }
        case 48: {
            free_from_slab(vm->slab_48, ptr);
            break;
        }
        default: {
            free(ptr);
            break;
        }
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
		fprintf(stderr,
			"Fatal error - Out of Memory: Failed to allocate %zu "
			"bytes.\n ",
			size);
		exit(1);
	}
	return result;
}


void *allocate_object_without_gc(const size_t size)
{
	void *result = malloc(size);
	if (result == NULL) {
		fprintf(stderr,
			"Fatal error - Out of Memory: Failed to allocate %zu "
			"bytes.\n"
			"Exiting!",
			size);
		exit(1);
	}
	return result;
}

void free_slab_object(VM* vm, void* ptr, const size_t curr_size){
    if (!ptr)
        return;

    switch (curr_size) {
        case 16: {
            free_from_slab(vm->slab_16, ptr);
            break;
        }
        case 24: {
            free_from_slab(vm->slab_24, ptr);
            break;
        }
        case 32: {
            free_from_slab(vm->slab_32, ptr);
            break;
        }
        case 48: {
            free_from_slab(vm->slab_48, ptr);
            break;
        }
        default: {
            free(ptr);
            break;
        }
    }
}

void *reallocate(VM *vm, void *pointer, const size_t oldSize,
		 const size_t newSize)
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
		fprintf(stderr,
			"Fatal error - Out of Memory: Failed to reallocate %zu "
			"bytes.\n "
			"Exiting!",
			newSize);
		exit(1);
	}

	return result;
}
