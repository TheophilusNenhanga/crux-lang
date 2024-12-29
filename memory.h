#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include "object.h"
#include "vm.h"

#define TABLE_MAX_LOAD 0.6

#define ALLOCATE(vm, type, count) (type *) reallocate(vm, NULL, 0, sizeof(type) * count)

#define FREE(vm, type, pointer) reallocate(vm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 16 ? 16 : (capacity) * 2)

#define GROW_ARRAY(vm, type, pointer, oldCount, newCount)                                                              \
	(type *) reallocate(vm, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(vm, type, pointer, oldCount) reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

void *reallocate(VM *vm, void *pointer, size_t oldSize, size_t newSize);

void markObject(VM *vm, Object *object);

void markValue(VM *vm, Value value);

void collectGarbage(VM *vm);

void freeObjects(VM *vm);

#endif // MEMORY_H
