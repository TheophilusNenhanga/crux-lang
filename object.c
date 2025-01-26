#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"

static Object *allocateObject(VM *vm, size_t size, ObjectType type) {
	Object *object = (Object *) reallocate(vm, NULL, 0, size);

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *) object);
	printValue(OBJECT_VAL(object));
	printf("\n");
#endif

	object->type = type;
	object->next = vm->objects;
	object->isMarked = false;
	vm->objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *) object, size, type);
#endif

	return object;
}
#define ALLOCATE_OBJECT(vm, type, objectType) (type *) allocateObject(vm, sizeof(type), objectType)

static uint64_t calculateCollectionCapacity(uint64_t n) {
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

static uint32_t hashValue(Value value) {
	if (IS_STRING(value)) {
		return AS_STRING(value)->hash;
	}
	if (IS_NUMBER(value)) {
		double num = AS_NUMBER(value);
		if (num == (int64_t) num) {
			return (uint32_t) ((int64_t) num ^ ((int64_t) num >> 32));
		}
		uint64_t bits;
		memcpy(&bits, &num, sizeof(bits));
		return (uint32_t) (bits ^ (bits >> 32));
	}
	if (IS_BOOL(value)) {
		return AS_BOOL(value) ? 1u : 0u;
	}
	if (IS_NIL(value)) {
		return 4321u;
	}
	return 0u;
}

ObjectBoundMethod *newBoundMethod(VM *vm, Value receiver, ObjectClosure *method) {
	ObjectBoundMethod *bound = ALLOCATE_OBJECT(vm, ObjectBoundMethod, OBJECT_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

ObjectClass *newClass(VM *vm, ObjectString *name) {
	ObjectClass *klass = ALLOCATE_OBJECT(vm, ObjectClass, OBJECT_CLASS);
	initTable(&klass->methods);
	klass->name = name;
	return klass;
}

ObjectUpvalue *newUpvalue(VM *vm, Value *slot) {
	ObjectUpvalue *upvalue = ALLOCATE_OBJECT(vm, ObjectUpvalue, OBJECT_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

ObjectClosure *newClosure(VM *vm, ObjectFunction *function) {
	ObjectUpvalue **upvalues = ALLOCATE(vm, ObjectUpvalue *, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjectClosure *closure = ALLOCATE_OBJECT(vm, ObjectClosure, OBJECT_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

static ObjectString *allocateString(VM *vm, char *chars, uint64_t length, uint32_t hash) {
	// creates a copy of the characters on the heap
	// that the ObjectString can own
	ObjectString *string = ALLOCATE_OBJECT(vm, ObjectString, OBJECT_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	push(vm, OBJECT_VAL(string));
	tableSet(vm, &vm->strings, string, NIL_VAL, false);
	pop(vm);
	return string;
}

uint32_t hashString(const char *key, uint64_t length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t) key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjectString *copyString(VM *vm, const char *chars, uint64_t length) {
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL)
		return interned;

	char *heapChars = ALLOCATE(vm, char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0'; // terminating the string because it is not terminated in the source
	return allocateString(vm, heapChars, length, hash);
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
			printf("'%s' <class>", AS_CLASS(value)->name->chars);
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
		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = AS_NATIVE_FUNCTION(value);
			if (native->name != NULL) {
				printf("<native fn %s>", native->name->chars);
			} else {
				printf("<native fn>");
			}
			break;
		}
		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = AS_NATIVE_METHOD(value);
			if (native->name != NULL) {
				printf("<native method %s>", native->name->chars);
			} else {
				printf("<native method>");
			}
			break;
		}
		case OBJECT_CLOSURE: {
			printFunction(AS_CLOSURE(value)->function);
			break;
		}
		case OBJECT_UPVALUE: {
			printf("<upvalue>");
			break;
		}
		case OBJECT_INSTANCE: {
			printf("'%s' <instance>", AS_INSTANCE(value)->klass->name->chars);
			break;
		}
		case OBJECT_BOUND_METHOD: {
			printFunction(AS_BOUND_METHOD(value)->method->function);
			break;
		}
		case OBJECT_ARRAY: {
			printf("<array>");
			break;
		}
		case OBJECT_TABLE: {
			printf("<table>");
			break;
		}
		case OBJECT_ERROR: {
			printf("<error>");
			break;
		}
	}
}

ObjectString *takeString(VM *vm, char *chars, uint64_t length) {
	// claims ownership of the string that you give it
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL) {
		// free the string that was passed to us.
		FREE_ARRAY(vm, char, chars, length + 1);
		return interned;
	}

	return allocateString(vm, chars, length, hash);
}

ObjectString *toString(VM *vm, Value value) {
	if (!IS_OBJECT(value)) {
		char buffer[32];
		if (IS_NUMBER(value)) {
			double num = AS_NUMBER(value);
			if (num == (int) num) {
				snprintf(buffer, sizeof(buffer), "%.0f", num);
			} else {
				snprintf(buffer, sizeof(buffer), "%g", num);
			}
		} else if (IS_BOOL(value)) {
			strcpy(buffer, AS_BOOL(value) ? "true" : "false");
		} else if (IS_NIL(value)) {
			strcpy(buffer, "nil");
		}
		return copyString(vm, buffer, (int) strlen(buffer));
	}

	switch (OBJECT_TYPE(value)) {
		case OBJECT_STRING:
			return AS_STRING(value);

		case OBJECT_FUNCTION: {
			ObjectFunction *function = AS_FUNCTION(value);
			if (function->name == NULL) {
				return copyString(vm, "<script>", 8);
			}
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = AS_NATIVE_FUNCTION(value);
			if (native->name != NULL) {
				char *start = "<native fn ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native fn>", 11);
		}

		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = AS_NATIVE_METHOD(value);
			if (native->name != NULL) {
				char *start = "<native method ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native method>", 15);
		}

		case OBJECT_CLOSURE: {
			ObjectFunction *function = AS_CLOSURE(value)->function;
			if (function->name == NULL) {
				return copyString(vm, "<script>", 8);
			}
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_UPVALUE: {
			return copyString(vm, "<upvalue>", 9);
		}

		case OBJECT_CLASS: {
			ObjectClass *klass = AS_CLASS(value);
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "%s <class>", klass->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_INSTANCE: {
			ObjectInstance *instance = AS_INSTANCE(value);
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "%s <instance>", instance->klass->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod *bound = AS_BOUND_METHOD(value);
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "<bound fn %s>", bound->method->function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_ARRAY: {
			ObjectArray *array = AS_ARRAY(value);
			size_t bufSize = 2; // [] minimum
			for (int i = 0; i < array->size; i++) {
				ObjectString *element = toString(vm, array->array[i]);
				bufSize += element->length + 2; // element + ", "
			}

			char *buffer = ALLOCATE(vm, char, bufSize);
			char *ptr = buffer;
			*ptr++ = '[';

			for (int i = 0; i < array->size; i++) {
				if (i > 0) {
					*ptr++ = ',';
					*ptr++ = ' ';
				}
				ObjectString *element = toString(vm, array->array[i]);
				memcpy(ptr, element->chars, element->length);
				ptr += element->length;
			}
			*ptr++ = ']';

			ObjectString *result = takeString(vm, buffer, ptr - buffer);
			return result;
		}

		case OBJECT_TABLE: {
			ObjectTable *table = AS_TABLE(value);
			size_t bufSize = 2; // {} minimum
			for (int i = 0; i < table->capacity; i++) {
				if (table->entries[i].isOccupied) {
					ObjectString *k = toString(vm, table->entries[i].key);
					ObjectString *v = toString(vm, table->entries[i].value);
					bufSize += k->length + v->length + 4; // key:value,
				}
			}

			char *buffer = ALLOCATE(vm, char, bufSize);
			char *ptr = buffer;
			*ptr++ = '{';

			bool first = true;
			for (int i = 0; i < table->capacity; i++) {
				if (table->entries[i].isOccupied) {
					if (!first) {
						*ptr++ = ',';
						*ptr++ = ' ';
					}
					first = false;

					ObjectString *key = toString(vm, table->entries[i].key);
					ObjectString *val = toString(vm, table->entries[i].value);

					memcpy(ptr, key->chars, key->length);
					ptr += key->length;
					*ptr++ = ':';
					memcpy(ptr, val->chars, val->length);
					ptr += val->length;
				}
			}
			*ptr++ = '}';

			ObjectString *result = takeString(vm, buffer, ptr - buffer);
			return result;
		}

		case OBJECT_ERROR: {
			ObjectError *error = AS_ERROR(value);
			char buffer[128];
			int length = snprintf(buffer, sizeof(buffer), "<error: %s>", error->message->chars);
			return copyString(vm, buffer, length);
		}

		default:
			return copyString(vm, "<unknown>", 9);
	}
}


ObjectFunction *newFunction(VM *vm) {
	ObjectFunction *function = ALLOCATE_OBJECT(vm, ObjectFunction, OBJECT_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	function->upvalueCount = 0;
	initChunk(&function->chunk);
	return function;
}

ObjectInstance *newInstance(VM *vm, ObjectClass *klass) {
	ObjectInstance *instance = ALLOCATE_OBJECT(vm, ObjectInstance, OBJECT_INSTANCE);
	instance->klass = klass;
	initTable(&instance->fields);
	return instance;
}

ObjectNativeFunction *newNativeFunction(VM *vm, NativeFn function, int arity, ObjectString *name) {
	ObjectNativeFunction *native = ALLOCATE_OBJECT(vm, ObjectNativeFunction, OBJECT_NATIVE_FUNCTION);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeMethod *newNativeMethod(VM *vm, NativeFn function, int arity, ObjectString *name) {
	ObjectNativeMethod *native = ALLOCATE_OBJECT(vm, ObjectNativeMethod, OBJECT_NATIVE_METHOD);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectFile *newFile(VM *vm, ObjectString *path, FILE *handle, ObjectString* mode) {
	ObjectFile *file = ALLOCATE_OBJECT(vm, ObjectFile, OBJECT_FILE);
	file->path = path;
	file->handle = handle;
	file->isOpen = false;
	file->position = NULL;
	file->size = 0;
	file->mode = mode;
	return file;
}

ObjectTable *newTable(VM *vm, int elementCount) {
	ObjectTable *table = ALLOCATE_OBJECT(vm, ObjectTable, OBJECT_TABLE);
	table->capacity = elementCount < 16 ? 16 : calculateCollectionCapacity(elementCount);
	table->size = 0;
	table->entries = ALLOCATE(vm, ObjectTableEntry, table->capacity);
	for (int i = 0; i < table->capacity; i++) {
		table->entries[i].value = NIL_VAL;
		table->entries[i].key = NIL_VAL;
		table->entries[i].isOccupied = false;
	}
	return table;
}

void freeObjectTable(VM *vm, ObjectTable *table) {
	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = NULL;
	table->capacity = 0;
	table->size = 0;
}

static ObjectTableEntry *findEntry(ObjectTableEntry *entries, uint16_t capacity, Value key) {
	uint32_t hash = hashValue(key);
	uint32_t index = hash & (capacity - 1);
	ObjectTableEntry *tombstone = NULL;

	while (1) {
		ObjectTableEntry *entry = &entries[index];
		if (!entry->isOccupied) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			} else if (tombstone == NULL) {
				tombstone = entry;
			}
		} else if (valuesEqual(entry->key, key)) {
			return entry;
		}
		// index = (index + 1) & (capacity - 1); // old probe
		index = (index * 5 + 1) & (capacity - 1); // new probe
	}
}

static bool adjustCapacity(VM *vm, ObjectTable *table, int capacity) {
	ObjectTableEntry *entries = ALLOCATE(vm, ObjectTableEntry, capacity);
	if (entries == NULL) {
		return false;
	}

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

		dest->key = entry->key;
		dest->value = entry->value;
		dest->isOccupied = true;
		table->size++;
	}

	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
	return true;
}

bool objectTableSet(VM *vm, ObjectTable *table, Value key, Value value) {
	if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		if (!adjustCapacity(vm, table, capacity))
			return false;
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = !entry->isOccupied;

	if (isNewKey && IS_NIL(entry->value)) {
		table->size++;
	}

	if (IS_OBJECT(key))
		markValue(vm, key);
	if (IS_OBJECT(value))
		markValue(vm, value);

	entry->key = key;
	entry->value = value;
	entry->isOccupied = true;

	return true;
}

bool objectTableGet(ObjectTable *table, Value key, Value *value) {
	if (table->size == 0) {
		return false;
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
	if (!entry->isOccupied) {
		return false;
	}
	*value = entry->value;
	return true;
}

ObjectArray *newArray(VM *vm, uint64_t elementCount) {
	ObjectArray *array = ALLOCATE_OBJECT(vm, ObjectArray, OBJECT_ARRAY);
	array->capacity = calculateCollectionCapacity(elementCount);
	array->size = 0;
	array->array = ALLOCATE(vm, Value, array->capacity);
	for (int i = 0; i < array->capacity; i++) {
		array->array[i] = NIL_VAL;
	}
	return array;
}

bool ensureCapacity(VM *vm, ObjectArray *array, uint64_t capacityNeeded) {
	if (capacityNeeded <= array->capacity) {
		return true;
	}
	uint64_t newCapacity = array->capacity;
	while (newCapacity < capacityNeeded) {
		if (newCapacity > INT_MAX / 2) {
			return false;
		}
		newCapacity *= 2;
	}
	Value *newArray = GROW_ARRAY(vm, Value, array->array, array->capacity, newCapacity);
	if (newArray == NULL) {
		return false;
	}
	for (uint64_t i = array->capacity; i < newCapacity; i++) {
		newArray[i] = NIL_VAL;
	}
	array->array = newArray;
	array->capacity = newCapacity;
	return true;
}

bool arraySet(VM *vm, ObjectArray *array, uint64_t index, Value value) {
	if (index >= array->size) {
		return false;
	}
	if (IS_OBJECT(value)) {
		markValue(vm, value);
	}
	array->array[index] = value;
	return true;
}

bool arrayGet(ObjectArray *array, uint64_t index, Value *value) {
	if (array == NULL) {
		return false;
	}
	*value = array->array[index];
	return true;
}

bool arrayAdd(VM *vm, ObjectArray *array, Value value, uint64_t index) {
	if (!ensureCapacity(vm, array, array->size + 1)) {
		return false;
	}
	if (IS_OBJECT(value)) {
		markValue(vm, value);
	}
	array->array[index] = value;
	array->size++;
	return true;
}

ObjectError *newError(VM *vm, ObjectString *message, ErrorType type, ErrorCreator creator) {
	ObjectError *error = ALLOCATE(vm, ObjectError, OBJECT_ERROR);
	error->object.type = OBJECT_ERROR;
	error->message = message;
	error->type = type;
	error->creator = creator;
	return error;
}

NativeReturn makeNativeReturn(VM *vm, uint8_t size) {
	NativeReturn nativeReturn;
	nativeReturn.size = size;
	nativeReturn.values = ALLOCATE(vm, Value, nativeReturn.size);
	return nativeReturn;
}

ObjectModule *newModule(VM *vm, const char *path) {
	ObjectModule *module = ALLOCATE_OBJECT(vm, ObjectModule, OBJECT_MODULE);
	module->path = copyString(vm, path, strlen(path));
	module->state = INITIAL;
	module->vmDepth = 0;
	initImportSet(&module->importedModules);
	return module;
}

void initImportSet(ImportSet* set) {
	set->paths = NULL;
	set->count = 0;
	set->capacity = 0;
}

bool importSetContains(ImportSet* set, ObjectString* path) {
	if (set->count == 0) {
		return false;
	}
	for (int i = 0; i < set->count; i++) {
		if (path == set->paths[i]) {
			return true;
		}
	}
	return false;
}

bool importSetAdd(VM* vm, ImportSet* set, ObjectString* path) {
	if (set->count + 1 > set->capacity) {
		int oldCapacity = set->capacity;
		set->capacity = GROW_CAPACITY(oldCapacity);
		set->paths = GROW_ARRAY(vm, ObjectString*, set->paths, oldCapacity, set->capacity);
	}
	set->paths[set->count++] = path;
	return true;
}

void freeImportSet(VM* vm, ImportSet* set) {
	FREE_ARRAY(vm, ObjectString*, set->paths, set->capacity);
	initImportSet(set);
}