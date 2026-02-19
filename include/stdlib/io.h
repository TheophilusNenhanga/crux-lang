#ifndef IO_H
#define IO_H

#include "value.h"
#include "std.h"

/*
 * io module — stream and terminal I/O
 *
 * Deals exclusively with reading from and writing to character streams
 * (stdin, stdout, stderr, or a named channel string).  Has no knowledge
 * of the filesystem; ObjectFile lives in fs.
 *
 * Channel strings accepted by the *_from / print_to variants:
 *   "stdin"  "stdout"  "stderr"
 */

/* ── Output ────────────────────────────────────────────────────────────────── */

/* print(value)
 * Writes a string representation of <value> to stdout. No newline. */
Value io_print_function(VM *vm, int arg_count, const Value *args);

/* println(value)
 * Writes a string representation of <value> to stdout followed by '\n'. */
Value io_println_function(VM *vm, int arg_count, const Value *args);

/* print_to(channel: string, value)  -> Result<nil>
 * Writes a string representation of <value> to the named channel.
 * Returns an error if the channel string is not recognised. */
Value io_print_to_function(VM *vm, int arg_count, const Value *args);

/* println_to(channel: string, value)  -> Result<nil>
 * Same as print_to but appends '\n'. */
Value io_println_to_function(VM *vm, int arg_count, const Value *args);

/* ── Input — stdin ─────────────────────────────────────────────────────────── */

/* scan()  -> Result<string>
 * Reads exactly one character from stdin and discards the rest of the line. */
Value io_scan_function(VM *vm, int arg_count, const Value *args);

/* scanln()  -> Result<string>
 * Reads from stdin up to (and discarding) the next '\n'.
 * Returns the line without the newline character. */
Value io_scanln_function(VM *vm, int arg_count, const Value *args);

/* nscan(n: int)  -> Result<string>
 * Reads up to <n> characters from stdin, stopping early on '\n'.
 * Discards any remaining characters up to '\n' when the limit is hit. */
Value io_nscan_function(VM *vm, int arg_count, const Value *args);

/* ── Input — named channel ─────────────────────────────────────────────────── */

/* scan_from(channel: string)  -> Result<string>
 * Reads exactly one character from the named channel,
 * discarding the rest of the line. */
Value io_scan_from_function(VM *vm, int arg_count, const Value *args);

/* scanln_from(channel: string)  -> Result<string>
 * Reads from the named channel up to (and discarding) the next '\n'. */
Value io_scanln_from_function(VM *vm, int arg_count,
                                      const Value *args);

/* nscan_from(channel: string, n: int)  -> Result<string>
 * Reads up to <n> characters from the named channel,
 * stopping early on '\n'. */
Value io_nscan_from_function(VM *vm, int arg_count, const Value *args);

#endif // IO_H
