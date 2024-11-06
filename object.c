#include "object.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, objectType) (type *) allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjectType type) {
	Object *object = (Object *) reallocate(NULL, 0, size);

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *) object);
	printValue(OBJECT_VAL(object));
	printf("\n");
#endif

	object->type = type;
	object->next = vm.objects;
	object->isMarked = false;
	vm.objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *) object, size, type);
#endif

	return object;
}

ObjectBoundMethod *newBoundMethod(Value receiver, ObjectClosure *method) {
	ObjectBoundMethod *bound = ALLOCATE_OBJECT(ObjectBoundMethod, OBJECT_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

ObjectClass *newClass(ObjectString *name) {
	ObjectClass *klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
	initTable(&klass->methods);
	klass->name = name;
	return klass;
}

ObjectUpvalue *newUpvalue(Value *slot) {
	ObjectUpvalue *upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

ObjectClosure *newClosure(ObjectFunction *function) {
	ObjectUpvalue **upvalues = ALLOCATE(ObjectUpvalue *, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjectClosure *closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash) {
	// creates a copy of the characters on the heap
	// that the ObjectString can own
	ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	push(OBJECT_VAL(string));
	tableSet(&vm.strings, string, NIL_VAL);
	pop();
	return string;
}

uint32_t hashString(const char *key, int length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t) key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjectString *copyString(const char *chars, int length) {
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL)
		return interned;

	char *heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0'; // terminating the string because it is not terminated in the source
	return allocateString(heapChars, length, hash);
}

static void printFunction(ObjectFunction *function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJECT_TYPE(value)) {
		case OBJECT_CLASS: {
			printf("'%s' <class>\n", AS_CLASS(value)->name->chars);
			break;
		}
		case OBJECT_STRING: {
			printf("%s", AS_CSTRING(value));
			break;
		}
		case OBJECT_FUNCTION: {
			printFunction(AS_FUNCTION(value));
			break;
		}
		case OBJECT_NATIVE: {
			printf("<native fn>\n");
			break;
		}
		case OBJECT_CLOSURE: {
			printFunction(AS_CLOSURE(value)->function);
			break;
		}
		case OBJECT_UPVALUE: {
			printf("<upvalue>\n");
			break;
		}
		case OBJECT_INSTANCE: {
			printf("'%s' <instance>\n", AS_INSTANCE(value)->klass->name->chars);
			break;
		}
		case OBJECT_BOUND_METHOD: {
			printFunction(AS_BOUND_METHOD(value)->method->function);
			break;
		}
		case OBJECT_ARRAY: {
			printf("<array>\n");
			break;
		}
		case OBJECT_TABLE: {
			printf("<table>\n");
			break;
		}
	}
}

ObjectString *takeString(const char *chars, int length) {
	// claims ownership of the string that you give it
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		// free the string that was passed to us.
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}

	return allocateString(chars, length, hash);
}

ObjectFunction *newFunction() {
	ObjectFunction *function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	function->upvalueCount = 0;
	initChunk(&function->chunk);
	return function;
}

ObjectInstance *newInstance(ObjectClass *klass) {
	ObjectInstance *instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
	instance->klass = klass;
	initTable(&instance->fields);
	return instance;
}

ObjectNative *newNative(NativeFn function, int arity) {
	ObjectNative *native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
	native->function = function;
	native->arity = arity;
	return native;
}

static int calculateArrayCapacity(int n) {
	if (n >= UINT16_MAX - 1) {
		return UINT16_MAX - 1;
	}

	if (n < 1)
		return 1;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return n + 1;
}

ObjectArray *newArray(int elementCount) {
	ObjectArray *array = ALLOCATE_OBJECT(ObjectArray, OBJECT_ARRAY);
	array->capacity = calculateArrayCapacity(elementCount);
	array->size = 0;
	array->array = ALLOCATE(Value, array->capacity);
	return array;
}

ObjectTable *newTable(int elementCount) {
	ObjectTable *table = ALLOCATE_OBJECT(ObjectTable, OBJECT_TABLE);
	table->capacity = elementCount < 16 ? 16 : calculateArrayCapacity(elementCount);
	table->size = 0;
	table->entries = ALLOCATE(ObjectTableEntry, table->capacity);
	for (int i = 0; i < table->capacity; i++) {
		table->entries[i].value = NIL_VAL;
		table->entries[i].key = NIL_VAL;
		table->entries[i].isOccupied = false;
	}
	return table;
}

ObjectArray *growArray(ObjectArray *array) {
	array->array = GROW_ARRAY(Value, array->array, array->capacity, array->capacity * 2);
	array->capacity *= 2;
	return array;
}

void freeObjectTable(ObjectTable *table) {
	FREE_ARRAY(ObjectTableEntry, table->entries, table->capacity);
	table->entries = NULL;
	table->capacity = 0;
	table->size = 0;
}

static ObjectTableEntry *findEntry(ObjectTableEntry *entries, uint16_t capacity, Value key) {
		uint32_t index;
		if (IS_STRING(key)) {
			index = AS_STRING(key)->hash & (capacity - 1);
		}else if (IS_NUMBER(key)) {
			index  = ((int) AS_NUMBER(key)) & (capacity - 1);
		}else {
			return NULL;
		}

		ObjectTableEntry *tombstone = NULL;

		while(1) {
			ObjectTableEntry* entry = &entries[index];
			if (!entry->isOccupied) {
				if (IS_NIL(entry->value)) {
					return tombstone != NULL ? tombstone : entry;
				} else if (tombstone == NULL) {
					tombstone = entry;
				}
			} else if (valuesEqual(entry->key, key)) {
				return entry;
			}
			index = (index + 1) & (capacity - 1);
		}
}

static bool adjustCapacity(ObjectTable *table, int capacity) {
	ObjectTableEntry *entries = ALLOCATE(ObjectTableEntry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NIL_VAL;
		entries[i].value = NIL_VAL;
		entries[i].isOccupied = false;
	}

	table->size = 0;

	for (int i = 0; i < table->capacity; i++) {
		ObjectTableEntry *entry = &table->entries[i];
		if (!entry->isOccupied) {
			continue;
		}

		ObjectTableEntry *dest = findEntry(entries, capacity, entry->key);
		if (dest == NULL) {

			FREE_ARRAY(ObjectTableEntry, entries, capacity);
			return false;
		}

		dest->key = entry->key;
		dest->value = entry->value;
		dest->isOccupied = true;
		table->size++;
	}

	FREE_ARRAY(ObjectTableEntry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
	return true;
}

bool objectTableSet(ObjectTable *table, Value key, Value value) {
	if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);

	if (entry == NULL) {
		return false;
	}

	bool isNewKey = !entry->isOccupied;
	if (isNewKey) {
		table->size++;
	}
	entry->key = key;
	entry->value = value;
	entry->isOccupied = true;

	return isNewKey;
}

bool objectTableGet(ObjectTable *table, Value key, Value *value) {
	if (table->size == 0) {
		return false;
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
	if (entry == NULL) {
		return false;
	}
	if (!entry->isOccupied) {
		return false;
	}
	*value = entry->value;
	return true;
}
