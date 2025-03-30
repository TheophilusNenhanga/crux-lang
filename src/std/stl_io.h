#ifndef STL_IO_H
#define STL_IO_H

#include "../value.h"
#include "std.h"

Value _print(VM *vm, int argCount, Value *args);				// write to stdout without newline
Value _println(VM *vm, int argCount, Value *args);			// write to stdout with newline

ObjectResult* _printTo(VM *vm, int argCount, Value *args);			// write to specified channel without newline

ObjectResult* _scan(VM *vm, int argCount, Value *args);				// read single char from stdin
ObjectResult* _scanln(VM *vm, int argCount, Value *args);			// read from stdin until [enter]/[return] key (newline) is reached
ObjectResult* _scanFrom(VM *vm, int argCount, Value *args);		// read single char from specified channel
ObjectResult* _scanlnFrom(VM *vm, int argCount, Value *args);	// read from specified channel until [enter]/[return] key (newline) is reached
ObjectResult* _nscan(VM *vm, int argCount, Value *args);				// read n characters from stdin
ObjectResult* _nscanFrom(VM *vm, int argCount, Value *args);		// read n characters from specified channel

ObjectResult* openFileFunction(VM *vm, int argCount, Value *args);
ObjectResult* readlnFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* readAllFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* writeFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* writelnFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* closeFileMethod(VM*vm, int argCount, Value* args);

#endif //STL_IO_H
