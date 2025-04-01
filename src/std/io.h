#ifndef IO_H
#define IO_H

#include "../value.h"
#include "std.h"

Value printFunction(VM *vm, int argCount, Value *args);				// write to stdout without newline
Value printlnFunction(VM *vm, int argCount, Value *args);			// write to stdout with newline

ObjectResult* printToFunction(VM *vm, int argCount, Value *args);			// write to specified channel without newline

ObjectResult* scanFunction(VM *vm, int argCount, Value *args);				// read single char from stdin
ObjectResult* scanlnFunction(VM *vm, int argCount, Value *args);			// read from stdin until [enter]/[return] key (newline) is reached
ObjectResult* scanFromFunction(VM *vm, int argCount, Value *args);		// read single char from specified channel
ObjectResult* scanlnFromFunction(VM *vm, int argCount, Value *args);	// read from specified channel until [enter]/[return] key (newline) is reached
ObjectResult* nscanFunction(VM *vm, int argCount, Value *args);				// read n characters from stdin
ObjectResult* nscanFromFunction(VM *vm, int argCount, Value *args);		// read n characters from specified channel

ObjectResult* openFileFunction(VM *vm, int argCount, Value *args);
ObjectResult* readlnFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* readAllFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* writeFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* writelnFileMethod(VM *vm, int argCount, Value *args);
ObjectResult* closeFileMethod(VM*vm, int argCount, Value* args);

#endif //IO_H
