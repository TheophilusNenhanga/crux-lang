#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "memory.h"
#include "object.h"
#include "panic.h"

static uint32_t get_new_pool_object(ObjectPool *pool)
{
	if (pool->free_top == 0) {
		const uint32_t new_capacity = pool->capacity *
					      OBJECT_POOL_GROWTH_FACTOR;
		if (new_capacity < pool->capacity) { // overflow
			fprintf(stderr, "Fatal Error: Cannot index memory. "
					"Shutting Down!");
			exit(1);
		}

		pool->objects = realloc(pool->objects,
					new_capacity * sizeof(PoolObject));
		if (pool->objects == NULL) {
			fprintf(stderr, "Fatal Error - Out of Memory: Failed "
					"to reallocate space for object pool. "
					"Shutting down!");
			exit(1);
		}

		pool->free_list = realloc(pool->free_list,
					  new_capacity * sizeof(uint32_t));
		if (pool->free_list == NULL) {
			fprintf(stderr, "Fatal Error - Out of Memory: Failed "
					"to reallocate space for object pool. "
					"Shutting down!");
			exit(1);
		}

		for (uint32_t i = pool->capacity; i < new_capacity; i++) {
			pool->free_list[pool->free_top++] = i;
			PoolObject* pool_object = &pool->objects[i];
			SET_DATA(pool_object, NULL);
			SET_MARKED(pool_object, false);
		}
		pool->capacity = new_capacity;
	}
	const uint32_t index = pool->free_list[--pool->free_top];
	pool->count++;
	return index;
}

/**
 * @brief Allocates a new object of the specified type.
 *
 * @param vm The virtual machine.
 * @param size The size of the object to allocate in bytes.
 * @param type The type of the object being allocated (ObjectType enum).
 *
 * @return A pointer to the newly allocated and initialized Object.
 */

static CruxObject *allocate_pooled_object(VM *vm, const size_t size,
					  const ObjectType type)
{
	CruxObject *object = allocate_object_with_gc(vm, size);

	const uint32_t pool_index = get_new_pool_object(vm->object_pool);

	object->type = type;
	object->pool_index = pool_index;

	PoolObject *pool_object = &vm->object_pool->objects[pool_index];

	SET_DATA(pool_object, object);
	SET_MARKED(pool_object, false);

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif

	return object;
}

static CruxObject *allocate_pooled_object_without_gc(const VM *vm,
						     const size_t size,
						     const ObjectType type)
{
	CruxObject *object = allocate_object_without_gc(size);

	const uint32_t pool_index = get_new_pool_object(vm->object_pool);

	object->pool_index = pool_index;
	object->type = type;

	PoolObject *pool_object = &vm->object_pool->objects[pool_index];

	SET_DATA(pool_object, object);
	SET_MARKED(pool_object, false);

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
	return object;
}

/**
 * @brief Macro to allocate a specific type of object.
 * @param vm The virtual machine.
 * @param type The C type of the object to allocate (e.g., ObjectString).
 * @param objectType The ObjectType enum value for the object.
 *
 * @return A pointer to the newly allocated object, cast to the specified type.
 */
#define ALLOCATE_OBJECT(vm, type, objectType)                                  \
	(type *)allocate_pooled_object(vm, sizeof(type), objectType)

#define ALLOCATE_OBJECT_WITHOUT_GC(vm, type, objectType)                       \
	(type *)allocate_pooled_object_without_gc(vm, sizeof(type), objectType)

/**
 * @brief Calculates the next power of 2 capacity for a collection.
 *
 * @param n The desired minimum capacity.
 *
 * @return The next power of 2 capacity greater than or equal to `n`, or
 * `UINT16_MAX - 1` if `n` is close to the maximum.
 */
static uint32_t calculateCollectionCapacity(uint32_t n)
{
	if (n >= UINT16_MAX - 1) {
		return UINT16_MAX - 1;
	}

	if (n < 8)
		return 8;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

/**
 * @brief Generates a hash code for a Value.
 *
 * @param value The Value to hash.
 *
 * @return A 32-bit hash code for the Value.
 */
static uint32_t hashValue(const Value value)
{
	if (IS_CRUX_STRING(value)) {
		return AS_CRUX_STRING(value)->hash;
	}
	if (IS_INT(value)) {
		return (uint32_t)AS_INT(value);
	}
	if (IS_FLOAT(value)) {
		const double num = AS_FLOAT(value);
		uint64_t bits;
		memcpy(&bits, &num, sizeof(bits));
		return (uint32_t)(bits ^ bits >> 32);
	}
	if (IS_BOOL(value)) {
		return AS_BOOL(value) ? 1u : 0u;
	}
	if (IS_NIL(value)) {
		return 4321u;
	}
	return 0u;
}

void print_type(const Value value)
{
	if (IS_INT(value)) {
		printf("'int'");
		return;
	}
	if (IS_FLOAT(value)) {
		printf("'float'");
		return;
	}
	if (IS_BOOL(value)) {
		printf("'bool'");
		return;
	}
	if (IS_NIL(value)) {
		printf("'nil'");
		return;
	}
	switch (OBJECT_TYPE(value)) {
	case OBJECT_STRING:
		printf("'string'");
		break;
	case OBJECT_FUNCTION:
	case OBJECT_NATIVE_FUNCTION:
	case OBJECT_NATIVE_INFALLIBLE_FUNCTION:
	case OBJECT_NATIVE_METHOD:
	case OBJECT_NATIVE_INFALLIBLE_METHOD:
	case OBJECT_CLOSURE:
		printf("'function'");
		break;
	case OBJECT_UPVALUE:
		printf("'upvalue'");
		break;
	case OBJECT_ARRAY:
		printf("'array'");
		break;
	case OBJECT_TABLE:
		printf("'table'");
		break;
	case OBJECT_ERROR:
		printf("'error'");
		break;
	case OBJECT_RESULT:
		printf("'result'");
		break;
	case OBJECT_RANDOM:
		printf("'random'");
		break;
	case OBJECT_FILE:
		printf("'file'");
		break;
	case OBJECT_VECTOR: {
		ObjectVector *vector = AS_CRUX_VECTOR(value);
		printf("'Vec<%d>'", vector->dimensions);
		break;
	}
	default:
		printf("'unknown'");
	}
}

ObjectUpvalue *new_upvalue(VM *vm, Value *slot)
{
	ObjectUpvalue *upvalue = ALLOCATE_OBJECT(vm, ObjectUpvalue,
						 OBJECT_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

ObjectClosure *new_closure(VM *vm, ObjectFunction *function)
{
	ObjectUpvalue **upvalues = ALLOCATE(vm, ObjectUpvalue *,
					    function->upvalue_count);
	for (int i = 0; i < function->upvalue_count; i++) {
		upvalues[i] = NULL;
	}

	ObjectClosure *closure = ALLOCATE_OBJECT(vm, ObjectClosure,
						 OBJECT_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalue_count = function->upvalue_count;
	return closure;
}

/**
 * @brief Allocates a new string object. calculates and stores the
 * string's hash value and interns the string in the VM's string table.
 *
 * @param vm The virtual machine.
 * @param chars The character array for the string. This memory is assumed to be
 * managed externally and copied.
 * @param length The length of the string.
 * @param hash The pre-calculated hash value of the string.
 *
 * @return A pointer to the newly created and interned ObjectString.
 */
static ObjectString *allocateString(VM *vm, char *chars, const uint32_t length,
				    const uint32_t hash)
{
	ObjectString *string = ALLOCATE_OBJECT(vm, ObjectString, OBJECT_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	// intern the string
	push(vm->current_module_record, OBJECT_VAL(string));
	table_set(vm, &vm->strings, string, NIL_VAL);
	pop(vm->current_module_record);
	return string;
}

/**
 * @brief Calculates the hash value of a C-style string.
 *
 * This function implements the FNV-1a hash algorithm to generate a hash
 * code for a given C-style string.
 *
 * @param key The null-terminated C-style string to hash.
 * @param length The length of the string (excluding null terminator).
 *
 * @return A 32-bit hash code for the string.
 */
static uint32_t hash_string(const char *key, const size_t length)
{
	static const uint32_t FNV_OFFSET_BIAS = 2166136261u;
	static const uint32_t FNV_PRIME = 16777619u;

	uint32_t hash = FNV_OFFSET_BIAS;
	for (size_t i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= FNV_PRIME;
	}
	return hash;
}

ObjectString *copy_string(VM *vm, const char *chars, const uint32_t length)
{
	const uint32_t hash = hash_string(chars, length);

	ObjectString *interned = table_find_string(&vm->strings, chars, length,
						   hash);
	if (interned != NULL)
		return interned;

	char *heapChars = ALLOCATE(vm, char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0'; // terminating the string because it is not
				  // terminated in the source
	return allocateString(vm, heapChars, length, hash);
}

static void print_error_type(const ErrorType type)
{
	switch (type) {
	case SYNTAX:
		printf("syntax");
		break;
	case MATH:
		printf("math");
		break;
	case BOUNDS:
		printf("bounds");
		break;
	case RUNTIME:
		printf("runtime");
		break;
	case TYPE:
		printf("type");
		break;
	case LOOP_EXTENT:
		printf("loop");
		break;
	case LIMIT:
		printf("limit");
		break;
	case BRANCH_EXTENT:
		printf("branch");
		break;
	case CLOSURE_EXTENT:
		printf("closure");
		break;
	case LOCAL_EXTENT:
		printf("local");
		break;
	case ARGUMENT_EXTENT:
		printf("argument");
		break;
	case NAME:
		printf("name");
		break;
	case COLLECTION_EXTENT:
		printf("collection");
		break;
	case VARIABLE_EXTENT:
		printf("variable");
		break;
	case RETURN_EXTENT:
		printf("return");
		break;
	case ARGUMENT_MISMATCH:
		printf("argument mismatch");
		break;
	case STACK_OVERFLOW:
		printf("stack overflow");
		break;
	case COLLECTION_GET:
		printf("collection get");
		break;
	case COLLECTION_SET:
		printf("collection set");
		break;
	case MEMORY:
		printf("memory");
		break;
	case VALUE:
		printf("value");
		break;
	case ASSERT:
		printf("assert");
		break;
	case IMPORT_EXTENT:
		printf("import");
		break;
	case IO:
		printf("io");
		break;
	case IMPORT:
		printf("import");
		break;
	}
}

/**
 * @brief Prints the name of a function object.
 *
 * This static helper function prints the name of a function object to the
 * console, used for debugging and representation. If the function is
 * anonymous (name is NULL), it prints "<script>".
 *
 * @param function The ObjectFunction to print the name of.
 */
static void print_function(const ObjectFunction *function)
{
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

static void print_array(const Value *values, const uint32_t size)
{
	printf("[");
	for (uint32_t i = 0; i < size; i++) {
		print_value(values[i], true);
		if (i != size - 1) {
			printf(", ");
		}
	}
	printf("]");
}

static void print_table(const ObjectTableEntry *entries,
			const uint32_t capacity, const uint32_t size)
{
	uint32_t printed = 0;
	printf("{");
	for (uint32_t i = 0; i < capacity; i++) {
		if (entries[i].is_occupied) {
			print_value(entries[i].key, true);
			printf(":");
			print_value(entries[i].value, true);
			if (printed != size - 1) {
				printf(", ");
			}
			printed++;
		}
	}
	printf("}");
}

static void print_struct_instance(const ObjectStructInstance *instance)
{
	printf("{");
	int printed = 0;
	const ObjectStruct *type = instance->struct_type;
	if (instance->fields == NULL) {
		printf("}");
		return;
	}
	for (int i = 0; i < type->fields.capacity; i++) {
		if (type->fields.entries[i].key != NULL) {
			const uint16_t index = (uint16_t)AS_INT(
				type->fields.entries[i].value);
			const ObjectString *fieldName =
				type->fields.entries[i].key;
			printf("%s: ", fieldName->chars);
			print_value(instance->fields[index], true);
			if (printed != type->fields.count - 1) {
				printf(", ");
			}
			printed++;
		}
	}
	printf("}");
}

static void print_result(const ObjectResult *result)
{
	if (result->is_ok) {
		printf("Ok<");
		print_type(result->as.value);
		printf(">");
	} else {
		printf("Err<");
		print_error_type(result->as.error->type);
		printf(">");
	}
}

void print_object(const Value value, const bool in_collection)
{
	switch (OBJECT_TYPE(value)) {
	case OBJECT_STRING: {
		if (in_collection) {
			printf("'%s'", AS_C_STRING(value));
			break;
		}
		printf("%s", AS_C_STRING(value));
		break;
	}
	case OBJECT_FUNCTION: {
		print_function(AS_CRUX_FUNCTION(value));
		break;
	}
	case OBJECT_NATIVE_FUNCTION: {
		const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(
			value);
		if (native->name != NULL) {
			printf("<native fn %s>", native->name->chars);
		} else {
			printf("<native fn>");
		}
		break;
	}
	case OBJECT_NATIVE_METHOD: {
		const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
		if (native->name != NULL) {
			printf("<native method %s>", native->name->chars);
		} else {
			printf("<native method>");
		}
		break;
	}
	case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
		const ObjectNativeInfallibleFunction *native =
			AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
		if (native->name != NULL) {
			printf("<native infallible fn %s>",
			       native->name->chars);
		} else {
			printf("<native infallible fn>");
		}
		break;
	}
	case OBJECT_CLOSURE: {
		print_function(AS_CRUX_CLOSURE(value)->function);
		break;
	}
	case OBJECT_UPVALUE: {
		print_value(value, false);
		break;
	}
	case OBJECT_ARRAY: {
		const ObjectArray *array = AS_CRUX_ARRAY(value);
		print_array(array->values, array->size);
		break;
	}
	case OBJECT_STATIC_ARRAY: {
		const ObjectStaticArray *staticArray = AS_CRUX_STATIC_ARRAY(
			value);
		print_array(staticArray->values, staticArray->size);
		break;
	}
	case OBJECT_TABLE: {
		const ObjectTable *table = AS_CRUX_TABLE(value);
		print_table(table->entries, table->capacity, table->size);
		break;
	}
	case OBJECT_STATIC_TABLE: {
		const ObjectStaticTable *table = AS_CRUX_STATIC_TABLE(value);
		print_table(table->entries, table->capacity, table->size);
		break;
	}
	case OBJECT_ERROR: {
		printf("<error ");
		print_error_type(AS_CRUX_ERROR(value)->type);
		printf(">");
		break;
	}
	case OBJECT_RESULT: {
		print_result(AS_CRUX_RESULT(value));
		break;
	}
	case OBJECT_RANDOM: {
		printf("<random>");
		break;
	}
	case OBJECT_FILE: {
		printf("<file>");
		break;
	}
	case OBJECT_NATIVE_INFALLIBLE_METHOD: {
		printf("<native infallible method>");
		break;
	}

	case OBJECT_MODULE_RECORD: {
		printf("<module record>");
		break;
	}

	case OBJECT_STRUCT: {
		printf("<struct type %s>", AS_CRUX_STRUCT(value)->name->chars);
		break;
	}
	case OBJECT_STRUCT_INSTANCE: {
		print_struct_instance(AS_CRUX_STRUCT_INSTANCE(value));
		break;
	}
	case OBJECT_VECTOR: {
		const ObjectVector *vector = AS_CRUX_VECTOR(value);
		printf("Vec%d(", vector->dimensions);
		if (vector->dimensions > 4) {
			for (uint32_t i = 0; i < vector->dimensions; i++) {
				printf("%.17g", vector->as.h_components[i]);
				if (i != vector->dimensions - 1) {
					printf(", ");
				}
			}
		}else {
			for (uint32_t i = 0; i <vector->dimensions; i++) {
				printf("%.17g", vector->as.s_components[i]);
				if (i != vector->dimensions - 1) {
					printf(", ");
				}
			}
		}
		printf(")");
		break;
	} 
	}
}

ObjectString *take_string(VM *vm, char *chars, const uint32_t length)
{
	const uint32_t hash = hash_string(chars, length);

	ObjectString *interned = table_find_string(&vm->strings, chars, length,
						   hash);
	if (interned != NULL) {
		// free the string that was passed to us.
		FREE_ARRAY(vm, char, chars, length + 1);
		return interned;
	}

	return allocateString(vm, chars, length, hash);
}

ObjectString *to_string(VM *vm, const Value value)
{
	if (!IS_CRUX_OBJECT(value)) {
		char buffer[32];
		if (IS_FLOAT(value)) {
			const double num = AS_FLOAT(value);
			snprintf(buffer, sizeof(buffer), "%.17g", num);
		} else if (IS_INT(value)) {
			const int32_t num = AS_INT(value);
			snprintf(buffer, sizeof(buffer), "%d", num);
		} else if (IS_BOOL(value)) {
			strcpy(buffer, AS_BOOL(value) ? "true" : "false");
		} else if (IS_NIL(value)) {
			strcpy(buffer, "nil");
		}
		return copy_string(vm, buffer, (int)strlen(buffer));
	}

	switch (OBJECT_TYPE(value)) {
	case OBJECT_STRING:
		return AS_CRUX_STRING(value);

	case OBJECT_FUNCTION: {
		const ObjectFunction *function = AS_CRUX_FUNCTION(value);
		if (function->name == NULL) {
			return copy_string(vm, "<script>", 8);
		}
		char buffer[64];
		const int length = snprintf(buffer, sizeof(buffer), "<fn %s>",
					    function->name->chars);
		return copy_string(vm, buffer, length);
	}

	case OBJECT_NATIVE_FUNCTION: {
		const ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(
			value);
		if (native->name != NULL) {
			const char *start = "<native fn ";
			const char *end = ">";
			char *buffer = ALLOCATE(vm, char,
						strlen(start) + strlen(end) +
							native->name->length +
							1);
			strcpy(buffer, start);
			strcat(buffer, native->name->chars);
			strcat(buffer, end);
			ObjectString *result = take_string(vm, buffer,
							   strlen(buffer));
			FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
			return result;
		}
		return copy_string(vm, "<native fn>", 11);
	}

	case OBJECT_NATIVE_METHOD: {
		const ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
		if (native->name != NULL) {
			const char *start = "<native method ";
			const char *end = ">";
			char *buffer = ALLOCATE(vm, char,
						strlen(start) + strlen(end) +
							native->name->length +
							1);
			strcpy(buffer, start);
			strcat(buffer, native->name->chars);
			strcat(buffer, end);
			ObjectString *result = take_string(vm, buffer,
							   strlen(buffer));
			FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
			return result;
		}
		return copy_string(vm, "<native method>", 15);
	}

	case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
		const ObjectNativeInfallibleFunction *native =
			AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
		if (native->name != NULL) {
			const char *start = "<native infallible fn ";
			const char *end = ">";
			char *buffer = ALLOCATE(vm, char,
						strlen(start) + strlen(end) +
							native->name->length +
							1);
			strcpy(buffer, start);
			strcat(buffer, native->name->chars);
			strcat(buffer, end);
			ObjectString *result = take_string(vm, buffer,
							   strlen(buffer));
			FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
			return result;
		}
		return copy_string(vm, "<native infallible fn>", 21);
	}

	case OBJECT_NATIVE_INFALLIBLE_METHOD: {
		const ObjectNativeInfallibleMethod *native =
			AS_CRUX_NATIVE_INFALLIBLE_METHOD(value);
		if (native->name != NULL) {
			const char *start = "<native infallible method ";
			const char *end = ">";
			char *buffer = ALLOCATE(vm, char,
						strlen(start) + strlen(end) +
							native->name->length +
							1);
			strcpy(buffer, start);
			strcat(buffer, native->name->chars);
			strcat(buffer, end);
			ObjectString *result = take_string(vm, buffer,
							   strlen(buffer));
			FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
			return result;
		}
		return copy_string(vm, "<native infallible method>", 25);
	}

	case OBJECT_CLOSURE: {
		const ObjectFunction *function =
			AS_CRUX_CLOSURE(value)->function;
		if (function->name == NULL) {
			return copy_string(vm, "<script>", 8);
		}
		char buffer[256];
		const int length = snprintf(buffer, sizeof(buffer), "<fn %s>",
					    function->name->chars);
		return copy_string(vm, buffer, length);
	}

	case OBJECT_UPVALUE: {
		return copy_string(vm, "<upvalue>", 9);
	}

	case OBJECT_ARRAY: {
		const ObjectArray *array = AS_CRUX_ARRAY(value);
		size_t bufSize = 2; // [] minimum
		for (uint32_t i = 0; i < array->size; i++) {
			const ObjectString *element =
				to_string(vm, array->values[i]);
			bufSize += element->length + 2; // element + ", "
		}

		char *buffer = ALLOCATE(vm, char, bufSize);
		char *ptr = buffer;
		*ptr++ = '[';

		for (uint32_t i = 0; i < array->size; i++) {
			if (i > 0) {
				*ptr++ = ',';
				*ptr++ = ' ';
			}
			const ObjectString *element =
				to_string(vm, array->values[i]);
			memcpy(ptr, element->chars, element->length);
			ptr += element->length;
		}
		*ptr++ = ']';

		ObjectString *result = take_string(vm, buffer, ptr - buffer);
		return result;
	}

	case OBJECT_TABLE: {
		const ObjectTable *table = AS_CRUX_TABLE(value);
		size_t bufSize = 2; // {} minimum
		for (uint32_t i = 0; i < table->capacity; i++) {
			if (table->entries[i].is_occupied) {
				const ObjectString *k =
					to_string(vm, table->entries[i].key);
				const ObjectString *v =
					to_string(vm, table->entries[i].value);
				bufSize += k->length + v->length +
					   4; // key:value
			}
		}

		char *buffer = ALLOCATE(vm, char, bufSize);
		char *ptr = buffer;
		*ptr++ = '{';

		bool first = true;
		for (uint32_t i = 0; i < table->capacity; i++) {
			if (table->entries[i].is_occupied) {
				if (!first) {
					*ptr++ = ',';
					*ptr++ = ' ';
				}
				first = false;

				const ObjectString *key =
					to_string(vm, table->entries[i].key);
				const ObjectString *val =
					to_string(vm, table->entries[i].value);

				memcpy(ptr, key->chars, key->length);
				ptr += key->length;
				*ptr++ = ':';
				memcpy(ptr, val->chars, val->length);
				ptr += val->length;
			}
		}
		*ptr++ = '}';

		ObjectString *result = take_string(vm, buffer, ptr - buffer);
		return result;
	}

	case OBJECT_ERROR: {
		const ObjectError *error = AS_CRUX_ERROR(value);
		char buffer[1024];
		const int length = snprintf(buffer, sizeof(buffer),
					    "<Error: %s>",
					    error->message->chars);
		return copy_string(vm, buffer, length);
	}

	case OBJECT_RESULT: {
		const ObjectResult *result = AS_CRUX_RESULT(value);
		if (result->is_ok) {
			return copy_string(vm, "<Ok>", 4);
		}
		return copy_string(vm, "<Err>", 5);
	}

	case OBJECT_RANDOM: {
		return copy_string(vm, "<Random>", 8);
	}

	case OBJECT_FILE: {
		return copy_string(vm, "<File>", 6);
	}

	case OBJECT_STRUCT: {
		return copy_string(vm, "<Struct>", 8);
	}

	case OBJECT_STRUCT_INSTANCE: {
		return copy_string(vm, "<Struct Instance>", 17);
	}

	case OBJECT_VECTOR: {
		return copy_string(vm, "<Vec<>>", 7);
	}

	default:
		return copy_string(vm, "<Crux Object>", 13);
	}
}

ObjectFunction *new_function(VM *vm)
{
	ObjectFunction *function = ALLOCATE_OBJECT(vm, ObjectFunction,
						   OBJECT_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	function->upvalue_count = 0;
	init_chunk(&function->chunk);
	function->module_record = vm->current_module_record;
	return function;
}

ObjectNativeFunction *new_native_function(VM *vm, const CruxCallable function,
					  const int arity, ObjectString *name)
{
	ObjectNativeFunction *native = ALLOCATE_OBJECT(vm, ObjectNativeFunction,
						       OBJECT_NATIVE_FUNCTION);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeMethod *new_native_method(VM *vm, const CruxCallable function,
				      const int arity, ObjectString *name)
{
	ObjectNativeMethod *native = ALLOCATE_OBJECT(vm, ObjectNativeMethod,
						     OBJECT_NATIVE_METHOD);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeInfallibleFunction *
new_native_infallible_function(VM *vm, const CruxInfallibleCallable function,
			       const int arity, ObjectString *name)
{
	ObjectNativeInfallibleFunction *native =
		ALLOCATE_OBJECT(vm, ObjectNativeInfallibleFunction,
				OBJECT_NATIVE_INFALLIBLE_FUNCTION);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeInfallibleMethod *
new_native_infallible_method(VM *vm, const CruxInfallibleCallable function,
			     const int arity, ObjectString *name)
{
	ObjectNativeInfallibleMethod *native =
		ALLOCATE_OBJECT(vm, ObjectNativeInfallibleMethod,
				OBJECT_NATIVE_INFALLIBLE_METHOD);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectTable *new_table(VM *vm, const int element_count,
		       ObjectModuleRecord *module_record)
{
	GC_PROTECT_START(module_record);
	ObjectTable *table = ALLOCATE_OBJECT(vm, ObjectTable, OBJECT_TABLE);
	GC_PROTECT(module_record, OBJECT_VAL(table));
	table->capacity = 0;
	table->size = 0;
	table->entries = NULL;
	const uint32_t newCapacity = element_count < 16
					     ? 16
					     : calculateCollectionCapacity(
						       element_count);
	table->entries = ALLOCATE(vm, ObjectTableEntry, newCapacity);
	for (uint32_t i = 0; i < table->capacity; i++) {
		table->entries[i].value = NIL_VAL;
		table->entries[i].key = NIL_VAL;
		table->entries[i].is_occupied = false;
	}
	GC_PROTECT_END(module_record);
	return table;
}

void free_object_table(VM *vm, ObjectTable *table)
{
	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = NULL;
	table->capacity = 0;
	table->size = 0;
}

void free_object_static_table(VM *vm, ObjectStaticTable *table)
{
	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = NULL;
	table->capacity = 0;
	table->size = 0;
}

ObjectFile *new_object_file(VM *vm, ObjectString *path, ObjectString *mode)
{
	ObjectFile *file = ALLOCATE_OBJECT(vm, ObjectFile, OBJECT_FILE);
	file->path = path;
	file->mode = mode;
	file->file = fopen(path->chars, mode->chars);
	file->is_open = file->file != NULL;
	file->position = 0;
	return file;
}

/**
 * @brief Finds an entry in an object table.
 *
 * This static helper function finds an entry in an ObjectTable's entry array
 * based on a given key. It uses open addressing with quadratic probing to
 * resolve collisions. It also handles tombstones (entries that were
 * previously occupied but are now deleted) to allow for rehashing after
 * deletions.
 *
 * @param entries The array of ObjectTableEntry.
 * @param capacity The capacity of the table's entry array.
 * @param key The key Value to search for.
 *
 * @return A pointer to the ObjectTableEntry for the key, or a pointer to an
 * empty entry (possibly a tombstone) if the key is not found.
 */
static ObjectTableEntry *find_entry(ObjectTableEntry *entries,
				    const uint16_t capacity, const Value key)
{
	const uint32_t hash = hashValue(key);
	uint32_t index = hash & (capacity - 1);
	ObjectTableEntry *tombstone = NULL;

	while (1) {
		ObjectTableEntry *entry = &entries[index];
		if (!entry->is_occupied) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL) {
				tombstone = entry;
			}
		} else if (values_equal(entry->key, key)) {
			return entry;
		}
		index = (index * 5 + 1) & (capacity - 1); // new probe
	}
}

/**
 * @brief Adjusts the capacity of an object table.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to resize.
 * @param capacity The new capacity for the table.
 *
 * @return true if the capacity adjustment was successful, false otherwise
 * (e.g., memory allocation failure).
 */
static bool adjust_capacity(VM *vm, ObjectTable *table, const int capacity)
{
	ObjectTableEntry *entries = ALLOCATE(vm, ObjectTableEntry, capacity);
	if (entries == NULL) {
		return false;
	}

	for (int i = 0; i < capacity; i++) {
		entries[i].key = NIL_VAL;
		entries[i].value = NIL_VAL;
		entries[i].is_occupied = false;
	}

	table->size = 0;

	for (uint32_t i = 0; i < table->capacity; i++) {
		const ObjectTableEntry *entry = &table->entries[i];
		if (!entry->is_occupied) {
			continue;
		}

		ObjectTableEntry *destination = find_entry(entries, capacity,
							   entry->key);

		destination->key = entry->key;
		destination->value = entry->value;
		destination->is_occupied = true;
		table->size++;
	}

	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
	return true;
}

bool object_table_set(VM *vm, ObjectTable *table, const Value key,
		      const Value value)
{
	if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
		const int capacity = GROW_CAPACITY(table->capacity);
		push(vm->current_module_record, OBJECT_VAL(table));
		if (!adjust_capacity(vm, table, capacity)) {
			pop(vm->current_module_record);
			return false;
		}
		pop(vm->current_module_record);
	}

	ObjectTableEntry *entry = find_entry(table->entries, table->capacity,
					     key);
	const bool isNewKey = !entry->is_occupied;

	if (isNewKey) {
		table->size++;
	}

	if (IS_CRUX_OBJECT(key))
		mark_value(vm, key);
	if (IS_CRUX_OBJECT(value))
		mark_value(vm, value);

	entry->key = key;
	entry->value = value;
	entry->is_occupied = true;

	return true;
}

bool object_table_remove(ObjectTable *table, const Value key)
{
	if (!table) {
		return false;
	}
	ObjectTableEntry *entry = find_entry(table->entries, table->capacity,
					     key);
	if (!entry->is_occupied) {
		return false;
	}
	entry->is_occupied = false;
	entry->key = NIL_VAL;
	entry->value = BOOL_VAL(false);
	table->size--;
	return true;
}

bool object_table_contains_key(ObjectTable *table, const Value key)
{
	if (!table)
		return false;

	const ObjectTableEntry *entry = find_entry(table->entries,
						   table->capacity, key);
	return entry->is_occupied;
}

bool entriesContainsKey(ObjectTableEntry *entries, const Value key,
			const uint32_t capacity)
{
	if (!entries)
		return false;
	const ObjectTableEntry *entry = find_entry(entries, capacity, key);
	return entry->is_occupied;
}

bool object_table_get(ObjectTableEntry *entries, const uint32_t size,
		      const uint32_t capacity, const Value key, Value *value)
{
	if (size == 0) {
		return false;
	}

	const ObjectTableEntry *entry = find_entry(entries, capacity, key);
	if (!entry->is_occupied) {
		return false;
	}
	*value = entry->value;
	return true;
}

ObjectArray *new_array(VM *vm, const uint32_t element_count,
		       ObjectModuleRecord *module_record)
{
	GC_PROTECT_START(module_record);
	ObjectArray *array = ALLOCATE_OBJECT(vm, ObjectArray, OBJECT_ARRAY);
	GC_PROTECT(module_record, OBJECT_VAL(array));
	array->capacity = calculateCollectionCapacity(element_count);
	array->size = 0;
	array->values = ALLOCATE(vm, Value, array->capacity);
	for (uint32_t i = 0; i < array->capacity; i++) {
		array->values[i] = NIL_VAL;
	}
	GC_PROTECT_END(module_record);
	return array;
}

bool ensure_capacity(VM *vm, ObjectArray *array, const uint32_t capacity_needed)
{
	if (capacity_needed <= array->capacity) {
		return true;
	}
	uint32_t newCapacity = array->capacity;
	while (newCapacity < capacity_needed) {
		if (newCapacity > INT_MAX / 2) {
			return false;
		}
		newCapacity *= 2;
	}
	Value *newArray = GROW_ARRAY(vm, Value, array->values, array->capacity,
				     newCapacity);
	if (newArray == NULL) {
		return false;
	}
	for (uint32_t i = array->capacity; i < newCapacity; i++) {
		newArray[i] = NIL_VAL;
	}
	array->values = newArray;
	array->capacity = newCapacity;
	return true;
}

bool array_set(VM *vm, const ObjectArray *array, const uint32_t index,
	       const Value value)
{
	if (index >= array->size) {
		return false;
	}
	if (IS_CRUX_OBJECT(value)) {
		mark_value(vm, value);
	}
	array->values[index] = value;
	return true;
}

bool array_add(VM *vm, ObjectArray *array, const Value value,
	       const uint32_t index)
{
	if (!ensure_capacity(vm, array, array->size + 1)) {
		return false;
	}
	if (IS_CRUX_OBJECT(value)) {
		mark_value(vm, value);
	}
	array->values[index] = value;
	array->size++;
	return true;
}

bool array_add_back(VM *vm, ObjectArray *array, const Value value)
{
	if (!ensure_capacity(vm, array, array->size + 1)) {
		return false;
	}
	array->values[array->size] = value;
	array->size++;
	return true;
}

ObjectError *new_error(VM *vm, ObjectString *message, const ErrorType type,
		       const bool is_panic)
{
	ObjectError *error = ALLOCATE_OBJECT(vm, ObjectError, OBJECT_ERROR);
	error->message = message;
	error->type = type;
	error->is_panic = is_panic;
	return error;
}

ObjectResult *new_ok_result(VM *vm, const Value value)
{
	ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
	result->is_ok = true;
	result->as.value = value;
	return result;
}

ObjectResult *new_error_result(VM *vm, ObjectError *error)
{
	ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
	result->is_ok = false;
	result->as.error = error;
	return result;
}

ObjectRandom *new_random(VM *vm)
{
	ObjectRandom *random = ALLOCATE_OBJECT(vm, ObjectRandom, OBJECT_RANDOM);
#ifdef _WIN32
	random->seed = (uint64_t)time(NULL) ^ GetTickCount() ^
		       (uint64_t)GetCurrentProcessId();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	random->seed = tv.tv_sec ^ tv.tv_usec ^ (uint64_t)getpid();
#endif
	return random;
}

void free_module_record(VM *vm, ObjectModuleRecord *module_record)
{
	if (module_record == NULL)
		return;

	free_object_module_record(vm, module_record);
}

ObjectModuleRecord *new_object_module_record(const VM *vm, ObjectString *path,
					     const bool is_repl,
					     const bool is_main)
{
	ObjectModuleRecord *moduleRecord = ALLOCATE_OBJECT_WITHOUT_GC(
		vm, ObjectModuleRecord, OBJECT_MODULE_RECORD);
	moduleRecord->path = path;
	init_table(&moduleRecord->globals);
	init_table(&moduleRecord->publics);
	moduleRecord->state = STATE_LOADING;
	moduleRecord->module_closure = NULL;
	moduleRecord->enclosing_module = NULL;

	moduleRecord->stack = (Value *)malloc(STACK_MAX * sizeof(Value));
	moduleRecord->stack_top = moduleRecord->stack;
	moduleRecord->stack_limit = moduleRecord->stack + STACK_MAX;
	moduleRecord->open_upvalues = NULL;

	moduleRecord->frames = (CallFrame *)malloc(FRAMES_MAX *
						   sizeof(CallFrame));
	moduleRecord->frame_count = 0;
	moduleRecord->frame_capacity = FRAMES_MAX;

	moduleRecord->is_main = is_main;
	moduleRecord->is_repl = is_repl;

	return moduleRecord;
}

/**
 * Frees ObjectModuleRecord internals
 * @param vm The VM
 * @param record The ObjectModuleRecord to free
 */
void free_object_module_record(VM *vm, ObjectModuleRecord *record)
{
	free(record->frames);
	record->frames = NULL;
	free(record->stack);
	record->stack = NULL;
	free_table(vm, &record->globals);
	free_table(vm, &record->publics);
}

ObjectStaticArray *new_static_array(VM *vm, const uint16_t element_count,
				    ObjectModuleRecord *module_record)
{
	GC_PROTECT_START(module_record);
	ObjectStaticArray *array = ALLOCATE_OBJECT(vm, ObjectStaticArray,
						   OBJECT_STATIC_ARRAY);
	array->values = NULL;
	array->size = 0;
	GC_PROTECT(module_record, OBJECT_VAL(array));
	array->size = element_count;
	array->values = ALLOCATE(vm, Value, element_count);
	GC_PROTECT_END(module_record);
	return array;
}

ObjectStaticTable *new_static_table(VM *vm, const uint16_t element_count,
				    ObjectModuleRecord *module_record)
{
	GC_PROTECT_START(module_record);
	ObjectStaticTable *table = ALLOCATE_OBJECT(vm, ObjectStaticTable,
						   OBJECT_STATIC_TABLE);
	table->capacity = 0;
	table->size = 0;
	table->entries = NULL;
	const uint32_t newCapacity = calculateCollectionCapacity(
		(uint32_t)((1 + TABLE_MAX_LOAD) * element_count));
	GC_PROTECT(module_record, OBJECT_VAL(table));

	table->entries = ALLOCATE(vm, ObjectTableEntry, newCapacity);
	for (uint32_t i = 0; i < table->capacity; i++) {
		table->entries[i].value = NIL_VAL;
		table->entries[i].key = NIL_VAL;
		table->entries[i].is_occupied = false;
	}
	GC_PROTECT_END(module_record);
	return table;
}

bool object_static_table_set(VM *vm, ObjectStaticTable *table, const Value key,
			     const Value value)
{
	ObjectTableEntry *entry = find_entry(table->entries, table->capacity,
					     key);
	const bool isNewKey = !entry->is_occupied;
	if (isNewKey) {
		table->size++;
	}

	if (IS_CRUX_OBJECT(key))
		mark_value(vm, key);
	if (IS_CRUX_OBJECT(value))
		mark_value(vm, value);

	entry->key = key;
	entry->value = value;
	entry->is_occupied = true;

	return true;
}

ObjectStruct *new_struct_type(VM *vm, ObjectString *name)
{
	ObjectStruct *structObject = ALLOCATE_OBJECT(vm, ObjectStruct,
						     OBJECT_STRUCT);
	structObject->name = name;
	init_table(&structObject->fields);
	return structObject;
}

ObjectStructInstance *new_struct_instance(VM *vm, ObjectStruct *struct_type,
					  const uint16_t field_count,
					  ObjectModuleRecord *module_record)
{
	GC_PROTECT_START(module_record);
	ObjectStructInstance *structInstance = ALLOCATE_OBJECT(
		vm, ObjectStructInstance, OBJECT_STRUCT_INSTANCE);
	GC_PROTECT(module_record, OBJECT_VAL(structInstance));
	structInstance->struct_type = struct_type;
	structInstance->fields = NULL;
	structInstance->field_count = 0;
	structInstance->fields = ALLOCATE(vm, Value, field_count);
	for (int i = 0; i < field_count; i++) {
		structInstance->fields[i] = NIL_VAL;
	}
	structInstance->field_count = field_count;
	GC_PROTECT_END(module_record);
	return structInstance;
}

ObjectVector *new_vector(VM *vm, const uint32_t dimensions)
{
	ObjectVector *vector = ALLOCATE_OBJECT(vm, ObjectVector, OBJECT_VECTOR);
	push(vm->current_module_record, OBJECT_VAL(vector));
	vector->dimensions = dimensions;
	if (vector->dimensions > 4) {
		vector->as.h_components = ALLOCATE(vm, double, dimensions);
	}
	pop(vm->current_module_record);
	return vector;
}