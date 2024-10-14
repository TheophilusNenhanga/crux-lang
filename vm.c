#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

VM vm;

static Value clockNative(int argCount, Value *args) { return FLOAT_VAL((double) clock() / CLOCKS_PER_SEC); }

static Value printNative(int argCount, Value *args) {
	Value value = args[0];
	switch (value.type) {
		case VAL_BOOL:
			printf(AS_BOOL(value) ? "true\n" : "false\n");
			break;
		case VAL_NIL:
			printf("nil\n");
			break;
		case VAL_INT:
			printf("%lld\n", AS_INT(value));
			break;
		case VAL_FLOAT:
			printf("%g\n", AS_FLOAT(value));
			break;
		case VAL_OBJECT:
			printObject(value);
			break;
	}
	return NIL_VAL;
}

static void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
}

static void runtimeError(const char *format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "-------RUNTIME ERROR-------\n");
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame *frame = &vm.frames[i];
		ObjectFunction *function = frame->function;
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			printf("%s()\n", function->name->chars);
		}
	}

	resetStack();
}

static void defineNative(const char *name, NativeFn function, int arity) {
	push(OBJECT_VAL(copyString(name, (int) strlen(name))));
	push(OBJECT_VAL(newNative(function, arity)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
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

static bool call(ObjectFunction *function, int argCount) {
	if (argCount != function->arity) {
		runtimeError("Expected %d arguments, got %d", function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow");
		return false;
	}

	CallFrame *frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if (IS_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
			case OBJECT_FUNCTION:
				return call(AS_FUNCTION(callee), argCount);
			case OBJECT_NATIVE: {
				ObjectNative *native = AS_NATIVE_FN(callee);
				if (argCount != native->arity) {
					runtimeError("Expected %d argument(s), got %d", native->arity, argCount);
					return false;
				}
				Value result = native->function(argCount, vm.stackTop - argCount);
				vm.stackTop -= argCount + 1;
				push(result);
				return true;
			}
			default:
				break;
		}
	}
	runtimeError("Can only call functions and classes");
	return false;
}

static bool isFalsy(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

static void concatenate() {
	ObjectString *b = AS_STRING(pop());
	ObjectString *a = AS_STRING(pop());

	int length = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjectString *result = takeString(chars, length);
	push(OBJECT_VAL(result));
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	initTable(&vm.strings);
	initTable(&vm.globals);

	defineNative("clock", clockNative, 0);
	defineNative("print", printNative, 1);
}

void freeVM() {
	freeTable(&vm.strings);
	freeTable(&vm.globals);
	freeObjects();
}

static InterpretResult run() {
	CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))

#define BOOL_BINARY_OP(op)                                                                                             \
	do {                                                                                                                 \
		Value b = peek(0);                                                                                                 \
		Value a = peek(1);                                                                                                 \
		pop();                                                                                                             \
		pop();                                                                                                             \
		if (IS_INT(a) && IS_INT(b)) {                                                                                      \
			int aNum = AS_INT(a);                                                                                            \
			int bNum = AS_INT(b);                                                                                            \
			push(BOOL_VAL(aNum op bNum));                                                                                    \
		} else if (IS_FLOAT(a) && IS_FLOAT(b)) {                                                                           \
			double aNum = AS_FLOAT(a);                                                                                       \
			double bNum = AS_FLOAT(b);                                                                                       \
			push(BOOL_VAL(aNum op bNum));                                                                                    \
		} else if ((IS_INT(a) && IS_FLOAT(b)) || (IS_FLOAT(a) && IS_INT(b))) {                                             \
			double aNum = IS_FLOAT(a) ? AS_FLOAT(a) : (double) AS_INT(a);                                                    \
			double bNum = IS_FLOAT(b) ? AS_FLOAT(b) : (double) AS_INT(b);                                                    \
			push(BOOL_VAL(aNum op bNum));                                                                                    \
		} else {                                                                                                           \
			runtimeError("{ Error: Boolean Binary Operation } Operands must be two numbers "                                 \
									 "or two integers.");                                                                                \
			return INTERPRET_RUNTIME_ERROR;                                                                                  \
		}                                                                                                                  \
	} while (false)

#define BINARY_OP(valueTypeInt, valueTypeFloat, isDivision, op)                                                        \
	do {                                                                                                                 \
		Value b = peek(0);                                                                                                 \
		Value a = peek(1);                                                                                                 \
                                                                                                                       \
		if (!(IS_INT(a) || IS_FLOAT(a)) || !(IS_INT(b) || IS_FLOAT(b))) {                                                  \
			runtimeError("{ Error: Binary Operation } Operands must be of type "                                             \
									 "<int> or <float>");                                                                                \
			return INTERPRET_RUNTIME_ERROR;                                                                                  \
		}                                                                                                                  \
		pop();                                                                                                             \
		pop();                                                                                                             \
		if (IS_INT(a) && IS_INT(b)) {                                                                                      \
			if (isDivision == 1) {                                                                                           \
				double aNum = AS_FLOAT(a);                                                                                     \
				double bNum = AS_FLOAT(b);                                                                                     \
				push(valueTypeFloat(aNum op bNum));                                                                            \
			} else {                                                                                                         \
				int aNum = AS_INT(a);                                                                                          \
				int bNum = AS_INT(b);                                                                                          \
				push(valueTypeInt(aNum op bNum));                                                                              \
			}                                                                                                                \
		} else if (IS_INT(a) && IS_FLOAT(b)) {                                                                             \
			int aNum = AS_INT(a);                                                                                            \
			double bNum = AS_FLOAT(b);                                                                                       \
			push(valueTypeFloat(aNum op bNum));                                                                              \
		} else if (IS_FLOAT(a) && IS_INT(b)) {                                                                             \
			double aNum = AS_FLOAT(a);                                                                                       \
			int bNum = AS_INT(b);                                                                                            \
			push(valueTypeFloat(aNum op bNum));                                                                              \
		} else {                                                                                                           \
			double aNum = AS_FLOAT(a);                                                                                       \
			double bNum = AS_FLOAT(b);                                                                                       \
			push(valueTypeFloat(aNum op bNum));                                                                              \
		}                                                                                                                  \
	} while (false);

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION

		printf("        ");
		for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[");
			printValue(*slot);
			printf("]");
		}
		printf("\n");

		disassembleInstruction(&frame->function->chunk, (int) (frame->ip - frame->function->chunk.code));
#endif

		uint8_t instruction;
		// decoding and dispatching
		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}

			case OP_RETURN: {
				Value result = pop();
				vm.frameCount--;
				if (vm.frameCount == 0) {
					pop();
					return INTERPRET_OK;
				}
				vm.stackTop = frame->slots;
				push(result);
				frame = &vm.frames[vm.frameCount - 1];
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
				BOOL_BINARY_OP(>);
				break;
			}

			case OP_LESS: {
				BOOL_BINARY_OP(<);
				break;
			}

			case OP_LESS_EQUAL: {
				BOOL_BINARY_OP(<=);
				break;
			}

			case OP_GREATER_EQUAL: {
				BOOL_BINARY_OP(>=);
				break;
			}

			case OP_NEGATE: {
				if (!IS_INT(peek(0)) && !IS_FLOAT(peek(0))) {
					runtimeError("Operand must be of type <int> or <float>.");
					return INTERPRET_RUNTIME_ERROR;
				}
				if (IS_INT(peek(0))) {
					push(INT_VAL(-AS_INT(pop())));
				} else if (IS_FLOAT(peek(0))) {
					push(FLOAT_VAL(-AS_FLOAT(pop())));
				}
				break;
			}

			case OP_ADD: {
				if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_INT(peek(0)) && IS_INT(peek(1))) {
					int b = AS_INT(pop());
					int a = AS_INT(pop());
					push(INT_VAL(a + b));
				} else if (IS_INT(peek(0)) && IS_FLOAT(peek(1))) {
					int b = AS_INT(pop());
					double a = AS_FLOAT(pop());
					push(FLOAT_VAL(a + b));
				} else if (IS_FLOAT(peek(0)) && IS_INT(peek(1))) {
					double b = AS_FLOAT(pop());
					int a = AS_INT(pop());
					push(FLOAT_VAL(a + b));
				} else if (IS_FLOAT(peek(0)) && IS_FLOAT(peek(1))) {
					double b = AS_FLOAT(pop());
					double a = AS_FLOAT(pop());
					push(FLOAT_VAL(a + b));
				} else {
					runtimeError("{ Error Binary Addition Operation: Operands must be of "
											 "type <int> or <float> }");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUBTRACT: {
				BINARY_OP(INT_VAL, FLOAT_VAL, 0, -);
				break;
			}

			case OP_MULTIPLY: {
				BINARY_OP(INT_VAL, FLOAT_VAL, 0, *);
				break;
			}

			case OP_DIVIDE: {
				BINARY_OP(INT_VAL, FLOAT_VAL, 1, /);
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

				switch (tableSet(&vm.globals, name, peek(0))) {
					case IMMUTABLE_OVERWRITE: {
						runtimeError("Cannot define '%s' because it is already defined.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					case NEW_KEY_SUCCESS: {
						pop();
						break;
					}
					case SET_SUCCESS: {
						runtimeError("Cannot define '%s' because it is already defined.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					default:
						break;
				}
				break;
			}

			case OP_GET_GLOBAL: {
				ObjectString *name = READ_STRING();
				Value value;

				switch (tableGet(&vm.globals, name, &value)) {
					case TABLE_EMPTY:
					case VAR_NOT_FOUND: {
						runtimeError("Undefined variable '%s'.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					case GET_SUCCESS: {
						push(value);
						break;
					}
					default:
						break;
				}
				break;
			}

			case OP_SET_GLOBAL: {
				ObjectString *name = READ_STRING();
				switch (tableSet(&vm.globals, name, peek(0))) {
					case SET_SUCCESS: {
						break;
					}
					case NEW_KEY_SUCCESS: {
						runtimeError("Cannot define '%s' because variable with this name already exists.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					case IMMUTABLE_OVERWRITE: {
						runtimeError("Cannot modify <set> defined variable '%s'.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					default:
						break;
				}
				break;
			}

			case OP_DEFINE_GLOBAL_CONSTANT: {
				ObjectString *name = READ_STRING();
				Value value = peek(0);
				switch (tableSet(&vm.globals, name, value)) {
					case IMMUTABLE_OVERWRITE:
					case SET_SUCCESS: {
						runtimeError("Cannot define '%s' because it is already defined.", name->chars);
						return INTERPRET_RUNTIME_ERROR;
					}
					case NEW_KEY_SUCCESS: {
						pop();
						break;
					}
					default: {
						break;
					}
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
				if (frame->slots[slot].isMutable) {
					frame->slots[slot] = peek(0);
				} else {
					runtimeError("Cannot modify <set> defined local variable");
					return INTERPRET_RUNTIME_ERROR;
				}
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

			default: {
			}
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}

InterpretResult interpret(const char *source) {
	ObjectFunction *function = compile(source);
	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	push(OBJECT_VAL(function));
	call(function, 0);
	return run();
}

// Store ip in a local variable to improve performance
