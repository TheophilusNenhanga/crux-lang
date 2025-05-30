#ifndef FS_H
#define FS_H

#include "../value.h"
#include "std.h"

ObjectResult* listDirFunction(VM *vm, int argCount, const Value *args);
ObjectResult* isFileFunction(VM *vm, int argCount, const Value *args);
ObjectResult* isDirFunction(VM *vm, int argCount, const Value *args);
ObjectResult* makeDirFunction(VM *vm, int argCount, const Value *args);
ObjectResult* deleteDirFunction(VM *vm, int argCount, const Value *args);
ObjectResult* pathExistsFunction(VM *vm, int argCount, const Value *args);
ObjectResult* renameFunction(VM *vm, int argCount, const Value *args);
ObjectResult* copyFileFunction(VM *vm, int argCount, const Value *args);
ObjectResult* isFileInFunction(VM* vm, int argCount, const Value *args);

#endif //FS_H
