#ifndef STL_IO_H
#define STL_IO_H

#include "../value.h"
#include "std.h"

NativeReturn _openFile(VM*vm, int argCount, Value* args);			// opens a file and makes a new File Object
NativeReturn _closeFile(VM*vm, int argCount, Value* args);

// File Reading functions
NativeReturn _readOne(VM *vm, int argCount, Value *args);    // Read single char from file
NativeReturn _readln(VM *vm, int argCount, Value *args);     // Read line from file

// File Writing functions
NativeReturn _writeOne(VM *vm, int argCount, Value *args);    // Write single char to file
NativeReturn _writeln(VM *vm, int argCount, Value *args);			// Write line to file

NativeReturn _print(VM *vm, int argCount, Value *args);				// write to stdout without newline
NativeReturn _println(VM *vm, int argCount, Value *args);			// write to stdout with newline
NativeReturn _printTo(VM *vm, int argCount, Value *args);			// write to specified channel without newline

NativeReturn _scan(VM *vm, int argCount, Value *args);				// read single char from stdin
NativeReturn _scanln(VM *vm, int argCount, Value *args);			// read from stdin until [enter]/[return] key (newline) is reached
NativeReturn _scanFrom(VM *vm, int argCount, Value *args);		// read single char from specified channel
NativeReturn _scanlnFrom(VM *vm, int argCount, Value *args);	// read from specified channel until [enter]/[return] key (newline) is reached
NativeReturn _nscan(VM *vm, int argCount, Value *args);				// read n characters from stdin
NativeReturn _nscanFrom(VM *vm, int argCount, Value *args);		// read n characters from specified channel

#endif //STL_IO_H
