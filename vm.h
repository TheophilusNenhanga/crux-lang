#ifndef VM_H
#define VM_H

#include "object.h"
#include "table.h"
#include "value.h"

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
	int grayCapacity;
	uint8_t previousInstruction;
} VM;

extern VM vm;

void initVM();

void freeVM();

void resetStack();

InterpretResult interpret(char *source);

void push(Value value);

Value pop();

#endif // VM_H
