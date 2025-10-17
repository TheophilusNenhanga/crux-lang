#include <stdio.h>

#include "file_handler.h"
#include "stdlib/std.h"
#include "vm.h"
#include "vm_helpers.h"
#include "vm_run.h"

#include <string.h>

#include "debug.h"
#include "object.h"
#include "panic.h"

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH() goto *dispatchTable[endIndex]
#else
#define DISPATCH()                                                             \
	instruction = READ_BYTE();                                             \
	goto *dispatchTable[instruction]
#endif

/**
 * Executes bytecode in the virtual machine.
 * @param vm The virtual machine
 * @param is_anonymous_frame Is this frame anonymous? (should this frame return
 * from run() at OP_RETURN?)
 * @return The interpretation result
 */
InterpretResult run(VM *vm, const bool is_anonymous_frame)
{
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	CallFrame *frame =
		&currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
	(frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_CRUX_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
	(frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT_16()                                                     \
	(frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING_16() AS_CRUX_STRING(READ_CONSTANT_16())

	static void *dispatchTable[] = {&&OP_RETURN,
					&&OP_CONSTANT,
					&&OP_NIL,
					&&OP_TRUE,
					&&OP_FALSE,
					&&OP_NEGATE,
					&&OP_EQUAL,
					&&OP_GREATER,
					&&OP_LESS,
					&&OP_LESS_EQUAL,
					&&OP_GREATER_EQUAL,
					&&OP_NOT_EQUAL,
					&&OP_ADD,
					&&OP_NOT,
					&&OP_SUBTRACT,
					&&OP_MULTIPLY,
					&&OP_DIVIDE,
					&&OP_POP,
					&&OP_DEFINE_GLOBAL,
					&&OP_GET_GLOBAL,
					&&OP_SET_GLOBAL,
					&&OP_GET_LOCAL,
					&&OP_SET_LOCAL,
					&&OP_JUMP_IF_FALSE,
					&&OP_JUMP,
					&&OP_LOOP,
					&&OP_CALL,
					&&OP_CLOSURE,
					&&OP_GET_UPVALUE,
					&&OP_SET_UPVALUE,
					&&OP_CLOSE_UPVALUE,
					&&OP_GET_PROPERTY,
					&&OP_SET_PROPERTY,
					&&OP_INVOKE,
					&&OP_ARRAY,
					&&OP_GET_COLLECTION,
					&&OP_SET_COLLECTION,
					&&OP_MODULUS,
					&&OP_LEFT_SHIFT,
					&&OP_RIGHT_SHIFT,
					&&OP_SET_LOCAL_SLASH,
					&&OP_SET_LOCAL_STAR,
					&&OP_SET_LOCAL_PLUS,
					&&OP_SET_LOCAL_MINUS,
					&&OP_SET_UPVALUE_SLASH,
					&&OP_SET_UPVALUE_STAR,
					&&OP_SET_UPVALUE_PLUS,
					&&OP_SET_UPVALUE_MINUS,
					&&OP_SET_GLOBAL_SLASH,
					&&OP_SET_GLOBAL_STAR,
					&&OP_SET_GLOBAL_PLUS,
					&&OP_SET_GLOBAL_MINUS,
					&&OP_TABLE,
					&&OP_ANON_FUNCTION,
					&&OP_PUB,
					&&OP_MATCH,
					&&OP_MATCH_JUMP,
					&&OP_MATCH_END,
					&&OP_RESULT_MATCH_OK,
					&&OP_RESULT_MATCH_ERR,
					&&OP_RESULT_BIND,
					&&OP_GIVE,
					&&OP_INT_DIVIDE,
					&&OP_POWER,
					&&OP_SET_GLOBAL_INT_DIVIDE,
					&&OP_SET_GLOBAL_MODULUS,
					&&OP_SET_LOCAL_INT_DIVIDE,
					&&OP_SET_LOCAL_MODULUS,
					&&OP_SET_UPVALUE_INT_DIVIDE,
					&&OP_SET_UPVALUE_MODULUS,
					&&OP_USE_NATIVE,
					&&OP_USE_MODULE,
					&&OP_FINISH_USE,
					&&OP_CONSTANT_16,
					&&OP_DEFINE_GLOBAL_16,
					&&OP_GET_GLOBAL_16,
					&&OP_SET_GLOBAL_16,
					&&OP_GET_PROPERTY_16,
					&&OP_SET_PROPERTY_16,
					&&OP_INVOKE_16,
					&&OP_TYPEOF,
					&&OP_STATIC_ARRAY,
					&&OP_STATIC_TABLE,
					&&OP_STRUCT,
					&&OP_STRUCT_16,
					&&OP_STRUCT_INSTANCE_START,
					&&OP_STRUCT_NAMED_FIELD,
					&&OP_STRUCT_NAMED_FIELD_16,
					&&OP_STRUCT_INSTANCE_END,
					&&OP_NIL_RETURN,
					&&OP_ANON_FUNCTION_16,
					&&OP_UNWRAP,
					&&end};

	uint8_t instruction;
#ifdef DEBUG_TRACE_EXECUTION
	static uint8_t endIndex = sizeof(dispatchTable) /
					  sizeof(dispatchTable[0]) -
				  1;
#endif
	DISPATCH();
OP_RETURN: {
	Value result = pop(currentModuleRecord);
	close_upvalues(currentModuleRecord, frame->slots);
	currentModuleRecord->frame_count--;
	if (currentModuleRecord->frame_count == 0) {
		pop(currentModuleRecord);
		return INTERPRET_OK;
	}
	currentModuleRecord->stack_top = frame->slots;
	push(currentModuleRecord, result);
	frame = &currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];

	if (is_anonymous_frame)
		return INTERPRET_OK;
	DISPATCH();
}

OP_CONSTANT: {
	Value constant = READ_CONSTANT();
	push(currentModuleRecord, constant);
	DISPATCH();
}

OP_NIL: {
	push(currentModuleRecord, NIL_VAL);
	DISPATCH();
}

OP_TRUE: {
	push(currentModuleRecord, BOOL_VAL(true));
	DISPATCH();
}

OP_FALSE: {
	push(currentModuleRecord, BOOL_VAL(false));
	DISPATCH();
}

OP_NEGATE: {
	Value operand = PEEK(currentModuleRecord, 0);
	if (IS_INT(operand)) {
		int32_t iVal = AS_INT(operand);
		if (iVal == INT32_MIN) {
			pop_push(currentModuleRecord,
				 FLOAT_VAL(-(double)INT32_MIN));
		} else {
			pop_push(currentModuleRecord, INT_VAL(-iVal));
		}
	} else if (IS_FLOAT(operand)) {
		pop_push(currentModuleRecord, FLOAT_VAL(-AS_FLOAT(operand)));
	} else {
		pop(currentModuleRecord);
		runtime_panic(currentModuleRecord, false, TYPE,
			      type_error_message(vm, operand, "int' | 'float"));
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_EQUAL: {
	Value b = pop(currentModuleRecord);
	Value a = pop(currentModuleRecord);
	push(currentModuleRecord, BOOL_VAL(values_equal(a, b)));
	DISPATCH();
}

OP_GREATER: {
	if (!binary_operation(vm, OP_GREATER)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_LESS: {
	if (!binary_operation(vm, OP_LESS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_LESS_EQUAL: {
	if (!binary_operation(vm, OP_LESS_EQUAL)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_GREATER_EQUAL: {
	if (!binary_operation(vm, OP_GREATER_EQUAL)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_NOT_EQUAL: {
	Value b = pop(currentModuleRecord);
	Value a = pop(currentModuleRecord);
	push(currentModuleRecord, BOOL_VAL(!values_equal(a, b)));
	DISPATCH();
}

OP_ADD: {
	if (IS_CRUX_STRING(PEEK(currentModuleRecord, 0)) &&
	    IS_CRUX_STRING(PEEK(currentModuleRecord, 1))) {
		if (!concatenate(vm)) {
			return INTERPRET_RUNTIME_ERROR;
		}
		DISPATCH();
	}
	if (!binary_operation(vm, OP_ADD)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_NOT: {
	push(currentModuleRecord, BOOL_VAL(is_falsy(pop(currentModuleRecord))));
	DISPATCH();
}

OP_SUBTRACT: {
	if (!binary_operation(vm, OP_SUBTRACT)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_MULTIPLY: {
	if (!binary_operation(vm, OP_MULTIPLY)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_DIVIDE: {
	if (!binary_operation(vm, OP_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_POP: {
	pop(currentModuleRecord);
	DISPATCH();
}

OP_DEFINE_GLOBAL: {
	ObjectString *name = READ_STRING();
	bool isPublic = false;
	if (check_previous_instruction(frame, 3, OP_PUB)) {
		isPublic = true;
	}
	if (table_set(vm, &currentModuleRecord->globals, name,
		      PEEK(currentModuleRecord, 0))) {
		if (isPublic) {
			table_set(vm, &currentModuleRecord->publics, name,
				  PEEK(currentModuleRecord, 0));
		}
		pop(currentModuleRecord);
		DISPATCH();
	}
	runtime_panic(
		currentModuleRecord, false, NAME,
		currentModuleRecord->is_repl
			? "Defined a name that already had a "
			  "definition"
			: "Cannot define '%s' because it is already defined.",
		name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL: {
	ObjectString *name = READ_STRING();
	Value value;
	if (table_get(&currentModuleRecord->globals, name, &value)) {
		push(currentModuleRecord, value);
		DISPATCH();
	}
	runtime_panic(currentModuleRecord, false, NAME,
		      "Undefined variable '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL: {
	ObjectString *name = READ_STRING();
	if (table_set(vm, &currentModuleRecord->globals, name,
		      PEEK(currentModuleRecord, 0))) {
		runtime_panic(
			currentModuleRecord, false, NAME,
			"Cannot give variable '%s' a value because it has "
			"not been "
			"defined\nDid you forget 'let'?",
			name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_GET_LOCAL: {
	uint8_t slot = READ_BYTE();
	push(currentModuleRecord, frame->slots[slot]);
	DISPATCH();
}

OP_SET_LOCAL: {
	uint8_t slot = READ_BYTE();
	frame->slots[slot] = PEEK(currentModuleRecord, 0);
	DISPATCH();
}

OP_JUMP_IF_FALSE: {
	uint16_t offset = READ_SHORT();
	if (is_falsy(PEEK(currentModuleRecord, 0))) {
		frame->ip += offset;
		DISPATCH();
	}
	DISPATCH();
}

OP_JUMP: {
	uint16_t offset = READ_SHORT();
	frame->ip += offset;
	DISPATCH();
}

OP_LOOP: {
	uint16_t offset = READ_SHORT();
	frame->ip -= offset;
	DISPATCH();
}

OP_CALL: {
	int arg_count = READ_BYTE();
	if (!call_value(vm, PEEK(currentModuleRecord, arg_count), arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];
	DISPATCH();
}

OP_CLOSURE: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(currentModuleRecord, OBJECT_VAL(closure));

	for (int i = 0; i < closure->upvalue_count; i++) {
		uint8_t isLocal = READ_BYTE();
		uint8_t index = READ_BYTE();

		if (isLocal) {
			closure->upvalues[i] = capture_upvalue(vm,
							       frame->slots +
								       index);
		} else {
			closure->upvalues[i] = frame->closure->upvalues[index];
		}
	}
	DISPATCH();
}

OP_GET_UPVALUE: {
	uint8_t slot = READ_BYTE();
	push(currentModuleRecord, *frame->closure->upvalues[slot]->location);
	DISPATCH();
}

OP_SET_UPVALUE: {
	uint8_t slot = READ_BYTE();
	*frame->closure->upvalues[slot]->location = PEEK(currentModuleRecord,
							 0);
	DISPATCH();
}

OP_CLOSE_UPVALUE: {
	close_upvalues(currentModuleRecord, currentModuleRecord->stack_top - 1);
	pop(currentModuleRecord);
	DISPATCH();
}

OP_GET_PROPERTY: {
	Value receiver = pop(currentModuleRecord);
	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		runtime_panic(
			currentModuleRecord, false, TYPE,
			"Cannot get property on non 'struct instance' type.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *name = READ_STRING();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, false, NAME,
			      "Property '%s' does not exist on struct '%s'.",
			      name->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	push(currentModuleRecord,
	     instance->fields[(uint16_t)AS_INT(indexValue)]);
	DISPATCH();
}

OP_SET_PROPERTY: {
	Value valueToSet = pop(currentModuleRecord);
	Value receiver = pop(currentModuleRecord);

	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		ObjectString *name = READ_STRING();
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Cannot set property '%s' on non struct instance "
			      "value. %s",
			      name->chars,
			      type_error_message(vm, receiver,
						 "struct instance"));
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectString *name = READ_STRING();
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, false, NAME,
			      "Property '%s' does not exist on struct '%s'.",
			      name->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	instance->fields[(uint16_t)AS_INT(indexValue)] = valueToSet;
	push(currentModuleRecord, valueToSet);

	DISPATCH();
}

OP_INVOKE: {
	ObjectString *methodName = READ_STRING();
	int arg_count = READ_BYTE();
	if (!invoke(vm, methodName, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];
	DISPATCH();
}

OP_ARRAY: {
	uint16_t elementCount = READ_SHORT();
	ObjectArray *array = new_array(vm, elementCount, currentModuleRecord);
	for (int i = elementCount - 1; i >= 0; i--) {
		array_add(vm, array, pop(currentModuleRecord), i);
	}
	push(currentModuleRecord, OBJECT_VAL(array));
	DISPATCH();
}

OP_GET_COLLECTION: {
	Value indexValue = pop(currentModuleRecord);
	if (!IS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))) {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (AS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))->type) {
	case OBJECT_TABLE: {
		if (IS_CRUX_HASHABLE(indexValue)) {
			ObjectTable *table = AS_CRUX_TABLE(
				PEEK(currentModuleRecord, 0));
			Value value;
			if (!object_table_get(table->entries, table->size,
					      table->capacity, indexValue,
					      &value)) {
				runtime_panic(currentModuleRecord, false,
					      COLLECTION_GET,
					      "Failed to get value from table");
				return INTERPRET_RUNTIME_ERROR;
			}
			pop_push(currentModuleRecord, value);
		} else {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Key cannot be hashed.", READ_STRING());
			return INTERPRET_RUNTIME_ERROR;
		}
		DISPATCH();
	}
	case OBJECT_ARRAY: {
		if (!IS_INT(indexValue)) {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t) AS_INT(indexValue);
		ObjectArray *array = AS_CRUX_ARRAY(
			PEEK(currentModuleRecord, 0));

		if (index >= array->size) {
			runtime_panic(currentModuleRecord, false, BOUNDS,
				      "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		Value value = array->values[index];

		pop_push(currentModuleRecord,
			 value); // pop the array off the stack // push the
				 // value onto the stack
		DISPATCH();
	}
	case OBJECT_STRING: {
		if (!IS_INT(indexValue)) {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t) AS_INT(indexValue);
		ObjectString *string = AS_CRUX_STRING(
			PEEK(currentModuleRecord, 0));
		ObjectString *ch;
		if (index >= string->length) {
			runtime_panic(currentModuleRecord, false, BOUNDS,
				      "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		// Only single character indexing
		ch = copy_string(vm, string->chars + index, 1);
		pop_push(currentModuleRecord, OBJECT_VAL(ch));
		DISPATCH();
	}

	case OBJECT_STATIC_ARRAY: {
		if (!IS_INT(indexValue)) {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t) AS_INT(indexValue);
		ObjectStaticArray *array = AS_CRUX_STATIC_ARRAY(
			PEEK(currentModuleRecord, 1));
		if (index >= array->size) {
			runtime_panic(currentModuleRecord, false, BOUNDS,
				      "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		pop_push(currentModuleRecord, array->values[index]);
		DISPATCH();
	}

	case OBJECT_STATIC_TABLE: {
		if (IS_CRUX_HASHABLE(indexValue)) {
			ObjectStaticTable *table = AS_CRUX_STATIC_TABLE(
				PEEK(currentModuleRecord, 0));
			Value value;
			if (!object_table_get(table->entries, table->size,
					      table->capacity, indexValue,
					      &value)) {
				runtime_panic(currentModuleRecord, false,
					      COLLECTION_GET,
					      "Failed to get value from table");
				return INTERPRET_RUNTIME_ERROR;
			}
			pop_push(currentModuleRecord, value);
		} else {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Key cannot be hashed.", READ_STRING());
			return INTERPRET_RUNTIME_ERROR;
		}
		DISPATCH();
	}

	default: {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	}
	DISPATCH();
}

OP_SET_COLLECTION: {
	Value value = pop(currentModuleRecord);
	Value indexValue = PEEK(currentModuleRecord, 0);
	Value collection = PEEK(currentModuleRecord, 1);

	switch (AS_CRUX_OBJECT(collection)->type) {
	case OBJECT_TABLE: {
		ObjectTable *table = AS_CRUX_TABLE(collection);
		if (IS_INT(indexValue) || IS_CRUX_STRING(indexValue)) {
			if (!object_table_set(vm, table, indexValue, value)) {
				runtime_panic(currentModuleRecord, false,
					      COLLECTION_GET,
					      "Failed to set value in table");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}

	case OBJECT_ARRAY: {
		ObjectArray *array = AS_CRUX_ARRAY(collection);
		int index = AS_INT(indexValue);
		if (!array_set(vm, array, index, value)) {
			runtime_panic(currentModuleRecord, false, BOUNDS,
				      "Cannot set a value in an empty array.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}

	case OBJECT_STATIC_ARRAY: {
		runtime_panic(currentModuleRecord, false, COLLECTION_SET,
			      "'static array' does not support value updates. "
			      "Use 'array' instead.");
		return INTERPRET_RUNTIME_ERROR;
	}

	case OBJECT_STATIC_TABLE: {
		runtime_panic(currentModuleRecord, false, COLLECTION_SET,
			      "'static table' does not support value updates. "
			      "Use 'table' instead.");
		return INTERPRET_RUNTIME_ERROR;
	}
	default: {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Value is not a mutable collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	}

	pop_two(currentModuleRecord); // indexValue and collection
	push(currentModuleRecord, indexValue);
	DISPATCH();
}

OP_MODULUS: {
	if (!binary_operation(vm, OP_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_LEFT_SHIFT: {
	if (!binary_operation(vm, OP_LEFT_SHIFT)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_RIGHT_SHIFT: {
	if (!binary_operation(vm, OP_RIGHT_SHIFT)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_SLASH: {
	uint8_t slot = READ_BYTE();
	Value currentValue = frame->slots[slot];
	Value operandValue = PEEK(currentModuleRecord, 0); // Right-hand side

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) &&
	      (operandIsInt || operandIsFloat))) {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Operands for '/=' must be numbers.");
		return INTERPRET_RUNTIME_ERROR;
	}

	Value resultValue;

	double dcurrent = currentIsFloat ? AS_FLOAT(currentValue)
					 : (double)AS_INT(currentValue);
	double doperand = operandIsFloat ? AS_FLOAT(operandValue)
					 : (double)AS_INT(operandValue);

	if (doperand == 0.0) {
		runtime_panic(currentModuleRecord, false, MATH,
			      "Division by zero in '/=' assignment.");
		return INTERPRET_RUNTIME_ERROR;
	}

	resultValue = FLOAT_VAL(dcurrent / doperand);
	frame->slots[slot] = resultValue;
	DISPATCH();
}

OP_SET_LOCAL_STAR: {
	uint8_t slot = READ_BYTE();
	Value currentValue = frame->slots[slot];
	Value operandValue = PEEK(currentModuleRecord, 0);

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) &&
	      (operandIsInt || operandIsFloat))) {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Operands for '*=' must be numbers.");
		return INTERPRET_RUNTIME_ERROR;
	}

	Value resultValue;

	if (currentIsInt && operandIsInt) {
		int32_t icurrent = AS_INT(currentValue);
		int32_t ioperand = AS_INT(operandValue);
		int64_t result = (int64_t)icurrent * (int64_t)ioperand;
		if (result >= INT32_MIN && result <= INT32_MAX) {
			resultValue = INT_VAL((int32_t)result);
		} else {
			resultValue = FLOAT_VAL((double)result); // Promote
		}
	} else {
		double dcurrent = currentIsFloat ? AS_FLOAT(currentValue)
						 : (double)AS_INT(currentValue);
		double doperand = operandIsFloat ? AS_FLOAT(operandValue)
						 : (double)AS_INT(operandValue);
		resultValue = FLOAT_VAL(dcurrent * doperand);
	}

	frame->slots[slot] = resultValue;
	DISPATCH();
}

OP_SET_LOCAL_PLUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					&frame->slots[slot],
					PEEK(currentModuleRecord, 0),
					OP_SET_LOCAL_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_MINUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					&frame->slots[slot],
					PEEK(currentModuleRecord, 0),
					OP_SET_LOCAL_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_SLASH: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_SLASH)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_STAR: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_STAR)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_PLUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MINUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_SLASH: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_SLASH, "/=") ==
	    INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_STAR: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_STAR, "*=") ==
	    INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_PLUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_PLUS, "+=") ==
	    INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MINUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_MINUS, "-=") ==
	    INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_TABLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectTable *table = new_table(vm, elementCount, currentModuleRecord);
	for (int i = elementCount - 1; i >= 0; i--) {
		Value value = pop(currentModuleRecord);
		Value key = pop(currentModuleRecord);
		if (IS_CRUX_HASHABLE(key)) {
			if (!object_table_set(vm, table, key, value)) {
				runtime_panic(currentModuleRecord, false,
					      COLLECTION_SET,
					      "Failed to set value in table.");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	push(currentModuleRecord, OBJECT_VAL(table));
	DISPATCH();
}

OP_ANON_FUNCTION: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(currentModuleRecord, OBJECT_VAL(closure));
	for (int i = 0; i < closure->upvalue_count; i++) {
		uint8_t isLocal = READ_BYTE();
		uint8_t index = READ_BYTE();

		if (isLocal) {
			closure->upvalues[i] = capture_upvalue(vm,
							       frame->slots +
								       index);
		} else {
			closure->upvalues[i] = frame->closure->upvalues[index];
		}
	}
	DISPATCH();
}

OP_PUB: {
	DISPATCH();
}

OP_MATCH: {
	Value target = PEEK(currentModuleRecord, 0);
	vm->match_handler.match_target = target;
	vm->match_handler.is_match_target = true;
	DISPATCH();
}

OP_MATCH_JUMP: {
	uint16_t offset = READ_SHORT();
	Value pattern = pop(currentModuleRecord);
	Value target = PEEK(currentModuleRecord, 0);
	if (!values_equal(pattern, target)) {
		frame->ip += offset;
	}
	DISPATCH();
}

OP_MATCH_END: {
	if (vm->match_handler.is_match_bind) {
		push(currentModuleRecord, vm->match_handler.match_bind);
	}
	vm->match_handler.match_target = NIL_VAL;
	vm->match_handler.match_bind = NIL_VAL;
	vm->match_handler.is_match_bind = false;
	vm->match_handler.is_match_target = false;
	DISPATCH();
}

OP_RESULT_MATCH_OK: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(currentModuleRecord, 0);
	if (!IS_CRUX_RESULT(target) || !AS_CRUX_RESULT(target)->is_ok) {
		frame->ip += offset;
	} else {
		Value value = AS_CRUX_RESULT(target)->as.value;
		pop_push(currentModuleRecord, value);
	}
	DISPATCH();
}

OP_RESULT_MATCH_ERR: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(currentModuleRecord, 0);
	if (!IS_CRUX_RESULT(target) || AS_CRUX_RESULT(target)->is_ok) {
		frame->ip += offset;
	} else {
		Value error = OBJECT_VAL(AS_CRUX_RESULT(target)->as.error);
		pop_push(currentModuleRecord, error);
	}
	DISPATCH();
}

OP_RESULT_BIND: {
	uint8_t slot = READ_BYTE();
	Value bind = PEEK(currentModuleRecord, 0);
	vm->match_handler.match_bind = bind;
	vm->match_handler.is_match_bind = true;
	frame->slots[slot] = bind;
	DISPATCH();
}

OP_GIVE: {
	Value result = pop(currentModuleRecord);
	pop_push(currentModuleRecord, result);
	DISPATCH();
}

OP_INT_DIVIDE: {
	if (!binary_operation(vm, OP_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_POWER: {
	if (!binary_operation(vm, OP_POWER)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_INT_DIVIDE: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_INT_DIVIDE,
				      "\\=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MODULUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_MODULUS, "%=") ==
	    INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_INT_DIVIDE: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					&frame->slots[slot],
					PEEK(currentModuleRecord, 0),
					OP_SET_LOCAL_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}
OP_SET_LOCAL_MODULUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					&frame->slots[slot],
					PEEK(currentModuleRecord, 0),
					OP_SET_LOCAL_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_INT_DIVIDE: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MODULUS: {
	uint8_t slot = READ_BYTE();
	if (!handle_compound_assignment(currentModuleRecord,
					frame->closure->upvalues[slot]->location,
					PEEK(currentModuleRecord, 0),
					OP_SET_UPVALUE_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_USE_NATIVE: {
	uint8_t nameCount = READ_BYTE();
	ObjectString *names[UINT8_MAX];
	ObjectString *aliases[UINT8_MAX];

	for (uint8_t i = 0; i < nameCount; i++) {
		names[i] = READ_STRING();
	}
	for (uint8_t i = 0; i < nameCount; i++) {
		aliases[i] = READ_STRING();
	}

	ObjectString *moduleName = READ_STRING();
	int moduleIndex = -1;
	for (int i = 0; i < vm->native_modules.count; i++) {
		if (moduleName == vm->native_modules.modules[i].name) {
			moduleIndex = i;
			break;
		}
	}
	if (moduleIndex == -1) {
		runtime_panic(currentModuleRecord, false, IMPORT,
			      "Module '%s' not found.", moduleName->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	Table *moduleTable = vm->native_modules.modules[moduleIndex].names;
	for (int i = 0; i < nameCount; i++) {
		Value value;
		bool getSuccess = table_get(moduleTable, names[i], &value);
		if (!getSuccess) {
			runtime_panic(currentModuleRecord, false, IMPORT,
				      "Failed to import '%s' from '%s'.",
				      names[i]->chars, moduleName->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		push(currentModuleRecord, OBJECT_VAL(value));
		bool setSuccess = table_set(vm,
					    &vm->current_module_record->globals,
					    aliases[i], value);

		if (!setSuccess) {
			runtime_panic(currentModuleRecord, false, IMPORT,
				      "Failed to import '%s' from '%s'.",
				      names[i]->chars, moduleName->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		pop(currentModuleRecord);
	}

	DISPATCH();
}

OP_USE_MODULE: {
	ObjectString *moduleName = READ_STRING();

	if (is_in_import_stack(vm, moduleName)) {
		runtime_panic(currentModuleRecord, false, IMPORT,
			      "Circular dependency detected when importing: %s",
			      moduleName->chars);
		vm->current_module_record->state = STATE_ERROR;
		return INTERPRET_RUNTIME_ERROR;
	}

	char *resolvedPathChars = resolve_path(
		vm->current_module_record->path->chars, moduleName->chars);
	if (resolvedPathChars == NULL) {
		runtime_panic(currentModuleRecord, false, IMPORT,
			      "Failed to resolve import path");
		vm->current_module_record->state = STATE_ERROR;
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectString *resolvedPath =
		take_string(vm, resolvedPathChars,
			    strlen(resolvedPathChars)); // VM takes ownership

	Value cachedModule;
	if (table_get(&vm->module_cache, resolvedPath, &cachedModule)) {
		push(currentModuleRecord, cachedModule);
		DISPATCH();
	}

	if (vm->import_count + 1 > IMPORT_MAX) {
		runtime_panic(currentModuleRecord, false, IMPORT,
			      "Import limit reached");
		return INTERPRET_RUNTIME_ERROR;
	}
	vm->import_count++;

	FileResult file = read_file(resolvedPath->chars);
	if (file.error != NULL) {
		runtime_panic(currentModuleRecord, false, IO, file.error);
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectModuleRecord *module = new_object_module_record(vm, resolvedPath,
							      false, false);
	module->enclosing_module = vm->current_module_record;
	reset_stack(module);
	if (module->frames == NULL) {
		runtime_panic(
			currentModuleRecord, false, MEMORY,
			"Failed to allocate memory for new module from \"%s\".",
			resolvedPath->chars);
		vm->current_module_record->state = STATE_ERROR;
		return INTERPRET_RUNTIME_ERROR;
	}
	push_import_stack(vm, resolvedPath);

	ObjectModuleRecord *previousModuleRecord = vm->current_module_record;
	vm->current_module_record = module;

	init_table(&vm->current_module_record->globals);
	init_table(&vm->current_module_record->publics);

	if (!initialize_std_lib(vm)) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to initialize stdlib for module:\"%s\".",
			      module->path->chars);
		module->state = STATE_ERROR;
		pop_import_stack(vm);
		vm->current_module_record = previousModuleRecord;
		push(currentModuleRecord, OBJECT_VAL(module));
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectFunction *function = compile(vm, file.content);
	free_file_result(file);

	if (function == NULL) {
		module->state = STATE_ERROR;
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to compile '%s'.", resolvedPath->chars);
		pop_import_stack(vm);
		vm->current_module_record = previousModuleRecord;
		push(currentModuleRecord, OBJECT_VAL(module));
		return INTERPRET_COMPILE_ERROR;
	}
	push(currentModuleRecord, OBJECT_VAL(function));
	ObjectClosure *closure = new_closure(vm, function);
	pop(currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(closure));

	module->module_closure = closure;

	table_set(vm, &vm->module_cache, resolvedPath, OBJECT_VAL(module));

	if (!call(currentModuleRecord, closure, 0)) {
		module->state = STATE_ERROR;
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to call module.");
		pop_import_stack(vm);
		vm->current_module_record = previousModuleRecord;
		push(currentModuleRecord, OBJECT_VAL(module));
		return INTERPRET_RUNTIME_ERROR;
	}

	InterpretResult result = run(vm, false);
	if (result != INTERPRET_OK) {
		module->state = STATE_ERROR;
		pop_import_stack(vm);
		vm->current_module_record = previousModuleRecord;
		push(currentModuleRecord, OBJECT_VAL(module));
		return result;
	}

	module->state = STATE_LOADED;

	pop_import_stack(vm);
	vm->current_module_record = previousModuleRecord;
	push(currentModuleRecord, OBJECT_VAL(module));

	DISPATCH();
}

OP_FINISH_USE: {
	uint8_t nameCount = READ_BYTE();
	ObjectString *names[UINT8_MAX];
	ObjectString *aliases[UINT8_MAX];

	for (int i = 0; i < nameCount; i++) {
		names[i] = READ_STRING();
	}
	for (int i = 0; i < nameCount; i++) {
		aliases[i] = READ_STRING();
	}
	if (!IS_CRUX_MODULE_RECORD(PEEK(currentModuleRecord, 0))) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Module record creation could not be completed.");
		return INTERPRET_RUNTIME_ERROR;
	}
	Value moduleValue = pop(currentModuleRecord);
	ObjectModuleRecord *importedModule = AS_CRUX_MODULE_RECORD(moduleValue);

	if (importedModule->state == STATE_ERROR) {
		runtime_panic(currentModuleRecord, false, IMPORT,
			      "Failed to import module from %s",
			      importedModule->path->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	// copy names
	for (uint8_t i = 0; i < nameCount; i++) {
		ObjectString *name = names[i];
		ObjectString *alias = aliases[i];

		Value value;
		if (!table_get(&importedModule->publics, name, &value)) {
			runtime_panic(currentModuleRecord, false, IMPORT,
				      "'%s' is not an exported name.",
				      name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}

		if (!table_set(vm, &vm->current_module_record->globals, alias,
			       value)) {
			runtime_panic(currentModuleRecord, false, IMPORT,
				      "Failed to import '%s'. This name may "
				      "already be in use in "
				      "this scope.",
				      name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	vm->import_count--;
	DISPATCH();
}

OP_CONSTANT_16: {
	Value constant = READ_CONSTANT_16();
	push(currentModuleRecord, constant);
	DISPATCH();
}

OP_DEFINE_GLOBAL_16: {
	ObjectString *name = READ_STRING_16();
	bool isPublic = false;
	if (check_previous_instruction(frame, 3, OP_PUB)) {
		isPublic = true;
	}
	if (table_set(vm, &currentModuleRecord->globals, name,
		      PEEK(currentModuleRecord, 0))) {
		if (isPublic) {
			table_set(vm, &currentModuleRecord->publics, name,
				  PEEK(currentModuleRecord, 0));
		}
		pop(currentModuleRecord);
		DISPATCH();
	}
	runtime_panic(currentModuleRecord, false, NAME,
		      "Cannot define '%s' because it is already defined.",
		      name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL_16: {
	ObjectString *name = READ_STRING_16();
	Value value;
	if (table_get(&currentModuleRecord->globals, name, &value)) {
		push(currentModuleRecord, value);
		DISPATCH();
	}
	runtime_panic(currentModuleRecord, false, NAME,
		      "Undefined variable '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL_16: {
	ObjectString *name = READ_STRING_16();
	if (table_set(vm, &currentModuleRecord->globals, name,
		      PEEK(currentModuleRecord, 0))) {
		runtime_panic(
			currentModuleRecord, false, NAME,
			"Cannot give variable '%s' a value because it has "
			"not been "
			"defined\nDid you forget 'let'?",
			name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_GET_PROPERTY_16: {
	Value receiver = pop(currentModuleRecord);
	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		runtime_panic(
			currentModuleRecord, false, TYPE,
			"Cannot get property on non 'struct instance' type.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *name = READ_STRING_16();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, false, NAME,
			      "Property '%s' does not exist on struct '%s'.",
			      name->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	push(currentModuleRecord,
	     instance->fields[(uint16_t)AS_INT(indexValue)]);
	DISPATCH();
}

OP_SET_PROPERTY_16: {
	Value valueToSet = pop(currentModuleRecord);
	Value receiver = pop(currentModuleRecord);

	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		ObjectString *name = READ_STRING_16();
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Cannot set property '%s' on non struct instance "
			      "value. %s",
			      name->chars,
			      type_error_message(vm, receiver,
						 "struct instance"));
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectString *name = READ_STRING();
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, false, NAME,
			      "Property '%s' does not exist on struct '%s'.",
			      name->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	instance->fields[(uint16_t)AS_INT(indexValue)] = valueToSet;
	push(currentModuleRecord, valueToSet);

	DISPATCH();
}

OP_INVOKE_16: {
	ObjectString *methodName = READ_STRING_16();
	int arg_count = READ_BYTE();
	if (!invoke(vm, methodName, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];
	DISPATCH();
}

OP_TYPEOF: {
	Value value = PEEK(currentModuleRecord, 0);
	Value typeValue = typeof_value(vm, value);
	pop(currentModuleRecord);
	push(currentModuleRecord, typeValue);
	DISPATCH();
}

OP_STATIC_ARRAY: {
	uint16_t elementCount = READ_SHORT();
	ObjectStaticArray *array = new_static_array(vm, elementCount,
						    currentModuleRecord);
	Value *values = array->values;
	for (int i = elementCount - 1; i >= 0; i--) {
		values[i] = pop(currentModuleRecord);
	}
	push(currentModuleRecord, OBJECT_VAL(array));
	DISPATCH();
}

OP_STATIC_TABLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectStaticTable *table = new_static_table(vm, elementCount,
						    currentModuleRecord);
	for (int i = elementCount - 1; i >= 0; i--) {
		Value value = pop(currentModuleRecord);
		Value key = pop(currentModuleRecord);
		if (IS_CRUX_HASHABLE(key)) {
			if (!object_static_table_set(vm, table, key, value)) {
				runtime_panic(
					currentModuleRecord, false,
					COLLECTION_SET,
					"Failed to set value in static table.");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(currentModuleRecord, false, TYPE,
				      "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	push(currentModuleRecord, OBJECT_VAL(table));
	DISPATCH();
}

OP_STRUCT: {
	ObjectStruct *structObject = AS_CRUX_STRUCT(READ_CONSTANT());
	push(currentModuleRecord, OBJECT_VAL(structObject));
	DISPATCH();
}

OP_STRUCT_16: {
	ObjectStruct *structObject = AS_CRUX_STRUCT(READ_CONSTANT_16());
	push(currentModuleRecord, OBJECT_VAL(structObject));
	DISPATCH();
}

OP_STRUCT_INSTANCE_START: {
	Value value = PEEK(currentModuleRecord, 0);
	ObjectStruct *objectStruct = AS_CRUX_STRUCT(value);
	ObjectStructInstance *structInstance = new_struct_instance(
		vm, objectStruct, objectStruct->fields.count,
		currentModuleRecord);
	pop(currentModuleRecord); // struct type
	if (!pushStructStack(vm, structInstance)) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to push struct onto stack.");
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_STRUCT_NAMED_FIELD: {
	ObjectStructInstance *structInstance = peek_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to get struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *fieldName = READ_STRING();

	ObjectStruct *structType = structInstance->struct_type;
	Value indexValue;
	if (!table_get(&structType->fields, fieldName, &indexValue)) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Field '%s' does not exist on strut type '%s'.",
			      fieldName->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	uint16_t index = (uint16_t)AS_INT(indexValue);
	structInstance->fields[index] = pop(currentModuleRecord);
	DISPATCH();
}

OP_STRUCT_NAMED_FIELD_16: {
	ObjectStructInstance *structInstance = peek_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to get struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *fieldName = READ_STRING_16();

	ObjectStruct *structType = structInstance->struct_type;
	Value indexValue;
	if (!table_get(&structType->fields, fieldName, &indexValue)) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Field '%s' does not exist on struct type '%s'.",
			      fieldName->chars, structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	uint16_t index = (uint16_t)AS_INT(indexValue);
	structInstance->fields[index] = pop(currentModuleRecord);
	DISPATCH();
}

OP_STRUCT_INSTANCE_END: {
	ObjectStructInstance *structInstance = pop_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(currentModuleRecord, false, RUNTIME,
			      "Failed to pop struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(currentModuleRecord, OBJECT_VAL(structInstance));
	DISPATCH();
}

OP_NIL_RETURN: {
	close_upvalues(currentModuleRecord, frame->slots);
	currentModuleRecord->frame_count--;
	if (currentModuleRecord->frame_count == 0) {
		pop(currentModuleRecord);
		return INTERPRET_OK;
	}
	currentModuleRecord->stack_top = frame->slots;
	push(currentModuleRecord, NIL_VAL);
	frame = &currentModuleRecord
			 ->frames[currentModuleRecord->frame_count - 1];

	if (is_anonymous_frame)
		return INTERPRET_OK;
	DISPATCH();
}

OP_ANON_FUNCTION_16: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT_16());
	ObjectClosure *closure = new_closure(vm, function);
	push(currentModuleRecord, OBJECT_VAL(closure));
	for (int i = 0; i < closure->upvalue_count; i++) {
		uint8_t isLocal = READ_BYTE();
		uint8_t index = READ_BYTE();

		if (isLocal) {
			closure->upvalues[i] = capture_upvalue(vm,
							       frame->slots +
								       index);
		} else {
			closure->upvalues[i] = frame->closure->upvalues[index];
		}
	}
	DISPATCH();
}

OP_UNWRAP: {
	Value value = pop(currentModuleRecord);
	if (!IS_CRUX_RESULT(value)) {
		runtime_panic(currentModuleRecord, false, TYPE,
			      "Only the 'result' type supports unwrapping.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectResult *result = AS_CRUX_RESULT(value);
	if (result->is_ok) {
		push(currentModuleRecord, result->as.value);
	} else {
		push(currentModuleRecord, OBJECT_VAL(result->as.error));
	}
	DISPATCH();
}

end: {
	printf("        ");
	for (Value *slot = currentModuleRecord->stack;
	     slot < currentModuleRecord->stack_top; slot++) {
		printf("[");
		print_value(*slot, false);
		printf("]");
	}
	printf("\n");

	disassemble_instruction(&frame->closure->function->chunk,
				(int)(frame->ip -
				      frame->closure->function->chunk.code));

	instruction = READ_BYTE();
	goto *dispatchTable[instruction];
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_STRING_16
#undef READ_SHORT
#undef READ_CONSTANT_16
#undef READ_STRING_16
}
