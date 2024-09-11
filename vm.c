//
// Created by theon on 08/09/2024.
//

#include "vm.h"

#include <stdio.h>

#include "common.h"
#include "debug.h"

VM vm;

static void resetStack() {
    vm.stackTop = vm.stack;
}

void push(Value value) {
    *vm.stackTop = value; // stores value in the array element at the top of the stack
    vm.stackTop++; // stack top points just past the last used element, at the next available one
}

Value pop() {
    vm.stackTop--; // Move teh stack pointer back to get the most recent slot in teh array
    return *vm.stackTop;
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
#define BINARY_OP(op) \
do {\
    double b = pop();\
    double a = pop();\
    push(a op b);\
}while (false) // Do this once

    for (;;) {
        // infinite loop

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
            case OP_NEGATE: {
                push(-pop()); // this can be optimized
                break;
            }
            case OP_ADD: BINARY_OP(+); // an operator can be passed as an argument to the preprocessor...it is just treated as a text token like everything else
                break;
            case OP_SUBTRACT: BINARY_OP(-);
                break;
            case OP_MULTIPLY: BINARY_OP(*);
                break;
            case OP_DIVIDE: BINARY_OP(/);
                break;
            default: {

            };
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk; // Store the chunk being executed in the VM
    vm.ip = vm.chunk->code; // points to teh instruction that is about to be executed
    return run();
}

// Store ip in a local variable to improve performance
