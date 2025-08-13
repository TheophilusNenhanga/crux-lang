#ifndef VM_H
#define VM_H

#include "../table.h"
#include "../value.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModuleRecord ObjectModuleRecord;
typedef struct ObjectResult ObjectResult;
typedef struct ObjectStructInstance ObjectStructInstance;

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
  ObjectString* name;
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
  uint8_t capacity;
  uint8_t count;
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

typedef struct  {
  ObjectStructInstance** structs;
  uint32_t count;
  uint32_t capacity;
}StructInstanceStack;

typedef enum {
  PAUSED,
  RUNNING,
}GC_STATUS;

struct VM {
  Object *objects;
  Table strings;

  Table moduleCache;
  ObjectModuleRecord *currentModuleRecord;
  ImportStack importStack;

  Table randomType;
  Table stringType;
  Table arrayType;
  Table tableType;
  Table errorType;
  Table fileType;
  Table resultType;
  Table vec2Type;
  Table vec3Type;

  StructInstanceStack structInstanceStack;

  NativeModules nativeModules;
  MatchHandler matchHandler;
  Args args;

  size_t bytesAllocated;
  size_t nextGC;
  Object **grayStack;
  int grayCapacity;
  int grayCount;
  GC_STATUS gcStatus;

  int importCount;
};

#define PUSH(moduleRecord, value) \
do { \
if (__builtin_expect((moduleRecord)->stackTop >= (moduleRecord)->stackLimit, 0)) { \
runtimePanic((moduleRecord), true, STACK_OVERFLOW, "Stack overflow error"); \
} \
*(moduleRecord)->stackTop++ = (value); \
} while (0)

#define POP(moduleRecord) \
({ \
if (__builtin_expect((moduleRecord)->stackTop <= (moduleRecord)->stack, 0)) { \
runtimePanic((moduleRecord), true, RUNTIME, "Stack underflow error"); \
} \
*--(moduleRecord)->stackTop; \
})

#define PEEK(moduleRecord, distance) \
( (moduleRecord)->stackTop[ -1 - (distance) ] )

VM *newVM(int argc, const char **argv);

void initVM(VM *vm, int argc, const char **argv);

void freeVM(VM *vm);

InterpretResult interpret(VM *vm, char *source);

#endif // VM_H
