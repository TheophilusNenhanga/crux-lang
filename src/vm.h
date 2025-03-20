#ifndef VM_H
#define VM_H

#include "table.h"
#include "value.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModule ObjectModule;

#define MAX_VM_DEPTH 64
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

/**
 * An ongoing function call
 */
typedef struct {
	ObjectClosure *closure;
	uint8_t *ip;
	Value *slots;
} CallFrame;

typedef struct {
	Table methods;
} NativeType;

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
}NativeModules;

struct VM {
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
	struct VM *enclosing;
	int grayCapacity;
	uint8_t previousInstruction; 
	ObjectModule *module;
	NativeModules nativeModules;
	MatchHandler matchHandler;
};

/**
 * Creates and initializes a new virtual machine.
 * @return A pointer to the newly allocated VM, or exits on allocation failure
 */
VM *newVM();

/**
 * Initializes a virtual machine.
 * @param vm The virtual machine to initialize
 */
void initVM(VM *vm);

/**
 * Frees all resources used by a virtual machine.
 * @param vm The virtual machine to free
 */
void freeVM(VM *vm);

/**
 * Resets the VM's stack to an empty state.
 * @param vm The virtual machine to reset
 */
void resetStack(VM *vm);

InterpretResult interpret(VM *vm, char *source);

/**
 * Pushes a value onto the VM's stack.
 * @param vm The virtual machine
 * @param value The value to push onto the stack
 */
void push(VM *vm, Value value);

/**
 * Removes and returns the top value from the VM's stack.
 * @param vm The virtual machine
 * @return The value that was at the top of the stack
 */
Value pop(VM *vm);

#endif // VM_H
