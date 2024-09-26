//
// Created by theon on 08/09/2024.
//

#include <stdio.h>
#include <stdarg.h>

#include "vm.h"
#include "common.h"
#include "debug.h"
#include "compiler.h"
#include "value.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    const size_t instruction = vm.ip - vm.chunk->code - 1;
    // -1 because the compiler advances past each instruction before executing it.
    const int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void push(const Value value) {
    *vm.stackTop = value; // stores value in the array element at the top of the stack
    vm.stackTop++; // stack top points just past the last used element, at the next available one
}

Value pop() {
    vm.stackTop--; // Move the stack pointer back to get the most recent slot in teh array
    return *vm.stackTop;
}

static Value peek(const int distance) {
    return vm.stackTop[-1 - distance];
}

static bool isFalsy(const Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void initVM() {
    resetStack();
}

void freeVM() {
}

// The most important function
static InterpretResult run() {
    // reads the byte  currently being pointed at by the ip and then advances the ip
#define READ_BYTE() (*vm.ip++)
    // Reads the next byte from the byte code treats the resulting number as an index and looks up the corresponding Value in the chunk's constant table
#define READ_CONSTANT() (vm.chunk -> constants.values[READ_BYTE()])

#define BOOL_BINARY_OP(op) \
do {\
Value b = peek(0); \
Value a = peek(1); \
pop(); \
pop(); \
if (IS_INT(a) && IS_INT(b)){\
int aNum = AS_INT(a);\
int bNum = AS_INT(b);\
push(BOOL_VAL(aNum op bNum));\
}else if (IS_FLOAT(a) && IS_FLOAT(b)) {\
        double aNum = AS_FLOAT(a);\
        double bNum = AS_FLOAT(b);\
        push(BOOL_VAL(aNum op bNum));\
}else if ((IS_INT(a) && IS_FLOAT(b)) || (IS_FLOAT(a) && IS_INT(b))){\
	double aNum = IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a); \
    double bNum = IS_FLOAT(b) ? AS_FLOAT(b) : (double)AS_INT(b); \
    push(BOOL_VAL(aNum op bNum)); \
} else { \
       runtimeError("{ Error: Boolean Binary } Operands must be two numbers or two integers."); \
       return INTERPRET_RUNTIME_ERROR; \
   } \
} while (false)

#define BINARY_OP(valueTypeInt, valueTypeFloat, isDivision, op) \
do { \
    Value b = peek(0); \
    Value a = peek(1); \
    \
    if (!(IS_INT(a) || IS_FLOAT(a)) || !(IS_INT(b) || IS_FLOAT(b))) { \
        runtimeError("{ Error: Binary } Operands must be of type <int> or <float>"); \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    pop(); \
    pop(); \
    if (IS_INT(a) && IS_INT(b)) { \
        if (isDivision == 1){\
        	double aNum = AS_FLOAT(a); \
            double bNum = AS_FLOAT(b); \
            push(valueTypeFloat(aNum op bNum));\
		}else{\
			int aNum = AS_INT(a); \
        	int bNum = AS_INT(b); \
         	push(valueTypeInt(aNum op bNum)); \
		}\
    } else if (IS_INT(a) && IS_FLOAT(b)){\
    	int aNum = AS_INT(a);\
     	double bNum = AS_FLOAT(b);\
      	push(valueTypeFloat(aNum op bNum));\
    } else if (IS_FLOAT(a) && IS_INT(b)){\
    	double aNum = AS_FLOAT(a);\
     	int bNum = AS_INT(b);\
      	push(valueTypeFloat(aNum op bNum));\
    } else {\
    	double aNum = AS_FLOAT(a);\
     	double bNum = AS_FLOAT(b);\
      	push(valueTypeFloat(aNum op bNum));\
    }\
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

        disassembleInstruction(vm.chunk, (int) (vm.ip - vm.chunk->code));
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
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_NIL: push(NIL_VAL);
                break;
            case OP_TRUE: push(BOOL_VAL(true));
                break;
            case OP_FALSE: push(BOOL_VAL(false));
                break;

            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
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
            case OP_ADD: BINARY_OP(INT_VAL, FLOAT_VAL, 0, +);
                break;
            case OP_SUBTRACT: BINARY_OP(INT_VAL, FLOAT_VAL, 0, -);
                break;
            case OP_MULTIPLY: BINARY_OP(INT_VAL, FLOAT_VAL, 0, *);
                break;
            case OP_DIVIDE: BINARY_OP(INT_VAL, FLOAT_VAL, 1, /);
                break;
            case OP_NOT: push(BOOL_VAL(isFalsy(pop())));
                break;
            default: {
            };
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
    Chunk chunk;
    initChunk(&chunk);
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    const InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

// Store ip in a local variable to improve performance
