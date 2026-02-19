#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "panic.h"
#include "stdlib/io.h"

#include "garbage_collector.h"
#include "vm.h"

/* ── Platform ────────────────────────────────────────────────────────────────
 */

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define ISATTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#endif

/* ── Internal helpers ────────────────────────────────────────────────────────
 */

#define SCANLN_BUFFER_SIZE 1024

/*
 * Resolves a channel name string to its FILE* handle.
 * Returns NULL if the channel name is not recognised.
 */
static FILE *get_channel(const char *channel)
{
	if (strcmp(channel, "stdout") == 0)
		return stdout;
	if (strcmp(channel, "stderr") == 0)
		return stderr;
	if (strcmp(channel, "stdin") == 0)
		return stdin;
	return NULL;
}

/*
 * Writes a string representation of <value> to <stream>.
 * Returns false if the write fails.
 */
static bool write_value_to_stream(FILE *stream, Value value)
{
	/* Delegate to the existing print_value infrastructure but capture
	 * failures via ferror.  We clear the error flag first so we are
	 * only testing this write. */
	clearerr(stream);
	print_value_to(stream, value, false);
	return ferror(stream) == 0;
}

/*
 * Discards characters on <stream> up to and including the next '\n' or EOF.
 * Used after bounded reads to leave the stream in a clean state.
 */
static void flush_line(FILE *stream)
{
	int ch;
	while ((ch = fgetc(stream)) != '\n' && ch != EOF)
		;
}

/*
 * Reads up to <max_len> characters from <stream> into a GC-managed buffer,
 * stopping early on '\n' or EOF.  Does NOT include the '\n' in the result.
 * If exactly <max_len> characters were read without hitting '\n', the
 * remainder of the line is discarded via flush_line().
 *
 * On success writes the ObjectString* into *out and returns true.
 * On allocation failure or read error returns false.
 */
static bool read_bounded_line(VM *vm, FILE *stream, const size_t max_len,
			      ObjectString **out)
{
	char *buffer = ALLOCATE(vm, char, max_len + 1);
	if (buffer == NULL)
		return false;

	size_t count = 0;
	bool hit_newline = false;

	while (count < max_len) {
		const int ch = fgetc(stream);
		if (ch == EOF)
			break;
		if (ch == '\n') {
			hit_newline = true;
			break;
		}
		buffer[count++] = (char)ch;
	}
	buffer[count] = '\0';

	/* Discard rest of line if we filled the buffer without a newline */
	if (!hit_newline && count == max_len) {
		flush_line(stream);
	}

	/* take_string transfers ownership of buffer to the GC */
	*out = take_string(vm, buffer, (uint32_t)count);
	return true;
}

/* ── Output ──────────────────────────────────────────────────────────────────
 */

/*
 * print(value) — writes to stdout, no newline.
 * Infallible from the language's perspective; we can't reasonably recover
 * from a broken stdout, so we match the existing io module's behaviour.
 */
Value io_print_function(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	print_value_to(stdout, args[0], false);
	return NIL_VAL;
}

/*
 * println(value) — writes to stdout followed by '\n'.
 */
Value io_println_function(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	print_value_to(stdout, args[0], false);
	fputc('\n', stdout);
	return NIL_VAL;
}

/*
 * print_to(channel: string, value) -> Result<nil>
 * Writes to the named channel without a trailing newline.
 */
Value io_print_to_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<channel> must be of type 'string'.",
					  TYPE);
	}

	FILE *stream = get_channel(AS_C_STRING(args[0]));
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Invalid channel. Expected \"stdout\", "
			"\"stderr\", or \"stdin\".",
			VALUE);
	}

	if (!write_value_to_stream(stream, args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to stream.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/*
 * println_to(channel: string, value) -> Result<nil>
 * Writes to the named channel followed by '\n'.
 */
Value io_println_to_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<channel> must be of type 'string'.",
					  TYPE);
	}

	FILE *stream = get_channel(AS_C_STRING(args[0]));
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Invalid channel. Expected \"stdout\", "
			"\"stderr\", or \"stdin\".",
			VALUE);
	}

	if (!write_value_to_stream(stream, args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to stream.", IO);
	}

	if (fputc('\n', stream) == EOF) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to stream.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/* ── Input — stdin ───────────────────────────────────────────────────────────
 */

/*
 * scan() -> Result<string>
 * Reads exactly one character from stdin, then discards the rest of the line.
 */
Value io_scan_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;

	const int ch = fgetc(stdin);
	if (ch == EOF) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Unexpected end of input on stdin.",
					  IO);
	}

	/* Discard rest of line (including the Enter key) */
	if (ch != '\n') {
		flush_line(stdin);
	}

	const char str[2] = {(char)ch, '\0'};
	ObjectString *s = copy_string(vm, str, 1);
	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/*
 * scanln() -> Result<string>
 * Reads from stdin up to (and discarding) the next '\n'.
 * Returns the line content without the newline.
 */
Value io_scanln_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;

	ObjectString *s = NULL;
	if (!read_bounded_line(vm, stdin, SCANLN_BUFFER_SIZE, &s)) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate buffer for input.", MEMORY);
	}

	/* read_bounded_line returns empty string on immediate EOF; treat as
	 * an error so the caller can distinguish "no input" from "empty line"
	 */
	if (ferror(stdin)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stdin.", IO);
	}

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/*
 * nscan(n: int) -> Result<string>
 * Reads up to <n> characters from stdin, stopping early at '\n'.
 * Any remaining characters on the line are discarded.
 */
Value io_nscan_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be of type 'int'.",
					  TYPE);
	}

	const int32_t n = AS_INT(args[0]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be a positive integer.",
					  VALUE);
	}

	ObjectString *s = NULL;
	if (!read_bounded_line(vm, stdin, (size_t)n, &s)) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate buffer for input.", MEMORY);
	}

	if (ferror(stdin)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stdin.", IO);
	}

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/* ── Input — named channel ───────────────────────────────────────────────────
 */

/*
 * scan_from(channel: string) -> Result<string>
 * Reads exactly one character from the named channel,
 * then discards the rest of that line.
 */
Value io_scan_from_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<channel> must be of type 'string'.",
					  TYPE);
	}

	FILE *stream = get_channel(AS_C_STRING(args[0]));
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Invalid channel. Expected \"stdout\", "
			"\"stderr\", or \"stdin\".",
			VALUE);
	}

	const int ch = fgetc(stream);
	if (ch == EOF) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Unexpected end of input on channel.",
					  IO);
	}

	if (ch != '\n') {
		flush_line(stream);
	}

	const char str[2] = {(char)ch, '\0'};
	ObjectString *s = copy_string(vm, str, 1);
	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/*
 * scanln_from(channel: string) -> Result<string>
 * Reads from the named channel up to (and discarding) the next '\n'.
 */
Value io_scanln_from_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<channel> must be of type 'string'.",
					  TYPE);
	}

	FILE *stream = get_channel(AS_C_STRING(args[0]));
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Invalid channel. Expected \"stdout\", "
			"\"stderr\", or \"stdin\".",
			VALUE);
	}

	ObjectString *s = NULL;
	if (!read_bounded_line(vm, stream, SCANLN_BUFFER_SIZE, &s)) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate buffer for input.", MEMORY);
	}

	if (ferror(stream)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from channel.",
					  IO);
	}

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/*
 * nscan_from(channel: string, n: int) -> Result<string>
 * Reads up to <n> characters from the named channel, stopping early at '\n'.
 */
Value io_nscan_from_function(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<channel> must be of type 'string'.",
					  TYPE);
	}

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be of type 'int'.",
					  TYPE);
	}

	FILE *stream = get_channel(AS_C_STRING(args[0]));
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Invalid channel. Expected \"stdout\", "
			"\"stderr\", or \"stdin\".",
			VALUE);
	}

	const int32_t n = AS_INT(args[1]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be a positive integer.",
					  VALUE);
	}

	ObjectString *s = NULL;
	if (!read_bounded_line(vm, stream, (size_t)n, &s)) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate buffer for input.", MEMORY);
	}

	if (ferror(stream)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from channel.",
					  IO);
	}

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}
