#ifndef MEMORY_H
#define MEMORY_H

#include "object.h"

#define TABLE_MAX_LOAD 0.6

#define ALLOCATE(vm, type, count)                                              \
	(type *)reallocate(vm, NULL, 0, sizeof(type) * count)

#define FREE(vm, type, pointer) reallocate(vm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 2 ? 2 : (capacity) * 2)

#define GROW_ARRAY(vm, type, pointer, oldCount, newCount)                      \
	(type *)reallocate(vm, pointer, sizeof(type) * (oldCount),             \
			   sizeof(type) * (newCount))

#define FREE_ARRAY(vm, type, pointer, oldCount)                                \
	reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief Reallocates a block of memory.
 *
 * This function acts as a wrapper around `realloc` and `free`, providing
 * garbage collection integration and debugging features. It updates the VM's
 * `bytesAllocated` counter, potentially triggers garbage collection if
 * allocation exceeds the `nextGC` threshold, and handles allocation failures.
 *
 * @param vm The virtual machine.
 * @param pointer The old block of memory to reallocate. If `NULL`, it's
 * equivalent to `malloc`.
 * @param oldSize The old size of the memory block. If zero, it's equivalent to
 * `malloc`.
 * @param newSize The new size of the memory block. If zero, it's equivalent to
 * `free`.
 *
 * @return A pointer to the reallocated memory block. Returns `NULL` if
 * `newSize` is zero. Exits the program if allocation fails and `realloc`
 * returns `NULL` when `newSize` is not zero.
 */
void *reallocate(VM *vm, void *pointer, size_t oldSize, size_t newSize);

/**
 * @brief Marks an object as reachable during garbage collection.
 *
 * This function marks the given `object` as reachable, preventing it from being
 * freed by the garbage collector. If the object is not already marked, it is
 * marked and added to the gray stack for further processing in the mark phase.
 *
 * @param vm The virtual machine.
 * @param object The object to mark. If `NULL`, the function returns
 * immediately.
 */
void mark_object(VM *vm, Object *object);

/**
 * @brief Marks a Value as reachable during garbage collection.
 *
 * If the given `value` is an object, this function calls `markObject` to mark
 * it. Primitive values are ignored as they are not managed by the garbage
 * collector.
 *
 * @param vm The virtual machine.
 * @param value The Value to mark.
 */
void mark_value(VM *vm, Value value);

/**
 * @brief Performs a full garbage collection cycle.
 *
 * This function orchestrates the garbage collection process:
 * 1. Marks root objects using `markRoots`.
 * 2. Traces references from gray objects using `traceReferences`.
 * 3. Removes white (unmarked) entries from the string interning table.
 * 4. Sweeps unmarked objects and frees their memory using `sweep`.
 * 5. Updates the `nextGC` threshold based on the current allocated memory.
 *
 * @param vm The virtual machine.
 */
void collect_garbage(VM *vm);

/**
 * @brief Frees all remaining objects in the VM's object list.
 *
 * This function is called when the VM is shut down to free all objects that
 * are still allocated. It iterates through the object list and frees each
 * object using `freeObject`. It also frees the gray stack.
 *
 * @param vm The virtual machine.
 */
void free_objects(VM *vm);

#endif // MEMORY_H
