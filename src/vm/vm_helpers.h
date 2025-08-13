#ifndef VM_HELPERS_H
#define VM_HELPERS_H

#include "vm.h"
#include "../chunk.h"


void resetStack(ObjectModuleRecord* moduleRecord);

void closeUpvalues(ObjectModuleRecord *moduleRecord, const Value *last);

void initImportStack(VM *vm);

void freeImportStack(VM *vm);

// Returns false on allocation error
bool pushImportStack(VM *vm, ObjectString *path);

void popImportStack(VM *vm);

bool isInImportStack(const VM *vm, const ObjectString *path);

ObjectResult* executeUserFunction(VM *vm, ObjectClosure *closure, int argCount, InterpretResult* result);

bool isFalsy(Value value);

void popPush(ObjectModuleRecord *moduleRecord, Value value);

void popTwo(ObjectModuleRecord *moduleRecord);

bool binaryOperation(VM *vm, OpCode operation);

bool concatenate(VM *vm);

/**
 * Checks if a previous instruction matches the expected opcode.
 * @param frame The current call frame
 * @param instructionsAgo How many instructions to look back
 * @param instruction The opcode to check for
 * @return true if the previous instruction matches, false otherwise
 */
bool checkPreviousInstruction(const CallFrame *frame,
int instructionsAgo,OpCode instruction);

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool callValue(VM *vm, Value callee, int argCount);

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
ObjectUpvalue *captureUpvalue(VM *vm, Value *local);

bool handleInvoke(VM *vm, int argCount, Value receiver,
Value original, Value value);

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invoke(VM *vm, const ObjectString *name, int argCount);

/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
void defineMethod(VM *vm, ObjectString *name);

InterpretResult globalCompoundOperation(VM *vm, ObjectString *name,
OpCode opcode, char *operation);

/**
 * Calls a function closure with the given arguments.
 * @param moduleRecord The currently executing module
 * @param closure The function closure to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call(ObjectModuleRecord* moduleRecord, ObjectClosure *closure, int argCount);


Value typeofValue(VM* vm, Value value);

ObjectStructInstance* popStructStack(VM *vm);
bool pushStructStack(VM *vm, ObjectStructInstance *structInstance);
ObjectStructInstance* peekStructStack(const VM *vm);

#endif //VM_HELPERS_H
