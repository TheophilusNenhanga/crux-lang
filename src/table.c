#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static Value deepCopyValue(ModuleCopyContext *ctx, Value value);

void initTable(Table *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(VM *vm, Table *table) {
	FREE_ARRAY(vm, Entry, table->entries, table->capacity);
	initTable(table);
}

static bool compareStrings(ObjectString *a, ObjectString *b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->chars, b->chars, a->length) == 0;
}

static Entry *findEntry(Entry *entries, int capacity, ObjectString *key) {
	uint32_t index = key->hash & (capacity - 1);
	Entry *tombstone = NULL;
	for (;;) {
		Entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL)
				tombstone = entry;
		} else if (entry->key == key || compareStrings(entry->key, key)) {
			return entry;
		}

		// We have collided, start probing
		index = (index + 1) & (capacity - 1);
	}
}

static void adjustCapacity(VM *vm, Table *table, int capacity) {
	Entry *entries = ALLOCATE(vm, Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		if (entry->key == NULL)
			continue;

		Entry *dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		dest->isPublic = entry->isPublic;
		table->count++;
	}
	FREE_ARRAY(vm, Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool tableSet(VM *vm, Table *table, ObjectString *key, Value value, bool isPublic) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(vm, table, capacity);
	}

	Entry *entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;

	bool isNilValue = IS_NIL(entry->value);

	if (isNewKey && isNilValue) {
		table->count++;
	}

	entry->isPublic = isPublic;
	entry->key = key;
	entry->value = value;
	return isNewKey;
}


bool tableDelete(Table *table, ObjectString *key) {
	if (table->count == 0)
		return false;

	// Find the entry
	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;

	// place a tombstone in the entry
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

bool tableGet(Table *table, ObjectString *key, Value *value) {
	if (table->count == 0)
		return false;

	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;
	*value = entry->value;
	return true;
}

bool tablePublicGet(Table *table, ObjectString *key, Value *value) {
	if (table->count == 0)
		return false;
	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;
	if (!entry->isPublic)
		return false;
	*value = entry->value;
	return true;
}

void tableAddAll(VM *vm, Table *from, Table *to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry *entry = &from->entries[i];
		if (entry->key != NULL) {
			tableSet(vm, to, entry->key, entry->value, entry->isPublic);
		}
	}
}

ObjectString *tableFindString(Table *table, const char *chars, uint64_t length, uint32_t hash) {
	if (table->count == 0)
		return NULL;

	uint32_t index = hash & (table->capacity - 1);
	for (;;) {
		Entry *entry = &table->entries[index];
		if (entry->key == NULL) {
			// Stop if we find an empty non tombstone entry
			if (IS_NIL(entry->value))
				return NULL;
		} else if (entry->key->length == length && entry->key->hash == hash &&
							 memcmp(entry->key->chars, chars, length) == 0) {
			// we found it
			return entry->key;
		}
		index = (index + 1) & (table->capacity - 1);
	}
}

void markTable(VM *vm, Table *table) {
	for (int i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		markObject(vm, (Object *) entry->key);
		markValue(vm, entry->value);
	}
}

void tableRemoveWhite(Table *table) {
	for (int i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		if (entry->key != NULL && !entry->key->Object.isMarked) {
			tableDelete(table, entry->key);
		}
	}
}

static void copyChunk(VM *vm, Chunk *from, Chunk *to, ModuleCopyContext *ctx) {
	for (int i = 0; i < from->count; i++) {
		writeChunk(vm, to, from->code[i], from->lines[i]);
	}

	for (int i = 0; i < from->constants.count; i++) {
		Value constant = from->constants.values[i];
		writeValueArray(vm, &to->constants, deepCopyValue(ctx, constant));
	}
}

static Object *findCopy(ModuleCopyContext *ctx, Object *original) {
	for (int i = 0; i < ctx->count; i++) {
		if (ctx->objects[i] == original) {
			return ctx->copies[i];
		}
	}
	return NULL;
}

static void trackCopy(ModuleCopyContext *ctx, Object *original, Object *copy) {
	if (ctx->count >= ctx->capacity) {
		int newCapacity = GROW_CAPACITY(ctx->capacity);
		ctx->objects = GROW_ARRAY(ctx->toVM, Object *, ctx->objects, ctx->capacity, newCapacity);
		ctx->copies = GROW_ARRAY(ctx->toVM, Object *, ctx->copies, ctx->capacity, newCapacity);
		ctx->capacity = newCapacity;
	}

	ctx->objects[ctx->count] = original;
	ctx->copies[ctx->count] = copy;
	ctx->count++;
}

static Value deepCopyValue(ModuleCopyContext *ctx, Value value) {
	if (!IS_STL_OBJECT(value))
		return value;

	Object *object = AS_STL_OBJECT(value);

	Object *existingCopy = findCopy(ctx, object);
	if (existingCopy != NULL) {
		return OBJECT_VAL(existingCopy);
	}

	switch (object->type) {
		case OBJECT_STRING: {
			ObjectString *string = AS_STL_STRING(value);
			ObjectString *copy = copyString(ctx->toVM, string->chars, string->length);
			trackCopy(ctx, object, (Object *) copy);
			return OBJECT_VAL(copy);
		}

		case OBJECT_FUNCTION: {
			ObjectFunction *function = AS_STL_FUNCTION(value);
			ObjectFunction *copy = newFunction(ctx->toVM);
			trackCopy(ctx, object, (Object *) copy);

			copy->arity = function->arity;
			copy->upvalueCount = function->upvalueCount;
			copy->name = copyString(ctx->toVM, function->name->chars, function->name->length);

			initChunk(&copy->chunk);
			copyChunk(ctx->toVM, &function->chunk, &copy->chunk, ctx);

			return OBJECT_VAL(copy);
		}

		case OBJECT_CLOSURE: {
			ObjectClosure *closure = AS_STL_CLOSURE(value);
			Value functionCopy = deepCopyValue(ctx, OBJECT_VAL(closure->function));
			ObjectClosure *copy = newClosure(ctx->toVM, AS_STL_FUNCTION(functionCopy));
			trackCopy(ctx, object, (Object *) copy);

			for (int i = 0; i < closure->upvalueCount; i++) {
				if (closure->upvalues[i]->location != &closure->upvalues[i]->closed) {
					copy->upvalues[i]->closed = deepCopyValue(ctx, *closure->upvalues[i]->location);
					copy->upvalues[i]->location = &copy->upvalues[i]->closed;
				} else {
					copy->upvalues[i]->closed = deepCopyValue(ctx, closure->upvalues[i]->closed);
					copy->upvalues[i]->location = &copy->upvalues[i]->closed;
				}
			}

			return OBJECT_VAL(copy);
		}

		case OBJECT_CLASS: {
			ObjectClass *klass = AS_STL_CLASS(value);
			ObjectClass *copy = newClass(ctx->toVM, copyString(ctx->toVM, klass->name->chars, klass->name->length));
			trackCopy(ctx, object, (Object *) copy);

			for (int i = 0; i < klass->methods.capacity; i++) {
				Entry *entry = &klass->methods.entries[i];
				if (entry->key != NULL) {
					Value methodCopy = deepCopyValue(ctx, entry->value);
					tableSet(ctx->toVM, &copy->methods, copyString(ctx->toVM, entry->key->chars, entry->key->length), methodCopy,
									 entry->isPublic);
				}
			}

			return OBJECT_VAL(copy);
		}

		case OBJECT_INSTANCE: {
			ObjectInstance *instance = AS_STL_INSTANCE(value);

			Value klassCopy = deepCopyValue(ctx, OBJECT_VAL(instance->klass));
			ObjectInstance *copy = newInstance(ctx->toVM, AS_STL_CLASS(klassCopy));
			trackCopy(ctx, object, (Object *) copy);

			for (int i = 0; i < instance->fields.capacity; i++) {
				Entry *entry = &instance->fields.entries[i];
				if (entry->key != NULL) {
					Value fieldCopy = deepCopyValue(ctx, entry->value);
					tableSet(ctx->toVM, &copy->fields, copyString(ctx->toVM, entry->key->chars, entry->key->length), fieldCopy,
									 entry->isPublic);
				}
			}

			return OBJECT_VAL(copy);
		}

		case OBJECT_ARRAY: {
			ObjectArray *array = AS_STL_ARRAY(value);
			ObjectArray *copy = newArray(ctx->toVM, array->size);
			trackCopy(ctx, object, (Object *) copy);

			for (int i = 0; i < array->size; i++) {
				copy->array[i] = deepCopyValue(ctx, array->array[i]);
			}
			copy->size = array->size;

			return OBJECT_VAL(copy);
		}

		case OBJECT_TABLE: {
			ObjectTable *table = AS_STL_TABLE(value);
			ObjectTable *copy = newTable(ctx->toVM, table->size);
			trackCopy(ctx, object, (Object *) copy);

			for (int i = 0; i < table->capacity; i++) {
				ObjectTableEntry *entry = &table->entries[i];
				if (entry->isOccupied) {
					Value keyCopy = deepCopyValue(ctx, entry->key);
					Value valueCopy = deepCopyValue(ctx, entry->value);
					objectTableSet(ctx->toVM, copy, keyCopy, valueCopy);
				}
			}

			return OBJECT_VAL(copy);
		}

		case OBJECT_ERROR: {
			ObjectError *error = AS_STL_ERROR(value);
			ObjectString *messageCopy = copyString(ctx->toVM, error->message->chars, error->message->length);
			ObjectError *copy = newError(ctx->toVM, messageCopy, error->type, error->isPanic);
			trackCopy(ctx, object, (Object *) copy);
			return OBJECT_VAL(copy);
		}

		// DO NOT COPY
		case OBJECT_NATIVE_FUNCTION:
		case OBJECT_NATIVE_METHOD:
		case OBJECT_BOUND_METHOD:
		case OBJECT_UPVALUE:
		case OBJECT_MODULE:
			return NIL_VAL;
		case OBJECT_RESULT: {
			ObjectResult *result = AS_STL_RESULT(value);
			ObjectResult* newResult;

			if (result->isOk) {
				newResult = stellaOk(ctx->toVM, deepCopyValue(ctx, result->as.value));
			}else {
				ObjectError* error = result->as.error;
				ObjectString *messageCopy = copyString(ctx->toVM, error->message->chars, error->message->length);
				ObjectError *copy = newError(ctx->toVM, messageCopy, error->type, error->isPanic);
				newResult = stellaErr(ctx->toVM, copy);
			}
			trackCopy(ctx, object, (Object*) newResult);
		}
			break;
	}
	return NIL_VAL;
}

bool tableDeepCopy(VM *fromVM, VM* toVM, Table* fromTable, Table* toTable,  ObjectString *key, ObjectString *newKey) {
	if (fromTable->count == 0 || key == NULL || fromVM == NULL || toVM == NULL || fromVM == toVM) {
		return false;
	}

	Entry *entry = findEntry(fromTable->entries, fromTable->capacity, key);
	if (entry->key == NULL || !entry->isPublic) {
		return false;
	}

	ModuleCopyContext context;
	context.capacity = 8;
	context.count = 0;
	context.objects = ALLOCATE(fromVM, Object *, context.capacity);
	context.copies = ALLOCATE(fromVM, Object *, context.capacity);
	context.fromVM = fromVM;
	context.toVM = toVM;

	if (context.copies == NULL || context.objects == NULL) {
		return false;
	}

	if (IS_NIL(entry->value)) {
		tableSet(toVM, toTable, newKey, NIL_VAL, false);
		return true;
	}
	Value copiedValue = deepCopyValue(&context, entry->value);
	bool success = !IS_NIL(copiedValue);

	if (success) {
		success = tableSet(context.toVM, toTable, newKey, copiedValue, false);
	}

	FREE_ARRAY(fromVM, Object *, context.objects, context.capacity);
	FREE_ARRAY(fromVM, Object *, context.copies, context.capacity);

	return success;
}
