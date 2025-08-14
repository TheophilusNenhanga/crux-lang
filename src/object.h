#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"
#include "vm/vm.h"

#define GC_PROTECT_START(currentModuleRecord)                                  \
	Value *gc_stack_start = (currentModuleRecord)->stack_top
#define GC_PROTECT(currentModuleRecord, value)                                 \
	push((currentModuleRecord), (value))
#define GC_PROTECT_END(currentModuleRecord)                                    \
	(currentModuleRecord)->stack_top = gc_stack_start

#define STATIC_STRING_LEN(static_string) sizeof((static_string)) - 1

// Only works with string literals -- gc_safe_static_message
#define MAKE_GC_SAFE_ERROR(vm, gc_safe_static_message, gcSafeErrorType)           \
	({                                                                     \
		GC_PROTECT_START((vm)->currentModuleRecord);                   \
		ObjectString *message =                                        \
			copy_string((vm), (gc_safe_static_message),                \
				   STATIC_STRING_LEN((gc_safe_static_message)));  \
		GC_PROTECT((vm)->currentModuleRecord, OBJECT_VAL(message));    \
		ObjectError *gcSafeError = new_error((vm), message,             \
						    (gcSafeErrorType), false); \
		GC_PROTECT((vm)->currentModuleRecord,                          \
			   OBJECT_VAL(gcSafeError));                           \
		ObjectResult *gcSafeErrorResult = new_error_result((vm),         \
								 gcSafeError); \
		GC_PROTECT_END((vm)->currentModuleRecord);                     \
		gcSafeErrorResult;                                             \
	})

#define MAKE_GC_SAFE_RESULT(vm, alreadySafeValue)                              \
	({                                                                     \
		GC_PROTECT_START((vm)->currentModuleRecord);                   \
		GC_PROTECT((vm)->currentModuleRecord, alreadySafeValue);       \
		ObjectResult *gcSafeResult = newOkResult((vm),                 \
							 alreadySafeValue);    \
		GC_PROTECT_END((vm)->currentModuleRecord);                     \
		gcSafeResult;                                                  \
	})

#define MAKE_GC_SAFE_RESULT_WITH_ALLOC(vm, allocatingExpression)               \
	({                                                                     \
		GC_PROTECT_START((vm)->currentModuleRecord);                   \
		Value allocatedValue = (allocatingExpression);                 \
		GC_PROTECT((vm)->currentModuleRecord, allocatedValue);         \
		ObjectResult *gcSafeResultWithAlloc =                          \
			newOkResult((vm), allocatedValue);                     \
		GC_PROTECT_END((vm)->currentModuleRecord);                     \
		gcSafeResultWithAlloc;                                         \
	})

#define OBJECT_TYPE(value) (AS_CRUX_OBJECT(value)->type)

#define IS_CRUX_STRING(value) is_object_type(value, OBJECT_STRING)
#define IS_CRUX_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_CRUX_NATIVE_FUNCTION(value)                                         \
	is_object_type(value, OBJECT_NATIVE_FUNCTION)
#define IS_CRUX_NATIVE_METHOD(value) is_object_type(value, OBJECT_NATIVE_METHOD)
#define IS_CRUX_CLOSURE(value) is_object_type(value, OBJECT_CLOSURE)
#define IS_CRUX_BOUND_METHOD(value) is_object_type(value, OBJECT_BOUND_METHOD)
#define IS_CRUX_ARRAY(value) is_object_type(value, OBJECT_ARRAY)
#define IS_CRUX_TABLE(value) is_object_type(value, OBJECT_TABLE)
#define IS_CRUX_ERROR(value) is_object_type(value, OBJECT_ERROR)
#define IS_CRUX_RESULT(value) is_object_type(value, OBJECT_RESULT)
#define IS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value)                              \
	is_object_type(value, OBJECT_NATIVE_INFALLIBLE_FUNCTION)
#define IS_CRUX_NATIVE_INFALLIBLE_METHOD(value)                                \
	is_object_type(value, OBJECT_NATIVE_INFALLIBLE_METHOD)
#define IS_CRUX_RANDOM(value) is_object_type(value, OBJECT_RANDOM)
#define IS_CRUX_FILE(value) is_object_type(value, OBJECT_FILE)
#define IS_CRUX_MODULE_RECORD(value) is_object_type(value, OBJECT_MODULE_RECORD)
#define IS_CRUX_STATIC_ARRAY(value) is_object_type(value, OBJECT_STATIC_ARRAY)
#define IS_CRUX_STATIC_TABLE(value) is_object_type(value, OBJECT_STATIC_TABLE)
#define IS_CRUX_STRUCT(value) is_object_type(value, OBJECT_STRUCT)
#define IS_CRUX_STRUCT_INSTANCE(value)                                         \
	is_object_type(value, OBJECT_STRUCT_INSTANCE)
#define IS_CRUX_VEC2(value) is_object_type(value, OBJECT_VEC2)
#define IS_CRUX_VEC3(value) is_object_type(value, OBJECT_VEC3)
#define AS_CRUX_STRING(value) ((ObjectString *)AS_CRUX_OBJECT(value))

#define AS_C_STRING(value) (((ObjectString *)AS_CRUX_OBJECT(value))->chars)
#define AS_CRUX_FUNCTION(value) ((ObjectFunction *)AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_FUNCTION(value)                                         \
	((ObjectNativeFunction *)AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_METHOD(value)                                           \
	((ObjectNativeMethod *)AS_CRUX_OBJECT(value))
#define AS_CRUX_CLOSURE(value) ((ObjectClosure *)AS_CRUX_OBJECT(value))
#define AS_CRUX_BOUND_METHOD(value) ((ObjectBoundMethod *)AS_CRUX_OBJECT(value))
#define AS_CRUX_ARRAY(value) ((ObjectArray *)AS_CRUX_OBJECT(value))
#define AS_CRUX_TABLE(value) ((ObjectTable *)AS_CRUX_OBJECT(value))
#define AS_CRUX_ERROR(value) ((ObjectError *)AS_CRUX_OBJECT(value))
#define AS_CRUX_RESULT(value) ((ObjectResult *)AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value)                              \
	((ObjectNativeInfallibleFunction *)AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_INFALLIBLE_METHOD(value)                                \
	((ObjectNativeInfallibleMethod *)AS_CRUX_OBJECT(value))
#define AS_CRUX_RANDOM(value) ((ObjectRandom *)AS_CRUX_OBJECT(value))
#define AS_CRUX_FILE(value) ((ObjectFile *)AS_CRUX_OBJECT(value))
#define AS_CRUX_MODULE_RECORD(value)                                           \
	((ObjectModuleRecord *)AS_CRUX_OBJECT(value))
#define AS_CRUX_UPVALUE(value) ((ObjectUpvalue *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STATIC_ARRAY(value) ((ObjectStaticArray *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STATIC_TABLE(value) ((ObjectStaticTable *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STRUCT(value) ((ObjectStruct *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STRUCT_INSTANCE(value)                                         \
	((ObjectStructInstance *)AS_CRUX_OBJECT(value))
#define AS_CRUX_VEC2(value) ((ObjectVec2 *)AS_CRUX_OBJECT(value))
#define AS_CRUX_VEC3(value) ((ObjectVec3 *)AS_CRUX_OBJECT(value))

#define IS_CRUX_HASHABLE(value)                                                \
	(IS_INT(value) || IS_FLOAT(value) || IS_CRUX_STRING(value) ||          \
	 IS_NIL(value) || IS_BOOL(value))

typedef enum {
	OBJECT_STRING,
	OBJECT_FUNCTION,
	OBJECT_NATIVE_FUNCTION,
	OBJECT_NATIVE_METHOD,
	OBJECT_CLOSURE,
	OBJECT_UPVALUE,
	OBJECT_ARRAY,
	OBJECT_TABLE,
	OBJECT_ERROR,
	OBJECT_RESULT,
	OBJECT_NATIVE_INFALLIBLE_FUNCTION,
	OBJECT_NATIVE_INFALLIBLE_METHOD,
	OBJECT_RANDOM,
	OBJECT_FILE,
	OBJECT_MODULE_RECORD,
	OBJECT_STATIC_ARRAY,
	OBJECT_STATIC_TABLE,
	OBJECT_STRUCT,
	OBJECT_STRUCT_INSTANCE,
	OBJECT_VEC2,
	OBJECT_VEC3,
} ObjectType;

struct Object {
	Object *next;
	ObjectType type;
	bool is_marked;
};

struct ObjectString {
	Object Object;
	char *chars;
	uint32_t length; // this is the length without the null terminator
	uint32_t hash;
};

typedef struct {
	Object object;
	int arity;
	int upvalue_count;
	Chunk chunk;
	ObjectString *name;
	ObjectModuleRecord *module_record;
} ObjectFunction;

typedef struct ObjectUpvalue {
	Object object;
	Value *location;
	Value closed;
	ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct ObjectClosure {
	Object object;
	ObjectFunction *function;
	ObjectUpvalue **upvalues;
	int upvalue_count;
} ObjectClosure;

typedef struct {
	Object object;
	Value *values;
	uint32_t size;
	uint32_t capacity;
} ObjectArray;

typedef struct {
	Object object;
	Value *values;
	uint32_t size; // size and capacity will always be the same
	uint32_t hash;
} ObjectStaticArray;

typedef struct {
	Object object;
	uint64_t seed;
} ObjectRandom;

typedef enum {
	SYNTAX,
	MATH, // Division by zero
	BOUNDS, // Index out of bounds
	RUNTIME,
	TYPE,
	LOOP_EXTENT,
	LIMIT,
	BRANCH_EXTENT,
	CLOSURE_EXTENT,
	LOCAL_EXTENT,
	ARGUMENT_EXTENT,
	NAME,
	COLLECTION_EXTENT,
	VARIABLE_EXTENT,
	RETURN_EXTENT,
	ARGUMENT_MISMATCH,
	STACK_OVERFLOW,
	COLLECTION_GET,
	COLLECTION_SET,
	UNPACK_MISMATCH,
	MEMORY,
	VALUE, // correct type, but incorrect value
	ASSERT,
	IMPORT_EXTENT,
	IO,
	IMPORT,
} ErrorType;

typedef struct {
	Object object;
	ObjectString *message;
	ErrorType type;
	bool is_panic;
} ObjectError;

struct ObjectResult {
	Object object;
	bool is_ok;
	union {
		Value value;
		ObjectError *error;
	} as;
};

typedef ObjectResult *(*CruxCallable)(VM *vm, int arg_count, const Value *args);
typedef Value (*CruxInfallibleCallable)(VM *vm, int arg_count,
					const Value *args);

typedef struct {
	Object object;
	CruxCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeFunction;

typedef struct {
	Object object;
	CruxCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeMethod;

typedef struct {
	Object object;
	CruxInfallibleCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeInfallibleFunction;

typedef struct {
	Object object;
	CruxInfallibleCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeInfallibleMethod;

typedef struct {
	Value key;
	Value value;
	bool is_occupied;
} ObjectTableEntry;

typedef struct {
	Object object;
	ObjectTableEntry *entries;
	uint32_t capacity;
	uint32_t size;
} ObjectTable;

typedef struct {
	Object object;
	ObjectTableEntry *entries;
	uint32_t capacity;
	uint32_t size;
} ObjectStaticTable;

typedef struct {
	Object object;
	ObjectString *path;
	ObjectString *mode;
	FILE *file;
	uint64_t position;
	bool is_open;
} ObjectFile;

typedef struct {
	Object object;
	ObjectString *name;
	Table fields; // <field_name: index>
} ObjectStruct;

struct ObjectStructInstance {
	Object object;
	ObjectStruct *struct_type;
	Value *fields;
	uint16_t field_count;
};

typedef struct {
	Object object;
	double x;
	double y;
} ObjectVec2;

typedef struct {
	Object object;
	double x;
	double y;
	double z;
} ObjectVec3;

typedef enum {
	STATE_LOADING,
	STATE_LOADED,
	STATE_ERROR,
} ModuleState;

struct ObjectModuleRecord {
	Object object;
	ObjectString *path;
	Table globals;
	Table publics;
	ObjectClosure *module_closure;
	ObjectModuleRecord *enclosing_module;
	ObjectUpvalue *open_upvalues;
	Value *stack;
	Value *stack_top;
	Value *stack_limit;
	CallFrame *frames;
	uint8_t frame_count;
	uint8_t frame_capacity;
	bool is_repl;
	bool is_main;
	ModuleState state;
};

static bool is_object_type(const Value value, const ObjectType type)
{
	return IS_CRUX_OBJECT(value) && AS_CRUX_OBJECT(value)->type == type;
}

/**
 * @brief Creates a new error object.
 *
 * This function allocates and initializes a new ObjectError, representing
 * an error in the crux language.
 *
 * @param vm The virtual machine.
 * @param message The error message as an ObjectString.
 * @param type The ErrorType enum value representing the type of error.
 * @param is_panic A boolean indicating whether this error represents a panic
 * (fatal error).
 *
 * @return A pointer to the newly created ObjectError.
 */
ObjectError *new_error(VM *vm, ObjectString *message, ErrorType type,
		      bool is_panic);

/**
 * @brief Creates a new upvalue object.
 *
 * Upvalues are used to implement closures, capturing variables from enclosing
 * scopes. This function allocates and initializes a new ObjectUpvalue.
 *
 * @param vm The virtual machine.
 * @param slot A pointer to the stack slot where the upvalue captures a
 * variable.
 *
 * @return A pointer to the newly created ObjectUpvalue.
 */
ObjectUpvalue *new_upvalue(VM *vm, Value *slot);

/**
 * @brief Creates a new closure object.
 *
 * Closures are function objects that capture their surrounding environment.
 * This function allocates and initializes a new ObjectClosure, linking it
 * to its underlying ObjectFunction and allocating space for upvalues.
 *
 * @param vm The virtual machine.
 * @param function The ObjectFunction to be wrapped in the closure.
 *
 * @return A pointer to the newly created ObjectClosure.
 */
ObjectClosure *new_closure(VM *vm, ObjectFunction *function);

/**
 * @brief Creates a new native function object.
 *
 * Native functions are implemented in C and callable from crux code.
 * This function allocates and initializes a new ObjectNativeFunction.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native function's
 * implementation.
 * @param arity The expected number of arguments for the native function.
 * @param name The name of the native function as an ObjectString (for
 * debugging).
 *
 * @return A pointer to the newly created ObjectNativeFunction.
 */
ObjectNativeFunction *new_native_function(VM *vm, CruxCallable function,
					int arity, ObjectString *name);

/**
 * @brief Creates a new native method object.
 *
 * Native methods are similar to native functions but are associated with
 * classes. This function allocates and initializes a new ObjectNativeMethod.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native method's
 * implementation.
 * @param arity The expected number of arguments for the native method.
 * @param name The name of the native method as an ObjectString (for debugging).
 *
 * @return A pointer to the newly created ObjectNativeMethod.
 */
ObjectNativeMethod *new_native_method(VM *vm, CruxCallable function, int arity,
				    ObjectString *name);

/**
 * @brief Creates a new native infallible function object.
 *
 * Native infallible functions are implemented in C, callable from crux code,
 * and guaranteed not to return errors. This function allocates and initializes
 * a new ObjectNativeInfallibleFunction.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native function's
 * implementation.
 * @param arity The expected number of arguments for the native function.
 * @param name The name of the native function as an ObjectString (for
 * debugging).
 *
 * @return A pointer to the newly created ObjectNativeInfallibleFunction.
 */
ObjectNativeInfallibleFunction *
new_native_infallible_function(VM *vm, CruxInfallibleCallable function, int arity,
			    ObjectString *name);

/**
 * @brief Creates a new native infallible method object.
 *
 * Native infallible methods are similar to native infallible functions but are
 * associated with classes. This function allocates and initializes a new
 * ObjectNativeInfallibleMethod.
 */
ObjectNativeInfallibleMethod *
new_native_infallible_method(VM *vm, CruxInfallibleCallable function, int arity,
			  ObjectString *name);

/**
 * @brief Creates a new function object.
 *
 * This function allocates and initializes a new ObjectFunction, representing
 * a function in the crux language. It sets default values for arity, name,
 * and upvalue count, and initializes the function's chunk.
 *
 * @param vm The virtual machine.
 *
 * @return A pointer to the newly created ObjectFunction.
 */
ObjectFunction *new_function(VM *vm);

/**
 * @brief Creates a new object table.
 *
 * Object tables are hash tables used to store key-value pairs. This function
 * allocates and initializes a new ObjectTable with a given initial capacity.
 *
 * @param vm The virtual machine.
 * @param element_count Hint for initial table size. The capacity will be the
 * next power of 2 greater than or equal to `elementCount` (or 16 if
 * `elementCount` is less than 16).
 * @param module_record
 *
 * @return A pointer to the newly created ObjectTable.
 */
ObjectTable *new_table(VM *vm, int element_count,
		      ObjectModuleRecord *module_record);

/**
 * @brief Creates a new ok result with a boxed value.
 *
 * This function creates a new ObjectResult representing a successful operation,
 * boxing a given Value within it.
 *
 * @param vm the virtual machine (for garbage collection purposes)
 * @param value The Value to box in the result.
 *
 * @return A pointer to the result with the value.
 */
ObjectResult *new_ok_result(VM *vm, Value value);

/**
 * @brief Creates a new error result with a boxed error.
 *
 * This function creates a new ObjectResult representing a failed operation,
 * boxing a given ObjectError within it.
 *
 * @param vm the virtual machine (for garbage collection purposes)
 * @param error The error to box in the result.
 *
 * @return A pointer to the result with the error.
 */
ObjectResult *new_error_result(VM *vm, ObjectError *error);

/**
 * @brief Creates a new array object.
 *
 * This function allocates and initializes a new ObjectArray, representing
 * a dynamic array in the crux language. It allocates memory for the array
 * elements and sets initial capacity and size.
 *
 * @param vm The virtual machine.
 * @param element_count Hint for initial array size. The capacity will be the
 * next power of 2 greater than or equal to `elementCount`.
 * @param module_record
 *
 * @return A pointer to the newly created ObjectArray.
 */
ObjectArray *new_array(VM *vm, uint32_t element_count,
		      ObjectModuleRecord *module_record);

/**
 * @brief Creates a new string object and takes ownership of the given character
 * buffer.
 *
 * This function creates a new ObjectString, taking ownership of the provided
 * character buffer. This means the function is responsible for freeing the
 * memory pointed to by `chars` when the string object is no longer needed.
 * It also interns the string. If an identical string already exists in the
 * string table, the provided `chars` buffer is freed, and the existing interned
 * string is returned.
 *
 * @param vm The crux Virtual Machine.
 * @param chars The character buffer to be owned by the ObjectString. This
 * buffer will be freed if the string is interned or when the ObjectString is
 * garbage collected.
 * @param length The length of the string.
 *
 * @return A pointer to the interned ObjectString.
 */
ObjectString *take_string(VM *vm, char *chars, uint32_t length);

/**
 * @brief Creates a new string object by copying a C-style string.
 *
 * This function creates a copy of a given C-style string on the heap and
 * wraps it in an ObjectString. It also interns the string to ensure that
 * identical strings share the same object in memory.
 *
 * @param vm The virtual machine.
 * @param chars The null-terminated C-style string to copy.
 * @param length The length of the string (excluding null terminator).
 *
 * @return A pointer to the interned ObjectString.
 */
ObjectString *copy_string(VM *vm, const char *chars, uint32_t length);

/**
 * @brief Converts a Value to its string representation.
 *
 * This function converts a Value to its string representation, returning an
 * ObjectString. It handles different value types, including numbers, booleans,
 * nil, and various object types, creating a string representation suitable
 * for display or string operations. For composite objects like arrays and
 * tables, it recursively converts their elements/entries to strings.
 *
 * @param vm The virtual machine.
 * @param value The Value to convert to a string.
 *
 * @return An ObjectString representing the Value.
 */
ObjectString *to_string(VM *vm, Value value);

/**
 * @brief Prints a Value to the console for debugging purposes.
 *
 * This function provides a human-readable representation of a Value.
 * @param value The Value to print.
 * @param in_collection is this object in a collection?
 */
void print_object(Value value, bool in_collection);

void print_type(Value value);

/**
 * @brief Frees the memory associated with an object table.
 *
 * This function frees the memory allocated for the entries array of an
 * ObjectTable. It is called during garbage collection when the table is no
 * longer reachable.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to free.
 */
void free_object_table(VM *vm, ObjectTable *table);

void free_object_module_record(VM *vm, ObjectModuleRecord *record);

/**
 * @brief Sets a key-value pair in an object table.
 *
 * This function inserts or updates a key-value pair in an ObjectTable. It
 * resizes the table if necessary to maintain performance and handles key
 * collisions using open addressing.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to modify.
 * @param key The key Value.
 * @param value The value Value.
 *
 * @return true if the set operation was successful, false otherwise (e.g.,
 * memory allocation failure during resizing).
 */
bool object_table_set(VM *vm, ObjectTable *table, Value key, Value value);

/**
 * @brief Retrieves a value from an object table by key.
 *
 * This function looks up a value in an ObjectTable based on a given key.
 *
 * @param entries The table's entries
 * @param size The number of actual entries
 * @param capacity The maximum capacity of entries
 * @param key The key Value to look up.
 * @param value A pointer to a Value where the retrieved value will be stored if
 * the key is found.
 *
 * @return true if the key was found and the value was retrieved, false
 * otherwise.
 */
bool object_table_get(ObjectTableEntry *entries, uint32_t size, uint32_t capacity,
		    Value key, Value *value);

void mark_object_table(VM *vm, const ObjectTableEntry *entries,
		     uint32_t capacity);

/**
 * @brief Ensures that an array has enough capacity.
 *
 * This function checks if an ObjectArray has enough capacity to hold a
 * required number of elements. If not, it resizes the array, doubling its
 * capacity until it meets the requirement.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to check and resize.
 * @param capacity_needed The minimum capacity required.
 *
 * @return true if capacity is ensured (either already sufficient or resizing
 * successful), false if resizing failed (e.g., memory allocation failure).
 */
bool ensure_capacity(VM *vm, ObjectArray *array, uint32_t capacity_needed);

/**
 * @brief Sets a value at a specific index in an array.
 *
 * This function sets a Value at a given index in an ObjectArray. It performs
 * bound checking to ensure the index is within the array's current size.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to modify.
 * @param index The index at which to set the value.
 * @param value The Value to set.
 *
 * @return true if the value was set successfully (index within bounds), false
 * otherwise (index out of bounds).
 */
bool array_set(VM *vm, const ObjectArray *array, uint32_t index, Value value);

/**
 * @brief Adds a value to the end of an array.
 *
 * This function appends a Value to the end of an ObjectArray, increasing its
 * size. It ensures sufficient capacity before adding the element.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to modify.
 * @param value The Value to add.
 * @param index The index at which to add the value, this should be
 * `array->size`.
 *
 * @return true if the value was added successfully, false otherwise (e.g.,
 * memory allocation failure during resizing).
 */
bool array_add(VM *vm, ObjectArray *array, Value value, uint32_t index);

/**
 * @brief Adds a value to the end of an array.
 *
 * This function appends a Value to the end of an ObjectArray, increasing its
 * size. It ensures sufficient capacity before adding the element.
 */
bool array_add_back(VM *vm, ObjectArray *array, Value value);

/**
 * @brief Creates a new random object.
 *
 * This function allocates and initializes a new ObjectRandom, representing
 * a random number generator in the crux language.
 *
 * @param vm The virtual machine.
 *
 * @return A pointer to the newly created ObjectRandom.
 */
ObjectRandom *new_random(VM *vm);

ObjectFile *new_object_file(VM *vm, ObjectString *path, ObjectString *mode);

ObjectModuleRecord *new_object_module_record(VM *vm, ObjectString *path,
					  bool is_repl, bool is_main);

/**
 * @brief Removes a value from an object table.
 *
 * This function removes a value associated with a given key from an
 * ObjectTable. It marks the entry as empty and decrements the table's size.
 * verify that key is hashable before calling
 * @param table The ObjectTable to modify.
 * @param key The key associated with the value to remove.
 *
 * @return true if the value was removed successfully, false otherwise (e.g.,
 * table is NULL or key is not found).
 */
bool object_table_remove(ObjectTable *table, Value key);

/**
 * @brief Checks if a key exists in an object table.
 *
 * This function checks whether a given key exists in an ObjectTable.
 * verify that key is hashable before calling
 * @param table The ObjectTable to search.
 * @param key The key to look for.
 *
 * @return true if the key exists, false otherwise (e.g., table is NULL or key
 * is not found).
 */
bool object_table_contains_key(ObjectTable *table, Value key);

ObjectStaticArray *new_static_array(VM *vm, uint16_t element_count,
				  ObjectModuleRecord *module_record);

ObjectStaticTable *new_static_table(VM *vm, uint16_t element_count,
				  ObjectModuleRecord *module_record);

bool object_static_table_set(VM *vm, ObjectStaticTable *table, Value key,
			  Value value);

ObjectStruct *new_struct_type(VM *vm, ObjectString *name);

ObjectStructInstance *new_struct_instance(VM *vm, ObjectStruct *struct_type,
					uint16_t field_count,
					ObjectModuleRecord *module_record);

ObjectVec2 *new_vec2(VM *vm, double x, double y);

ObjectVec3 *new_vec3(VM *vm, double x, double y, double z);

void free_object_static_table(VM *vm, ObjectStaticTable *table);

bool init_module_record(ObjectModuleRecord *module_record, ObjectString *path,
		      bool is_repl, bool is_main);

void free_module_record(VM *vm, ObjectModuleRecord *module_record);

#endif
