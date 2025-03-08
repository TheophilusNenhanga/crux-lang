#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "file_handler.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "std/std.h"
#include "value.h"


VM *newVM() {
	VM *vm = (VM *) malloc(sizeof(VM));
	if (vm == NULL) {
		fprintf(stderr, "Stella Fatal Error: Could not allocate memory for VM\n");
		exit(1);
	}
	initVM(vm);
	return vm;
}


void resetStack(VM *vm) {
	vm->stackTop = vm->stack;
	vm->frameCount = 0;
	vm->openUpvalues = NULL;
}


void push(VM *vm, Value value) {
	*vm->stackTop = value; // stores value in the array element at the top of the stack
	vm->stackTop++; // stack top points just past the last used element, at the next available one
}


Value pop(VM *vm) {
	vm->stackTop--; // Move the stack pointer back to get the most recent slot in the array
	return *vm->stackTop;
}

/**
 * Returns a value from the stack without removing it.
 * @param vm The virtual machine
 * @param distance How far from the top of the stack to look (0 is the top)
 * @return The value at the specified distance from the top
 */
static Value peek(VM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Calls a function closure with the given arguments.
 * @param vm The virtual machine
 * @param closure The function closure to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
static bool call(VM *vm, ObjectClosure *closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d arguments, got %d", closure->function->arity, argCount);
		return false;
	}

	if (vm->frameCount == FRAMES_MAX) {
		runtimePanic(vm, STACK_OVERFLOW, "Stack overflow");
		return false;
	}

	CallFrame *frame = &vm->frames[vm->frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm->stackTop - argCount - 1;
	return true;
}

/**
 * Calls a value as a function with the given arguments.
 * @param vm The virtual machine
 * @param callee The value to call
 * @param argCount Number of arguments on the stack
 * @return true if the call succeeds, false otherwise
 */
static bool callValue(VM *vm, Value callee, int argCount) {
	if (IS_STL_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
			case OBJECT_CLOSURE:
				return call(vm, AS_STL_CLOSURE(callee), argCount);
			case OBJECT_NATIVE_METHOD: {
				ObjectNativeMethod *native = AS_STL_NATIVE_METHOD(callee);
				if (argCount != native->arity) {
					runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d", native->arity, argCount);
					return false;
				}

				ObjectResult *result = native->function(vm, argCount, vm->stackTop - argCount);

				vm->stackTop -= argCount;

				if (!result->isOk) {
					if (result->as.error->isPanic) {
						runtimePanic(vm, result->as.error->type, result->as.error->message->chars);
						return false;
					}
				}

				push(vm, OBJECT_VAL(result));

				return true;
			}
			case OBJECT_NATIVE_FUNCTION: {
				ObjectNativeFunction *native = AS_STL_NATIVE_FUNCTION(callee);
				if (argCount != native->arity) {
					runtimePanic(vm, ARGUMENT_MISMATCH, "Expected %d argument(s), got %d", native->arity, argCount);
					return false;
				}

				ObjectResult *result = native->function(vm, argCount, vm->stackTop - argCount);
				vm->stackTop -= argCount + 1;

				if (!result->isOk) {
					if (result->as.error->isPanic) {
						runtimePanic(vm, result->as.error->type, result->as.error->message->chars);
						return false;
					}
				}

				push(vm, OBJECT_VAL(result));
				return true;
			}
			case OBJECT_CLASS: {
				ObjectClass *klass = AS_STL_CLASS(callee);
				vm->stackTop[-argCount - 1] = OBJECT_VAL(newInstance(vm, klass));
				Value initializer;

				if (tableGet(&klass->methods, vm->initString, &initializer)) {
					return call(vm, AS_STL_CLOSURE(initializer), argCount);
				}
				if (argCount != 0) {
					runtimePanic(vm, ARGUMENT_MISMATCH, "Expected 0 arguments but got %d arguments.", argCount);
					return false;
				}
				return true;
			}
			case OBJECT_BOUND_METHOD: {
				ObjectBoundMethod *bound = AS_STL_BOUND_METHOD(callee);
				vm->stackTop[-argCount - 1] = bound->receiver;
				return call(vm, bound->method, argCount);
			}
			default:
				break;
		}
	}
	runtimePanic(vm, TYPE, "Can only call functions and classes.");
	return false;
}

/**
 * Invokes a method from a class with the given arguments.
 * @param vm The virtual machine
 * @param klass The class containing the method
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
static bool invokeFromClass(VM *vm, ObjectClass *klass, ObjectString *name, int argCount) {
	Value method;
	if (tableGet(&klass->methods, name, &method)) {
		return call(vm, AS_STL_CLOSURE(method), argCount);
	}
	runtimePanic(vm, NAME, "Undefined property '%s'.", name->chars);
	return false;
}


/**
 * Invokes a method on an object with the given arguments.
 * @param vm The virtual machine
 * @param name The name of the method to invoke
 * @param argCount Number of arguments on the stack
 * @return true if the method invocation succeeds, false otherwise
 */
static bool invoke(VM *vm, ObjectString *name, int argCount) {
	Value receiver = peek(vm, argCount);

	if (!IS_STL_INSTANCE(receiver)) {
		argCount++; // for the value that the method will act upon
		if (IS_STL_STRING(receiver)) {
			Value value;
			if (tableGet(&vm->stringType.methods, name, &value)) {
				vm->stackTop[-argCount - 1] = value;
				vm->stackTop[-argCount] = receiver;
				return callValue(vm, value, argCount);
			}
			runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
			return false;
		}

		if (IS_STL_ARRAY(receiver)) {
			Value value;
			if (tableGet(&vm->arrayType.methods, name, &value)) {
				vm->stackTop[-argCount - 1] = value;
				vm->stackTop[-argCount] = receiver;
				return callValue(vm, value, argCount);
			}
			runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
			return false;
		}

		if (IS_STL_ERROR(receiver)) {
			Value value;
			if (tableGet(&vm->errorType.methods, name, &value)) {
				vm->stackTop[-argCount - 1] = value;
				vm->stackTop[-argCount] = receiver;
				return callValue(vm, value, argCount);
			}
			runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
			return false;
		}

		if (IS_STL_TABLE(receiver)) {
			Value value;
			if (tableGet(&vm->tableType.methods, name, &value)) {
				vm->stackTop[-argCount - 1] = value;
				vm->stackTop[-argCount] = receiver;
				return callValue(vm, value, argCount);
			}
			runtimePanic(vm, NAME, "Undefined method '%s'.", name->chars);
			return false;
		}

		runtimePanic(vm, TYPE, "Only instances have methods.");
		return false;
	}

	ObjectInstance *instance = AS_STL_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm->stackTop[-argCount - 1] = value;
		return callValue(vm, value, argCount);
	}

	return invokeFromClass(vm, instance->klass, name, argCount);
}


/**
 * Binds a method from a class to an instance.
 * @param vm The virtual machine
 * @param klass The class containing the method
 * @param name The name of the method to bind
 * @return true if the binding succeeds, false otherwise
 */
static bool bindMethod(VM *vm, ObjectClass *klass, ObjectString *name) {
	Value method;
	if (!tableGet(&klass->methods, name, &method)) {
		runtimePanic(vm, NAME, "Undefined property '%s'", name->chars);
		return false;
	}

	ObjectBoundMethod *bound = newBoundMethod(vm, peek(vm, 0), AS_STL_CLOSURE(method));
	pop(vm);
	push(vm, OBJECT_VAL(bound));
	return true;
}

/**
 * Captures a local variable in an upvalue for closures.
 * @param vm The virtual machine
 * @param local Pointer to the local variable to capture
 * @return The created or reused upvalue
 */
static ObjectUpvalue *captureUpvalue(VM *vm, Value *local) {
	ObjectUpvalue *prevUpvalue = NULL;
	ObjectUpvalue *upvalue = vm->openUpvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjectUpvalue *createdUpvalue = newUpvalue(vm, local);

	createdUpvalue->next = upvalue;
	if (prevUpvalue == NULL) {
		vm->openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

/**
 * Closes all upvalues up to a certain stack position.
 * @param vm The virtual machine
 * @param last Pointer to the last variable to close
 */
static void closeUpvalues(VM *vm, Value *last) {
	while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
		ObjectUpvalue *upvalue = vm->openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm->openUpvalues = upvalue->next;
	}
}


/**
 * Defines a method on a class.
 * @param vm The virtual machine
 * @param name The name of the method
 */
static void defineMethod(VM *vm, ObjectString *name) {
	Value method = peek(vm, 0);
	ObjectClass *klass = AS_STL_CLASS(peek(vm, 1));
	if (tableSet(vm, &klass->methods, name, method, false)) {
		pop(vm);
	}
}

/**
 * Determines if a value is falsy (nil or false).
 * @param value The value to check
 * @return true if the value is falsy, false otherwise
 */
static bool isFalsy(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

/**
 * Concatenates two values as strings.
 * @param vm The virtual machine
 * @return true if concatenation succeeds, false otherwise
 */
static bool concatenate(VM *vm) {
	Value b = peek(vm, 0);
	Value a = peek(vm, 1);

	ObjectString *stringB;
	ObjectString *stringA;

	if (IS_STL_STRING(b)) {
		stringB = AS_STL_STRING(b);
	} else {
		stringB = toString(vm, b);
		if (stringB == NULL) {
			runtimePanic(vm, TYPE, "Could not convert right operand to a string.");
			return false;
		}
	}

	if (IS_STL_STRING(a)) {
		stringA = AS_STL_STRING(a);
	} else {
		stringA = toString(vm, a);
		if (stringA == NULL) {
			runtimePanic(vm, TYPE, "Could not convert left operand to a string.");
			return false;
		}
	}

	uint64_t length = stringA->length + stringB->length;
	char *chars = ALLOCATE(vm, char, length + 1);

	if (chars == NULL) {
		runtimePanic(vm, MEMORY, "Could not allocate memory for concatenation.");
		return false;
	}

	memcpy(chars, stringA->chars, stringA->length);
	memcpy(chars + stringA->length, stringB->chars, stringB->length);
	chars[length] = '\0';

	ObjectString *result = takeString(vm, chars, length);

	pop(vm);
	pop(vm);
	push(vm, OBJECT_VAL(result));
	return true;
}


void initVM(VM *vm) {
	resetStack(vm);
	vm->objects = NULL;
	vm->bytesAllocated = 0;
	vm->nextGC = 1024 * 1024;
	vm->grayCount = 0;
	vm->grayCapacity = 0;
	vm->grayStack = NULL;
	vm->previousInstruction = 0;
	vm->enclosing = NULL;
	vm->module = NULL;
	vm->nativeModules = (NativeModules){.modules = ALLOCATE(vm, NativeModule, 8), .count = 0, .capacity = 8};

	initTable(&vm->stringType.methods);
	initTable(&vm->arrayType.methods);
	initTable(&vm->tableType.methods);
	initTable(&vm->errorType.methods);
	initTable(&vm->strings);
	initTable(&vm->globals);

	vm->initString = NULL;
	vm->initString = copyString(vm, "init", 4);

	defineMethods(vm, &vm->stringType.methods, stringMethods);
	defineMethods(vm, &vm->arrayType.methods, arrayMethods);
	defineMethods(vm, &vm->tableType.methods, tableMethods);
	defineMethods(vm, &vm->errorType.methods, errorMethods);

	defineNativeFunctions(vm, &vm->globals);
	defineStandardLibrary(vm);
}


void freeVM(VM *vm) {
	freeTable(vm, &vm->strings);
	freeTable(vm, &vm->globals);
	vm->initString = NULL;
	freeObjects(vm);
	if (vm->enclosing != NULL) {
		free(vm);
	}
}

/**
 * Performs a binary operation on the top two values of the stack.
 * @param vm The virtual machine
 * @param operation The operation code to perform
 * @return true if the operation succeeds, false otherwise
 */
static bool binaryOperation(VM *vm, OpCode operation) {
	Value b = peek(vm, 0);
	Value a = peek(vm, 1);


	if (!(IS_NUMBER(a) || IS_NUMBER(b))) {
		runtimePanic(vm, TYPE, "Operands must be of type 'number'.");
		return false;
	}

	pop(vm);
	pop(vm);

	double aNum = AS_NUMBER(a);
	double bNum = AS_NUMBER(b);

	switch (operation) {
		case OP_ADD: {
			push(vm, NUMBER_VAL(aNum + bNum));
			break;
		}

		case OP_SUBTRACT: {
			push(vm, NUMBER_VAL(aNum - bNum));
			break;
		}

		case OP_MULTIPLY: {
			push(vm, NUMBER_VAL(aNum * bNum));
			break;
		}

		case OP_DIVIDE: {
			if (bNum == 0) {
				runtimePanic(vm, DIVISION_BY_ZERO, "Tried to divide by zero.");
				return false;
			}
			push(vm, NUMBER_VAL(aNum / bNum));
			break;
		}

		case OP_LESS_EQUAL: {
			push(vm, BOOL_VAL(aNum <= bNum));
			break;
		}

		case OP_GREATER_EQUAL: {
			push(vm, BOOL_VAL(aNum >= bNum));
			break;
		}

		case OP_LESS: {
			push(vm, BOOL_VAL(aNum < bNum));
			break;
		}

		case OP_GREATER: {
			push(vm, BOOL_VAL(aNum > bNum));
			break;
		}
		case OP_MODULUS: {
			push(vm, NUMBER_VAL((int64_t) aNum % (int64_t) bNum));
			break;
		}
		case OP_LEFT_SHIFT: {
			push(vm, NUMBER_VAL((int64_t) aNum << (int64_t) bNum));
			break;
		}
		case OP_RIGHT_SHIFT: {
			push(vm, NUMBER_VAL((int64_t) aNum >> (int64_t) bNum));
			break;
		}
		default: {
		}
	}
	return true;
}

/**
 * Performs a compound assignment operation on a global variable.
 * @param vm The virtual machine
 * @param name The name of the global variable
 * @param opcode The operation code to perform
 * @param operation String representation of the operation for error messages
 * @return The interpretation result
 */
InterpretResult globalCompoundOperation(VM *vm, ObjectString *name, OpCode opcode, char *operation) {
	Value currentValue;
	if (!tableGet(&vm->globals, name, &currentValue)) {
		runtimePanic(vm, NAME, "Undefined variable '%s'.", name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
		runtimePanic(vm, TYPE, "Operands must be ot type 'number' for '%s' operator.", operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (opcode) {
		case OP_SET_GLOBAL_SLASH: {
			if (AS_NUMBER(peek(vm, 0)) != 0.0) {
				double result = AS_NUMBER(currentValue) / AS_NUMBER(peek(vm, 0));
				tableSet(vm, &vm->globals, name, NUMBER_VAL(result), false);
				break;
			}
			runtimePanic(vm, DIVISION_BY_ZERO, "Division by zero error: '%s'.", name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		case OP_SET_GLOBAL_STAR: {
			double result = AS_NUMBER(currentValue) * AS_NUMBER(peek(vm, 0));
			tableSet(vm, &vm->globals, name, NUMBER_VAL(result), false);
			break;
		}
		case OP_SET_GLOBAL_PLUS: {
			double result = AS_NUMBER(currentValue) + AS_NUMBER(peek(vm, 0));
			tableSet(vm, &vm->globals, name, NUMBER_VAL(result), false);
			break;
		}
		case OP_SET_GLOBAL_MINUS: {
			double result = AS_NUMBER(currentValue) - AS_NUMBER(peek(vm, 0));
			tableSet(vm, &vm->globals, name, NUMBER_VAL(result), false);
			break;
		}
		default: ;
	}
	return INTERPRET_OK;
}

/**
 * Checks if a previous instruction matches the expected opcode.
 * @param frame The current call frame
 * @param instructionsAgo How many instructions to look back
 * @param instruction The opcode to check for
 * @return true if the previous instruction matches, false otherwise
 */
static bool checkPreviousInstruction(CallFrame *frame, int instructionsAgo, OpCode instruction) {
	uint8_t *current = frame->ip;
	if (current - instructionsAgo < frame->closure->function->chunk.code) {
		return false;
	}
	return *(current - (instructionsAgo + 2)) == instruction; // +2 to account for offset
}


/**
 * Executes bytecode in the virtual machine.
 * @param vm The virtual machine
 * @return The interpretation result
 */
static InterpretResult run(VM *vm) {
	CallFrame *frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STL_STRING(READ_CONSTANT())
#define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))
	uint8_t instruction;
	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("        ");
		for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
			printf("[");
			printValue(*slot);
			printf("]");
		}
		printf("\n");

		disassembleInstruction(&frame->closure->function->chunk,
		                       (int) (frame->ip - frame->closure->function->chunk.code));
#endif

		instruction = READ_BYTE();
		switch (instruction) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(vm, constant);
				break;
			}

			case OP_RETURN: {
				Value result = pop(vm);
				closeUpvalues(vm, frame->slots);
				vm->frameCount--;
				if (vm->frameCount == 0) {
					pop(vm);
					return INTERPRET_OK;
				}
				vm->stackTop = frame->slots;
				push(vm, result);
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}

			case OP_CLOSURE: {
				ObjectFunction *function = AS_STL_FUNCTION(READ_CONSTANT());
				ObjectClosure *closure = newClosure(vm, function);
				push(vm, OBJECT_VAL(closure));

				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();

					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}
				break;
			}

			case OP_NIL: {
				push(vm, NIL_VAL);
				break;
			}

			case OP_TRUE: {
				push(vm, BOOL_VAL(true));
				break;
			}

			case OP_FALSE: {
				push(vm, BOOL_VAL(false));
				break;
			}

			case OP_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(valuesEqual(a, b)));
				break;
			}

			case OP_NOT_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(!valuesEqual(a, b)));
				break;
			}

			case OP_GREATER: {
				if (!binaryOperation(vm, OP_GREATER)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS: {
				if (!binaryOperation(vm, OP_LESS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS_EQUAL: {
				if (!binaryOperation(vm, OP_LESS_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GREATER_EQUAL: {
				if (!binaryOperation(vm, OP_GREATER_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NEGATE: {
				if (IS_NUMBER(peek(vm, 0))) {
					push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
					break;
				}
				runtimePanic(vm, TYPE, "Operand must be of type 'number'.");
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_ADD: {
				if (IS_STL_STRING(peek(vm, 0)) || IS_STL_STRING(peek(vm, 1))) {
					if (!concatenate(vm)) {
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				if (!binaryOperation(vm, OP_ADD)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUBTRACT: {
				if (!binaryOperation(vm, OP_SUBTRACT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MULTIPLY: {
				if (!binaryOperation(vm, OP_MULTIPLY)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_DIVIDE: {
				if (!binaryOperation(vm, OP_DIVIDE)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MODULUS: {
				if (!binaryOperation(vm, OP_MODULUS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LEFT_SHIFT: {
				if (!binaryOperation(vm, OP_LEFT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_RIGHT_SHIFT: {
				if (!binaryOperation(vm, OP_RIGHT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NOT: {
				push(vm, BOOL_VAL(isFalsy(pop(vm))));
				break;
			}

			case OP_POP: {
				pop(vm);
				break;
			}

			case OP_DEFINE_GLOBAL: {
				ObjectString *name = READ_STRING();
				bool isPublic = false;
				if (checkPreviousInstruction(frame, 3, OP_PUB)) {
					isPublic = true;
				}
				if (tableSet(vm, &vm->globals, name, peek(vm, 0), isPublic)) {
					pop(vm);
					break;
				}
				runtimePanic(vm, NAME, "Cannot define '%s' because it is already defined.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_GET_GLOBAL: {
				ObjectString *name = READ_STRING();
				Value value;
				if (tableGet(&vm->globals, name, &value)) {
					push(vm, value);
					break;
				}
				runtimePanic(vm, NAME, "Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_SET_GLOBAL: {
				ObjectString *name = READ_STRING();
				if (!tableSet(vm, &vm->globals, name, peek(vm, 0), false)) {
					runtimePanic(vm, NAME, "Cannot give variable '%s' a value because it has not been defined",
					             name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(vm, frame->slots[slot]);
				break;
			}

			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(vm, 0);
				break;
			}

			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsy(peek(vm, 0))) {
					frame->ip += offset;
					break;
				}
				break;
			}

			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}

			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}

			case OP_CALL: {
				int argCount = READ_BYTE();
				if (!callValue(vm, peek(vm, argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}

			case OP_GET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				push(vm, *frame->closure->upvalues[slot]->location);
				break;
			}

			case OP_SET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				*frame->closure->upvalues[slot]->location = peek(vm, 0);
				break;
			}

			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm, vm->stackTop - 1);
				pop(vm);
				break;
			}

			case OP_CLASS: {
				push(vm, OBJECT_VAL(newClass(vm, READ_STRING())));
				break;
			}

			case OP_GET_PROPERTY: {
				if (!IS_STL_INSTANCE(peek(vm, 0))) {
					runtimePanic(vm, TYPE, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjectInstance *instance = AS_STL_INSTANCE(peek(vm, 0));
				ObjectString *name = READ_STRING();

				Value value;
				bool fieldFound = false;
				if (tableGet(&instance->fields, name, &value)) {
					pop(vm);
					push(vm, value);
					fieldFound = true;
					break;
				}

				if (!fieldFound) {
					if (!bindMethod(vm, instance->klass, name)) {
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				break;
			}

			case OP_SET_PROPERTY: {
				if (!IS_STL_INSTANCE(peek(vm, 1))) {
					runtimePanic(vm, TYPE, "Only instances have fields.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectInstance *instance = AS_STL_INSTANCE(peek(vm, 1));
				if (tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0), false)) {
					Value value = pop(vm);
					pop(vm);
					push(vm, value);
					break;
				}
				runtimePanic(vm, NAME, "Undefined property '%s'.", READ_STRING());
				return INTERPRET_RUNTIME_ERROR;
			}
			case OP_METHOD: {
				defineMethod(vm, READ_STRING());
				break;
			}

			case OP_INVOKE: {
				ObjectString *methodName = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(vm, methodName, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}

			case OP_INHERIT: {
				Value superClass = peek(vm, 1);

				if (!IS_STL_CLASS(superClass)) {
					runtimePanic(vm, TYPE, "Cannot inherit from non class object.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectClass *subClass = AS_STL_CLASS(peek(vm, 0));
				tableAddAll(vm, &AS_STL_CLASS(superClass)->methods, &subClass->methods);
				pop(vm);
				break;
			}

			case OP_GET_SUPER: {
				ObjectString *name = READ_STRING();
				ObjectClass *superClass = AS_STL_CLASS(pop(vm));

				if (!bindMethod(vm, superClass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUPER_INVOKE: {
				ObjectString *method = READ_STRING();
				int argCount = READ_BYTE();
				ObjectClass *superClass = AS_STL_CLASS(pop(vm));
				if (!invokeFromClass(vm, superClass, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}

			case OP_ARRAY: {
				uint16_t elementCount = READ_SHORT();
				ObjectArray *array = newArray(vm, elementCount);
				for (int i = elementCount - 1; i >= 0; i--) {
					arrayAdd(vm, array, pop(vm), i);
				}
				push(vm, OBJECT_VAL(array));
				break;
			}

			case OP_TABLE: {
				uint16_t elementCount = READ_SHORT();
				ObjectTable *table = newTable(vm, elementCount);
				for (int i = elementCount - 1; i >= 0; i--) {
					Value value = pop(vm);
					Value key = pop(vm);
					if (IS_NUMBER(key) || IS_STL_STRING(key)) {
						if (!objectTableSet(vm, table, key, value)) {
							runtimePanic(vm, COLLECTION_SET, "Failed to set value in table");
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimePanic(vm, TYPE, "Key cannot be hashed.", READ_STRING());
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				push(vm, OBJECT_VAL(table));
				break;
			}

			case OP_GET_COLLECTION: {
				Value indexValue = pop(vm);
				if (!IS_STL_OBJECT(peek(vm, 0))) {
					runtimePanic(vm, TYPE, "Cannot get from a non-collection type.");
					return INTERPRET_RUNTIME_ERROR;
				}
				switch (AS_STL_OBJECT(peek(vm, 0))->type) {
					case OBJECT_TABLE: {
						if (IS_STL_STRING(indexValue) || IS_NUMBER(indexValue)) {
							ObjectTable *table = AS_STL_TABLE(peek(vm, 0));
							Value value;
							if (!objectTableGet(table, indexValue, &value)) {
								runtimePanic(vm, COLLECTION_GET, "Failed to get value from table");
								return INTERPRET_RUNTIME_ERROR;
							}
							pop(vm);
							push(vm, value);
						} else {
							runtimePanic(vm, TYPE, "Key cannot be hashed.", READ_STRING());
							return INTERPRET_RUNTIME_ERROR;
						}
						break;
					}
					case OBJECT_ARRAY: {
						if (!IS_NUMBER(indexValue)) {
							runtimePanic(vm, TYPE, "Index must be of type 'number'.");
							return INTERPRET_RUNTIME_ERROR;
						}
						int index = AS_NUMBER(indexValue);
						ObjectArray *array = AS_STL_ARRAY(peek(vm, 0));
						Value value;
						if (index < 0 || index >= array->size) {
							runtimePanic(vm, INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
							return INTERPRET_RUNTIME_ERROR;
						}
						if (!arrayGet(array, index, &value)) {
							runtimePanic(vm, COLLECTION_GET, "Failed to get value from array");
							return INTERPRET_RUNTIME_ERROR;
						}
						pop(vm); // pop the array off the stack
						push(vm, value); // push the value onto the stack
						break;
					}
					case OBJECT_STRING: {
						if (!IS_NUMBER(indexValue)) {
							runtimePanic(vm, TYPE, "Index must be of type 'number'.");
							return INTERPRET_RUNTIME_ERROR;
						}
						int index = AS_NUMBER(indexValue);
						ObjectString *string = AS_STL_STRING(peek(vm, 0));
						ObjectString *ch;
						if (index < 0 || index >= string->length) {
							runtimePanic(vm, INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
							return INTERPRET_RUNTIME_ERROR;
						}
						// Only single character indexing
						ch = copyString(vm, string->chars+index, 1);
						pop(vm);
						push(vm, OBJECT_VAL(ch));
						break;
					}
					default: {
						runtimePanic(vm, TYPE, "Cannot get from a non-collection type.");
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				break;
			}

			case OP_SET_COLLECTION: {
				Value value = pop(vm);
				Value indexValue = peek(vm, 0);

				if (IS_STL_TABLE(peek(vm, 1))) {
					ObjectTable *table = AS_STL_TABLE(peek(vm, 1));
					if (IS_NUMBER(indexValue) || IS_STL_STRING(indexValue)) {
						if (!objectTableSet(vm, table, indexValue, value)) {
							runtimePanic(vm, COLLECTION_GET, "Failed to set value in table");
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimePanic(vm, TYPE, "Key cannot be hashed.");
						return INTERPRET_RUNTIME_ERROR;
					}
				} else if (IS_STL_ARRAY(peek(vm, 1))) {
					ObjectArray *array = AS_STL_ARRAY(peek(vm, 1));
					int index = AS_NUMBER(indexValue);
					if (!arraySet(vm, array, index, value)) {
						runtimePanic(vm, COLLECTION_SET, "Failed to set value in array");
						return INTERPRET_RUNTIME_ERROR;
					}
				} else {
					runtimePanic(vm, TYPE, "Value is not a mutable collection type.");
					return INTERPRET_RUNTIME_ERROR;
				}
				pop(vm); // collection
				pop(vm); // indexValue
				push(vm, indexValue);
				break;
			}

			case OP_SET_LOCAL_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(vm, 0));

				if (divisor == 0.0) {
					runtimePanic(vm, DIVISION_BY_ZERO, "Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);
				break;
			}
			case OP_SET_LOCAL_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) * AS_NUMBER(peek(vm, 0)));
				break;
			}
			case OP_SET_LOCAL_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) + AS_NUMBER(peek(vm, 0)));
				break;
			}
			case OP_SET_LOCAL_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) - AS_NUMBER(peek(vm, 0)));
				break;
			}
			case OP_SET_UPVALUE_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				double divisor = AS_NUMBER(peek(vm, 0));
				if (divisor == 0.0) {
					runtimePanic(vm, DIVISION_BY_ZERO, "Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);

				break;
			}
			case OP_SET_UPVALUE_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location =
						NUMBER_VAL(AS_NUMBER(currentValue) * AS_NUMBER(peek(vm, 0)));
				break;
			}
			case OP_SET_UPVALUE_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location =
						NUMBER_VAL(AS_NUMBER(currentValue) + AS_NUMBER(peek(vm, 0)));

				break;
			}
			case OP_SET_UPVALUE_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(vm, 0)))) {
					runtimePanic(vm, TYPE, "Both operands must be of type 'number' for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				*frame->closure->upvalues[slot]->location =
						NUMBER_VAL(AS_NUMBER(currentValue) / AS_NUMBER(peek(vm, 0)));
				break;
			}
			case OP_SET_GLOBAL_SLASH: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_SLASH, "/=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_STAR: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_STAR, "*=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_PLUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_PLUS, "+=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_MINUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(vm, name, OP_SET_GLOBAL_MINUS, "-=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_ANON_FUNCTION: {
				ObjectFunction *function = AS_STL_FUNCTION(READ_CONSTANT());
				ObjectClosure *closure = newClosure(vm, function);
				push(vm, OBJECT_VAL(closure));
				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();

					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}
				break;
			}

			case OP_PUB: {
				break;
			}

			case OP_USE: {
				uint8_t nameCount = READ_BYTE();
				ObjectString *names[UINT8_MAX];
				ObjectString *aliases[UINT8_MAX];
				for (int i = 0; i < nameCount; i++) {
					names[i] = READ_STRING();
				}
				for (int i = 0; i < nameCount; i++) {
					aliases[i] = READ_STRING();
				}
				ObjectString *modulePath = READ_STRING();

				if (importSetContains(&vm->module->importedModules, modulePath)) {
					runtimePanic(vm, IMPORT,
					             "Module '%s' has already been imported. All imports must be done in a single 'use' statement.",
					             modulePath->chars);
					return INTERPRET_RUNTIME_ERROR;
				}

				if (modulePath->length > 4 && memcmp(modulePath->chars, "stl:", 4) == 0) {
					char *moduleName = modulePath->chars + 4;
					int moduleIndex = -1;
					for (int i = 0; i < vm->nativeModules.count; i++) {
						if (memcmp(moduleName, vm->nativeModules.modules[i].name, strlen(moduleName)) == 0) {
							moduleIndex = i;
							break;
						}
					}
					if (moduleIndex == -1) {
						runtimePanic(vm, IMPORT, "Module '%s' does not exist.", modulePath->chars);
						return INTERPRET_RUNTIME_ERROR;
					}

					Table *moduleTable = vm->nativeModules.modules[moduleIndex].names;
					for (int i = 0; i < nameCount; i++) {
						Value value;
						bool getSuccess = tableGet(moduleTable, names[i], &value);
						if (!getSuccess) {
							runtimePanic(vm, IMPORT, "Failed to import '%s' from '%s'.", names[i]->chars,
							             modulePath->chars);
							return INTERPRET_RUNTIME_ERROR;
						}
						push(vm, OBJECT_VAL(value));
						bool setSuccess = false;
						if (aliases[i] != NULL) {
							setSuccess = tableSet(vm, &vm->globals, aliases[i], value, false);
						} else {
							setSuccess = tableSet(vm, &vm->globals, names[i], value, false);
						}

						if (!setSuccess) {
							runtimePanic(vm, IMPORT, "Failed to import '%s' from '%s'.", names[i]->chars,
							             modulePath->chars);
							return INTERPRET_RUNTIME_ERROR;
						}
						pop(vm);
					}
					importSetAdd(vm, &vm->module->importedModules, modulePath);
					break;
				}

				char *resolvedPath = resolvePath(vm->module->path->chars, modulePath->chars);
				if (resolvedPath == NULL) {
					runtimePanic(vm, IO, "Could not resolve path to module.");
					return INTERPRET_RUNTIME_ERROR;
				}

				FileResult source = readFile(resolvedPath);
				if (source.error != NULL) {
					runtimePanic(vm, IO, source.error);
					return INTERPRET_RUNTIME_ERROR;
				}

				if (vm->module->state == IN_PROGRESS) {
					freeFileResult(source);
					free(resolvedPath);
					runtimePanic(vm, IMPORT, "Circular import detected in \"%s\".", modulePath->chars);
					return INTERPRET_RUNTIME_ERROR;
				}

				if (vm->module->vmDepth > MAX_VM_DEPTH) {
					freeFileResult(source);
					free(resolvedPath);
					runtimePanic(vm, IMPORT, "Maximum import depth exceeded.");
					return INTERPRET_RUNTIME_ERROR;
				}

				VM *newModuleVM = newVM();
				newModuleVM->enclosing = vm;

				newModuleVM->module = newModule(newModuleVM, modulePath->chars);
				newModuleVM->module->state = IN_PROGRESS;
				newModuleVM->module->vmDepth = vm->module->vmDepth + 1;

				InterpretResult result = interpret(newModuleVM, source.content);
				freeFileResult(source);
				free(resolvedPath);
				if (result != INTERPRET_OK) {
					freeVM(newModuleVM);
					return result;
				}

				newModuleVM->module->state = IMPORTED;

				// add the imported names to the current module (deep copy)
				bool success = true;
				for (int i = 0; i < nameCount; i++) {
					if (aliases[i] != NULL && IS_STL_STRING(OBJECT_VAL(aliases[i]))) {
						success = tableDeepCopy(newModuleVM, vm, &newModuleVM->globals, &vm->globals, names[i],
						                        aliases[i]);
					} else {
						success = tableDeepCopy(newModuleVM, vm, &newModuleVM->globals, &vm->globals, names[i],
						                        names[i]);
					}
					if (!success) {
						for (int j = 0; j < i; j++) {
							tableDelete(&vm->globals, names[j]);
						}
						runtimePanic(vm, NAME, "Failed to import '%s' from module '%s'.", names[i]->chars,
						             modulePath->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
				}

				importSetAdd(vm, &vm->module->importedModules, modulePath);
				freeVM(newModuleVM);
				break;
			}

			case OP_MATCH: {
				break;
			}

			case OP_MATCH_JUMP: {
				uint16_t offset = READ_SHORT();
				Value pattern = pop(vm);
				Value target = peek(vm, 0);
				if (!valuesEqual(pattern, target)) {
					frame->ip += offset;
				}

				break;
			}

			case OP_RESULT_MATCH_ERR: {
				uint16_t offset = READ_SHORT();
				Value target = peek(vm, 0);

				if (!IS_STL_RESULT(target) || AS_STL_RESULT(target)->isOk) {
					frame->ip += offset;
				} else {
					Value error = OBJECT_VAL(AS_STL_RESULT(target)->as.error);
					pop(vm);
					push(vm, error);
				}
				break;
			}

			case OP_RESULT_MATCH_OK: {
				uint16_t offset = READ_SHORT();
				Value target = peek(vm, 0);

				if (!IS_STL_RESULT(target) || !AS_STL_RESULT(target)->isOk) {
					frame->ip += offset;
				} else {
					Value value = AS_STL_RESULT(target)->as.value;
					pop(vm);
					push(vm, value);
				}
				break;
			}

			case OP_RESULT_BIND: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(vm, 0);
				break;
			}

			case OP_MATCH_END: {
				pop(vm);
				break;
			}
		}
		vm->previousInstruction = instruction;
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}

InterpretResult interpret(VM *vm, char *source) {
	ObjectFunction *function = compile(vm, source);
	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	push(vm, OBJECT_VAL(function));
	ObjectClosure *closure = newClosure(vm, function);
	pop(vm);
	push(vm, OBJECT_VAL(closure));
	call(vm, closure, 0);

	return run(vm);
}
