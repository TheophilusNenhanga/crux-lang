#ifndef FS_H
#define FS_H

#include "value.h"

/*
 * fs module — filesystem operations
 *
 * All functions that touch files on disk live here.  The ObjectFile handle
 * (defined in object.h) is the currency for the method-style operations.
 *
 * Conventions:
 *   - Every function that can fail returns Result<T>.
 *   - Methods take the ObjectFile as args[0] (the receiver).
 *   - Paths are always ObjectString values.
 *   - Byte counts and offsets are int (int32_t in Value terms).
 *   - "whence" for seek is a string: "start" | "current" | "end"
 */

/* ── File handle construction ──────────────────────────────────────────────── */

/* open(path: string, mode: string)  -> Result<File>
 * Opens the file at <path> with the given <mode> string ("r", "w", "a",
 * "r+", "rb", etc.).  The path is resolved relative to the current module. */
Value fs_open_function(VM *vm, const Value *args);

/* ── Methods on File handles ───────────────────────────────────────────────── */

/* close()  -> Result<nil>
 * Flushes and closes the file handle.  Subsequent calls return an error. */
Value fs_close_method(VM *vm, const Value *args);

/* flush()  -> Result<nil>
 * Flushes any buffered writes to disk without closing the handle. */
Value fs_flush_method(VM *vm, const Value *args);

/* read(n: int)  -> Result<string>
 * Reads up to <n> bytes and returns them as a string.
 * Returns an empty string when at EOF. */
Value fs_read_method(VM *vm, const Value *args);

/* readln()  -> Result<string>
 * Reads from the current position up to the next '\n' (exclusive).
 * Returns an empty string when at EOF. */
Value fs_readln_method(VM *vm, const Value *args);

/* read_all()  -> Result<string>
 * Reads the entire remaining content of the file from the current position. */
Value fs_read_all_method(VM *vm, const Value *args);

/* read_lines()  -> Result<Array<string>>
 * Reads all remaining lines into an Array.  Newline characters are stripped. */
Value fs_read_lines_method(VM *vm, const Value *args);

/* write(content: string)  -> Result<nil>
 * Writes <content> to the file at the current position. */
Value fs_write_method(VM *vm, const Value *args);

/* writeln(content: string)  -> Result<nil>
 * Writes <content> followed by '\n' to the file. */
Value fs_writeln_method(VM *vm, const Value *args);

/* seek(offset: int, whence: string)  -> Result<nil>
 * Moves the file position.
 * <whence> must be one of: "start" | "current" | "end" */
Value fs_seek_method(VM *vm, const Value *args);

/* tell()  -> Result<int>
 * Returns the current byte offset within the file. */
Value fs_tell_method(VM *vm, const Value *args);

/* is_open()  -> bool   (infallible)
 * Returns true if the file handle is currently open. */
Value fs_is_open_method(VM *vm, const Value *args);

/* ── Filesystem queries (path-based, no handle required) ───────────────────── */

/* exists(path: string)  -> bool   (infallible)
 * Returns true if a file or directory exists at <path>. */
Value fs_exists_function(VM *vm, const Value *args);

/* is_file(path: string)  -> bool   (infallible)
 * Returns true if <path> exists and is a regular file. */
Value fs_is_file_function(VM *vm, const Value *args);

/* is_dir(path: string)  -> bool   (infallible)
 * Returns true if <path> exists and is a directory. */
Value fs_is_dir_function(VM *vm, const Value *args);

/* file_size(path: string)  -> Result<int>
 * Returns the size of the file in bytes. */
Value fs_file_size_function(VM *vm, const Value *args);

/* ── Filesystem mutations (path-based) ─────────────────────────────────────── */

/* remove(path: string)  -> Result<nil>
 * Deletes the file at <path>. Returns an error if it does not exist
 * or is a directory. */
Value fs_remove_function(VM *vm, const Value *args);

/* rename(from: string, to: string)  -> Result<nil>
 * Moves / renames a file or directory. */
Value fs_rename_function(VM *vm, const Value *args);

/* copy_file(from: string, to: string)  -> Result<nil>
 * Copies the contents of <from> to <to>, creating or truncating <to>. */
Value fs_copy_file_function(VM *vm, const Value *args);

/* mkdir(path: string)  -> Result<nil>
 * Creates a single directory.  Fails if the parent does not exist. */
Value fs_mkdir_function(VM *vm, const Value *args);

/* ── Convenience (one-shot, no handle) ─────────────────────────────────────── */

/* read_file(path: string)  -> Result<string>
 * Opens the file, reads its entire content, closes it, returns the string. */
Value fs_read_file_function(VM *vm, const Value *args);

/* write_file(path: string, content: string)  -> Result<nil>
 * Creates or truncates the file and writes <content> to it. */
Value fs_write_file_function(VM *vm, const Value *args);

/* append_file(path: string, content: string)  -> Result<nil>
 * Opens the file in append mode and writes <content> to it. */
Value fs_append_file_function(VM *vm, const Value *args);

#endif // FS_H
