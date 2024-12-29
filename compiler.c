#include "compiler.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// Make these two non globals
Parser parser;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;
Chunk *compilingChunk;

#include "panic.h"
static void expression();

static void parsePrecedence(Precedence precedence);

static ParseRule *getRule(TokenType type);

static void binary(bool canAssign);

static void unary(bool canAssign);

static void grouping(bool canAssign);

static void number(bool canAssign);

static void statement();

static void declaration();

static Chunk *currentChunk() { return &current->function->chunk; }

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR)
			break;
		compilerPanic(&parser, parser.current.start, SYNTAX);
	}
}

/**
 * Reads the next token. Validates that the token has the expected type. If not
 * reports an error.
 */
static void consume(TokenType type, const char *message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	compilerPanic(&parser, message, SYNTAX);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

static void emitByte( uint8_t byte) { writeChunk(current->owner, currentChunk(), byte, parser.previous.line); }

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte( byte1);
	emitByte( byte2);
}

static void emitLoop(int loopStart) {
	emitByte( OP_LOOP);
	int offset = currentChunk()->count - loopStart + 2; // +2 takes into account the size of the OP_LOOP
	if (offset > UINT16_MAX) {
		compilerPanic(&parser, "Loop body too large.", LOOP_EXTENT);
	}
	emitBytes( ((offset >> 8) & 0xff), (offset & 0xff));
}

static int emitJump(uint8_t instruction) {
	emitByte( instruction);
	emitBytes(0xff, 0xff);
	return currentChunk()->count - 2;
}

static void emitReturn() {
	if (current->type == TYPE_INITIALIZER) {
		emitBytes(OP_GET_LOCAL, 0);
	} else {
		emitByte(OP_NIL);
	}
	emitBytes(OP_RETURN, 0);
}

static uint8_t makeConstant(Value value) {
	int constant = addConstant(current->owner, currentChunk(), value);
	// Add constant adds the given value to the end of the constant table and
	// returns the index.
	if (constant > UINT8_MAX) {
		// Only 256 constants can be stored and loaded in a chunk.
		// TODO: Add new instruction 'OP_CONSTANT_16' to store two byte operands
		// that can handle more constants when needed
		compilerPanic(&parser, "Too many constants in one chunk.", LIMIT);
		return 0;
	}
	return (uint8_t) constant;
}

static void emitConstant(Value value) { emitBytes(OP_CONSTANT, makeConstant(value)); }

static void patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself
	int jump = currentChunk()->count - offset - 2;
	if (jump > UINT16_MAX) {
		compilerPanic(&parser, "Too much code to jump over.", BRANCH_EXTENT);
	}
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

static void markPublic(Compiler* compiler, ObjectString* name) { 
	if (compiler->module != NULL) {
		// actual name value will be set at runtime
		tableSet(compiler->owner, &compiler->module->publicNames, name, NIL_VAL);
	}
}

static void initCompiler(Compiler *compiler, FunctionType type, VM* vm) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->owner = vm;

	if (compiler->enclosing == NULL) {
		if (compiler->owner->currentScriptName == NULL) {
			compiler->owner->currentScriptName = copyString(current->owner, "<script>", strlen("<script>"));
		}
		compiler->module = newModule(compiler->owner, compiler->owner->currentScriptName);
	}else {
		compiler->module = ((Compiler*)(compiler->enclosing))->module;
	}


	compiler->function = newFunction(compiler->owner);
	current = compiler;

	if (type == TYPE_ANONYMOUS) {
		current->function->name = copyString(current->owner, "anonymous", 9);
	} else if (type != TYPE_SCRIPT) {
		current->function->name = copyString(current->owner, parser.previous.start, parser.previous.length);
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

static uint8_t identifierConstant(Token *name) {
	return makeConstant(OBJECT_VAL(copyString(current->owner, name->start, name->length)));
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (current->locals[current->localCount - 1].isCaptured) {
			emitByte(OP_CLOSE_UPVALUE);
		} else {
			emitByte(OP_POP);
		}
		current->localCount--;
	}
}

static bool identifiersEqual(Token *a, Token *b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local *local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				compilerPanic(&parser, "Cannot read local variable in its own initializer", NAME);
			}
			return i;
		}
	}
	return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
	int upvalueCount = compiler->function->upvalueCount;

	for (int i = 0; i < upvalueCount; i++) {
		Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}

	if (upvalueCount >= UINT8_COUNT) {
		compilerPanic(&parser, "Too many closure variables in function.", CLOSURE_EXTENT);
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
	if (compiler->enclosing == NULL)
		return -1;

	int local = resolveLocal(compiler->enclosing, name);
	if (local != -1) {
		((Compiler *) compiler->enclosing)->locals[local].isCaptured = true; //
		return addUpvalue(compiler, (uint8_t) local, true);
	}

	int upValue = resolveUpvalue(compiler->enclosing, name);
	if (upValue != -1) {
		return addUpvalue(compiler, (uint8_t) upValue, false);
	}

	return -1;
}

static void addLocal(Token name) {
	if (current->localCount == UINT8_COUNT) {
		compilerPanic(&parser, "Too many local variables in function.", LOCAL_EXTENT);
		return;
	}

	Local *local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

static void declareVariable() {
	if (current->scopeDepth == 0)
		return;

	Token *name = &parser.previous;

	for (int i = current->localCount - 1; i >= 0; i--) {
		Local *local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}
		if (identifiersEqual(name, &local->name)) {
			compilerPanic(&parser, "Cannot redefine variable in the same scope", NAME);
		}
	}

	addLocal(*name);
}

static void markInitialized() {
	if (current->scopeDepth == 0)
		return;
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static uint8_t parseVariable(const char *errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	declareVariable();
	if (current->scopeDepth > 0)
		return 0;
	return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (argCount == 255) {
				compilerPanic(&parser, "Cannot have more than 255 arguments.", ARGUMENT_EXTENT);
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	return argCount;
}

static void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	patchJump(endJump);
}

static void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);
	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

static ObjectFunction *endCompiler() {
	emitReturn();
	ObjectFunction *function = current->function;
	function->module = current->module;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif
	current = current->enclosing;
	return function;
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	ParseRule *rule = getRule(operatorType);
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
		default:
			return; // unreachable
	}
}

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) {
	switch (parser.previous.type) {
		case TOKEN_FALSE:
			emitByte(OP_FALSE);
			break;
		case TOKEN_NIL:
			emitByte(OP_NIL);
			break;
		case TOKEN_TRUE:
			emitByte(OP_TRUE);
		default:
			return; // unreachable
	}
}

static void dot(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
	uint8_t name = identifierConstant(&parser.previous);

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(OP_SET_PROPERTY, name);
	} else if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		emitBytes(OP_INVOKE, name);
		emitByte(argCount);
	} else {
		emitBytes(OP_GET_PROPERTY, name);
	}
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }


static OpCode getCompoundOpcode(OpCode setOp, CompoundOp op) {
	switch (setOp) {
		case OP_SET_LOCAL:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_LOCAL_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_LOCAL_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_LOCAL_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_LOCAL_SLASH;
			}
		case OP_SET_UPVALUE:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_UPVALUE_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_UPVALUE_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_UPVALUE_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_UPVALUE_SLASH;
			}
		case OP_SET_GLOBAL:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_GLOBAL_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_GLOBAL_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_GLOBAL_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_GLOBAL_SLASH;
			}
		default:
			return setOp; // Should never happen
	}
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = resolveUpvalue(current, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
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

static void variable(bool canAssign) { namedVariable(parser.previous, canAssign); }

static Token syntheticToken(const char *text) {
	Token token;
	token.start = text;
	token.length = (int) strlen(text);
	return token;
}

static void super_(bool canAssign) {
	if (currentClass == NULL) {
		compilerPanic(&parser, "Cannot use 'super' outside of a class", NAME);
	} else if (!currentClass->hasSuperclass) {
		compilerPanic(&parser, "Cannot use 'super' in a class that does not have a superclass", NAME);
	}

	consume(TOKEN_DOT, "Expected '.' after 'super'.");
	consume(TOKEN_IDENTIFIER, "Expected superclass method name.");
	uint8_t name = identifierConstant(&parser.previous);
	namedVariable(syntheticToken("self"), false);
	namedVariable(syntheticToken("super"), false);

	if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_SUPER_INVOKE, name);
		emitByte(argCount);
	} else {
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_GET_SUPER, name);
	}
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type, current->owner);
	beginScope();

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				compilerPanic(&parser, "Functions cannot have more than 255 arguments", ARGUMENT_EXTENT);
			}
			uint8_t constant = parseVariable("Expected parameter name");
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

static void method() {
	consume(TOKEN_FN, "Expected 'fn' to start a method declaration.");
	consume(TOKEN_IDENTIFIER, "Expected method name.");
	uint8_t constant = identifierConstant(&parser.previous);

	FunctionType type = TYPE_METHOD;

	if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
		type = TYPE_INITIALIZER;
	}

	function(type);

	emitBytes(OP_METHOD, constant);
}

static void classDeclaration() {
	consume(TOKEN_IDENTIFIER, "Expected class name");
	Token className = parser.previous;
	uint8_t nameConstant = identifierConstant(&parser.previous);
	declareVariable();

	emitBytes(OP_CLASS, nameConstant);
	defineVariable(nameConstant);

	ClassCompiler classCompiler;
	classCompiler.enclosing = currentClass;
	classCompiler.hasSuperclass = false;
	currentClass = &classCompiler;

	if (match(TOKEN_LESS)) {
		consume(TOKEN_IDENTIFIER, "Expected super class name after '<'.");
		variable(false);

		if (identifiersEqual(&className, &parser.previous)) {
			compilerPanic(&parser, "A class cannot inherit from itself", NAME);
		}

		beginScope();
		addLocal(syntheticToken("super"));
		defineVariable(0);

		namedVariable(className, false);
		emitByte(OP_INHERIT);
		classCompiler.hasSuperclass = true;
	}

	namedVariable(className, false);

	consume(TOKEN_LEFT_BRACE, "Expected '{' before class body");

	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		method();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after class body");
	emitByte(OP_POP);

	if (classCompiler.hasSuperclass) {
		endScope();
	}

	currentClass = classCompiler.enclosing;
}

static void fnDeclaration() {
	uint8_t global = parseVariable("Expected function name");
	markInitialized();
	function(TYPE_FUNCTION);
	defineVariable(global);
}

static void anonymousFunction(bool canAssign) {
	Compiler compiler;
	initCompiler(&compiler, TYPE_ANONYMOUS, current->owner);
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expected '(' to start argument list");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				compilerPanic(&parser, "Functions cannot have more than 255 arguments", ARGUMENT_EXTENT);
			}
			uint8_t constant = parseVariable("Expected parameter name");
			defineVariable(constant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	consume(TOKEN_LEFT_BRACE, "Expected '{' before function body");
	block();
	ObjectFunction *function = endCompiler();
	emitBytes(OP_ANON_FUNCTION, makeConstant(OBJECT_VAL(function)));

	for (int i = 0; i < function->upvalueCount; i++) {
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}

static void arrayLiteral(bool canAssign) {
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_SQUARE)) {
		do {
			expression();
			if (elementCount >= UINT16_MAX) {
				compilerPanic(&parser, "Too many elements in array literal", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array elements");
	}
	emitByte(OP_ARRAY);
	emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

static void tableLiteral(bool canAssign) {
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			consume(TOKEN_COLON, "Expected ':' after <table> key");
			expression();
			if (elementCount >= UINT16_MAX) {
				compilerPanic(&parser, "Too many elements in table literal", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after table elements");
	}
	emitByte(OP_TABLE);
	emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

static void collectionIndex(bool canAssign) {
	expression(); // array index
	consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array index");

	if (match(TOKEN_EQUAL)) {
		expression();
		emitByte(OP_SET_COLLECTION);
	} else {
		emitByte(OP_GET_COLLECTION);
	}
}

static void varDeclaration() {
	uint8_t global = parseVariable("Expected variable name");

	if (match(TOKEN_COMMA)) {
		uint8_t variableCount = 1;
		uint8_t variables[255];
		variables[0] = global;

		markInitialized();
		do {
			if (variableCount >= 255) {
				compilerPanic(&parser, "Cannot declare more than 255 variables at one time.", VARIABLE_EXTENT);
				return;
			}
			variables[variableCount] = parseVariable("Expected variable name");
			markInitialized();
			variableCount++;
		} while (match(TOKEN_COMMA));

		if (match(TOKEN_EQUAL)) {
			int defined = 0;
			do {
				if (defined >= variableCount) {
					compilerPanic(&parser, "Too many values given for variable declaration.", VARIABLE_DECLARATION_MISMATCH);
					return;
				}
				expression();
				defined++;
			} while (match(TOKEN_COMMA));

		} else {
			for (int i = 0; i < variableCount; i++) {
				emitByte(OP_NIL);
			}
		}

		emitBytes(OP_UNPACK_TUPLE, variableCount);
		emitByte(current->scopeDepth);

		for (uint8_t i = 0; i < variableCount; i++) {
			defineVariable(variables[i]);
		}
	} else {
		if (match(TOKEN_EQUAL)) {
			expression();
		} else {
			emitByte(OP_NIL);
		}
		defineVariable(global);
	}

	consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
}

static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_POP);
}


static void whileStatement() {
	beginScope();
	int loopStart = currentChunk()->count;
	expression();
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);

	statement();

	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);

	endScope();
}

static void forStatement() {
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

	int bodyJump = emitJump(OP_JUMP);
	int incrementStart = currentChunk()->count;
	expression();
	emitByte(OP_POP);

	emitLoop(loopStart); // main loop that takes us back to the top of the for loop
	loopStart = incrementStart;
	patchJump(bodyJump);

	statement();
	emitLoop(loopStart);

	if (exitJump != -1) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}

	endScope();
}

static void ifStatement() {
	expression();
	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	int elseJump = emitJump(OP_JUMP);
	patchJump(thenJump);
	emitByte(OP_POP);

	if (match(TOKEN_ELSE))
		statement();
	patchJump(elseJump);
}

static void returnStatement() {
	if (current->type == TYPE_SCRIPT) {
		compilerPanic(&parser, "Cannot use <return> outside of a function", SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		emitReturn();
	} else {
		if (current->type == TYPE_INITIALIZER) {
			compilerPanic(&parser, "Cannot return a value from an 'init' function", SYNTAX);
		}

		uint8_t valueCount = 0;
		do {
			if (valueCount >= 255) {
				compilerPanic(&parser, "Cannot return more than 255 values.", RETURN_EXTENT);
			}
			expression();
			valueCount++;
		} while (match(TOKEN_COMMA));

		consume(TOKEN_SEMICOLON, "Expected ';' after return value");
		emitBytes(OP_RETURN, valueCount);
	}
}

static void useStatement() {
	
	uint8_t nameCount = 0;
	uint8_t names[UINT8_MAX];
	do {
		if (nameCount >= UINT8_MAX) {
			compilerPanic(&parser, "Cannot import more than 255 names from another module.", IMPORT_EXTENT);
		}
		consume(TOKEN_IDENTIFIER, "Expected name to import from external module");
		uint8_t name = identifierConstant(&parser.previous);
		names[nameCount] = name;
		nameCount++;
	} while (match(TOKEN_COMMA));

	consume(TOKEN_FROM, "Expected 'from' after 'use' statement.");
	consume(TOKEN_STRING, "Expected string literal for module name");
	uint8_t module = makeConstant(OBJECT_VAL(copyString(current->owner, parser.previous.start + 1, parser.previous.length - 2)));
	emitBytes(OP_USE, nameCount);
	for (uint8_t i = 0; i < nameCount; i++) {
		emitByte(names[i]);
	}
	emitByte(module);
	consume(TOKEN_SEMICOLON, "Expected semicolon after import statement.");
}

static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;
		switch (parser.current.type) {
			case TOKEN_CLASS:
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

static void publicDeclaration() {
	if (current->scopeDepth > 0) {
		compilerPanic(&parser, "Cannot declare public members in a local scope.", SYNTAX);
	}
	emitByte(OP_PUB);
	if (match(TOKEN_FN)) {
		Token functionName = parser.current;
		fnDeclaration();
		markPublic(current, copyString(current->owner, functionName.start, functionName.length));
	} else if (match(TOKEN_LET)) {
		Token variableName = parser.current;
		varDeclaration();
		markPublic(current, copyString(current->owner, variableName.start, variableName.length));
	} else if (match(TOKEN_CLASS)) {
		Token className = parser.current;
		classDeclaration();
		markPublic(current, copyString(current->owner, className.start, className.length));
	} else {
		compilerPanic(&parser, "Expected 'fn', 'let', or 'class' after 'pub'.", SYNTAX);
	}
}


static void declaration() {
	if (match(TOKEN_LET)) {
		varDeclaration();
	} else if (match(TOKEN_CLASS)) {
		classDeclaration();
	} else if (match(TOKEN_FN)) {
		fnDeclaration();
	} else if (match(TOKEN_PUB)) {
		publicDeclaration();
	} else {
		statement();
	}

	if (parser.panicMode)
		synchronize();
}

static void statement() {
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
	} else {
		expressionStatement();
	}
}

static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	// +1 -2 trims the leading and trailing quotation marks
	emitConstant(OBJECT_VAL(copyString(current->owner, parser.previous.start + 1, parser.previous.length - 2)));
	// TODO: translate string escape sequences
}


static void self(bool canAssign) {
	if (currentClass == NULL) {
		compilerPanic(&parser, "'self' cannot be used outside of a class.", NAME);
		return;
	}

	variable(false);
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

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

// Prefix Infix Precedence
ParseRule rules[] = {
		[TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
		[TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
		[TOKEN_LEFT_BRACE] = {tableLiteral, NULL, PREC_NONE},
		[TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
		[TOKEN_LEFT_SQUARE] = {arrayLiteral, collectionIndex, PREC_CALL},
		[TOKEN_RIGHT_SQUARE] = {NULL, NULL, PREC_NONE},
		[TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
		[TOKEN_DOT] = {NULL, dot, PREC_CALL},
		[TOKEN_MINUS] = {unary, binary, PREC_TERM},
		[TOKEN_PLUS] = {NULL, binary, PREC_TERM},
		[TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
		[TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
		[TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
		[TOKEN_PERCENT] = {NULL, binary, PREC_FACTOR},
		[TOKEN_LEFT_SHIFT] = {NULL, binary, PREC_SHIFT},
		[TOKEN_RIGHT_SHIFT] = {NULL, binary, PREC_SHIFT},
		[TOKEN_NOT] = {unary, NULL, PREC_NONE},
		[TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
		[TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
		[TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
		[TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
		[TOKEN_STRING] = {string, NULL, PREC_NONE},
		[TOKEN_INT] = {number, NULL, PREC_NONE},
		[TOKEN_FLOAT] = {number, NULL, PREC_NONE},
		[TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
		[TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
		[TOKEN_AND] = {NULL, and_, PREC_AND},
		[TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
		[TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
		[TOKEN_FALSE] = {literal, NULL, PREC_NONE},
		[TOKEN_FOR] = {NULL, NULL, PREC_NONE},
		[TOKEN_FN] = {anonymousFunction, NULL, PREC_NONE},
		[TOKEN_IF] = {NULL, NULL, PREC_NONE},
		[TOKEN_NIL] = {literal, NULL, PREC_NONE},
		[TOKEN_OR] = {NULL, or_, PREC_OR},
		[TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
		[TOKEN_SUPER] = {super_, NULL, PREC_NONE},
		[TOKEN_SELF] = {self, NULL, PREC_NONE},
		[TOKEN_TRUE] = {literal, NULL, PREC_NONE},
		[TOKEN_LET] = {NULL, NULL, PREC_NONE},
		[TOKEN_USE] = {NULL, NULL, PREC_NONE}, 
		[TOKEN_FROM] = {NULL, NULL, PREC_NONE},
		[TOKEN_PUB] = {NULL, NULL, PREC_NONE},
		[TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
		[TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
		[TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		compilerPanic(&parser, "Expected expression.", SYNTAX);
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		compilerPanic(&parser, "Invalid Assignment Target", SYNTAX);
	}
}

static ParseRule *getRule(TokenType type) {
	return &rules[type]; // Returns the rule at the given index
}

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
	return parser.hadError ? NULL : function;
}

void markCompilerRoots(VM *vm) {
	Compiler *compiler = current;
	while (compiler != NULL) {
		markObject(vm, (Object *) compiler->function);
		markObject(vm, (Object *) compiler->module);
		compiler = (Compiler *) compiler->enclosing;
	}
}
