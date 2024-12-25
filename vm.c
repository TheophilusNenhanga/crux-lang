#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "std/std.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "value.h"

VM vm;

void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
	vm.openUpvalues = NULL;
}

void push(Value value) {
	*vm.stackTop = value; // stores value in the array element at the top of the stack
	vm.stackTop++; // stack top points just past the last used element, at the next available one
}

Value pop() {
	vm.stackTop--; // Move the stack pointer back to get the most recent slot in the array
	return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool call(ObjectClosure *closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimePanic(ARGUMENT_MISMATCH,  "Expected %d arguments, got %d", closure->function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimePanic(STACK_OVERFLOW, "Stack overflow");
		return false;
	}

	CallFrame *frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if (IS_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
			case OBJECT_CLOSURE:
				return call(AS_CLOSURE(callee), argCount);
			case OBJECT_NATIVE: {
				ObjectNative *native = AS_NATIVE_FN(callee);
				if (argCount != native->arity) {
					runtimePanic(ARGUMENT_MISMATCH, "Expected %d argument(s), got %d", native->arity, argCount);
					return false;
				}

				NativeReturn result = native->function(argCount, vm.stackTop - argCount);
				vm.stackTop -= argCount;

				if (result.size > 0) {
					// The last Value returned from a native function must be the error if it has one
					Value last = result.values[result.size - 1];
					if (IS_ERROR(last)) {
						ObjectError *error = AS_ERROR(last);
						if (error->creator == PANIC) {
							runtimePanic(error->type, "%s", error->message->chars);
							return false;
						}
					}
				}

				for (int i = 0; i < result.size; i++) {
					push(result.values[i]);
				}

				free(result.values);
				return true;
			}
			case OBJECT_CLASS: {
				ObjectClass *klass = AS_CLASS(callee);
				vm.stackTop[-argCount - 1] = OBJECT_VAL(newInstance(klass));
				Value initializer;

				if (tableGet(&klass->methods, vm.initString, &initializer)) {
					return call(AS_CLOSURE(initializer), argCount);
				}
				if (argCount != 0) {
					runtimePanic(ARGUMENT_MISMATCH, "Expected 0 arguments but got %d arguments.", argCount);
					return false;
				}
				return true;
			}
			case OBJECT_BOUND_METHOD: {
				ObjectBoundMethod *bound = AS_BOUND_METHOD(callee);
				vm.stackTop[-argCount - 1] = bound->receiver;
				return call(bound->method, argCount);
			}
			default:
				break;
		}
	}
	runtimePanic(TYPE, "Can only call functions and classes.");
	return false;
}

static bool invokeFromClass(ObjectClass *klass, ObjectString *name, int argCount) {
	Value method;
	if (tableGet(&klass->methods, name, &method)) {
		return call(AS_CLOSURE(method), argCount);
	}
	runtimePanic(NAME, "Undefined property '%s'.", name->chars);
	return false;
}

static bool invoke(ObjectString *name, int argCount) {
	Value receiver = peek(argCount);

	if (!IS_INSTANCE(receiver)) {
		argCount++; // for the value that the method will act upon
		if (IS_STRING(receiver)) {
			Value value;
			if (tableGet(&vm.stringType.methods, name, &value)) {
				vm.stackTop[-argCount - 1] = value;
				vm.stackTop[-argCount] = receiver;
				return callValue(value, argCount);
			} else {
				runtimePanic(NAME, "Undefined method '%s'.", name->chars);
				return false;
			}
		} else if (IS_ARRAY(receiver)) {
			Value value;
			if (tableGet(&vm.arrayType.methods, name, &value)) {
				vm.stackTop[-argCount - 1] = value;
				vm.stackTop[-argCount] = receiver;
				return callValue(value, argCount);
			} else {
				runtimePanic(NAME, "Undefined method '%s'.", name->chars);
				return false;
			}
		} else if (IS_ERROR(receiver)) {
			Value value;
			if (tableGet(&vm.errorType.methods, name, &value)) {
				vm.stackTop[-argCount - 1] = value;
				vm.stackTop[-argCount] = receiver;
				return callValue(value, argCount);
			} else {
				runtimePanic(NAME, "Undefined method '%s'.", name->chars);
				return false;
			}
		} else if (IS_TABLE(receiver)) {
			Value value;
			if (tableGet(&vm.tableType.methods, name, &value)) {
				vm.stackTop[-argCount - 1] = value;
				vm.stackTop[-argCount] = receiver;
				return callValue(value, argCount);
			} else {
				runtimePanic(NAME, "Undefined method '%s'.", name->chars);
				return false;
			}
		}

		runtimePanic(TYPE, "Only instances have methods.");
		return false;
	}

	ObjectInstance *instance = AS_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}

	return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjectClass *klass, ObjectString *name) {
	Value method;
	if (!tableGet(&klass->methods, name, &method)) {
		runtimePanic(NAME, "Undefined property '%s'", name->chars);
		return false;
	}

	ObjectBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
	pop();
	push(OBJECT_VAL(bound));
	return true;
}

static ObjectUpvalue *captureUpvalue(Value *local) {
	ObjectUpvalue *prevUpvalue = NULL;
	ObjectUpvalue *upvalue = vm.openUpvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjectUpvalue *createdUpvalue = newUpvalue(local);

	createdUpvalue->next = upvalue;
	if (prevUpvalue == NULL) {
		vm.openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void closeUpvalues(Value *last) {
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
		ObjectUpvalue *upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

static void defineMethod(ObjectString *name) {
	Value method = peek(0);
	ObjectClass *klass = AS_CLASS(peek(1));
	if (tableSet(&klass->methods, name, method)) {
		pop();
	}
}

static bool isFalsy(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

static bool concatenate() {
	Value b = peek(0);
	Value a = peek(1);

	ObjectString *stringB;
	ObjectString *stringA;

	if (IS_STRING(b)) {
		stringB = AS_STRING(b);
	} else {
		stringB = toString(b);
		if (stringB == NULL) {
			runtimePanic(TYPE, "Could not convert right operand to a string.");
			return false;
		}
	}

	if (IS_STRING(a)) {
		stringA = AS_STRING(a);
	} else {
		stringA = toString(a);
		if (stringA == NULL) {
			runtimePanic(TYPE, "Could not convert left operand to a string.");
			return false;
		}
	}

	uint64_t length = stringA->length + stringB->length;
	char *chars = ALLOCATE(char, length + 1);

	if (chars == NULL) {
		runtimePanic(MEMORY, "Could not allocate memory for concatenation.");
		return false;
	}

	memcpy(chars, stringA->chars, stringA->length);
	memcpy(chars + stringA->length, stringB->chars, stringB->length);
	chars[length] = '\0';

	ObjectString *result = takeString(chars, length);

	pop();
	pop();
	push(OBJECT_VAL(result));
	return true;
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024;
	vm.currentScriptName = NULL;
	vm.grayCount = 0;
	vm.grayCapacity = 0;
	vm.grayStack = NULL;
	vm.previousInstruction = 0;

	initTable(&vm.stringType.methods);
	initTable(&vm.arrayType.methods);
	initTable(&vm.strings);
	initTable(&vm.globals);

	vm.initString = NULL;
	vm.initString = copyString("init", 4);

	defineMethods(&vm.stringType.methods, stringMethods);
	defineMethods(&vm.arrayType.methods, arrayMethods);
	defineMethods(&vm.tableType.methods, tableMethods);
	defineMethods(&vm.errorType.methods, errorMethods);

	defineNativeFunctions(&vm.globals);

}

void freeVM() {
	freeTable(&vm.strings);
	freeTable(&vm.globals);
	vm.initString = NULL;
	vm.currentScriptName = NULL;
	freeObjects();
}

static bool binaryOperation(OpCode operation) {

	Value b = peek(0);
	Value a = peek(1);


	if (!(IS_NUMBER(a) || IS_NUMBER(b))) {
		runtimePanic(TYPE, "Operands must be of type 'number'.");
		return false;
	}

	pop();
	pop();

	double aNum = AS_NUMBER(a);
	double bNum = AS_NUMBER(b);

	switch (operation) {
		case OP_ADD: {
			push(NUMBER_VAL(aNum + bNum));
			break;
		}

		case OP_SUBTRACT: {
			push(NUMBER_VAL(aNum - bNum));
			break;
		}

		case OP_MULTIPLY: {
			push(NUMBER_VAL(aNum * bNum));
			break;
		}

		case OP_DIVIDE: {
			if (bNum == 0) {
				runtimePanic(DIVISION_BY_ZERO, "Tried to divide by zero.");
				return false;
			}
			push(NUMBER_VAL(aNum / bNum));
			break;
		}

		case OP_LESS_EQUAL: {
			push(BOOL_VAL(aNum <= bNum));
			break;
		}

		case OP_GREATER_EQUAL: {
			push(BOOL_VAL(aNum >= bNum));
			break;
		}

		case OP_LESS: {
			push(BOOL_VAL(aNum < bNum));
			break;
		}

		case OP_GREATER: {
			push(BOOL_VAL(aNum > bNum));
			break;
		}
		case OP_MODULUS: {
			push(NUMBER_VAL((int64_t) aNum % (int64_t) bNum));
			break;
		}
		case OP_LEFT_SHIFT: {
			push(NUMBER_VAL((int64_t) aNum << (int64_t) bNum));
			break;
		}
		case OP_RIGHT_SHIFT: {
			push(NUMBER_VAL((int64_t) aNum >> (int64_t) bNum));
			break;
		}
		default: {
		}
	}
	return true;
}

InterpretResult globalCompoundOperation(ObjectString *name, OpCode opcode, char *operation) {
	Value currentValue;
	if (!tableGet(&vm.globals, name, &currentValue)) {
		runtimePanic(NAME, "Undefined variable '%s'.", name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
		runtimePanic(TYPE, "Operands must be ot type 'number' for '%s' operator.", operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (opcode) {
		case OP_SET_GLOBAL_SLASH: {
			if (AS_NUMBER(peek(0)) != 0.0) {
				double result = AS_NUMBER(currentValue) / AS_NUMBER(peek(0));
				tableSet(&vm.globals, name, NUMBER_VAL(result));
				break;
			}
			runtimePanic(DIVISION_BY_ZERO, "Division by zero error: '%s'.", name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		case OP_SET_GLOBAL_STAR: {
			double result = AS_NUMBER(currentValue) * AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		case OP_SET_GLOBAL_PLUS: {
			double result = AS_NUMBER(currentValue) + AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		case OP_SET_GLOBAL_MINUS: {
			double result = AS_NUMBER(currentValue) - AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		default:;
	}
	return INTERPRET_OK;
}

static void reverse_stack(int actual) {
	Value *start = vm.stackTop - actual;
	for (int i = 0; i < actual / 2; i++) {
		Value temp = start[i];
		start[i] = start[actual - 1 - i];
		start[actual - 1 - i] = temp;
	}
}

static InterpretResult run() {
	CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))
	uint8_t instruction;
	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("        ");
		for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[");
			printValue(*slot);
			printf("]");
		}
		printf("\n");

		disassembleInstruction(&frame->closure->function->chunk, (int) (frame->ip - frame->closure->function->chunk.code));
#endif

		instruction = READ_BYTE();
		switch (instruction) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}

			case OP_RETURN: {
				uint8_t valueCount = READ_BYTE();
				Value values[255];

				if (valueCount == 1 && vm.previousInstruction == OP_RETURN) {
					Value lastValue = peek(0);
					if (IS_NUMBER(lastValue)) {
						valueCount = (uint8_t) AS_NUMBER(pop());
					}
				}

				for (int i = 0; i < valueCount; i++) {
					values[i] = pop();
				}

				pop(); // pop the closure

				closeUpvalues(frame->slots);
				vm.frameCount--;
				if (vm.frameCount == 0) {
					pop();
					return INTERPRET_OK;
				}
				vm.stackTop = frame->slots;

				for (int i = valueCount - 1; i >= 0; i--) {
					push(values[i]);
				}

				if (valueCount > 1) {
					push(NUMBER_VAL(valueCount));
				}

				CallFrame* callerFrame = &vm.frames[vm.frameCount - 1];
				uint8_t nextInstr = *callerFrame->ip;

				if (nextInstr != OP_RETURN && nextInstr != OP_UNPACK_TUPLE) {
					runtimePanic(UNPACK_MISMATCH, "Expected to unpack %d values.", valueCount);
					return INTERPRET_RUNTIME_ERROR;
				}

				frame = callerFrame;
				break;
			}

			case OP_CLOSURE: {
				ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
				ObjectClosure *closure = newClosure(function);
				push(OBJECT_VAL(closure));

				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();

					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}

				break;
			}

			case OP_NIL: {
				push(NIL_VAL);
				break;
			}

			case OP_TRUE: {
				push(BOOL_VAL(true));
				break;
			}

			case OP_FALSE: {
				push(BOOL_VAL(false));
				break;
			}

			case OP_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}

			case OP_NOT_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(!valuesEqual(a, b)));
				break;
			}

			case OP_GREATER: {
				if (!binaryOperation(OP_GREATER)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS: {
				if (!binaryOperation(OP_LESS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS_EQUAL: {
				if (!binaryOperation(OP_LESS_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GREATER_EQUAL: {
				if (!binaryOperation(OP_GREATER_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NEGATE: {
				if (IS_NUMBER(peek(0))) {
					push(NUMBER_VAL(-AS_NUMBER(pop())));
					break;
				}
				runtimePanic(TYPE, "Operand must be of type 'number'.");
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_ADD: {
				if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
					if (!concatenate()) {
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				if (!binaryOperation(OP_ADD)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUBTRACT: {
				if (!binaryOperation(OP_SUBTRACT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MULTIPLY: {
				if (!binaryOperation(OP_MULTIPLY)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_DIVIDE: {
				if (!binaryOperation(OP_DIVIDE)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MODULUS: {
				if (!binaryOperation(OP_MODULUS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LEFT_SHIFT: {
				if (!binaryOperation(OP_LEFT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_RIGHT_SHIFT: {
				if (!binaryOperation(OP_RIGHT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NOT: {
				push(BOOL_VAL(isFalsy(pop())));
				break;
			}

			case OP_PRINT: {
				printValue(pop());
				printf("\n");
				break;
			}

			case OP_POP: {
				pop();
				break;
			}

			case OP_DEFINE_GLOBAL: {
				ObjectString *name = READ_STRING();
				if (tableSet(&vm.globals, name, peek(0))) {
					pop();
					break;
				}
				runtimePanic(NAME, "Cannot define '%s' because it is already defined.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_GET_GLOBAL: {
				ObjectString *name = READ_STRING();
				Value value;
				if (tableGet(&vm.globals, name, &value)) {
					push(value);
					break;
				}
				runtimePanic(NAME, "Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_SET_GLOBAL: {
				ObjectString *name = READ_STRING();
				if (!tableSet(&vm.globals, name, peek(0))) {
					runtimePanic(NAME, "Cannot give variable '%s' a value because it has not been defined", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(frame->slots[slot]);
				break;
			}

			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(0);
				break;
			}

			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsy(peek(0))) {
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
				if (!callValue(peek(argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_GET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				push(*frame->closure->upvalues[slot]->location);
				break;
			}

			case OP_SET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				*frame->closure->upvalues[slot]->location = peek(0);
				break;
			}

			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm.stackTop - 1);
				pop();
				break;
			}

			case OP_CLASS: {
				push(OBJECT_VAL(newClass(READ_STRING())));
				break;
			}

			case OP_GET_PROPERTY: {
				if (!IS_INSTANCE(peek(0))) {
					runtimePanic(TYPE, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjectInstance *instance = AS_INSTANCE(peek(0));
				ObjectString *name = READ_STRING();

				Value value;
				bool fieldFound = false;
				if (tableGet(&instance->fields, name, &value)) {
					pop();
					push(value);
					fieldFound = true;
					break;
				}

				if (!fieldFound) {
					if (!bindMethod(instance->klass, name)) {
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				break;
			}

			case OP_SET_PROPERTY: {
				if (!IS_INSTANCE(peek(1))) {
					runtimePanic(TYPE, "Only instances have fields.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectInstance *instance = AS_INSTANCE(peek(1));
				if (tableSet(&instance->fields, READ_STRING(), peek(0))) {
					Value value = pop();
					pop();
					push(value);
					break;
				}
				runtimePanic(NAME, "Undefined property '%s'.", READ_STRING());
				return INTERPRET_RUNTIME_ERROR;
			}
			case OP_METHOD: {
				defineMethod(READ_STRING());
				break;
			}

			case OP_INVOKE: {
				ObjectString *method = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_INHERIT: {
				Value superClass = peek(1);

				if (!IS_CLASS(superClass)) {
					runtimePanic(TYPE, "Cannot inherit from non class object.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectClass *subClass = AS_CLASS(peek(0));
				tableAddAll(&AS_CLASS(superClass)->methods, &subClass->methods);
				pop();
				break;
			}

			case OP_GET_SUPER: {
				ObjectString *name = READ_STRING();
				ObjectClass *superClass = AS_CLASS(pop());

				if (!bindMethod(superClass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUPER_INVOKE: {
				ObjectString *method = READ_STRING();
				int argCount = READ_BYTE();
				ObjectClass *superClass = AS_CLASS(pop());
				if (!invokeFromClass(superClass, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_ARRAY: {
				uint16_t elementCount = READ_SHORT();
				ObjectArray *array = newArray(elementCount);
				for (int i = elementCount - 1; i >= 0; i--) {
					arrayAdd(array, pop(), i);
				}
				push(OBJECT_VAL(array));
				break;
			}

			case OP_TABLE: {
				uint16_t elementCount = READ_SHORT();
				ObjectTable *table = newTable(elementCount);
				for (int i = elementCount - 1; i >= 0; i--) {
					Value value = pop();
					Value key = pop();
					if (IS_NUMBER(key) || IS_STRING(key)) {
						if (!objectTableSet(table, key, value)) {
							runtimePanic(COLLECTION_SET, "Failed to set value in table");
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimePanic(TYPE, "Key cannot be hashed.", READ_STRING());
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				push(OBJECT_VAL(table));
				break;
			}

			case OP_GET_COLLECTION: {
				Value indexValue = pop();
				if (IS_TABLE(peek(0))) {
					if (IS_STRING(indexValue) || IS_NUMBER(indexValue)) {
						ObjectTable *table = AS_TABLE(peek(0));
						Value value;
						if (!objectTableGet(table, indexValue, &value)) {
							runtimePanic(COLLECTION_GET, "Failed to get value from table");
							return INTERPRET_RUNTIME_ERROR;
						}
						pop();
						push(value);
					} else {
						runtimePanic(TYPE, "Key cannot be hashed.", READ_STRING());
						return INTERPRET_RUNTIME_ERROR;
					}
				} else if (IS_ARRAY(peek(0))) {
					if (!IS_NUMBER(indexValue)) {
						runtimePanic(TYPE, "Index must be of type 'number'.");
						return INTERPRET_RUNTIME_ERROR;
					}
					int index = AS_NUMBER(indexValue);
					ObjectArray *array = AS_ARRAY(peek(0));
					Value value;
					if (index < 0 || index >= array->size) {
						runtimePanic(INDEX_OUT_OF_BOUNDS, "Index out of bounds.");
						return INTERPRET_RUNTIME_ERROR;
					}
					if (!arrayGet(array, index, &value)) {
						runtimePanic(COLLECTION_GET, "Failed to get value from array");
						return INTERPRET_RUNTIME_ERROR;
					}
					pop(); // pop the array off the stack
					push(value); // push the value onto the stack
					break;
				}
				break;
			}

			case OP_SET_COLLECTION: {
				Value value = pop();
				Value indexValue = peek(0);

				if (IS_TABLE(peek(1))) {
					ObjectTable *table = AS_TABLE(peek(1));
					if (IS_NUMBER(indexValue) || IS_STRING(indexValue)) {
						if (!objectTableSet(table, indexValue, value)) {
							runtimePanic(COLLECTION_GET, "Failed to set value in table");
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimePanic(TYPE, "Key cannot be hashed.");
						return INTERPRET_RUNTIME_ERROR;
					}
				} else if (IS_ARRAY(peek(1))) {
					ObjectArray *array = AS_ARRAY(peek(1));
					int index = AS_NUMBER(indexValue);
					if (!arraySet(array, index, value)) {
						runtimePanic(COLLECTION_SET, "Failed to set value in array");
						return INTERPRET_RUNTIME_ERROR;
					}
				} else {
					runtimePanic(TYPE, "Value is not a mutable collection type.");
					return INTERPRET_RUNTIME_ERROR;
				}
				pop(); // collection
				pop(); // indexValue
				push(indexValue);
				break;
			}

			case OP_SET_LOCAL_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(0));

				if (divisor == 0.0) {
					runtimePanic(DIVISION_BY_ZERO, "Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);
				break;
			}
			case OP_SET_LOCAL_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) * AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_LOCAL_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) + AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_LOCAL_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) - AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_UPVALUE_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				double divisor = AS_NUMBER(peek(0));
				if (divisor == 0.0) {
					runtimePanic(DIVISION_BY_ZERO, "Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);

				break;
			}
			case OP_SET_UPVALUE_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) * AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_UPVALUE_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) + AS_NUMBER(peek(0)));

				break;
			}
			case OP_SET_UPVALUE_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimePanic(TYPE, "Both operands must be of type 'number' for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) / AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_GLOBAL_SLASH: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_SLASH, "/=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_STAR: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_STAR, "*=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_PLUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_PLUS, "+=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_MINUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_MINUS, "-=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_ANON_FUNCTION: {
				ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
				ObjectClosure *closure = newClosure(function);
				push(OBJECT_VAL(closure));
				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();

					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}

				break;
			}

			case OP_UNPACK_TUPLE: {
				uint8_t variableCount = READ_BYTE();
				uint8_t scopeDepth = READ_BYTE();
				int actual = variableCount;

				if (vm.previousInstruction == OP_RETURN) {
					Value countValue = peek(0);
					if (IS_NUMBER(countValue)) {
						actual = AS_NUMBER(pop());
						if (variableCount != actual) {
							runtimePanic(UNPACK_MISMATCH, "Expected %d values to unpack but got %d.", variableCount, actual);
							return INTERPRET_RUNTIME_ERROR;
						}
					}
				} else {
					int valuesOnStack = (int)(vm.stackTop - vm.stack - vm.frameCount);
					if (valuesOnStack < variableCount) {
						runtimePanic(UNPACK_MISMATCH, "Not enough values to unpack. Expected %d but got %d.", variableCount,
												 valuesOnStack);
						return INTERPRET_RUNTIME_ERROR;
					}

				}
				if (scopeDepth == 0) {
					reverse_stack(actual);
				}
				break;
			}

			case OP_USE: {
				uint8_t nameCount = READ_BYTE();
				ObjectString *names[UINT8_MAX];
				for (int i = 0; i < nameCount; i++) {
					names[i] = READ_STRING();
				}
				ObjectString *module = READ_STRING();
				
			}
		}
		vm.previousInstruction = instruction;
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}

InterpretResult interpret(char *source, char* path) {
	if (path != NULL) {
		vm.currentScriptName = copyString(path, strlen(path));
	}else {
		vm.currentScriptName = copyString("<script>", strlen("<script>"));
	}

	ObjectFunction *function = compile(source);
	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	push(OBJECT_VAL(function));
	ObjectClosure *closure = newClosure(function);
	pop();
	push(OBJECT_VAL(closure));
	call(closure, 0);

	return run();
}
