#include "compiler.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "errno.h"
#include "file_handler.h"
#include "memory.h"
#include "object.h"
#include "panic.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Parser parser;

Compiler *current = NULL;

Chunk *compilingChunk;

static void expression(void);

static void parsePrecedence(Precedence precedence);

static ParseRule *getRule(CruxTokenType type);

static void binary(bool canAssign);

static void unary(bool canAssign);

static void grouping(bool canAssign);

static void number(bool canAssign);

static void statement(void);

static void declaration(void);

static Chunk *currentChunk(void) { return &current->function->chunk; }

static void advance(void) {
  parser.prevPrevious = parser.previous;
  parser.previous = parser.current;
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;
    compilerPanic(&parser, parser.current.start, SYNTAX);
  }
}

static void consume(const CruxTokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  compilerPanic(&parser, message, SYNTAX);
}

static bool check(const CruxTokenType type) {
  return parser.current.type == type;
}

static bool match(const CruxTokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emitByte(const uint8_t byte) {
  writeChunk(current->owner, currentChunk(), byte, parser.previous.line);
}

static void emitBytes(const uint8_t byte1, const uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

/**
 * emits an OP_LOOP instruction
 * @param loopStart The starting point of the loop
 */
static void emitLoop(const int loopStart) {
  emitByte(OP_LOOP);
  const int offset = currentChunk()->count - loopStart +
                     2; // +2 takes into account the size of the OP_LOOP
  if (offset > UINT16_MAX) {
    compilerPanic(&parser, "Loop body too large.", LOOP_EXTENT);
  }
  emitBytes(((offset >> 8) & 0xff), (offset & 0xff));
}

/**
 * Emits a jump instruction and placeholders for the jump offset.
 *
 * @param instruction The opcode for the jump instruction (e.g., OP_JUMP,
 * OP_JUMP_IF_FALSE).
 * @return The index of the jump instruction in the bytecode, used for patching
 * later.
 */
static int emitJump(const uint8_t instruction) {
  emitByte(instruction);
  emitBytes(0xff, 0xff);
  return currentChunk()->count - 2;
}

/**
 * Patches a jump instruction with the calculated offset.
 *
 * @param offset The index of the jump instruction in the bytecode to patch.
 */
static void patchJump(const int offset) {
  // -2 to adjust for the bytecode for the jump offset itself
  const int jump = currentChunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    compilerPanic(&parser, "Too much code to jump over.", BRANCH_EXTENT);
  }
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

/**
 * Emits OP_NIL_RETURN that signals the end of a scope.
 */
static void emitReturn(void) { emitByte(OP_NIL_RETURN); }

/**
 * Creates a constant value and adds it to the current chunk's constant pool.
 *
 * @param value The value to add as a constant.
 * @return The index of the constant in the constant pool.
 */
static uint16_t makeConstant(const Value value) {
  const int constant = addConstant(current->owner, currentChunk(), value);
  if (constant > UINT16_MAX) {
    compilerPanic(&parser, "Too many constants in one chunk.", LIMIT);
    return 0;
  }
  return (uint16_t)constant;
}

/**
 * Emits an OP_CONSTANT instruction and its operand (the constant index).
 *
 * @param value The constant value to emit.
 */
static void emitConstant(const Value value) {
  const uint16_t constant = makeConstant(value);
  if (constant <= UINT8_MAX) {
    emitBytes(OP_CONSTANT, (uint8_t)constant);
  } else {
    emitByte(OP_CONSTANT_16);
    emitBytes(((constant >> 8) & 0xff), (constant & 0xff));
  }
}

/**
 * Initializes a compiler.
 *
 * Sets up the compiler state for compiling a new function (or script).
 *
 * @param compiler The compiler to initialize.
 * @param type The type of function being compiled (e.g., TYPE_FUNCTION,
 * TYPE_SCRIPT).
 * @param vm The virtual machine instance.
 */
static void initCompiler(Compiler *compiler, const FunctionType type, VM *vm) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->matchDepth = 0;
  compiler->loopDepth = 0;
  compiler->owner = vm;

  compiler->function = newFunction(compiler->owner);
  current = compiler;

  if (type == TYPE_ANONYMOUS) {
    current->function->name = copyString(current->owner, "anonymous", 9);
  } else if (type != TYPE_SCRIPT) {
    current->function->name = copyString(current->owner, parser.previous.start,
                                         parser.previous.length);
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
  local->isCaptured = false;

  if (type != TYPE_FUNCTION) {
    local->name.start = "self";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

/**
 * Creates a constant for an identifier token.
 *
 * @param name The token representing the identifier.
 * @return The index of the identifier constant in the constant pool.
 */
static uint16_t identifierConstant(const Token *name) {
  return makeConstant(
      OBJECT_VAL(copyString(current->owner, name->start, name->length)));
}

/**
 * Begins a new scope.
 *
 * Increases the scope depth, indicating that variables declared subsequently
 * are in a new, inner scope.
 */
static void beginScope(void) { current->scopeDepth++; }

static void cleanupLocalsToDepth(const int targetDepth) {
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > targetDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }
}

/**
 * Ends the current scope.
 *
 * Decreases the scope depth and emits OP_POP instructions to remove local
 * variables that go out of scope.
 */
static void endScope(void) {
  current->scopeDepth--;
  cleanupLocalsToDepth(current->scopeDepth);
}

/**
 * Checks if two identifier tokens are equal.
 *
 * @param a The first token.
 * @param b The second token.
 * @return true if the tokens represent the same identifier, false otherwise.
 */
static bool identifiersEqual(const Token *a, const Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Resolves a local variable in the current compiler's scope.
 *
 * Searches the current compiler's local variables for a variable with the given
 * name.
 *
 * @param compiler The compiler whose scope to search.
 * @param name The token representing the variable name.
 * @return The index of the local variable if found, -1 otherwise.
 */
static int resolveLocal(const Compiler *compiler, const Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    const Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        compilerPanic(
            &parser, "Cannot read local variable in its own initializer", NAME);
      }
      return i;
    }
  }
  return -1;
}

static void pushLoopContext(const LoopType type, const int continueTarget) {
  if (current->loopDepth >= 255) {
    compilerPanic(&parser, "Too many nested loops.", LOOP_EXTENT);
    return;
  }

  LoopContext *context = &current->loopStack[current->loopDepth++];
  context->type = type;
  context->continueTarget = continueTarget;
  context->breakJumps = NULL;
  context->scopeDepth = current->scopeDepth;
}

static void popLoopContext(void) {
  if (current->loopDepth <= 0) {
    return;
  }

  const LoopContext *context = &current->loopStack[--current->loopDepth];

  // Patch all break jumps to jump to current position
  BreakJump *breakJump = context->breakJumps;
  while (breakJump != NULL) {
    patchJump(breakJump->jumpOffset);
    BreakJump *next = breakJump->next;
    FREE(current->owner, BreakJump, breakJump);
    breakJump = next;
  }
}

static void addBreakJump(const int jumpOffset) {
  if (current->loopDepth <= 0) {
    compilerPanic(&parser, "Cannot use 'break' outside of a loop.", SYNTAX);
    return;
  }

  LoopContext *context = &current->loopStack[current->loopDepth - 1];

  BreakJump *breakJump = ALLOCATE(current->owner, BreakJump, 1);
  breakJump->jumpOffset = jumpOffset;
  breakJump->next = context->breakJumps;
  context->breakJumps = breakJump;
}

static int getCurrentContinueTarget(void) {
  if (current->loopDepth <= 0) {
    compilerPanic(&parser, "Cannot use 'continue' outside of a loop.", SYNTAX);
    return -1;
  }

  return current->loopStack[current->loopDepth - 1].continueTarget;
}

/**
 * Adds an upvalue to the current function.
 *
 * An upvalue is a variable captured from an enclosing function's scope.
 *
 * @param compiler The current compiler.
 * @param index The index of the local variable in the enclosing function, or
 * the index of the upvalue.
 * @param isLocal true if the upvalue is a local variable in the enclosing
 * function, false if it's an upvalue itself.
 * @return The index of the added upvalue in the current function's upvalue
 * array.
 */
static int addUpvalue(Compiler *compiler, const uint8_t index,
                      const bool isLocal) {
  const int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    const Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount >= UINT8_COUNT) {
    compilerPanic(&parser, "Too many closure variables in function.",
                  CLOSURE_EXTENT);
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

/**
 * Resolves an upvalue in enclosing scopes.
 *
 * Searches enclosing compiler scopes for a local variable or upvalue with the
 * given name.
 *
 * @param compiler The current compiler.
 * @param name The token representing the variable name.
 * @return The index of the resolved upvalue if found, -1 otherwise.
 */
static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  const int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    (compiler->enclosing)->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  const int upValue = resolveUpvalue(compiler->enclosing, name);
  if (upValue != -1) {
    return addUpvalue(compiler, (uint8_t)upValue, false);
  }

  return -1;
}

/**
 * Adds a local variable to the current scope.
 *
 * @param name The token representing the name of the local variable.
 */
static void addLocal(const Token name) {
  if (current->localCount == UINT8_COUNT) {
    compilerPanic(&parser, "Too many local variables in function.",
                  LOCAL_EXTENT);
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

/**
 * Declares a variable in the current scope.
 *
 * Adds the variable name to the list of local variables, but does not mark it
 * as initialized yet.
 */
static void declareVariable(void) {
  if (current->scopeDepth == 0)
    return;

  const Token *name = &parser.previous;

  for (int i = current->localCount - 1; i >= 0; i--) {
    const Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifiersEqual(name, &local->name)) {
      compilerPanic(&parser, "Cannot redefine variable in the same scope",
                    NAME);
    }
  }

  addLocal(*name);
}

/**
 * Marks the most recently declared local variable as initialized.
 *
 * This prevents reading a local variable before it has been assigned a value.
 */
static void markInitialized(void) {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * Parses a variable name and returns its constant pool index.
 *
 * @param errorMessage The error message to display if an identifier is not
 * found.
 * @return The index of the variable name constant in the constant pool.
 */
static uint16_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0)
    return 0;
  return identifierConstant(&parser.previous);
}

/**
 * Defines a variable, emitting the bytecode to store its value.
 *
 * If the variable is global, emits OP_DEFINE_GLOBAL. Otherwise, it's a local
 * variable, and no bytecode is emitted at definition time (local variables are
 * implicitly defined when they are declared and initialized).
 *
 * @param global The index of the variable name constant in the constant pool
 * (for global variables).
 */
static void defineVariable(const uint16_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  if (global <= UINT8_MAX) {
    emitBytes(OP_DEFINE_GLOBAL, (uint8_t)global);
  } else {
    emitByte(OP_DEFINE_GLOBAL_16);
    emitBytes(((global >> 8) & 0xff), (global & 0xff));
  }
}

/**
 * Parses an argument list for a function call.
 *
 * @return The number of arguments parsed.
 */
static uint8_t argumentList(void) {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        compilerPanic(&parser, "Cannot have more than 255 arguments.",
                      ARGUMENT_EXTENT);
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
  return argCount;
}

/**
 * Parses the 'and' operator. Implemented using short-circuiting.
 *
 * @param canAssign Whether the 'and' expression can be the target of an
 * assignment.
 */
static void and_(bool canAssign __attribute__((unused))) {
  const int endJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

/**
 * Parses the 'or' operator. Implemented using short-circuiting.
 *
 * @param canAssign Whether the 'or' expression can be the target of an
 * assignment.
 */
static void or_(bool canAssign __attribute__((unused))) {
  const int elseJump = emitJump(OP_JUMP_IF_FALSE);
  const int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);
  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

/**
 * Finishes compilation and returns the compiled function.
 *
 * Emits the final OP_RETURN instruction, frees the compiler state, and returns
 * the compiled function object.
 *
 * @return The compiled function object.
 */
static ObjectFunction *endCompiler(void) {
  emitReturn();
  ObjectFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  function->moduleRecord = current->owner->currentModuleRecord;
  current = current->enclosing;
  return function;
}

static void binary(bool canAssign __attribute__((unused))) {
  const CruxTokenType operatorType = parser.previous.type;
  const ParseRule *rule = getRule(operatorType);
  parsePrecedence(rule->precedence + 1);

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitByte(OP_NOT_EQUAL);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitByte(OP_GREATER_EQUAL);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitByte(OP_LESS_EQUAL);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_PERCENT:
    emitByte(OP_MODULUS);
    break;
  case TOKEN_RIGHT_SHIFT:
    emitByte(OP_RIGHT_SHIFT);
    break;
  case TOKEN_LEFT_SHIFT:
    emitByte(OP_LEFT_SHIFT);
    break;
  case TOKEN_BACKSLASH:
    emitByte(OP_INT_DIVIDE);
    break;
  case TOKEN_STAR_STAR:
    emitByte(OP_POWER);
    break;

  default:
    return; // unreachable
  }
}

static void call(bool canAssign __attribute__((unused))) {
  const uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

/**
 * Parses a literal boolean or nil value.
 *
 * @param canAssign Whether the literal can be the target of an assignment
 * (always false).
 */
static void literal(bool canAssign __attribute__((unused))) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return; // unreachable
  }
}

/**
 * Parses a dot (.) property access expression.
 *
 * @param canAssign Whether the dot expression can be the target of an
 * assignment.
 */
static void dot(const bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
  const uint16_t name = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    if (name <= UINT8_MAX) {
      emitBytes(OP_SET_PROPERTY, (uint8_t)name);
    } else {
      emitByte(OP_SET_PROPERTY_16);
      emitBytes(((name >> 8) & 0xff), (name & 0xff));
    }
  } else if (match(TOKEN_LEFT_PAREN)) {
    const uint8_t argCount = argumentList();
    if (name <= UINT8_MAX) {
      emitBytes(OP_INVOKE, (uint8_t)name);
      emitByte(argCount);
    } else {
      emitByte(OP_INVOKE_16);
      emitBytes(((name >> 8) & 0xff), (name & 0xff));
      emitByte(argCount);
    }
  } else {
    if (name <= UINT8_MAX) {
      emitBytes(OP_GET_PROPERTY, (uint8_t)name);
    } else {
      emitByte(OP_GET_PROPERTY_16);
      emitBytes(((name >> 8) & 0xff), (name & 0xff));
    }
  }
}

/**
 * Parses an expression. Entry point for expression parsing.
 */
static void expression(void) { parsePrecedence(PREC_ASSIGNMENT); }

/**
 * Gets the compound opcode based on the set opcode and compound operation.
 *
 * This helper function is used for compound assignments (e.g., +=, -=, *=, /=).
 *
 * @param setOp The base set opcode (e.g., OP_SET_LOCAL, OP_SET_GLOBAL).
 * @param op The compound operation (e.g., COMPOUND_OP_PLUS, COMPOUND_OP_MINUS).
 * @return The corresponding compound opcode.
 */
static OpCode getCompoundOpcode(const OpCode setOp, const CompoundOp op) {
  switch (setOp) {
  case OP_SET_LOCAL: {
    switch (op) {
    case COMPOUND_OP_PLUS:
      return OP_SET_LOCAL_PLUS;
    case COMPOUND_OP_MINUS:
      return OP_SET_LOCAL_MINUS;
    case COMPOUND_OP_STAR:
      return OP_SET_LOCAL_STAR;
    case COMPOUND_OP_SLASH:
      return OP_SET_LOCAL_SLASH;
    case COMPOUND_OP_BACK_SLASH:
      return OP_SET_LOCAL_INT_DIVIDE;
    case COMPOUND_OP_PERCENT:
      return OP_SET_LOCAL_MODULUS;
    default: {
      compilerPanic(
          &parser,
          "Compiler Error: Failed to create bytecode for compound operation.",
          RUNTIME);
      break;
    }
    }
    break;
  }
  case OP_SET_UPVALUE: {
    switch (op) {
    case COMPOUND_OP_PLUS:
      return OP_SET_UPVALUE_PLUS;
    case COMPOUND_OP_MINUS:
      return OP_SET_UPVALUE_MINUS;
    case COMPOUND_OP_STAR:
      return OP_SET_UPVALUE_STAR;
    case COMPOUND_OP_SLASH:
      return OP_SET_UPVALUE_SLASH;
    case COMPOUND_OP_BACK_SLASH:
      return OP_SET_UPVALUE_INT_DIVIDE;
    case COMPOUND_OP_PERCENT:
      return OP_SET_UPVALUE_MODULUS;
    default: {
      compilerPanic(
          &parser,
          "Compiler Error: Failed to create bytecode for compound operation.",
          RUNTIME);
      break;
    }
    }
    break;
  }
  case OP_SET_GLOBAL: {
    switch (op) {
    case COMPOUND_OP_PLUS:
      return OP_SET_GLOBAL_PLUS;
    case COMPOUND_OP_MINUS:
      return OP_SET_GLOBAL_MINUS;
    case COMPOUND_OP_STAR:
      return OP_SET_GLOBAL_STAR;
    case COMPOUND_OP_SLASH:
      return OP_SET_GLOBAL_SLASH;
    case COMPOUND_OP_BACK_SLASH:
      return OP_SET_GLOBAL_INT_DIVIDE;
    case COMPOUND_OP_PERCENT:
      return OP_SET_GLOBAL_MODULUS;
    default: {
      compilerPanic(
          &parser,
          "Compiler Error: Failed to create bytecode for compound operation.",
          RUNTIME);
      break;
    }
    }
    break;
  }
  default:
    return setOp; // Should never happen
  }
  return setOp;
}

/**
 * Parses a named variable (local, upvalue, or global).
 *
 * @param name The token representing the variable name.
 * @param canAssign Whether the variable expression can be the target of an
 * assignment.
 */
static void namedVariable(Token name, const bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    const uint16_t globalArg = identifierConstant(&name);

    if (canAssign) {
      if (match(TOKEN_EQUAL)) {
        expression();
        if (globalArg <= UINT8_MAX) {
          emitBytes(OP_SET_GLOBAL, (uint8_t)globalArg);
        } else {
          emitByte(OP_SET_GLOBAL_16);
          emitBytes(globalArg >> 8 & 0xff, globalArg & 0xff);
        }
        return;
      }

      CompoundOp op;
      bool isCompoundAssignment = true;

      if (match(TOKEN_PLUS_EQUAL)) {
        op = COMPOUND_OP_PLUS;
      } else if (match(TOKEN_MINUS_EQUAL)) {
        op = COMPOUND_OP_MINUS;
      } else if (match(TOKEN_STAR_EQUAL)) {
        op = COMPOUND_OP_STAR;
      } else if (match(TOKEN_SLASH_EQUAL)) {
        op = COMPOUND_OP_SLASH;
      } else if (match(TOKEN_BACK_SLASH_EQUAL)) {
        op = COMPOUND_OP_BACK_SLASH;
      } else if (match(TOKEN_PERCENT_EQUAL)) {
        op = COMPOUND_OP_PERCENT;
      } else {
        isCompoundAssignment = false;
      }

      if (isCompoundAssignment) {
        expression();
        const OpCode compoundOp = getCompoundOpcode(OP_SET_GLOBAL, op);
        if (globalArg <= UINT8_MAX) {
          emitBytes(compoundOp, (uint8_t)globalArg);
        } else {
          if (globalArg <= UINT8_MAX) {
            emitBytes(OP_GET_GLOBAL, (uint8_t)globalArg);
          } else {
            emitByte(OP_GET_GLOBAL_16);
            emitBytes(((globalArg >> 8) & 0xff), (globalArg & 0xff));
          }
          switch (op) {
          case COMPOUND_OP_PLUS:
            emitByte(OP_ADD);
            break;
          case COMPOUND_OP_MINUS:
            emitByte(OP_SUBTRACT);
            break;
          case COMPOUND_OP_STAR:
            emitByte(OP_MULTIPLY);
            break;
          case COMPOUND_OP_SLASH:
            emitByte(OP_DIVIDE);
            break;
          case COMPOUND_OP_BACK_SLASH:
            emitByte(OP_INT_DIVIDE);
            break;
          case COMPOUND_OP_PERCENT:
            emitByte(OP_MODULUS);
            break;
          }
          if (globalArg <= UINT8_MAX) {
            emitBytes(OP_SET_GLOBAL, (uint8_t)globalArg);
          } else {
            emitByte(OP_SET_GLOBAL_16);
            emitBytes(globalArg >> 8 & 0xff, globalArg & 0xff);
          }
        }
        return;
      }
    }

    if (globalArg <= UINT8_MAX) {
      emitBytes(OP_GET_GLOBAL, (uint8_t)globalArg);
    } else {
      emitByte(OP_GET_GLOBAL_16);
      emitBytes(((globalArg >> 8) & 0xff), (globalArg & 0xff));
    }
    return;
  }

  if (canAssign) {
    if (match(TOKEN_EQUAL)) {
      expression();
      emitBytes(setOp, arg);
      return;
    }

    CompoundOp op;
    bool isCompoundAssignment = true;

    if (match(TOKEN_PLUS_EQUAL)) {
      op = COMPOUND_OP_PLUS;
    } else if (match(TOKEN_MINUS_EQUAL)) {
      op = COMPOUND_OP_MINUS;
    } else if (match(TOKEN_STAR_EQUAL)) {
      op = COMPOUND_OP_STAR;
    } else if (match(TOKEN_SLASH_EQUAL)) {
      op = COMPOUND_OP_SLASH;
    } else if (match(TOKEN_BACK_SLASH_EQUAL)) {
      op = COMPOUND_OP_BACK_SLASH;
    } else if (match(TOKEN_PERCENT_EQUAL)) {
      op = COMPOUND_OP_PERCENT;
    } else {
      isCompoundAssignment = false;
    }

    if (isCompoundAssignment) {
      expression();
      emitBytes(getCompoundOpcode(setOp, op), arg);
      return;
    }
  }
  emitBytes(getOp, arg);
}

void structInstance(const bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expected struct name to start initialization.");
  namedVariable(parser.previous, canAssign);
  if (!match(TOKEN_LEFT_BRACE)) {
    compilerPanic(&parser, "Expected '{' to start struct instance.", SYNTAX);
    return;
  }
  uint16_t fieldCount = 0;
  emitByte(OP_STRUCT_INSTANCE_START);

  if (!match(TOKEN_RIGHT_BRACE)) {
    do {
      if (fieldCount == UINT16_MAX) {
        compilerPanic(&parser, "Too many fields in struct initializer", SYNTAX);
        return;
      }
      consume(TOKEN_IDENTIFIER, "Expected field name.");
      // field_name: value
      ObjectString *fieldName = copyString(
          current->owner, parser.previous.start, parser.previous.length);
      consume(TOKEN_COLON, "Expected ':' after struct field name.");
      expression();
      const uint16_t fieldNameConstant = makeConstant(OBJECT_VAL(fieldName));
      if (fieldNameConstant <= UINT8_MAX) {
        emitBytes(OP_STRUCT_NAMED_FIELD, (uint8_t)fieldNameConstant);
      } else {
        emitByte(OP_STRUCT_NAMED_FIELD_16);
        emitBytes(fieldNameConstant >> 8 & 0xff, fieldNameConstant & 0xff);
      }

      fieldCount++;
    } while (match(TOKEN_COMMA));
  }

  if (fieldCount != 0) {
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after struct field list.");
  }
  emitByte(OP_STRUCT_INSTANCE_END);
}

static void variable(const bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void block(void) {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(const FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type, current->owner);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        compilerPanic(&parser, "Functions cannot have more than 255 arguments",
                      ARGUMENT_EXTENT);
      }
      const uint8_t constant = parseVariable("Expected parameter name");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjectFunction *function = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJECT_VAL(function)));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void fnDeclaration(void) {
  const uint16_t global = parseVariable("Expected function name");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void anonymousFunction(bool canAssign __attribute__((unused))) {
  Compiler compiler;
  initCompiler(&compiler, TYPE_ANONYMOUS, current->owner);
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expected '(' to start argument list");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        compilerPanic(&parser, "Functions cannot have more than 255 arguments",
                      ARGUMENT_EXTENT);
      }
      const uint8_t constant = parseVariable("Expected parameter name");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
  consume(TOKEN_LEFT_BRACE, "Expected '{' before function body");
  block();
  ObjectFunction *function = endCompiler();

  const uint16_t constantIndex = makeConstant(OBJECT_VAL(function));
  if (constantIndex > UINT8_MAX) {
    emitByte(OP_ANON_FUNCTION_16);
    emitBytes(constantIndex >> 8 & 0xff, constantIndex & 0xff);
  } else {
    emitBytes(OP_ANON_FUNCTION, (uint8_t)constantIndex);
  }

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void createArray(const OpCode creationOpCode, const char *typeName) {
  uint16_t elementCount = 0;

  if (!match(TOKEN_RIGHT_SQUARE)) {
    do {
      expression();
      if (elementCount >= UINT16_MAX) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Too many elements in %s literal.",
                 typeName);
        compilerPanic(&parser, buffer, COLLECTION_EXTENT);
      }
      elementCount++;
    } while (match(TOKEN_COMMA));
    consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array elements");
  }
  emitByte(creationOpCode);
  emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

static void arrayLiteral(bool canAssign __attribute__((unused))) {
  createArray(OP_ARRAY, "array");
}

static void staticArrayLiteral(bool canAssign __attribute__((unused))) {
  createArray(OP_STATIC_ARRAY, "static array");
}

static void createTable(const OpCode creationOpCode, const char *typeName) {
  uint16_t elementCount = 0;

  if (!match(TOKEN_RIGHT_BRACE)) {
    do {
      expression();
      consume(TOKEN_COLON, "Expected ':' after <table> key");
      expression();
      if (elementCount >= UINT16_MAX) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Too many elements in %s literal.",
                 typeName);
        compilerPanic(&parser, buffer, COLLECTION_EXTENT);
      }
      elementCount++;
    } while (match(TOKEN_COMMA));
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after table elements");
  }
  emitByte(creationOpCode);
  emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

static void tableLiteral(bool canAssign __attribute__((unused))) {
  createTable(OP_TABLE, "table");
}

static void staticTableLiteral(bool canAssign __attribute__((unused))) {
  createTable(OP_STATIC_TABLE, "static table");
}

/**
 * Parses a collection index access expression (e.g., array[index]).
 *
 * @param canAssign Whether the collection index expression can be the target of
 * an assignment.
 */
static void collectionIndex(const bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_SQUARE, "Expected ']' after index");

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitByte(OP_SET_COLLECTION);
  } else {
    emitByte(OP_GET_COLLECTION);
  }
}

static void varDeclaration(void) {
  const uint16_t global = parseVariable("Expected Variable Name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
  defineVariable(global);
}

static void expressionStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' after expression");
  emitByte(OP_POP);
}

static void whileStatement(void) {
  beginScope();
  const int loopStart = currentChunk()->count;

  pushLoopContext(LOOP_WHILE, loopStart);

  expression();
  const int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  statement();

  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);

  popLoopContext();
  endScope();
}

static void forStatement(void) {
  beginScope();

  if (match(TOKEN_SEMICOLON)) {
    // no initializer
  } else if (match(TOKEN_LET)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int exitJump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after loop condition");

    // Jump out of the loop if the condition is false
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // condition
  }

  const int bodyJump = emitJump(OP_JUMP);
  const int incrementStart = currentChunk()->count;

  pushLoopContext(LOOP_FOR, incrementStart);

  expression();
  emitByte(OP_POP);

  emitLoop(
      loopStart); // main loop that takes us back to the top of the for loop
  loopStart = incrementStart;
  patchJump(bodyJump);

  statement();
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }

  popLoopContext();
  endScope();
}

static void ifStatement(void) {
  expression();
  const int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  const int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE))
    statement();
  patchJump(elseJump);
}

static void returnStatement(void) {
  if (current->type == TYPE_SCRIPT) {
    compilerPanic(&parser, "Cannot use <return> outside of a function", SYNTAX);
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after return value");
    emitByte(OP_RETURN);
  }
}

static void useStatement(void) {
  bool hasParen = false;
  if (parser.current.type == TOKEN_LEFT_PAREN) {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after use statement");
    hasParen = true;
  }

  uint8_t nameCount = 0;
  uint8_t names[UINT8_MAX];
  uint8_t aliases[UINT8_MAX];
  bool aliasPresence[UINT8_MAX];

  for (int i = 0; i < UINT8_MAX; i++) {
    names[i] = 0;
    aliases[i] = 0;
    aliasPresence[i] = false;
  }

  do {
    if (nameCount >= UINT8_MAX) {
      compilerPanic(&parser,
                    "Cannot import more than 255 names from another module.",
                    IMPORT_EXTENT);
    }
    consume(TOKEN_IDENTIFIER, "Expected name to import from module");

    uint8_t name;
    if (parser.current.type == TOKEN_AS) {
      name = identifierConstant(&parser.previous);
      consume(TOKEN_AS, "Expected 'as' keyword.");
      consume(TOKEN_IDENTIFIER,
              "Expected name to alias import from external module.");
      const uint8_t alias = identifierConstant(&parser.previous);
      aliases[nameCount] = alias;
      aliasPresence[nameCount] = true;
    } else {
      name = identifierConstant(&parser.previous);
    }

    names[nameCount] = name;
    nameCount++;
  } while (match(TOKEN_COMMA));
  if (hasParen) {
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after last imported name.");
  }

  consume(TOKEN_FROM, "Expected 'from' after 'use' statement.");
  consume(TOKEN_STRING, "Expected string literal for module name");

  bool isNative = false;
  if (memcmp(parser.previous.start, "\"crux:", 6) == 0) {
    isNative = true;
  }

  uint8_t module;
  if (isNative) {
    module = makeConstant(
        OBJECT_VAL(copyString(current->owner, parser.previous.start + 6,
                              parser.previous.length - 7)));
    emitBytes(OP_USE_NATIVE, nameCount);
    for (uint8_t i = 0; i < nameCount; i++) {
      emitByte(names[i]);
    }
    for (uint8_t i = 0; i < nameCount; i++) {
      if (aliasPresence[i]) {
        emitByte(aliases[i]);
      } else {
        emitByte(names[i]);
      }
    }
    emitByte(module);
  } else {
    module = makeConstant(
        OBJECT_VAL(copyString(current->owner, parser.previous.start + 1,
                              parser.previous.length - 2)));
    emitBytes(OP_USE_MODULE, module);

    emitBytes(OP_FINISH_USE, nameCount);
    for (uint8_t i = 0; i < nameCount; i++) {
      emitByte(names[i]);
    }
    for (uint8_t i = 0; i < nameCount; i++) {
      if (aliasPresence[i]) {
        emitByte(aliases[i]);
      } else {
        emitByte(names[i]);
      }
    }
  }

  consume(TOKEN_SEMICOLON, "Expected semicolon after import statement.");
}

static void structDeclaration(void) {
  consume(TOKEN_IDENTIFIER, "Expected class name");
  const Token structName = parser.previous;
  GC_PROTECT_START(current->owner->currentModuleRecord);
  ObjectString *structNameString =
      copyString(current->owner, structName.start, structName.length);
  GC_PROTECT(current->owner->currentModuleRecord, OBJECT_VAL(structNameString));
  const uint16_t nameConstant = identifierConstant(&structName);
  ObjectStruct *structObject = newStructType(current->owner, structNameString);
  GC_PROTECT(current->owner->currentModuleRecord, OBJECT_VAL(structObject));

  declareVariable();

  const uint16_t structConstant = makeConstant(OBJECT_VAL(structObject));
  if (structConstant <= UINT8_MAX) {
    emitBytes(OP_STRUCT, (uint8_t)structConstant);
  } else {
    emitByte(OP_STRUCT_16);
    emitBytes(((structConstant >> 8) & 0xff), (structConstant & 0xff));
  }

  defineVariable(nameConstant);

  consume(TOKEN_LEFT_BRACE, "Expected '{' before struct body");
  int fieldCount = 0;

  if (!match(TOKEN_RIGHT_BRACE)) {
    do {
      if (fieldCount >= UINT16_MAX) {
        compilerPanic(&parser, "Too many fields in struct", SYNTAX);
        break;
      }

      consume(TOKEN_IDENTIFIER, "Expected field name");
      ObjectString *fieldName = copyString(
          current->owner, parser.previous.start, parser.previous.length);

      GC_PROTECT(current->owner->currentModuleRecord, OBJECT_VAL(fieldName));

      Value fieldNameCheck;
      if (tableGet(&structObject->fields, fieldName, &fieldNameCheck)) {
        compilerPanic(&parser, "Duplicate field name in struct declaration",
                      SYNTAX);
        break;
      }

      tableSet(current->owner, &structObject->fields, fieldName,
               INT_VAL(fieldCount));
      fieldCount++;
    } while (match(TOKEN_COMMA));
  }
  if (fieldCount != 0) {
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after struct body");
  }
  GC_PROTECT_END(current->owner->currentModuleRecord);
}

static void resultUnwrap(bool canAssign __attribute__((unused))) {
  emitByte(OP_UNWRAP);
}

static double getFloatFromSource(const Token *token) {
  char *end;
  errno = 0;

  const char *numberStart = token->start;
  const double number = strtod(numberStart, &end);

  if (end == numberStart) {
    compilerPanic(&parser, "Failed to form number", SYNTAX);
    return 0;
  }
  if (errno == ERANGE) {
    return number;
  }
  return number;
}

static int32_t getIntFromSource(const Token *token) {
  char *end;
  errno = 0;

  const char *numberStart = token->start;
  const int32_t number = (int32_t)strtod(numberStart, &end);

  if (end == numberStart) {
    compilerPanic(&parser, "Failed to form number", SYNTAX);
    return 0;
  }
  if (errno == ERANGE) {
    return number;
  }
  return number;
}

/**
 * Synchronizes the parser after encountering a syntax error.
 *
 * Discards tokens until a statement boundary is found to minimize cascading
 * errors.
 */
static void synchronize(void) {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_STRUCT:
    case TOKEN_PUB:
    case TOKEN_FN:
    case TOKEN_LET:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_RETURN:
      return;
    default:;
    }
    advance();
  }
}

static void publicDeclaration(void) {
  if (current->scopeDepth > 0) {
    compilerPanic(&parser, "Cannot declare public members in a local scope.",
                  SYNTAX);
  }
  emitByte(OP_PUB);
  if (match(TOKEN_FN)) {
    fnDeclaration();
  } else if (match(TOKEN_LET)) {
    varDeclaration();
  } else if (match(TOKEN_STRUCT)) {
    structDeclaration();
  } else {
    compilerPanic(&parser, "Expected 'fn', 'let', or 'struct' after 'pub'.",
                  SYNTAX);
  }
}

static void beginMatchScope(void) {
  if (current->matchDepth > 0) {
    compilerPanic(&parser, "Nesting match statements is not allowed.", SYNTAX);
  }
  current->matchDepth++;
}

static void endMatchScope(void) { current->matchDepth--; }

static void giveStatement(void) {
  if (current->matchDepth == 0) {
    compilerPanic(&parser, "'give' can only be used inside a match expression.",
                  SYNTAX);
  }

  if (match(TOKEN_SEMICOLON)) {
    emitByte(OP_NIL);
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after give statement.");
  }

  emitByte(OP_GIVE);
}

/**
 * Parses a match expression.
 */
static void matchExpression(bool canAssign __attribute__((unused))) {
  beginMatchScope();
  expression(); // compile match target
  consume(TOKEN_LEFT_BRACE, "Expected '{' after match target.");

  int *endJumps = ALLOCATE(current->owner, int, 8);
  int jumpCount = 0;
  int jumpCapacity = 8;

  emitByte(OP_MATCH);
  bool hasDefault = false;
  bool hasOkPattern = false;
  bool hasErrPattern = false;
  uint8_t bindingSlot = UINT8_MAX;
  bool hasBinding = false;

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    int jumpIfNotMatch = -1;
    bindingSlot = UINT8_MAX;
    hasBinding = false;

    if (match(TOKEN_DEFAULT)) {
      if (hasDefault) {
        compilerPanic(&parser, "Cannot have multiple default patterns.",
                      SYNTAX);
      }
      hasDefault = true;
    } else if (match(TOKEN_OK)) {
      if (hasOkPattern) {
        compilerPanic(&parser, "Cannot have multiple 'Ok' patterns.", SYNTAX);
      }
      hasOkPattern = true;
      jumpIfNotMatch = emitJump(OP_RESULT_MATCH_OK);

      if (match(TOKEN_LEFT_PAREN)) {
        beginScope();
        hasBinding = true;
        consume(TOKEN_IDENTIFIER, "Expected identifier after 'Ok' pattern.");
        declareVariable();
        bindingSlot = current->localCount - 1;
        markInitialized();
        consume(TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
      }
    } else if (match(TOKEN_ERR)) {
      if (hasErrPattern) {
        compilerPanic(&parser, "Cannot have multiple 'Err' patterns.", SYNTAX);
      }
      hasErrPattern = true;
      jumpIfNotMatch = emitJump(OP_RESULT_MATCH_ERR);

      if (match(TOKEN_LEFT_PAREN)) {
        beginScope();
        hasBinding = true;
        consume(TOKEN_IDENTIFIER, "Expected identifier after 'Err' pattern.");
        declareVariable();
        bindingSlot = current->localCount - 1;
        markInitialized();
        consume(TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
      }
    } else {
      expression();
      jumpIfNotMatch = emitJump(OP_MATCH_JUMP);
    }

    consume(TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

    if (bindingSlot != UINT8_MAX) {
      emitBytes(OP_RESULT_BIND, bindingSlot);
    }

    // Compile match arm body

    if (match(TOKEN_LEFT_BRACE)) {
      block();
    } else if (match(TOKEN_GIVE)) {
      if (match(TOKEN_SEMICOLON)) {
        emitByte(OP_NIL);
      } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after give expression.");
      }
      emitByte(OP_GIVE);
    } else {
      expression();
      consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
    }

    if (hasBinding) {
      endScope();
    }

    if (jumpCount + 1 > jumpCapacity) {
      const int oldCapacity = jumpCapacity;
      jumpCapacity = GROW_CAPACITY(oldCapacity);
      endJumps =
          GROW_ARRAY(current->owner, int, endJumps, oldCapacity, jumpCapacity);
    }

    endJumps[jumpCount++] = emitJump(OP_JUMP);

    if (jumpIfNotMatch != -1) {
      patchJump(jumpIfNotMatch);
    }
  }

  if (jumpCount == 0) {
    compilerPanic(&parser, "'match' expression must have at least one arm.",
                  SYNTAX);
  }

  if (hasOkPattern || hasErrPattern) {
    if (!hasDefault && !(hasOkPattern && hasErrPattern)) {
      compilerPanic(&parser,
                    "Result 'match' must have both 'Ok' and 'Err' patterns, or "
                    "include a default case.",
                    SYNTAX);
    }
  } else if (!hasDefault) {
    compilerPanic(&parser,
                  "'match' expression must have default case 'default'.",
                  SYNTAX);
  }

  for (int i = 0; i < jumpCount; i++) {
    patchJump(endJumps[i]);
  }

  emitByte(OP_MATCH_END);

  FREE_ARRAY(current->owner, int, endJumps, jumpCapacity);
  consume(TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
  endMatchScope();
}

static void continueStatement(void) {
  consume(TOKEN_SEMICOLON, "Expected ';' after 'continue',");
  const int continueTarget = getCurrentContinueTarget();
  if (continueTarget == -1) {
    return;
  }
  const LoopContext *loopContext = &current->loopStack[current->loopDepth - 1];
  cleanupLocalsToDepth(loopContext->scopeDepth);
  emitLoop(continueTarget);
}

static void breakStatement(void) {
  consume(TOKEN_SEMICOLON, "Expected ';' after 'break'.");
  if (current->loopDepth <= 0) {
    compilerPanic(&parser, "Cannot use 'break' outside of a loop.", SYNTAX);
    return;
  }
  const LoopContext *loopContext = &current->loopStack[current->loopDepth - 1];
  cleanupLocalsToDepth(loopContext->scopeDepth);
  addBreakJump(emitJump(OP_JUMP));
}

/**
 * Parses a declaration, which can be a variable, function, or class
 * declaration.
 */
static void declaration(void) {
  if (match(TOKEN_LET)) {
    varDeclaration();
  } else if (match(TOKEN_FN)) {
    fnDeclaration();
  } else if (match(TOKEN_STRUCT)) {
    structDeclaration();
  } else if (match(TOKEN_PUB)) {
    publicDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

/**
 * Parses a statement.
 */
static void statement(void) {
  if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_USE)) {
    useStatement();
  } else if (match(TOKEN_GIVE)) {
    giveStatement();
  } else if (match(TOKEN_BREAK)) {
    breakStatement();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else {
    expressionStatement();
  }
}

/**
 * Parses a grouping expression.
 *
 * @param canAssign Whether the grouping expression can be the target of an
 * assignment.
 */
static void grouping(bool canAssign __attribute__((unused))) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool canAssign __attribute__((unused))) {
  char *end;
  errno = 0;

  const char *numberStart = parser.previous.start;
  const double number = strtod(numberStart, &end);

  if (end == numberStart) {
    compilerPanic(&parser, "Failed to form number", SYNTAX);
    return;
  }
  if (errno == ERANGE) {
    emitConstant(FLOAT_VAL(number));
    return;
  }
  bool hasDecimalNotation = false;
  for (const char *c = numberStart; c < end; c++) {
    if (*c == '.' || *c == 'e' || *c == 'E') {
      hasDecimalNotation = true;
      break;
    }
  }
  if (hasDecimalNotation) {
    emitConstant(FLOAT_VAL(number));
  } else {
    const int32_t integer = (int32_t)number;
    if ((double)integer == number) {
      emitConstant(INT_VAL(integer));
    } else {
      emitConstant(FLOAT_VAL(number));
    }
  }
}

/**
 * Processes an escape sequence within a string literal.
 *
 * @param escape The escape character (e.g., 'n', 't', '\\').
 * @param hasError A pointer to a boolean to indicate if an error occurred
 * during processing.
 * @return The processed escaped character, or '\0' if an error occurred.
 */
static char processEscapeSequence(const char escape, bool *hasError) {
  *hasError = false;
  switch (escape) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '\\':
    return '\\';
  case '"':
    return '"';
  case '\'':
    return '\'';
  case '0':
    return ' ';
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'v':
    return '\v';
  default: {
    *hasError = true;
    return '\0';
  }
  }
}

/**
 * Parses a string literal.
 *
 * @param canAssign Whether the string literal can be the target of an
 * assignment.
 */
static void string(bool canAssign __attribute__((unused))) {
  char *processed = ALLOCATE(current->owner, char, parser.previous.length);

  if (processed == NULL) {
    compilerPanic(&parser, "Cannot allocate memory for string expression.",
                  MEMORY);
    return;
  }

  int processedLength = 0;
  const char *src = (char *)parser.previous.start + 1;
  const int srcLength = parser.previous.length - 2;

  if (srcLength == 0) {
    ObjectString *string = copyString(current->owner, "", 0);
    emitConstant(OBJECT_VAL(string));
    return;
  }

  for (int i = 0; i < srcLength; i++) {
    if (src[i] == '\\') {
      if (i + 1 >= srcLength) {
        compilerPanic(&parser, "Unterminated escape sequence at end of string",
                      SYNTAX);
        FREE_ARRAY(current->owner, char, processed, parser.previous.length);
        return;
      }

      bool error;
      const char escaped = processEscapeSequence(src[i + 1], &error);
      if (error) {
        char errorMessage[64];
        snprintf(errorMessage, 64, "Unexpected escape sequence '\\%c'",
                 src[i + 1]);
        compilerPanic(&parser, errorMessage, SYNTAX);
        FREE_ARRAY(current->owner, char, processed, parser.previous.length);
        return;
      }

      processed[processedLength++] = escaped;
      i++;
    } else {
      processed[processedLength++] = src[i];
    }
  }

  char *temp = GROW_ARRAY(current->owner, char, processed,
                          parser.previous.length, processedLength + 1);
  if (temp == NULL) {
    compilerPanic(&parser, "Cannot allocate memory for string expression.",
                  MEMORY);
    FREE_ARRAY(current->owner, char, processed, parser.previous.length);
    return;
  }
  processed = temp;
  processed[processedLength] = '\0';
  ObjectString *string = takeString(current->owner, processed, processedLength);
  emitConstant(OBJECT_VAL(string));
}

/**
 * Parses a unary operator expression.
 *
 * @param canAssign Whether the unary expression can be the target of an
 * assignment.
 */
static void unary(bool canAssign __attribute__((unused))) {
  const CruxTokenType operatorType = parser.previous.type;

  // compile the operand
  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_NOT:
    emitByte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return; // unreachable
  }
}

static void typeofExpression(bool canAssign __attribute__((unused))) {
  parsePrecedence(PREC_UNARY);
  emitByte(OP_TYPEOF);
}

/**
 * @brief Parse rules for each token type.
 *
 * Defines prefix and infix parsing functions and precedence for each token
 * type.
 */
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, NULL, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {tableLiteral, NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_SQUARE] = {arrayLiteral, collectionIndex, NULL, PREC_CALL},
    [TOKEN_RIGHT_SQUARE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, NULL, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_BACKSLASH] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_STAR_STAR] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_PERCENT] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_LEFT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
    [TOKEN_RIGHT_SHIFT] = {NULL, binary, NULL, PREC_SHIFT},
    [TOKEN_NOT] = {unary, NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
    [TOKEN_INT] = {number, NULL, NULL, PREC_NONE},
    [TOKEN_FLOAT] = {number, NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_BREAK] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, NULL, PREC_AND},
    [TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_FN] = {anonymousFunction, NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, NULL, PREC_OR},
    [TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_USE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_FROM] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_PUB] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_DEFAULT] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_ARROW] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_MATCH] = {matchExpression, NULL, NULL, PREC_PRIMARY},
    [TOKEN_TYPEOF] = {typeofExpression, NULL, NULL, PREC_UNARY},
    [TOKEN_DOLLAR_LEFT_CURLY] = {staticTableLiteral, NULL, NULL, PREC_NONE},
    [TOKEN_DOLLAR_LEFT_SQUARE] = {staticArrayLiteral, NULL, NULL, PREC_NONE},
    [TOKEN_STRUCT] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_NEW] = {structInstance, NULL, NULL, PREC_UNARY},
    [TOKEN_EOF] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_QUESTION_MARK] = {NULL, NULL, resultUnwrap, PREC_CALL},
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parsePrecedence(const Precedence precedence) {
  advance();
  const ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    compilerPanic(&parser, "Expected expression.", SYNTAX);
    return;
  }

  const bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    const ParseRule *rule = getRule(parser.previous.type);
    if (rule->infix != NULL) {
      rule->infix(canAssign);
    } else if (rule->postfix != NULL) {
      rule->postfix(canAssign);
    }
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    compilerPanic(&parser, "Invalid Assignment Target", SYNTAX);
  }
}

/**
 * Retrieves the parse rule for a given token type.
 *
 * @param type The token type to get the rule for.
 * @return A pointer to the ParseRule struct for the given token type.
 */
static ParseRule *getRule(const CruxTokenType type) {
  return &rules[type]; // Returns the rule at the given index
}

/**
 * Compiles source code into bytecode.
 *
 * @param vm The virtual machine instance.
 * @param source The source code string to compile.
 * @return A pointer to the compiled function object if compilation succeeds,
 * NULL otherwise.
 */
ObjectFunction *compile(VM *vm, char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT, vm);

  parser.hadError = false;
  parser.panicMode = false;
  parser.source = source;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjectFunction *function = endCompiler();
  if (function != NULL) {
    function->moduleRecord = vm->currentModuleRecord;
  }
  return parser.hadError ? NULL : function;
}

/**
 * Marks compiler-related objects as reachable for garbage collection.
 *
 * Traverses the compiler chain and marks the function objects associated with
 * each compiler.
 *
 * @param vm The virtual machine instance.
 */
void markCompilerRoots(VM *vm) {
  const Compiler *compiler = current;
  while (compiler != NULL) {
    markObject(vm, (Object *)compiler->function);
    compiler = (Compiler *)compiler->enclosing;
  }
}
