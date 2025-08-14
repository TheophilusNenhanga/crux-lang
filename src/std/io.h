#ifndef IO_H
#define IO_H

#include "../value.h"
#include "std.h"

Value print_function(VM *vm, int arg_count,
		     const Value *args); // write to stdout without newline
Value println_function(VM *vm, int arg_count,
		       const Value *args); // write to stdout with newline

ObjectResult *print_to_function(
	VM *vm, int arg_count,
	const Value *args); // write to specified channel without newline
ObjectResult *scan_function(VM *vm, int arg_count,
			    const Value *args); // read single char from stdin
ObjectResult *
scanln_function(VM *vm, int arg_count,
		const Value *args); // read from stdin until [enter]/[return]
				    // key (newline) is reached
ObjectResult *scan_from_function(
	VM *vm, int arg_count,
	const Value *args); // read single char from specified channel
ObjectResult *scanln_from_function(
	VM *vm, int arg_count,
	const Value *args); // read from specified channel until
			    // [enter]/[return] key (newline) is reached
ObjectResult *nscan_function(VM *vm, int arg_count,
			     const Value *args); // read n characters from stdin
ObjectResult *nscan_from_function(
	VM *vm, int arg_count,
	const Value *args); // read n characters from specified channel

ObjectResult *open_file_function(VM *vm, int arg_count, const Value *args);
ObjectResult *readln_file_method(VM *vm, int arg_count, const Value *args);
ObjectResult *read_all_file_method(VM *vm, int arg_count, const Value *args);
ObjectResult *write_file_method(VM *vm, int arg_count, const Value *args);
ObjectResult *writeln_file_method(VM *vm, int arg_count, const Value *args);
ObjectResult *close_file_method(VM *vm, int arg_count, const Value *args);

#endif // IO_H
