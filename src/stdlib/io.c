#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_handler.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "stdlib/io.h"
#include "vm.h"

#define MAX_LINE_LENGTH 4096

static FILE *get_channel(const char *channel)
{
	if (strcmp(channel, "stdin") == 0)
		return stdin;
	if (strcmp(channel, "stdout") == 0)
		return stdout;
	if (strcmp(channel, "stderr") == 0)
		return stderr;
	return NULL;
}

Value print_function(VM *vm, int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	print_value(args[0], false);
	return NIL_VAL;
}

Value println_function(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	print_value(args[0], false);
	printf("\n");
	return NIL_VAL;
}

ObjectResult *print_to_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Channel and content must be strings.", TYPE);
	}

	const char *channel = AS_C_STRING(args[0]);
	const char *content = AS_C_STRING(args[1]);

	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Invalid channel specified.",
					  VALUE);
	}

	if (fprintf(stream, "%s", content) < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to stream.", IO);
	}

	return new_ok_result(vm, BOOL_VAL(true));
}

ObjectResult *scan_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	const int ch = getchar();
	if (ch == EOF) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stdin.", IO);
	}

	int overflow;
	while ((overflow = getchar()) != '\n' && overflow != EOF)
		;

	const char str[2] = {ch, '\0'};
	ObjectString *result_str = copy_string(vm, str, 1);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *scanln_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stdin.", IO);
	}

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	ObjectString *result_str = copy_string(vm, buffer, len);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *scan_from_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Channel must be a string.",
					  TYPE);
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Invalid channel specified.",
					  VALUE);
	}

	const int ch = fgetc(stream);
	if (ch == EOF) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stream.", IO);
	}

	int overflow;
	while ((overflow = fgetc(stream)) != '\n' && overflow != EOF)
		;

	const char str[2] = {ch, '\0'};
	ObjectString *result_str = copy_string(vm, str, 1);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *scanln_from_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Channel must be a string.",
					  TYPE);
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Invalid channel specified.",
					  VALUE);
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from stream.", IO);
	}

	size_t len = strlen(buffer);
	// discard extra characters
	if (len > 0 && buffer[len - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF)
			;
	}

	// Removing trailing newline character
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	ObjectString *result_str = copy_string(vm, buffer, len);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *nscan_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Number of characters must be a number.", TYPE);
	}

	const size_t n = (size_t)AS_INT(args[0]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Number of characters must be positive.", VALUE);
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate memory for input buffer.",
			MEMORY);
	}

	size_t read = 0;
	while (read < n) {
		const int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return MAKE_GC_SAFE_ERROR(vm,
						  "Error reading from stdin.",
						  IO);
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = getchar()) != '\n' && ch != EOF)
			;
	}

	ObjectString *result_str = copy_string(vm, buffer, read);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	FREE_ARRAY(vm, char, buffer, n + 1);
	return res;
}

ObjectResult *nscan_from_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Channel must be a string.",
					  TYPE);
	}

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<char_count> must be of type 'int'.",
					  TYPE);
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Invalid channel specified.",
					  VALUE);
	}

	const size_t n = (size_t)AS_INT(args[1]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Number of characters must be positive.", VALUE);
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate memory for input buffer.",
			MEMORY);
	}

	size_t read = 0;
	while (read < n) {
		const int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return MAKE_GC_SAFE_ERROR(vm,
						  "Error reading from stream.",
						  IO);
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF)
			;
	}

	ObjectString *result_str = copy_string(vm, buffer, read);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	FREE_ARRAY(vm, char, buffer, n + 1);
	return res;
}

ObjectResult *open_file_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	if (!IS_CRUX_STRING(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<file_path> must be of type 'string'.", IO);
	}

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<file_mode> must be of type 'string'.", IO);
	}

	const ObjectString *path = AS_CRUX_STRING(args[0]);
	ObjectString *mode = AS_CRUX_STRING(args[1]);

	char *resolvedPath = resolve_path(
		vm->current_module_record->path->chars, path->chars);
	if (resolvedPath == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not resolve path to file.",
					  IO);
	}

	ObjectString *newPath = take_string(vm, resolvedPath,
					    strlen(resolvedPath));
	push(vm->current_module_record, OBJECT_VAL(newPath));

	ObjectFile *file = new_object_file(vm, newPath, mode);
	push(vm->current_module_record, OBJECT_VAL(file));

	if (file->file == NULL) {
		pop(vm->current_module_record); // pop file
		pop(vm->current_module_record); // pop newPath
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open file.", IO);
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(file));
	pop(vm->current_module_record); // pop file
	pop(vm->current_module_record); // pop newPath

	return res;
}

static bool is_readable(const ObjectString *mode)
{
	return strcmp(mode->chars, "r") == 0 ||
	       strcmp(mode->chars, "rb") == 0 ||
	       strcmp(mode->chars, "r+") == 0 ||
	       strcmp(mode->chars, "rb+") == 0 ||
	       strcmp(mode->chars, "a+") == 0 ||
	       strcmp(mode->chars, "ab+") == 0 ||
	       strcmp(mode->chars, "w+") == 0 ||
	       strcmp(mode->chars, "wb+") == 0;
}

static bool is_writable(const ObjectString *mode)
{
	return strcmp(mode->chars, "w") == 0 ||
	       strcmp(mode->chars, "wb") == 0 ||
	       strcmp(mode->chars, "w+") == 0 ||
	       strcmp(mode->chars, "wb+") == 0 ||
	       strcmp(mode->chars, "a") == 0 ||
	       strcmp(mode->chars, "ab") == 0 ||
	       strcmp(mode->chars, "a+") == 0 ||
	       strcmp(mode->chars, "ab+") == 0 ||
	       strcmp(mode->chars, "r+") == 0 ||
	       strcmp(mode->chars, "rb+") == 0;
}

static bool is_appendable(const ObjectString *mode)
{
	return strcmp(mode->chars, "a") == 0 ||
	       strcmp(mode->chars, "ab") == 0 ||
	       strcmp(mode->chars, "a+") == 0 ||
	       strcmp(mode->chars, "ab+") == 0 ||
	       strcmp(mode->chars, "r+") == 0 ||
	       strcmp(mode->chars, "rb+") == 0 ||
	       strcmp(mode->chars, "w+") == 0 ||
	       strcmp(mode->chars, "wb+") == 0 ||
	       strcmp(mode->chars, "rb") == 0;
}

ObjectResult *readln_file_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not read file.", IO);
	}

	if (!file->is_open) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open.", IO);
	}

	if (!is_readable(file->mode) && !is_appendable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not readable.", IO);
	}

	char *buffer = ALLOCATE(vm, char, MAX_LINE_LENGTH);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate memory for file content.",
			MEMORY);
	}

	int readCount = 0;
	while (readCount < MAX_LINE_LENGTH) {
		const int ch = fgetc(file->file);
		if (ch == EOF || ch == '\n') {
			break;
		}
		buffer[readCount++] = ch;
	}
	buffer[readCount] = '\0';
	file->position += readCount;

	ObjectString *result_str = take_string(vm, buffer, readCount);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *read_all_file_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	const ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not read file.", IO);
	}

	if (!file->is_open) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open.", IO);
	}

	if (!is_readable(file->mode) && !is_appendable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not readable.", IO);
	}

	fseek(file->file, 0, SEEK_END);
	const long fileSize = ftell(file->file);
	fseek(file->file, 0, SEEK_SET);

	char *buffer = ALLOCATE(vm, char, fileSize + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Failed to allocate memory for file content.",
			MEMORY);
	}

	size_t _ = fread(buffer, 1, fileSize, file->file);
	buffer[fileSize] = '\0';

	ObjectString *result_str = take_string(vm, buffer, fileSize);
	push(vm->current_module_record, OBJECT_VAL(result_str));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_str));
	pop(vm->current_module_record);

	return res;
}

ObjectResult *close_file_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not close file.", IO);
	}

	if (!file->is_open) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open.", IO);
	}

	fclose(file->file);
	file->file = NULL;
	file->is_open = false;
	file->position = 0;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *write_file_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (file->file == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not write to file.", IO);
	}

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<content> must be of type 'string'.",
					  IO);
	}

	if (!file->is_open) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open.", IO);
	}

	if (!is_writable(file->mode) && !is_appendable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not writable.", IO);
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);

	fwrite(content->chars, sizeof(char), content->length, file->file);
	file->position += content->length;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *writeln_file_method(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (file->file == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not write to file.", IO);
	}

	if (!file->is_open) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open.", IO);
	}

	if (!is_writable(file->mode) && !is_appendable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not writable.", IO);
	}

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<content> must be of type 'string'.",
					  IO);
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);
	fwrite(content->chars, sizeof(char), content->length, file->file);
	fwrite("\n", sizeof(char), 1, file->file);
	file->position += content->length + 1;
	return new_ok_result(vm, NIL_VAL);
}
