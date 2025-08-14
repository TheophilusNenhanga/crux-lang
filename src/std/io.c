#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file_handler.h"
#include "../memory.h"
#include "../object.h"
#include "../vm/vm.h"

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

Value print_function(VM *vm __attribute__((unused)),
		     int arg_count __attribute__((unused)), const Value *args)
{
	print_value(args[0], false);
	return NIL_VAL;
}

Value println_function(VM *vm __attribute__((unused)),
		       const int arg_count __attribute__((unused)),
		       const Value *args)
{
	print_value(args[0], false);
	printf("\n");
	return NIL_VAL;
}

ObjectResult *print_to_function(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "Channel and content must be strings.",
					 36),
				 TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	const char *content = AS_C_STRING(args[1]);

	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Invalid channel specified.",
					    26),
				 VALUE, false));
	}

	if (fprintf(stream, "%s", content) < 0) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Error writing to stream.", 26),
				 IO, false));
	}

	return new_ok_result(vm, BOOL_VAL(true));
}

ObjectResult *scan_function(VM *vm, int arg_count __attribute__((unused)),
			    const Value *args __attribute__((unused)))
{
	const int ch = getchar();
	if (ch == EOF) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Error reading from stdin.",
						25),
				     IO, false));
	}

	int overflow;
	while ((overflow = getchar()) != '\n' && overflow != EOF)
		;

	const char str[2] = {ch, '\0'};
	return new_ok_result(vm, OBJECT_VAL(copy_string(vm, str, 1)));
}

ObjectResult *scanln_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args __attribute__((unused)))
{
	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Error reading from stdin.",
						26),
				     IO, false));
	}

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	return new_ok_result(vm, OBJECT_VAL(copy_string(vm, buffer, len)));
}

ObjectResult *scan_from_function(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Channel must be a string.",
						25),
				     TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Invalid channel specified.",
					    26),
				 VALUE, false));
	}

	const int ch = fgetc(stream);
	if (ch == EOF) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Error reading from stream.",
					    26),
				 IO, false));
	}

	int overflow;
	while ((overflow = fgetc(stream)) != '\n' && overflow != EOF)
		;

	const char str[2] = {ch, '\0'};
	return new_ok_result(vm, OBJECT_VAL(copy_string(vm, str, 1)));
}

ObjectResult *scanln_from_function(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Channel must be a string.",
						25),
				     TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Invalid channel specified.",
					    26),
				 VALUE, false));
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Error reading from stream.",
					    26),
				 IO, false));
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

	return new_ok_result(vm, OBJECT_VAL(copy_string(vm, buffer, len)));
}

ObjectResult *nscan_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	if (!IS_INT(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Number of characters must be "
						"a number.",
						38),
				     TYPE, false));
	}

	const size_t n = (size_t)AS_INT(args[0]);
	if (n <= 0) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Number of characters must be "
						"positive.",
						38),
				     VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory for "
						"input buffer.",
						43),
				     MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		const int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return new_error_result(
				vm,
				new_error(vm,
					 copy_string(vm,
						    "Error reading from stdin.",
						    25),
					 IO, false));
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

	const Value string = OBJECT_VAL(copy_string(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return new_ok_result(vm, string);
}

ObjectResult *nscan_from_function(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "Channel must be a string.",
						25),
				     TYPE, false));
	}

	if (!IS_INT(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "<char_count> must be of type 'int'.",
					 38),
				 TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = get_channel(channel);
	if (stream == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Invalid channel specified.",
					    26),
				 VALUE, false));
	}

	const size_t n = (size_t)AS_INT(args[1]);
	if (n <= 0) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Number of characters must be "
						"positive.",
						38),
				     VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory for "
						"input buffer.",
						43),
				     MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		const int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return new_error_result(
				vm,
				new_error(
					vm,
					copy_string(vm,
						   "Error reading from stream.",
						   26),
					IO, false));
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

	const Value string = OBJECT_VAL(copy_string(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return new_ok_result(vm, string);
}

ObjectResult *open_file_function(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return new_error_result(
			vm,
			new_error(
				vm,
				copy_string(
					vm,
					"<file_path> must be of type 'string'.",
					37),
				IO, false));
	}

	if (!IS_CRUX_STRING(args[1])) {
		return new_error_result(
			vm,
			new_error(
				vm,
				copy_string(
					vm,
					"<file_mode> must be of type 'string'.",
					37),
				IO, false));
	}

	const ObjectString *path = AS_CRUX_STRING(args[0]);
	ObjectString *mode = AS_CRUX_STRING(args[1]);

	char *resolvedPath = resolve_path(vm->currentModuleRecord->path->chars,
					 path->chars);
	if (resolvedPath == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Could not resolve path to file.",
					    31),
				 IO, false));
	}

	ObjectString *newPath = take_string(vm, resolvedPath,
					   strlen(resolvedPath));

	ObjectFile *file = new_object_file(vm, newPath, mode);
	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "Failed to open file.", 20),
				 IO, false));
	}

	return new_ok_result(vm, OBJECT_VAL(file));
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

ObjectResult *readln_file_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "Could not read file.", 20),
				 IO, false));
	}

	if (!file->is_open) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "File is not open.", 17),
				 IO, false));
	}

	if (!is_readable(file->mode) && !is_appendable(file->mode)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "File is not readable.", 21),
				 IO, false));
	}

	char *buffer = ALLOCATE(vm, char, MAX_LINE_LENGTH);
	if (buffer == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory for "
						"file content.",
						43),
				     MEMORY, false));
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
	return new_ok_result(vm, OBJECT_VAL(take_string(vm, buffer, readCount)));
}

ObjectResult *read_all_file_method(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	const ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "Could not read file.", 20),
				 IO, false));
	}

	if (!file->is_open) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "File is not open.", 17),
				 IO, false));
	}

	if (!is_readable(file->mode) && !is_appendable(file->mode)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "File is not readable.", 21),
				 IO, false));
	}

	fseek(file->file, 0, SEEK_END);
	const long fileSize = ftell(file->file);
	fseek(file->file, 0, SEEK_SET);

	char *buffer = ALLOCATE(vm, char, fileSize + 1);
	if (buffer == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory for "
						"file content.",
						43),
				     MEMORY, false));
	}

	size_t _ __attribute__((unused)) = fread(buffer, 1, fileSize,
						 file->file);
	buffer[fileSize] = '\0';

	return new_ok_result(vm, OBJECT_VAL(take_string(vm, buffer, fileSize)));
}

ObjectResult *close_file_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectFile *file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Could not close file.", 21),
				 IO, false));
	}

	if (!file->is_open) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "File is not open.", 17),
				 IO, false));
	}

	fclose(file->file);
	file->is_open = false;
	file->position = 0;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *write_file_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Could not write to file.", 21),
				 IO, false));
	}

	if (!IS_CRUX_STRING(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "<content> must be of type 'string'.",
					 37),
				 IO, false));
	}

	if (!file->is_open) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "File is not open.", 17),
				 IO, false));
	}

	if (!is_writable(file->mode) && !is_appendable(file->mode)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "File is not writable.", 21),
				 IO, false));
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);

	fwrite(content->chars, sizeof(char), content->length, file->file);
	file->position += content->length;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *writeln_file_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (file->file == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Could not write to file.", 21),
				 IO, false));
	}

	if (!file->is_open) {
		return new_error_result(
			vm,
			new_error(vm, copy_string(vm, "File is not open.", 17),
				 IO, false));
	}

	if (!is_writable(file->mode) && !is_appendable(file->mode)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "File is not writable.", 21),
				 IO, false));
	}

	if (!IS_CRUX_STRING(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "<content> must be of type 'string'.",
					 37),
				 IO, false));
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);
	fwrite(content->chars, sizeof(char), content->length, file->file);
	fwrite("\n", sizeof(char), 1, file->file);
	file->position += content->length + 1;
	return new_ok_result(vm, NIL_VAL);
}
