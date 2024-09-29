//
// Created by theon on 08/09/2024.
//

#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  Chunk *chunk;
  uint8_t *ip;            // instruction pointer
  Value stack[STACK_MAX]; // always points just past the last item
  Value *stackTop;
  Object *objects;
  Table strings;
  Table globals;
} VM;

extern VM vm;

void initVM();

void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);

Value pop();

#endif // VM_H
