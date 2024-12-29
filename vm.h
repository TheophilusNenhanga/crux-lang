#ifndef VM_H
#define VM_H

#include "value.h"
#include "table.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

// A single ongoing function call
typedef struct {
	ObjectClosure *closure;
	uint8_t *ip;
	Value *slots;
} CallFrame;

typedef struct {
	Table methods;
}NativeType;

struct VM{
	Value stack[STACK_MAX]; // always points just past the last item
	Value *stackTop;
	Object *objects;
	Table strings;
	Table globals;
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	int grayCount;
	ObjectUpvalue *openUpvalues;
	size_t bytesAllocated;
	size_t nextGC;
	Object **grayStack;
	ObjectString *initString;
	NativeType stringType;
	NativeType arrayType;
	NativeType tableType;
	NativeType errorType;
	ObjectString *currentScriptName;
	struct VM* enclosing;
	int grayCapacity;
	uint8_t previousInstruction;
};

VM* newVM();

void initVM(VM* vm);

void freeVM(VM* vm);

void resetStack(VM* vm);

InterpretResult interpret(VM* vm, char *source, char* path);

void push(VM* vm, Value value);

Value pop(VM* vm);

#endif // VM_H
