#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "table.h"
#include "utf8.h"
#include "value.h"
#include "vm.h"

#define NATIVE_FUNCTION_MAX_ARGS 8

#define STATIC_STRING_LEN(static_string) sizeof((static_string)) - 1

// Only works with string literals -- gc_safe_static_message
#define MAKE_GC_SAFE_ERROR(vm, gc_safe_static_message, gc_safe_error_type)                                             \
	({                                                                                                                 \
		ObjectString *message = copy_string((vm), (gc_safe_static_message),                                            \
											STATIC_STRING_LEN((gc_safe_static_message)));                              \
		push((vm)->current_module_record, OBJECT_VAL(message));                                                        \
		ObjectError *gc_safe_error = new_error((vm), message, (gc_safe_error_type), false);                            \
		push((vm)->current_module_record, OBJECT_VAL(gc_safe_error));                                                  \
		ObjectResult *gcSafeErrorResult = new_error_result((vm), gc_safe_error);                                       \
		pop((vm)->current_module_record);                                                                              \
		pop((vm)->current_module_record);                                                                              \
		OBJECT_VAL(gcSafeErrorResult);                                                                                 \
	})

#define MAKE_GC_SAFE_RESULT(vm, object)                                                                                \
	({                                                                                                                 \
		push((vm)->current_module_record, OBJECT_VAL(object));                                                         \
		ObjectResult *gcSafeResult = new_ok_result((vm), OBJECT_VAL(object));                                          \
		pop((vm)->current_module_record);                                                                              \
		OBJECT_VAL(gcSafeResult);                                                                                      \
	})

#define OBJECT_TYPE(value) (AS_CRUX_OBJECT(value)->type)
#define OBJECT_SET_TYPE(obj, obj_type) ((obj)->type = (obj_type))

#define IS_CRUX_STRING(value) is_object_type(value, OBJECT_STRING)
#define IS_CRUX_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_CRUX_NATIVE_CALLABLE(value) is_object_type(value, OBJECT_NATIVE_CALLABLE)
#define IS_CRUX_CLOSURE(value) is_object_type(value, OBJECT_CLOSURE)
#define IS_CRUX_BOUND_METHOD(value) is_object_type(value, OBJECT_BOUND_METHOD)
#define IS_CRUX_ARRAY(value) is_object_type(value, OBJECT_ARRAY)
#define IS_CRUX_TABLE(value) is_object_type(value, OBJECT_TABLE)
#define IS_CRUX_ERROR(value) is_object_type(value, OBJECT_ERROR)
#define IS_CRUX_RESULT(value) is_object_type(value, OBJECT_RESULT)
#define IS_CRUX_RANDOM(value) is_object_type(value, OBJECT_RANDOM)
#define IS_CRUX_FILE(value) is_object_type(value, OBJECT_FILE)
#define IS_CRUX_MODULE_RECORD(value) is_object_type(value, OBJECT_MODULE_RECORD)
#define IS_CRUX_STRUCT(value) is_object_type(value, OBJECT_STRUCT)
#define IS_CRUX_STRUCT_INSTANCE(value) is_object_type(value, OBJECT_STRUCT_INSTANCE)
#define IS_CRUX_VECTOR(value) is_object_type(value, OBJECT_VECTOR)
#define IS_CRUX_COMPLEX(value) is_object_type(value, OBJECT_COMPLEX)
#define IS_CRUX_MATRIX(value) is_object_type(value, OBJECT_MATRIX)
#define IS_CRUX_BUFFER(value) is_object_type(value, OBJECT_BUFFER)
#define IS_CRUX_SET(value) is_object_type(value, OBJECT_SET)
#define IS_CRUX_TUPLE(value) is_object_type(value, OBJECT_TUPLE)
#define IS_CRUX_RANGE(value) is_object_type(value, OBJECT_RANGE)
#define IS_CRUX_TYPE_RECORD(value) is_object_type(value, OBJECT_TYPE_RECORD)
#define IS_CRUX_TYPE_TABLE(value) is_object_type(value, OBJECT_TYPE_TABLE)

#define AS_CRUX_STRING(value) ((ObjectString *)AS_CRUX_OBJECT(value))
#define AS_C_STRING(value) (((ObjectString *)AS_CRUX_OBJECT(value))->chars)
#define AS_CRUX_FUNCTION(value) ((ObjectFunction *)AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_CALLABLE(value) ((ObjectNativeCallable *)AS_CRUX_OBJECT(value))
#define AS_CRUX_CLOSURE(value) ((ObjectClosure *)AS_CRUX_OBJECT(value))
#define AS_CRUX_BOUND_METHOD(value) ((ObjectBoundMethod *)AS_CRUX_OBJECT(value))
#define AS_CRUX_ARRAY(value) ((ObjectArray *)AS_CRUX_OBJECT(value))
#define AS_CRUX_TABLE(value) ((ObjectTable *)AS_CRUX_OBJECT(value))
#define AS_CRUX_ERROR(value) ((ObjectError *)AS_CRUX_OBJECT(value))
#define AS_CRUX_RESULT(value) ((ObjectResult *)AS_CRUX_OBJECT(value))
#define AS_CRUX_RANDOM(value) ((ObjectRandom *)AS_CRUX_OBJECT(value))
#define AS_CRUX_FILE(value) ((ObjectFile *)AS_CRUX_OBJECT(value))
#define AS_CRUX_MODULE_RECORD(value) ((ObjectModuleRecord *)AS_CRUX_OBJECT(value))
#define AS_CRUX_UPVALUE(value) ((ObjectUpvalue *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STRUCT(value) ((ObjectStruct *)AS_CRUX_OBJECT(value))
#define AS_CRUX_STRUCT_INSTANCE(value) ((ObjectStructInstance *)AS_CRUX_OBJECT(value))
#define AS_CRUX_VECTOR(value) ((ObjectVector *)AS_CRUX_OBJECT(value))
#define AS_CRUX_COMPLEX(value) ((ObjectComplex *)AS_CRUX_OBJECT(value))
#define AS_CRUX_MATRIX(value) ((ObjectMatrix *)AS_CRUX_OBJECT(value))
#define AS_CRUX_BUFFER(value) ((ObjectBuffer *)AS_CRUX_OBJECT(value))
#define AS_CRUX_SET(value) ((ObjectSet *)AS_CRUX_OBJECT(value))
#define AS_CRUX_TUPLE(value) ((ObjectTuple *)AS_CRUX_OBJECT(value))
#define AS_CRUX_RANGE(value) ((ObjectRange *)AS_CRUX_OBJECT(value))
#define AS_CRUX_TYPE_RECORD(value) ((ObjectTypeRecord *)AS_CRUX_OBJECT(value))
#define AS_CRUX_TYPE_TABLE(value) ((ObjectTypeTable *)AS_CRUX_OBJECT(value))

#define IS_CRUX_HASHABLE(value)                                                                                        \
	(IS_INT(value) || IS_FLOAT(value) || IS_CRUX_STRING(value) || IS_NIL(value) || IS_BOOL(value))

typedef enum {
	OBJECT_STRING,
	OBJECT_FUNCTION,
	OBJECT_NATIVE_CALLABLE,
	OBJECT_CLOSURE,
	OBJECT_UPVALUE,
	OBJECT_ARRAY,
	OBJECT_TABLE,
	OBJECT_ERROR,
	OBJECT_RESULT,
	OBJECT_RANDOM,
	OBJECT_FILE,
	OBJECT_MODULE_RECORD,
	OBJECT_STRUCT,
	OBJECT_STRUCT_INSTANCE,
	OBJECT_VECTOR,
	OBJECT_COMPLEX,
	OBJECT_MATRIX,
	OBJECT_BUFFER,
	OBJECT_SET,
	OBJECT_TUPLE,
	OBJECT_RANGE,
	OBJECT_TYPE_RECORD,
	OBJECT_TYPE_TABLE,
} ObjectType;

struct CruxObject { // 8
	uint32_t pool_index;
	ObjectType type;
};

struct PoolObject { // 8
	void *data;
};

#define MARK_BIT ((uintptr_t)1 << 63)
#define PTR_MASK (~MARK_BIT)
#define IS_MARKED(obj) ((uintptr_t)(obj)->data & MARK_BIT)
#define GET_DATA(obj) ((void *)((uintptr_t)(obj)->data & PTR_MASK))
#define SET_DATA(obj, ptr) ((obj)->data = (void *)((uintptr_t)(ptr) | ((uintptr_t)(obj)->data & MARK_BIT)))
#define SET_MARKED(obj, marked)                                                                                        \
	((obj)->data = (void *)(((uintptr_t)(obj)->data & PTR_MASK) | ((marked) ? MARK_BIT : 0)))

#define TO_DOUBLE(value) (IS_INT((value)) ? (double)AS_INT((value)) : AS_FLOAT((value)))

struct ObjectString { // 24
	CruxObject object;
	utf8_int8_t* chars;
	uint32_t byte_length; // this is the length without the null terminator
	uint32_t code_point_length;
	uint32_t hash;
};

typedef struct ObjectModuleRecord ObjectModuleRecord;

typedef struct { // 72
	CruxObject object;
	int arity;
	int upvalue_count;
	Chunk chunk;
	ObjectString *name;
	ObjectModuleRecord *module_record;
} ObjectFunction;

typedef struct ObjectUpvalue { // 32
	CruxObject object;
	Value *location;
	Value closed;
	ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct ObjectClosure { // 32
	CruxObject object;
	ObjectFunction *function;
	ObjectUpvalue **upvalues;
	int upvalue_count;
} ObjectClosure;

typedef struct { // 24
	CruxObject object;
	Value *values;
	uint32_t size;
	uint32_t capacity;
} ObjectArray;

typedef struct { // 16
	CruxObject object;
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
	MEMORY,
	VALUE, // correct type, but incorrect value
	ASSERT,
	IMPORT_EXTENT,
	IO,
	IMPORT,
} ErrorType;

typedef struct { // 24
	CruxObject object;
	ObjectString *message;
	ErrorType type;
	bool is_panic;
} ObjectError;

struct ObjectResult { // 24
	CruxObject object;
	bool is_ok;
	union {
		Value value;
		ObjectError *error;
	} as;
};

typedef struct { // 32
	CruxObject object;
	ObjectString *name;
	Table fields;
	Table methods;
} ObjectStruct;

typedef struct ObjectTypeTable ObjectTypeTable;

typedef struct {
	ObjectString *key;
	ObjectTypeRecord *value;
} TypeEntry;

struct ObjectTypeTable { // 24
	CruxObject object;
	TypeEntry *entries;
	int count;
	int capacity;
};

struct ObjectTypeRecord { // 40
	CruxObject object;
	TypeMask base_type;
	union {
		struct {
			ObjectTypeRecord *element_type;
		} array_type;
		struct {
			ObjectTypeRecord *key_type; // must be Hashable
			ObjectTypeRecord *value_type;
		} table_type;
		struct {
			ObjectTypeRecord *ok_type;
		} result_type;
		struct {
			ObjectTypeTable *field_types;
			int field_count;
			ObjectStruct *definition; // has the field names
		} struct_type;
		struct {
			int dimensions;
		} vector_type;
		struct {
			ObjectTypeRecord **element_types;
			int element_count;
		} tuple_type;
		struct {
			int rows;
			int cols;
		} matrix_type;
		struct {
			ObjectTypeRecord **arg_types;
			int arg_count;
			ObjectTypeRecord *return_type;
		} function_type;
		struct {
			ObjectTypeRecord *element_type;
		} set_type;
		struct {
			ObjectTypeRecord **element_types;
			ObjectString **element_names;
			int element_count;
		} union_type;
		struct {
			ObjectTypeTable *element_types;
			int element_count;
		} shape_type;
	} as;
};

typedef Value (*CruxCallable)(VM *vm, const Value *args);

typedef struct { // 48
	CruxObject object;
	CruxCallable function;
	ObjectString *name;
	int arity;
	ObjectTypeRecord **arg_types;
	ObjectTypeRecord *return_type;
} ObjectNativeCallable;

typedef struct { // 24
	Value key;
	Value value;
	bool is_occupied;
} ObjectTableEntry;

typedef struct { // 24
	CruxObject object;
	ObjectTableEntry *entries;
	uint32_t capacity;
	uint32_t size;
} ObjectTable;

typedef struct { // 48
	CruxObject object;
	ObjectString *path;
	ObjectString *mode;
	FILE *file;
	uint64_t position;
	bool is_open;
} ObjectFile;

struct ObjectStructInstance { // 32
	CruxObject object;
	ObjectStruct *struct_type;
	Value *fields;
	uint16_t field_count;
};

#define STATIC_VECTOR_SIZE 4
#define VECTOR_COMPONENTS(vec)                                                                                         \
	((vec)->dimensions <= STATIC_VECTOR_SIZE ? (vec)->as.s_components : (vec)->as.h_components)

typedef struct { // 48
	CruxObject object;
	uint32_t dimensions;
	union {
		double *h_components; /* heap components */
		double s_components[STATIC_VECTOR_SIZE]; /* static components */
	} as;
} ObjectVector;

typedef struct // 32
{
	CruxObject object;
	double real;
	double imag;
} ObjectComplex;

#define MATRIX_AT(m, i, j) ((m)->data[(i) * (m)->col_dim + (j)])
typedef struct {
	CruxObject object;
	uint16_t row_dim;
	uint16_t col_dim;
	double *data;
} ObjectMatrix;

typedef struct TypeArena TypeArena;

typedef enum {
	STATE_LOADING,
	STATE_LOADED,
	STATE_ERROR,
	STATE_EXECUTED,
} ModuleState;

struct ObjectModuleRecord { // 120
	CruxObject object;
	ObjectString *path;
	Table globals;
	Table publics;
	ObjectTypeTable *types;
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
	VM* owner;
};

typedef struct { // 40
	CruxObject object;
	int32_t start;
	int32_t end;
	int32_t step;
} ObjectRange;

typedef struct { // 24
	CruxObject object;
	ObjectTable *entries;
} ObjectSet;

typedef struct { // 32
	CruxObject object;
	uint32_t read_pos;
	uint32_t write_pos;
	uint32_t capacity;
	uint8_t *data;
} ObjectBuffer;

typedef struct { // 20
	CruxObject object;
	uint32_t size;
	Value *elements;
} ObjectTuple;

static bool is_object_type(const Value value, const ObjectType type)
{
	return IS_CRUX_OBJECT(value) && AS_CRUX_OBJECT(value)->type == type;
}

ObjectError *new_error(VM *vm, ObjectString *message, ErrorType type, bool is_panic);
ObjectUpvalue *new_upvalue(VM *vm, Value *slot);
ObjectClosure *new_closure(VM *vm, ObjectFunction *function);
ObjectNativeCallable *new_native_callable(VM *vm, CruxCallable function, int arity, ObjectString *name,
										  ObjectTypeRecord **arg_types, ObjectTypeRecord *return_type);
ObjectFunction *new_function(VM *vm);
ObjectTable *new_object_table(VM *vm, int element_count);
ObjectResult *new_ok_result(VM *vm, Value value);
ObjectResult *new_error_result(VM *vm, ObjectError *error);
ObjectArray *new_array(VM *vm, uint32_t element_count);
ObjectString *take_string(VM *vm, char *chars, uint32_t length);
ObjectString *copy_string(VM *vm, const char *chars, uint32_t length);
ObjectString *to_string(VM *vm, Value value);
void print_object(Value value, bool in_collection);
void print_type_to(FILE *stream, Value value);
int sprint_type_to(char *buffer, size_t size, Value value);
void print_value_to(FILE *stream, Value value, bool inCollection);
void print_error_type_to(FILE *stream, ErrorType type);
void free_object_table(VM *vm, ObjectTable *table);
void free_object_module_record(VM *vm, ObjectModuleRecord *record);
bool object_table_set(VM *vm, ObjectTable *table, Value key, Value value);
bool object_table_get(ObjectTableEntry *entries, uint32_t size, uint32_t capacity, Value key, Value *value);
void mark_object_table(VM *vm, const ObjectTableEntry *entries, uint32_t capacity);
bool ensure_capacity(VM *vm, ObjectArray *array, uint32_t capacity_needed);
bool array_set(VM *vm, const ObjectArray *array, uint32_t index, Value value);
bool array_add(VM *vm, ObjectArray *array, Value value, uint32_t index);
bool array_add_back(VM *vm, ObjectArray *array, Value value);
ObjectRandom *new_random(VM *vm);
ObjectFile *new_object_file(VM *vm, ObjectString *path, ObjectString *mode);
ObjectModuleRecord *new_object_module_record(VM *vm, ObjectString *path, bool is_repl, bool is_main);
bool object_table_remove(ObjectTable *table, Value key);
bool object_table_contains_key(ObjectTable *table, Value key);
ObjectStruct *new_struct_type(VM *vm, ObjectString *name);
ObjectStructInstance *new_struct_instance(VM *vm, ObjectStruct *struct_type, uint16_t field_count);
ObjectVector *new_vector(VM *vm, uint32_t dimensions);
void free_module_record(VM *vm, ObjectModuleRecord *module_record);
ObjectComplex *new_complex_number(VM *vm, double real, double imaginary);
ObjectMatrix *new_matrix(VM *vm, uint16_t row_dim, uint16_t col_dim);
ObjectRange *new_range(VM *vm, uint64_t start, uint64_t end, uint64_t step);
ObjectSet *new_set(VM *vm, uint32_t element_count);
ObjectBuffer *new_buffer(VM *vm, uint32_t buffer_size);
ObjectTuple *new_tuple(VM *vm, uint32_t size);
void mark_object_type_table(VM *vm, ObjectTypeTable *table);
ObjectTypeTable *new_type_table(VM *vm, int capacity);
bool set_add_value(VM *vm, ObjectSet *set, Value value);
bool validate_range_values(int32_t start, int32_t step, int32_t end, const char **error_message);

#endif
