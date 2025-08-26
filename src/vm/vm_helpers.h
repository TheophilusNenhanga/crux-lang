#ifndef VM_HELPERS_H
#define VM_HELPERS_H

#include "../chunk.h"
#include "vm.h"

void reset_stack(ObjectModuleRecord *moduleRecord);

void close_upvalues(ObjectModuleRecord *moduleRecord, const Value *last);

void init_import_stack(VM *vm);

void free_import_stack(VM *vm);

// Returns false on allocation error
bool push_import_stack(VM *vm, ObjectString *path);

void pop_import_stack(VM *vm);

bool is_in_import_stack(const VM *vm, const ObjectString *path);

ObjectResult *execute_user_function(VM *vm, ObjectClosure *closure,
				    int arg_count, InterpretResult *result);

bool is_falsy(Value value);

void pop_push(ObjectModuleRecord *moduleRecord, Value value);

#define pop_two(module_record)                                                 \
	pop((module_record));                                                  \
	pop((module_record))

#define pop_push(module_record, value)                                         \
	pop((module_record));                                                  \
	push((module_record), (value))

bool binary_operation(VM *vm, OpCode operation);

bool concatenate(VM *vm);

/**
 * Checks if a previous instruction matches the expected opcode.
 * @param frame The current call frame
 * @param instructions_ago How many instructions to look back
 * @param instruction The opcode to check for
 * @return true if the previous instruction matches, false otherwise
 */
bool check_previous_instruction(const CallFrame *frame, int instructions_ago,
				OpCode instruction);

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param arg_count Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call_value(VM *vm, Value callee, int arg_count);

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
ObjectUpvalue *capture_upvalue(VM *vm, Value *local);

bool handle_invoke(VM *vm, int arg_count, Value receiver, Value original,
		   Value value);

/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param arg_count Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
bool invoke(VM *vm, const ObjectString *name, int arg_count);

/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
void define_method(VM *vm, ObjectString *name);

InterpretResult global_compound_operation(VM *vm, ObjectString *name,
					  OpCode opcode, char *operation);

/**
 * Calls a function closure with the given arguments.
 * @param module_record The currently executing module
 * @param closure The function closure to call
 * @param arg_count Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
bool call(ObjectModuleRecord *module_record, ObjectClosure *closure,
	  int arg_count);

Value typeof_value(VM *vm, Value value);

ObjectStructInstance *pop_struct_stack(VM *vm);
bool pushStructStack(VM *vm, ObjectStructInstance *struct_instance);
ObjectStructInstance *peek_struct_stack(const VM *vm);

#endif // VM_HELPERS_H
