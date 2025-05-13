#ifndef VM_H
#define VM_H

#include "table.h"
#include "value.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModuleRecord ObjectModuleRecord;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
#define IMPORT_MAX FRAMES_MAX / 2

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

/**
 * An ongoing function call
 */
typedef struct {
  ObjectClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  char *name;
  Table *names;
} NativeModule;

typedef struct {
  Value matchTarget;
  Value matchBind;
  bool isMatchTarget;
  bool isMatchBind;
} MatchHandler;

typedef struct {
  NativeModule *modules;
  int count;
  int capacity;
} NativeModules;

typedef struct {
  const char **argv;
  int argc;
} Args;

typedef struct {
  ObjectString **paths; // resolved module path strings
  uint32_t count;
  uint32_t capacity;
} ImportStack;

struct VM {
  Value *stack;

  Object *objects;
  Table strings;

  Table moduleCache;
  ObjectModuleRecord *currentModuleRecord;
  ImportStack importStack;

  size_t bytesAllocated;
  size_t nextGC;
  Object **grayStack;
  int grayCapacity;
  int grayCount;

  ObjectString *initString;
  Table randomType;
  Table stringType;
  Table arrayType;
  Table tableType;
  Table errorType;
  Table fileType;

  NativeModules nativeModules;
  MatchHandler matchHandler;
  Args args;
  int importCount;
};

VM *newVM(int argc, const char **argv);

void initVM(VM *vm, int argc, const char **argv);

void freeVM(VM *vm);

void resetStack(VM *vm);

InterpretResult interpret(VM *vm, char *source);

void push(VM *vm, Value value);

Value pop(VM *vm);

void initImportStack(VM *vm);

void freeImportStack(VM *vm);

// Returns false on allocation error
bool pushImportStack(VM *vm, ObjectString *path);

void popImportStack(VM *vm);

bool isInImportStack(VM *vm, ObjectString *path);

#endif // VM_H
