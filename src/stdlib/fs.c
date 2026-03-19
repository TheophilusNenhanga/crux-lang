#include "stdlib/fs.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "file_handler.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "vm.h"
#ifdef _WIN32
#include <direct.h> /* _mkdir */
#include <sys/stat.h>
#include <windows.h>
#define FS_STAT(path, st) _stat((path), (st))
#define FS_STAT_T struct _stat
#define FS_S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#define FS_S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define FS_MKDIR(path) _mkdir((path))
#define FS_FSEEK(f, o, w) _fseeki64((f), (o), (w))
#define FS_FTELL(f) _ftelli64((f))
typedef __int64 fs_off_t;
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define FS_STAT(path, st) stat((path), (st))
#define FS_STAT_T struct stat
#define FS_S_ISREG(m) S_ISREG(m)
#define FS_S_ISDIR(m) S_ISDIR(m)
#define FS_MKDIR(path) mkdir((path), 0755)
#define FS_FSEEK(f, o, w) fseeko((f), (o), (w))
#define FS_FTELL(f) ftello((f))
typedef off_t fs_off_t;
#endif

#define READLN_BUFFER_SIZE 4096
#define COPY_BUFFER_SIZE 65536 /* 64 KiB chunks for copy_file */

/*
 * All file methods share the same three pre-conditions: the receiver must
 * be an ObjectFile, the underlying FILE* must be non-NULL, and is_open
 * must be true.
 */
#define REQUIRE_OPEN_FILE(args, op)                                                                                    \
	do {                                                                                                               \
		const ObjectFile *_f = AS_CRUX_FILE((args)[0]);                                                                \
		if (!_f->is_open || _f->file == NULL) {                                                                        \
			return OBJECT_VAL(MAKE_GC_SAFE_ERROR(vm, "Cannot " op " a closed file.", IO));                             \
		}                                                                                                              \
	} while (0)

/*
 * Mode-capability predicates
 */
static bool mode_is_readable(const ObjectString *mode)
{
	const char *m = mode->chars;
	return strcmp(m, "r") == 0 || strcmp(m, "rb") == 0 || strcmp(m, "r+") == 0 || strcmp(m, "rb+") == 0 ||
		   strcmp(m, "r+b") == 0 || strcmp(m, "w+") == 0 || strcmp(m, "wb+") == 0 || strcmp(m, "w+b") == 0 ||
		   strcmp(m, "a+") == 0 || strcmp(m, "ab+") == 0 || strcmp(m, "a+b") == 0;
}

static bool mode_is_writable(const ObjectString *mode)
{
	const char *m = mode->chars;
	return strcmp(m, "w") == 0 || strcmp(m, "wb") == 0 || strcmp(m, "w+") == 0 || strcmp(m, "wb+") == 0 ||
		   strcmp(m, "w+b") == 0 || strcmp(m, "a") == 0 || strcmp(m, "ab") == 0 || strcmp(m, "a+") == 0 ||
		   strcmp(m, "ab+") == 0 || strcmp(m, "a+b") == 0 || strcmp(m, "r+") == 0 || strcmp(m, "rb+") == 0 ||
		   strcmp(m, "r+b") == 0;
}

/*
 * Converts the "whence" string used by fs.seek into a SEEK_* constant.
 * Returns -1 if the string is not recognised.
 */
static int whence_from_string(const char *s)
{
	if (strcmp(s, "start") == 0)
		return SEEK_SET;
	if (strcmp(s, "current") == 0)
		return SEEK_CUR;
	if (strcmp(s, "end") == 0)
		return SEEK_END;
	return -1;
}

/*
 * Reads the entire remaining content of an open FILE* from its current
 * position into a GC-managed buffer.  On success stores the result in
 * *out_str and returns true.  On failure returns false without allocating
 * any GC string (the raw buffer is freed internally if needed).
 *
 * This is shared by fs_read_all_method and the convenience
 * fs_read_file_function.
 */
static bool read_remaining(VM *vm, FILE *fp, ObjectString **out_str)
{
	/* Record current position, seek to end to get size, seek back. */
	const fs_off_t start = FS_FTELL(fp);
	if (start < 0)
		return false;

	if (FS_FSEEK(fp, 0, SEEK_END) != 0)
		return false;
	const fs_off_t end = FS_FTELL(fp);
	if (end < 0)
		return false;

	if (FS_FSEEK(fp, start, SEEK_SET) != 0)
		return false;

	const size_t byte_count = (size_t)(end - start);

	char *buffer = ALLOCATE(vm, char, byte_count + 1);
	if (buffer == NULL)
		return false;

	const size_t actually_read = fread(buffer, 1, byte_count, fp);
	buffer[actually_read] = '\0';

	/* take_string hands ownership of buffer to the GC */
	*out_str = take_string(vm, buffer, (uint32_t)actually_read);
	return true;
}

/**
 * Opens a file with the specified path and mode
 * arg0 -> path: String
 * arg1 -> mode: String (e.g., "r", "w", "a")
 * Returns Result<File>
 */
Value fs_open_function(VM *vm, const Value *args)
{
	const ObjectString *path_str = AS_CRUX_STRING(args[0]);
	ObjectString *mode_str = AS_CRUX_STRING(args[1]);

	char *resolved = resolve_path(vm->current_module_record->path->chars, path_str->chars);
	if (resolved == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Could not resolve file path.", IO);
	}

	ObjectString *full_path = take_string(vm, resolved, strlen(resolved));
	push(vm->current_module_record, OBJECT_VAL(full_path));

	ObjectFile *file = new_object_file(vm, full_path, mode_str);
	push(vm->current_module_record, OBJECT_VAL(file));

	if (file->file == NULL) {
		pop(vm->current_module_record); /* file */
		pop(vm->current_module_record); /* full_path */
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open file.", IO);
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(file));
	pop(vm->current_module_record); /* file */
	pop(vm->current_module_record); /* full_path */
	return OBJECT_VAL(res);
}

/**
 * Closes an open file
 * arg0 -> file: File
 * Returns Result<Nil>
 */
Value fs_close_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "close");

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	fflush(file->file);
	fclose(file->file);
	file->file = NULL;
	file->is_open = false;
	file->position = 0;

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Flushes any buffered data to the file
 * arg0 -> file: File
 * Returns Result<Nil>
 */
Value fs_flush_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "flush");

	const ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (fflush(file->file) != 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to flush file.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Reads up to n bytes from the current position in the file
 * arg0 -> file: File
 * arg1 -> n: Int
 * Returns Result<String>
 */
Value fs_read_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "read");

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be of type 'int'.", TYPE);
	}

	const int32_t n = AS_INT(args[1]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "<n> must be a positive integer.", VALUE);
	}

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_readable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for reading.", IO);
	}

	char *buffer = ALLOCATE(vm, char, (size_t)n + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate read buffer.", MEMORY);
	}

	const size_t actually_read = fread(buffer, 1, (size_t)n, file->file);
	buffer[actually_read] = '\0';
	file->position += (uint64_t)actually_read;

	if (ferror(file->file)) {
		FREE_ARRAY(vm, char, buffer, (size_t)n + 1);
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from file.", IO);
	}

	ObjectString *s = take_string(vm, buffer, (uint32_t)actually_read);
	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Reads a line from the file (up to but excluding newline)
 * arg0 -> file: File
 * Returns Result<String>
 */
Value fs_readln_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "read");

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_readable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for reading.", IO);
	}

	char *buffer = ALLOCATE(vm, char, READLN_BUFFER_SIZE + 1);
	if (buffer == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate read buffer.", MEMORY);
	}

	uint32_t count = 0;
	while (count < READLN_BUFFER_SIZE) {
		const int ch = fgetc(file->file);
		if (ch == EOF || ch == '\n')
			break;
		/* Normalise Windows CRLF: skip bare '\r' before '\n' */
		if (ch == '\r') {
			const int next = fgetc(file->file);
			if (next == '\n' || next == EOF)
				break;
			/* Lone '\r' — treat as line ending */
			ungetc(next, file->file);
			break;
		}
		buffer[count++] = (char)ch;
	}
	buffer[count] = '\0';
	file->position += count;

	if (ferror(file->file)) {
		FREE_ARRAY(vm, char, buffer, READLN_BUFFER_SIZE + 1);
		return MAKE_GC_SAFE_ERROR(vm, "Error reading from file.", IO);
	}

	ObjectString *s = take_string(vm, buffer, count);
	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Reads all remaining content from the current position to EOF
 * arg0 -> file: File
 * Returns Result<String>
 */
Value fs_read_all_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "read");

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_readable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for reading.", IO);
	}

	ObjectString *s = NULL;
	if (!read_remaining(vm, file->file, &s)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to read file contents.", IO);
	}

	file->position += s->byte_length;

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Reads all remaining lines from the file into an array (newlines stripped)
 * arg0 -> file: File
 * Returns Result<Array<String>>
 */
Value fs_read_lines_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "read");

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_readable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for reading.", IO);
	}

	ObjectArray *lines = new_array(vm, 2);
	push(vm->current_module_record, OBJECT_VAL(lines));

	char *buffer = ALLOCATE(vm, char, READLN_BUFFER_SIZE + 1);
	if (buffer == NULL) {
		pop(vm->current_module_record); /* lines */
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate read buffer.", MEMORY);
	}

	for (;;) {
		uint32_t count = 0;
		bool at_eof = false;

		while (count < READLN_BUFFER_SIZE) {
			const int ch = fgetc(file->file);
			if (ch == EOF) {
				at_eof = true;
				break;
			}
			if (ch == '\n')
				break;
			if (ch == '\r') {
				const int next = fgetc(file->file);
				if (next == '\n' || next == EOF) {
					at_eof = (next == EOF);
					break;
				}
				ungetc(next, file->file);
				break;
			}
			buffer[count++] = (char)ch;
		}
		buffer[count] = '\0';
		file->position += count;

		if (ferror(file->file)) {
			FREE_ARRAY(vm, char, buffer, READLN_BUFFER_SIZE + 1);
			pop(vm->current_module_record); /* lines */
			return MAKE_GC_SAFE_ERROR(vm, "Error reading from file.", IO);
		}

		/* Only skip the very first iteration's empty result if we are
		 * immediately at EOF (i.e. the file was empty). */
		if (at_eof && count == 0)
			break;

		ObjectString *line = copy_string(vm, buffer, count);
		push(vm->current_module_record, OBJECT_VAL(line));
		array_add_back(vm, lines, OBJECT_VAL(line));
		pop(vm->current_module_record); /* line */

		if (at_eof)
			break;
	}

	FREE_ARRAY(vm, char, buffer, READLN_BUFFER_SIZE + 1);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(lines));
	pop(vm->current_module_record); /* lines */
	return OBJECT_VAL(res);
}

/**
 * Writes a string to the file
 * arg0 -> file: File
 * arg1 -> content: String
 * Returns Result<Nil>
 */
Value fs_write_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "write");

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<content> must be of type 'string'.", TYPE);
	}

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_writable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for writing.", IO);
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);
	const size_t written = fwrite(content->chars, 1, content->byte_length, file->file);

	if (written < content->byte_length || ferror(file->file)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to file.", IO);
	}

	file->position += written;
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Writes a string to the file followed by a newline
 * arg0 -> file: File
 * arg1 -> content: String
 * Returns Result<Nil>
 */
Value fs_writeln_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "write");

	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<content> must be of type 'string'.", TYPE);
	}

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	if (!mode_is_writable(file->mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "File is not open for writing.", IO);
	}

	const ObjectString *content = AS_CRUX_STRING(args[1]);
	const size_t written = fwrite(content->chars, 1, content->byte_length, file->file);

	if (written < content->byte_length || ferror(file->file)) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to file.", IO);
	}

	if (fputc('\n', file->file) == EOF) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to file.", IO);
	}

	file->position += written + 1;
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Seeks to a position in the file
 * arg0 -> file: File
 * arg1 -> offset: Int
 * arg2 -> whence: String ("start", "current", or "end")
 * Returns Result<Nil>
 */
Value fs_seek_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "seek");

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<offset> must be of type 'int'.", TYPE);
	}
	if (!IS_CRUX_STRING(args[2])) {
		return MAKE_GC_SAFE_ERROR(vm, "<whence> must be of type 'string'.", TYPE);
	}

	const int whence = whence_from_string(AS_C_STRING(args[2]));
	if (whence == -1) {
		return MAKE_GC_SAFE_ERROR(vm,
								  "Invalid <whence>. Expected \"start\", "
								  "\"current\", or \"end\".",
								  VALUE);
	}

	ObjectFile *file = AS_CRUX_FILE(args[0]);
	const fs_off_t offset = (fs_off_t)AS_INT(args[1]);

	if (FS_FSEEK(file->file, offset, whence) != 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to seek in file.", IO);
	}

	const fs_off_t new_pos = FS_FTELL(file->file);
	if (new_pos < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to determine position after seek.", IO);
	}

	file->position = (uint64_t)new_pos;
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Returns the current position in the file
 * arg0 -> file: File
 * Returns Result<Int>
 */
Value fs_tell_method(VM *vm, const Value *args)
{
	REQUIRE_OPEN_FILE(args, "tell");

	ObjectFile *file = AS_CRUX_FILE(args[0]);

	const fs_off_t pos = FS_FTELL(file->file);
	if (pos < 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to determine file position.", IO);
	}

	file->position = (uint64_t)pos;
	return OBJECT_VAL(new_ok_result(vm, INT_VAL((int32_t)pos)));
}

/**
 * Checks if the file is currently open
 * arg0 -> file: File
 * Returns Bool
 */
Value fs_is_open_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectFile *file = AS_CRUX_FILE(args[0]);
	return BOOL_VAL(file->is_open && file->file != NULL);
}

/**
 * Checks if a path exists
 * arg0 -> path: String
 * Returns Bool
 */
Value fs_exists_function(VM *vm, const Value *args)
{
	(void)vm;

	FS_STAT_T st;
	return BOOL_VAL(FS_STAT(AS_C_STRING(args[0]), &st) == 0);
}

/**
 * Checks if a path is a regular file
 * arg0 -> path: String
 * Returns Bool
 */
Value fs_is_file_function(VM *vm, const Value *args)
{
	(void)vm;
	FS_STAT_T st;
	if (FS_STAT(AS_C_STRING(args[0]), &st) != 0)
		return BOOL_VAL(false);
	return BOOL_VAL(FS_S_ISREG(st.st_mode));
}

/**
 * Checks if a path is a directory
 * arg0 -> path: String
 * Returns Bool
 */
Value fs_is_dir_function(VM *vm, const Value *args)
{
	(void)vm;
	FS_STAT_T st;
	if (FS_STAT(AS_C_STRING(args[0]), &st) != 0)
		return BOOL_VAL(false);
	return BOOL_VAL(FS_S_ISDIR(st.st_mode));
}

/**
 * Returns the size of a file in bytes
 * arg0 -> path: String
 * Returns Result<Float>
 */
Value fs_file_size_function(VM *vm, const Value *args)
{
	FS_STAT_T st;
	if (FS_STAT(AS_C_STRING(args[0]), &st) != 0) {
		return MAKE_GC_SAFE_ERROR(vm, "File not found or inaccessible.", IO);
	}

	if (!FS_S_ISREG(st.st_mode)) {
		return MAKE_GC_SAFE_ERROR(vm, "Path is not a regular file.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL((double)st.st_size)));
}

/**
 * Deletes a file
 * arg0 -> path: String
 * Returns Result<Nil>
 */
Value fs_remove_function(VM *vm, const Value *args)
{
	const char *path = AS_C_STRING(args[0]);

	/* Reject directories explicitly for a clear error message */
	FS_STAT_T st;
	if (FS_STAT(path, &st) == 0 && FS_S_ISDIR(st.st_mode)) {
		return MAKE_GC_SAFE_ERROR(vm,
								  "Path is a directory. Use fs.remove_dir to remove "
								  "directories.",
								  IO);
	}

	if (remove(path) != 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to remove file.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/*
 * rename(from: string, to: string) -> Result<nil>
 */
Value fs_rename_function(VM *vm, const Value *args)
{
	const char *from = AS_C_STRING(args[0]);
	const char *to = AS_C_STRING(args[1]);

#ifdef _WIN32
	/*
	 * On Windows, rename() fails if <to> already exists.
	 * MoveFileExA with MOVEFILE_REPLACE_EXISTING gives POSIX rename()
	 * semantics (atomic replace on the same volume).
	 */
	if (!MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to rename file.", IO);
	}
#else
	if (rename(from, to) != 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to rename file.", IO);
	}
#endif

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Copies a file from one path to another
 * arg0 -> from: String
 * arg1 -> to: String
 * Returns Result<Nil>
 */
Value fs_copy_file_function(VM *vm, const Value *args)
{
	const char *from = AS_C_STRING(args[0]);
	const char *to = AS_C_STRING(args[1]);

#ifdef _WIN32
	/*
	 * CopyFileA is atomic w.r.t. file system consistency on Windows and
	 * handles all the edge cases (attributes, sharing) correctly.
	 * FALSE = do not fail if destination exists (overwrite).
	 */
	if (!CopyFileA(from, to, FALSE)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to copy file.", IO);
	}
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
#else
	FILE *src = fopen(from, "rb");
	if (src == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open source file.", IO);
	}

	FILE *dst = fopen(to, "wb");
	if (dst == NULL) {
		fclose(src);
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open destination file.", IO);
	}

	/* Use a stack buffer — no GC allocation needed for the copy loop */
	char chunk[COPY_BUFFER_SIZE];
	bool ok = true;

	while (!feof(src)) {
		const size_t n = fread(chunk, 1, sizeof(chunk), src);
		if (ferror(src)) {
			ok = false;
			break;
		}
		if (n == 0)
			break;
		if (fwrite(chunk, 1, n, dst) < n) {
			ok = false;
			break;
		}
	}

	fclose(src);
	fclose(dst);

	if (!ok) {
		/* Best-effort cleanup of a partial destination */
		remove(to);
		return MAKE_GC_SAFE_ERROR(vm, "Error during file copy.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
#endif
}

/**
 * Creates a directory
 * arg0 -> path: String
 * Returns Result<Nil>
 */
Value fs_mkdir_function(VM *vm, const Value *args)
{
	const char *path = AS_C_STRING(args[0]);

	if (FS_MKDIR(path) != 0) {
		if (errno == EEXIST) {
			return MAKE_GC_SAFE_ERROR(vm, "Directory already exists.", IO);
		}
		if (errno == ENOENT) {
			return MAKE_GC_SAFE_ERROR(vm,
									  "Parent directory does not exist. "
									  "Create parent directories first.",
									  IO);
		}
		return MAKE_GC_SAFE_ERROR(vm, "Failed to create directory.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Reads the entire contents of a file
 * arg0 -> path: String
 * Returns Result<String>
 */
Value fs_read_file_function(VM *vm, const Value *args)
{
	const char *path = AS_C_STRING(args[0]);

	FILE *fp = fopen(path, "rb");
	if (fp == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open file.", IO);
	}

	ObjectString *s = NULL;
	if (!read_remaining(vm, fp, &s)) {
		fclose(fp);
		return MAKE_GC_SAFE_ERROR(vm, "Failed to read file contents.", IO);
	}

	fclose(fp);

	push(vm->current_module_record, OBJECT_VAL(s));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(s));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Writes content to a file (creates or truncates the file)
 * arg0 -> path: String
 * arg1 -> content: String
 * Returns Result<Nil>
 */
Value fs_write_file_function(VM *vm, const Value *args)
{
	const char *path = AS_C_STRING(args[0]);
	const ObjectString *content = AS_CRUX_STRING(args[1]);

	FILE *fp = fopen(path, "wb");
	if (fp == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open file.", IO);
	}

	const size_t written = fwrite(content->chars, 1, content->byte_length, fp);
	const bool ok = (written == content->byte_length) && !ferror(fp);
	fclose(fp);

	if (!ok) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to file.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Appends content to a file (creates the file if it doesn't exist)
 * arg0 -> path: String
 * arg1 -> content: String
 * Returns Result<Nil>
 */
Value fs_append_file_function(VM *vm, const Value *args)
{
	const char *path = AS_C_STRING(args[0]);
	const ObjectString *content = AS_CRUX_STRING(args[1]);

	FILE *fp = fopen(path, "ab");
	if (fp == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to open file.", IO);
	}

	const size_t written = fwrite(content->chars, 1, content->byte_length, fp);
	const bool ok = (written == content->byte_length) && !ferror(fp);
	fclose(fp);

	if (!ok) {
		return MAKE_GC_SAFE_ERROR(vm, "Error writing to file.", IO);
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}
