#include "stdlib/buffer.h"
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "value.h"

#define BUFFER_GROWTH_ERROR                                                    \
	return MAKE_GC_SAFE_ERROR(                                             \
		vm, "Failed to grow buffer - buffer is at maximum capacity.",  \
		BOUNDS);

#define BUFFER_READABLE(buf) ((buf)->write_pos - (buf)->read_pos)

/**
 * Grows the buffer's data until it can hold the required bytes.
 * Returns false if growth would exceed UINT32_MAX.
 */
static bool grow_buffer(VM *vm, ObjectBuffer *buffer, uint32_t required)
{
	uint64_t new_capacity = buffer->capacity;
	while (new_capacity < required) {
		new_capacity = new_capacity < 8 ? 8 : new_capacity * 2;
		if (new_capacity > UINT32_MAX)
			return false;
	}
	buffer->data = GROW_ARRAY(vm, uint8_t, buffer->data, buffer->capacity,
				  (uint32_t)new_capacity);
	buffer->capacity = (uint32_t)new_capacity;
	return true;
}

/**
 * Ensures the buffer has room for count additional bytes.
 * Returns false if growth fails.
 */
static bool ensure_write_capacity(VM *vm, ObjectBuffer *buffer, uint32_t count)
{
	uint32_t required = buffer->write_pos + count;
	if (required <= buffer->capacity)
		return true;
	return grow_buffer(vm, buffer, required);
}

/**
 * Reinterprets 4 bytes as a float.
 */
static float bytes_to_float32(const uint8_t *src)
{
	float result;
	memcpy(&result, src, sizeof(float));
	return result;
}

/**
 * reinterprets 8 bytes at src as a double.
 */
static double bytes_to_float64(const uint8_t *src)
{
	double result;
	memcpy(&result, src, sizeof(double));
	return result;
}

/**
 * Creates a new empty buffer with the default initial capacity.
 * Returns Buffer
 */
Value new_buffer_function(VM *vm, const Value *args)
{
	(void)args;
	ObjectBuffer *buffer = new_buffer(vm, INITIAL_BUFFER_CAPACITY);
	return OBJECT_VAL(buffer);
}

/**
 * Writes a single byte to the buffer.
 * arg0 -> buffer: Buffer
 * arg1 -> byte: Int  (0-255, upper bits are silently truncated)
 * Returns Result<Buffer>
 */
Value write_byte_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	uint8_t byte = (uint8_t)AS_INT(args[1]);

	if (!ensure_write_capacity(vm, buffer, 1))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = byte;

	push(vm->current_module_record, OBJECT_VAL(buffer));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(buffer));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Writes a 16-bit signed integer as 2 bytes, little-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Int
 * Returns Nil
 */
Value write_int16_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int16_t value = (int16_t)AS_INT(args[1]);

	if (!ensure_write_capacity(vm, buffer, 2))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (value) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 8) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a 16-bit signed integer as 2 bytes, big-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Int
 * Returns Nil
 */
Value write_int16_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int16_t value = (int16_t)AS_INT(args[1]);

	if (!ensure_write_capacity(vm, buffer, 2))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (value >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (value) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a 32-bit signed integer as 4 bytes, little-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Int
 * Returns Nil
 */
Value write_int32_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int32_t value = (int32_t)AS_INT(args[1]);

	if (!ensure_write_capacity(vm, buffer, 4))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (value) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 24) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a 32-bit signed integer as 4 bytes, big-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Int
 * Returns Nil
 */
Value write_int32_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int32_t value = (int32_t)AS_INT(args[1]);

	if (!ensure_write_capacity(vm, buffer, 4))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (value >> 24) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (value >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (value) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a float narrowed to float32 as 4 bytes, little-endian.
 * Note: precision loss occurs for values that cannot be represented
 * exactly as float32.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Float
 * Returns Nil
 */
Value write_float32_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	float value = (float)AS_FLOAT(args[1]);
	uint32_t bits;
	memcpy(&bits, &value, sizeof(float));

	if (!ensure_write_capacity(vm, buffer, 4))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (bits) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 24) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a float narrowed to float32 as 4 bytes, big-endian.
 * Note: precision loss occurs for values that cannot be represented
 * exactly as float32.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Float
 * Returns Nil
 */
Value write_float32_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	float value = (float)AS_FLOAT(args[1]);
	uint32_t bits;
	memcpy(&bits, &value, sizeof(float));

	if (!ensure_write_capacity(vm, buffer, 4))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (bits >> 24) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a double as 8 bytes, little-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Float
 * Returns Nil
 */
Value write_float64_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	double value = AS_FLOAT(args[1]);
	uint64_t bits;
	memcpy(&bits, &value, sizeof(double));

	if (!ensure_write_capacity(vm, buffer, 8))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (bits) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 24) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 32) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 40) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 48) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 56) & 0xFF;
	return NIL_VAL;
}

/**
 * Writes a double as 8 bytes, big-endian.
 * arg0 -> buffer: Buffer
 * arg1 -> value: Float
 * Returns Nil
 */
Value write_float64_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	double value = AS_FLOAT(args[1]);
	uint64_t bits;
	memcpy(&bits, &value, sizeof(double));

	if (!ensure_write_capacity(vm, buffer, 8))
		BUFFER_GROWTH_ERROR

	buffer->data[buffer->write_pos++] = (bits >> 56) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 48) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 40) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 32) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 24) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 16) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits >> 8) & 0xFF;
	buffer->data[buffer->write_pos++] = (bits) & 0xFF;
	return NIL_VAL;
}

/**
 * Appends the raw bytes of a string to the buffer.
 * arg0 -> buffer: Buffer
 * arg1 -> string: String
 * Returns Result<Buffer>
 */
Value write_string_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	ObjectString *string = AS_CRUX_STRING(args[1]);

	if (!ensure_write_capacity(vm, buffer, (uint32_t)string->length))
		BUFFER_GROWTH_ERROR

	memcpy(buffer->data + buffer->write_pos, string->chars, string->length);
	buffer->write_pos += (uint32_t)string->length;

	push(vm->current_module_record, OBJECT_VAL(buffer));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(buffer));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Appends the readable bytes of another buffer to this buffer.
 * Only the bytes between read_pos and write_pos of the source are copied.
 * arg0 -> self: Buffer
 * arg1 -> other: Buffer
 * Returns Result<Buffer>
 */
Value write_buffer_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *self = AS_CRUX_BUFFER(args[0]);
	ObjectBuffer *other = AS_CRUX_BUFFER(args[1]);
	uint32_t readable = BUFFER_READABLE(other);

	if (!ensure_write_capacity(vm, self, readable))
		BUFFER_GROWTH_ERROR

	memcpy(self->data + self->write_pos, other->data + other->read_pos,
	       readable);
	self->write_pos += readable;

	push(vm->current_module_record, OBJECT_VAL(self));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(self));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/* -------------------------------------------------------------------------
 * Read methods
 * ------------------------------------------------------------------------- */

/**
 * Reads a single byte from the buffer and advances the read position.
 * arg0 -> buffer: Buffer
 * Returns Result<Int>
 */
Value read_byte_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 1)
		return MAKE_GC_SAFE_ERROR(vm, "Not enough bytes to read byte.",
					  BOUNDS);

	int32_t byte = (int32_t)buffer->data[buffer->read_pos++];
	return OBJECT_VAL(new_ok_result(vm, INT_VAL(byte)));
}

/**
 * Reads exactly n bytes from the buffer as a string.
 * arg0 -> buffer: Buffer
 * arg1 -> n: Int
 * Returns Result<String>
 */
Value read_string_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int32_t n = AS_INT(args[1]);

	if (n < 0)
		return MAKE_GC_SAFE_ERROR(vm, "Read length cannot be negative.",
					  VALUE);

	if (BUFFER_READABLE(buffer) < (uint32_t)n)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Not enough bytes to read string.",
					  BOUNDS);

	ObjectString *string = copy_string(
		vm, (const char *)(buffer->data + buffer->read_pos), n);
	buffer->read_pos += (uint32_t)n;

	push(vm->current_module_record, OBJECT_VAL(string));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(string));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Reads bytes from the buffer until a newline byte (0x0A) is found or
 * the buffer is exhausted. The newline is consumed but not included in
 * the returned string.
 * arg0 -> buffer: Buffer
 * Returns Result<String>
 */
Value read_line_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) == 0)
		return MAKE_GC_SAFE_ERROR(vm, "Buffer is empty.", BOUNDS);

	uint32_t start = buffer->read_pos;
	uint32_t end = buffer->read_pos;

	while (end < buffer->write_pos && buffer->data[end] != '\n')
		end++;

	uint32_t length = end - start;
	ObjectString *string = copy_string(vm,
					   (const char *)(buffer->data + start),
					   (int)length);

	// advance past the newline if we found one
	buffer->read_pos = (end < buffer->write_pos) ? end + 1 : end;

	push(vm->current_module_record, OBJECT_VAL(string));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(string));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Reads all remaining readable bytes from the buffer as a string.
 * arg0 -> buffer: Buffer
 * Returns Result<String>
 */
Value read_all_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	uint32_t readable = BUFFER_READABLE(buffer);

	if (readable == 0)
		return MAKE_GC_SAFE_ERROR(vm, "Buffer is empty.", BOUNDS);

	ObjectString *string = copy_string(vm,
					   (const char *)(buffer->data +
							  buffer->read_pos),
					   (int)readable);
	buffer->read_pos = buffer->write_pos;

	push(vm->current_module_record, OBJECT_VAL(string));
	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(string));
	pop(vm->current_module_record);
	return OBJECT_VAL(result);
}

/**
 * Reads a 16-bit signed integer from 2 bytes, little-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Int>
 */
Value read_int16_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 2)
		return MAKE_GC_SAFE_ERROR(vm, "Not enough bytes to read Int16.",
					  BOUNDS);

	int16_t value = (int16_t)((uint16_t)buffer->data[buffer->read_pos] |
				  (uint16_t)buffer->data[buffer->read_pos + 1]
					  << 8);
	buffer->read_pos += 2;
	return OBJECT_VAL(new_ok_result(vm, INT_VAL((int32_t)value)));
}

/**
 * Reads a 16-bit signed integer from 2 bytes, big-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Int>
 */
Value read_int16_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 2)
		return MAKE_GC_SAFE_ERROR(vm, "Not enough bytes to read Int16.",
					  BOUNDS);

	int16_t value = (int16_t)((uint16_t)buffer->data[buffer->read_pos]
					  << 8 |
				  (uint16_t)buffer->data[buffer->read_pos + 1]);
	buffer->read_pos += 2;
	return OBJECT_VAL(new_ok_result(vm, INT_VAL((int32_t)value)));
}

/**
 * Reads a 32-bit signed integer from 4 bytes, little-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Int>
 */
Value read_int32_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 4)
		return MAKE_GC_SAFE_ERROR(vm, "Not enough bytes to read Int32.",
					  BOUNDS);

	int32_t value =
		(int32_t)((uint32_t)buffer->data[buffer->read_pos] |
			  (uint32_t)buffer->data[buffer->read_pos + 1] << 8 |
			  (uint32_t)buffer->data[buffer->read_pos + 2] << 16 |
			  (uint32_t)buffer->data[buffer->read_pos + 3] << 24);
	buffer->read_pos += 4;
	return OBJECT_VAL(new_ok_result(vm, INT_VAL(value)));
}

/**
 * Reads a 32-bit signed integer from 4 bytes, big-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Int>
 */
Value read_int32_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 4)
		return MAKE_GC_SAFE_ERROR(vm, "Not enough bytes to read Int32.",
					  BOUNDS);

	int32_t value =
		(int32_t)((uint32_t)buffer->data[buffer->read_pos] << 24 |
			  (uint32_t)buffer->data[buffer->read_pos + 1] << 16 |
			  (uint32_t)buffer->data[buffer->read_pos + 2] << 8 |
			  (uint32_t)buffer->data[buffer->read_pos + 3]);
	buffer->read_pos += 4;
	return OBJECT_VAL(new_ok_result(vm, INT_VAL(value)));
}

/**
 * Reads 4 bytes as a float32 and widens to double, little-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Float>
 */
Value read_float32_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 4)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Not enough bytes to read Float32.",
					  BOUNDS);

	float value = bytes_to_float32(buffer->data + buffer->read_pos);
	buffer->read_pos += 4;
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL((double)value)));
}

/**
 * Reads 4 bytes as a float32 and widens to double, big-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Float>
 */
Value read_float32_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 4)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Not enough bytes to read Float32.",
					  BOUNDS);

	// reverse bytes into a temp array then pun
	uint8_t tmp[4] = {
		buffer->data[buffer->read_pos + 3],
		buffer->data[buffer->read_pos + 2],
		buffer->data[buffer->read_pos + 1],
		buffer->data[buffer->read_pos + 0],
	};
	float value = bytes_to_float32(tmp);
	buffer->read_pos += 4;
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL((double)value)));
}

/**
 * Reads 8 bytes as a double, little-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Float>
 */
Value read_float64_le_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 8)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Not enough bytes to read Float64.",
					  BOUNDS);

	double value = bytes_to_float64(buffer->data + buffer->read_pos);
	buffer->read_pos += 8;
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(value)));
}

/**
 * Reads 8 bytes as a double, big-endian.
 * arg0 -> buffer: Buffer
 * Returns Result<Float>
 */
Value read_float64_be_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);

	if (BUFFER_READABLE(buffer) < 8)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Not enough bytes to read Float64.",
					  BOUNDS);

	uint8_t tmp[8] = {
		buffer->data[buffer->read_pos + 7],
		buffer->data[buffer->read_pos + 6],
		buffer->data[buffer->read_pos + 5],
		buffer->data[buffer->read_pos + 4],
		buffer->data[buffer->read_pos + 3],
		buffer->data[buffer->read_pos + 2],
		buffer->data[buffer->read_pos + 1],
		buffer->data[buffer->read_pos + 0],
	};
	double value = bytes_to_float64(tmp);
	buffer->read_pos += 8;
	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(value)));
}

/**
 * Returns the total allocated capacity in bytes.
 * Returned as Float because capacity is uint32_t which may exceed Int range.
 * arg0 -> buffer: Buffer
 * Returns Float
 */
Value capacity_buffer_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	return FLOAT_VAL((double)buffer->capacity);
}

/**
 * Returns true if there are no readable bytes remaining.
 * arg0 -> buffer: Buffer
 * Returns Bool
 */
Value is_empty_buffer_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	return BOOL_VAL(buffer->read_pos == buffer->write_pos);
}

/**
 * Resets read and write positions to 0 without freeing memory.
 * arg0 -> buffer: Buffer
 * Returns Nil
 */
Value clear_buffer_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	buffer->read_pos = 0;
	buffer->write_pos = 0;
	return NIL_VAL;
}

/**
 * Returns the next readable byte without advancing the read position.
 * Returns -1 as Int if the buffer is empty.
 * arg0 -> buffer: Buffer
 * Returns Int
 */
Value peek_byte_buffer_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	if (BUFFER_READABLE(buffer) == 0)
		return INT_VAL(-1);
	return INT_VAL((int32_t)buffer->data[buffer->read_pos]);
}

/**
 * Advances the read position by n bytes without returning the data.
 * Returns an error if skipping would go past the write position.
 * arg0 -> buffer: Buffer
 * arg1 -> n: Int
 * Returns Nil | Error
 */
Value skip_bytes_buffer_method(VM *vm, const Value *args)
{
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	int32_t n = AS_INT(args[1]);

	if (n < 0)
		return MAKE_GC_SAFE_ERROR(vm, "Skip amount cannot be negative.",
					  VALUE);

	if (BUFFER_READABLE(buffer) < (uint32_t)n)
		return MAKE_GC_SAFE_ERROR(vm,
					  "Cannot skip past write position.",
					  BOUNDS);

	buffer->read_pos += (uint32_t)n;
	return NIL_VAL;
}

/**
 * Copies all readable bytes into a new string without advancing read_pos.
 * arg0 -> buffer: Buffer
 * Returns String
 */
Value to_string_buffer_method(VM *vm, const Value *args)
{
	const ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	uint32_t readable = BUFFER_READABLE(buffer);

	ObjectString *string = copy_string(vm,
					   (const char *)(buffer->data +
							  buffer->read_pos),
					   (int)readable);
	return OBJECT_VAL(string);
}

/**
 * Returns a deep copy of the buffer including all allocated data.
 * The clone's read and write positions match the original.
 * arg0 -> buffer: Buffer
 * Returns Buffer
 */
Value clone_buffer_method(VM *vm, const Value *args)
{
	const ObjectBuffer *src = AS_CRUX_BUFFER(args[0]);
	ObjectBuffer *dst = new_buffer(vm, src->capacity);

	push(vm->current_module_record, OBJECT_VAL(dst));

	memcpy(dst->data, src->data, src->write_pos);
	dst->read_pos = src->read_pos;
	dst->write_pos = src->write_pos;

	pop(vm->current_module_record);
	return OBJECT_VAL(dst);
}

/**
 * Moves all readable bytes to the front of the internal array and resets
 * read_pos to 0. Reduces wasted space at the front of the buffer without
 * reallocating.
 * arg0 -> buffer: Buffer
 * Returns Nil
 */
Value compact_buffer_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectBuffer *buffer = AS_CRUX_BUFFER(args[0]);
	uint32_t readable = BUFFER_READABLE(buffer);

	if (buffer->read_pos == 0 || readable == 0) {
		// already compact or empty - reset both positions if empty
		if (readable == 0) {
			buffer->read_pos = 0;
			buffer->write_pos = 0;
		}
		return NIL_VAL;
	}

	memmove(buffer->data, buffer->data + buffer->read_pos, readable);
	buffer->read_pos = 0;
	buffer->write_pos = readable;
	return NIL_VAL;
}
