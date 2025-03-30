#ifndef STL_IO_H
#define STL_IO_H

#include "../value.h"
#include "std.h"

ObjectResult* _openFile(VM*vm, int argCount, Value* args);			// opens a file and makes a new File Object
ObjectResult* _closeFile(VM*vm, int argCount, Value* args);

// File Writing functions
ObjectResult* _writeln(VM *vm, int argCount, Value *args);			// Write line to file

Value _print(VM *vm, int argCount, Value *args);				// write to stdout without newline
Value _println(VM *vm, int argCount, Value *args);			// write to stdout with newline

// Functions that can fail
ObjectResult* _printTo(VM *vm, int argCount, Value *args);			// write to specified channel without newline

ObjectResult* _scan(VM *vm, int argCount, Value *args);				// read single char from stdin
ObjectResult* _scanln(VM *vm, int argCount, Value *args);			// read from stdin until [enter]/[return] key (newline) is reached
ObjectResult* _scanFrom(VM *vm, int argCount, Value *args);		// read single char from specified channel
ObjectResult* _scanlnFrom(VM *vm, int argCount, Value *args);	// read from specified channel until [enter]/[return] key (newline) is reached
ObjectResult* _nscan(VM *vm, int argCount, Value *args);				// read n characters from stdin
ObjectResult* _nscanFrom(VM *vm, int argCount, Value *args);		// read n characters from specified channel

ObjectResult* _openFile(VM *vm, int argCount, Value *args);
ObjectResult* readlnFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* readAllFileMethod(VM *vm, int argCount, Value *args);

#endif //STL_IO_H
