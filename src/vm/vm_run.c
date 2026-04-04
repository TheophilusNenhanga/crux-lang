#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "file_handler.h"
#include "garbage_collector.h"
#include "stdlib/stdlib.h"
#include "type_system.h"
#include "utf8.h"
#include "value.h"
#include "vm.h"

#include <string.h>

#include "debug.h"
#include "object.h"
#include "panic.h"
#include "stdlib/range.h"
#include "stdlib/set.h"

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH()                                                                                                     \
	if (vm->is_exiting)                                                                                                \
		return INTERPRET_EXIT;                                                                                         \
	goto *dispatchTable[endIndex]
#else
#define DISPATCH()                                                                                                     \
	if (vm->is_exiting)                                                                                                \
		return INTERPRET_EXIT;                                                                                         \
	instruction = READ_SHORT();                                                                                        \
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
	CallFrame *frame = &currentModuleRecord->frames[currentModuleRecord->frame_count - 1];

#define READ_SHORT() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING() AS_CRUX_STRING(READ_CONSTANT())

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
									&&OP_SET,
									&&OP_TUPLE,
									&&OP_RANGE,
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
									&&OP_TYPEOF,
									&&OP_STRUCT,
									&&OP_STRUCT_INSTANCE_START,
									&&OP_STRUCT_NAMED_FIELD,
									&&OP_STRUCT_INSTANCE_END,
									&&OP_NIL_RETURN,
									&&OP_UNWRAP,
									&&OP_PANIC,
									&&OP_BITWISE_AND,
									&&OP_BITWISE_XOR,
									&&OP_BITWISE_OR,
									&&OP_METHOD,
									&&OP_SET_PROPERTY_PLUS,
									&&OP_SET_PROPERTY_MINUS,
									&&OP_SET_PROPERTY_STAR,
									&&OP_SET_PROPERTY_SLASH,
									&&OP_SET_PROPERTY_INT_DIVIDE,
									&&OP_SET_PROPERTY_MODULUS,
									&&OP_BITWISE_NOT,
									&&OP_TYPE_COERCE,
									&&OP_GET_SLICE,
									&&OP_IN,
									&&OP_ITER_INIT,
									&&OP_ITER_NEXT,
									&&OP_OK,
									&&OP_ERR,
									&&OP_SOME,
									&&OP_NONE,
									&&end};

	uint16_t instruction;
#ifdef DEBUG_TRACE_EXECUTION
	static uint16_t endIndex = sizeof(dispatchTable) / sizeof(dispatchTable[0]) - 1;
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
	frame = &currentModuleRecord->frames[currentModuleRecord->frame_count - 1];

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
			pop_push(currentModuleRecord, FLOAT_VAL(-(double)INT32_MIN));
		} else {
			pop_push(currentModuleRecord, INT_VAL(-iVal));
		}
	} else if (IS_FLOAT(operand)) {
		pop_push(currentModuleRecord, FLOAT_VAL(-AS_FLOAT(operand)));
	} else {
		pop(currentModuleRecord);
		runtime_panic(currentModuleRecord, TYPE, type_error_message(vm, operand, "int' | 'float"));
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
	if (IS_CRUX_STRING(PEEK(currentModuleRecord, 0)) && IS_CRUX_STRING(PEEK(currentModuleRecord, 1))) {
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

	// using module record from function to resolve names from imported modules

OP_DEFINE_GLOBAL: {
	ObjectString *name = READ_STRING();
	bool isPublic = false;
	if (check_previous_instruction(frame, 3, OP_PUB)) {
		isPublic = true;
	}

	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	if (table_set(vm, &frame_module_record->globals, name, PEEK(currentModuleRecord, 0))) {
		if (isPublic) {
			table_set(vm, &frame_module_record->publics, name, PEEK(currentModuleRecord, 0));
		}
		pop(currentModuleRecord);
		DISPATCH();
	}
	runtime_panic(currentModuleRecord, NAME, "Cannot define '%s' because it is already defined.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_GET_GLOBAL: {
	ObjectString *name = READ_STRING();
	Value value;
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	if (table_get(&frame_module_record->globals, name, &value)) {
		push(currentModuleRecord, value);
		DISPATCH();
	}
	runtime_panic(currentModuleRecord, NAME, "Undefined variable '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_SET_GLOBAL: {
	ObjectString *name = READ_STRING();
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	if (table_set(vm, &frame_module_record->globals, name, PEEK(currentModuleRecord, 0))) {
		runtime_panic(currentModuleRecord, NAME,
					  "Cannot give variable '%s' a value because it has not been defined\nDid you forget 'let'?",
					  name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_GET_LOCAL: {
	uint16_t slot = READ_SHORT();
	push(currentModuleRecord, frame->slots[slot]);
	DISPATCH();
}

OP_SET_LOCAL: {
	uint16_t slot = READ_SHORT();
	frame->slots[slot] = PEEK(currentModuleRecord, 0);
	DISPATCH();
}

OP_ITER_INIT: {
	const Value iterable = PEEK(currentModuleRecord, 0);
	Value iterator;
	if (!get_iterator_from_value(vm, iterable, &iterator)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	currentModuleRecord->stack_top[-1] = iterator;
	DISPATCH();
}

OP_ITER_NEXT: {
	uint16_t offset = READ_SHORT();
	Value option;
	if (!get_next_option_from_iterator(vm, PEEK(currentModuleRecord, 0), &option)) {
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectOption *next_option = AS_CRUX_OPTION(option);
	if (!next_option->is_some) {
		pop(currentModuleRecord);
		frame->ip += offset; // jump to after the loop
		DISPATCH();
	}

	currentModuleRecord->stack_top[-1] = next_option->value; // set the value to the next item
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
	int arg_count = READ_SHORT();
	if (!call_value(vm, PEEK(currentModuleRecord, arg_count), arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &currentModuleRecord->frames[currentModuleRecord->frame_count - 1];
	DISPATCH();
}

OP_CLOSURE: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(currentModuleRecord, OBJECT_VAL(closure));

	for (int i = 0; i < closure->upvalue_count; i++) {
		uint16_t isLocal = READ_SHORT();
		uint16_t index = READ_SHORT();

		if (isLocal) {
			closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
		} else {
			closure->upvalues[i] = frame->closure->upvalues[index];
		}
	}
	DISPATCH();
}

OP_GET_UPVALUE: {
	uint16_t slot = READ_SHORT();
	push(currentModuleRecord, *frame->closure->upvalues[slot]->location);
	DISPATCH();
}

OP_SET_UPVALUE: {
	uint16_t slot = READ_SHORT();
	*frame->closure->upvalues[slot]->location = PEEK(currentModuleRecord, 0);
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
		runtime_panic(currentModuleRecord, TYPE, "Cannot get property on non 'struct instance' type.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *name = READ_STRING();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, NAME, "Property '%s' does not exist on struct '%s'.", name->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	push(currentModuleRecord, instance->fields[(uint16_t)AS_INT(indexValue)]);
	DISPATCH();
}

OP_SET_PROPERTY: {
	Value valueToSet = pop(currentModuleRecord);
	Value receiver = pop(currentModuleRecord);

	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		ObjectString *name = READ_STRING();
		runtime_panic(currentModuleRecord, TYPE,
					  "Cannot set property '%s' on non struct instance "
					  "value. %s",
					  name->chars, type_error_message(vm, receiver, "struct instance"));
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectString *name = READ_STRING();
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(currentModuleRecord, NAME, "Property '%s' does not exist on struct '%s'.", name->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	instance->fields[(uint16_t)AS_INT(indexValue)] = valueToSet;
	push(currentModuleRecord, valueToSet);

	DISPATCH();
}

OP_INVOKE: {
	ObjectString *methodName = READ_STRING();
	int arg_count = READ_SHORT();
	if (!invoke(vm, methodName, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &currentModuleRecord->frames[currentModuleRecord->frame_count - 1];
	DISPATCH();
}

OP_ARRAY: {
	uint16_t elementCount = READ_SHORT();
	ObjectArray *array = new_array(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		array_add(vm, array, pop(currentModuleRecord), i);
	}
	push(currentModuleRecord, OBJECT_VAL(array));
	DISPATCH();
}

OP_GET_COLLECTION: {
	Value indexValue = pop(currentModuleRecord);
	if (!IS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))) {
		runtime_panic(currentModuleRecord, TYPE, "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (AS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))->type) {
	case OBJECT_TABLE: {
		if (IS_CRUX_HASHABLE(indexValue)) {
			ObjectTable *table = AS_CRUX_TABLE(PEEK(currentModuleRecord, 0));
			Value value;
			if (!object_table_get(table->entries, table->size, table->capacity, indexValue, &value)) {
				runtime_panic(currentModuleRecord, COLLECTION_GET, "Failed to get value from table");
				return INTERPRET_RUNTIME_ERROR;
			}
			pop_push(currentModuleRecord, value);
		} else {
			runtime_panic(currentModuleRecord, TYPE, "Key cannot be hashed.", READ_STRING());
			return INTERPRET_RUNTIME_ERROR;
		}
		DISPATCH();
	}
	case OBJECT_ARRAY: {
		if (!IS_INT(indexValue)) {
			char buf[64];
			type_mask_name(get_type_mask(indexValue), buf, sizeof(buf));
			runtime_panic(currentModuleRecord, TYPE,
						  "Index must be of type 'Int' but "
						  "got type '%s'.",
						  buf);
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectArray *array = AS_CRUX_ARRAY(PEEK(currentModuleRecord, 0));

		if (index >= array->size) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
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
			runtime_panic(currentModuleRecord, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectString *string = AS_CRUX_STRING(PEEK(currentModuleRecord, 0));
		ObjectString *ch;
		if (index >= string->code_point_length) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		// Only single character indexing
		ch = copy_string(vm, string->chars + index, 1);
		pop_push(currentModuleRecord,
				 OBJECT_VAL(ch)); // pop the string off the stack, push the character onto the stack
		DISPATCH();
	}
	case OBJECT_TUPLE: {
		if (!IS_INT(indexValue)) {
			runtime_panic(currentModuleRecord, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectTuple *tuple = AS_CRUX_TUPLE(PEEK(currentModuleRecord, 0));
		if (index >= tuple->size) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		Value value = tuple->elements[index];
		pop_push(currentModuleRecord, value); // pop the tuple off the stack, push the value onto the stack
		DISPATCH();
	}
	case OBJECT_BUFFER: {
		if (!IS_INT(indexValue)) {
			runtime_panic(currentModuleRecord, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectBuffer *buffer = AS_CRUX_BUFFER(PEEK(currentModuleRecord, 0));
		if (index >= (buffer->write_pos - buffer->read_pos)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		Value value = AS_INT(buffer->data[index + buffer->read_pos]);
		pop_push(currentModuleRecord, value); // pop the buffer off the stack, push the value onto the stack
		DISPATCH();
	}

	default: {
		runtime_panic(currentModuleRecord, TYPE, "Cannot get from a non-collection type.");
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
				runtime_panic(currentModuleRecord, COLLECTION_GET, "Failed to set value in table");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(currentModuleRecord, TYPE, "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}

	case OBJECT_ARRAY: {
		ObjectArray *array = AS_CRUX_ARRAY(collection);
		int index = AS_INT(indexValue);
		if (!array_set(vm, array, index, value)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Cannot set a value in an empty array.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}
	default: {
		runtime_panic(currentModuleRecord, TYPE, "Value is not a mutable collection type.");
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
	uint16_t slot = READ_SHORT();
	Value currentValue = frame->slots[slot];
	Value operandValue = PEEK(currentModuleRecord, 0); // Right-hand side

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
		runtime_panic(currentModuleRecord, TYPE, "Operands for '/=' must be numbers.");
		return INTERPRET_RUNTIME_ERROR;
	}

	Value resultValue;

	double dcurrent = currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
	double doperand = operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);

	if (doperand == 0.0) {
		runtime_panic(currentModuleRecord, MATH, "Division by zero in '/=' assignment.");
		return INTERPRET_RUNTIME_ERROR;
	}

	resultValue = FLOAT_VAL(dcurrent / doperand);
	frame->slots[slot] = resultValue;
	DISPATCH();
}

OP_SET_LOCAL_STAR: {
	uint16_t slot = READ_SHORT();
	Value currentValue = frame->slots[slot];
	Value operandValue = PEEK(currentModuleRecord, 0);

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
		runtime_panic(currentModuleRecord, TYPE, "Operands for '*=' must be numbers.");
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
		double dcurrent = currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
		double doperand = operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);
		resultValue = FLOAT_VAL(dcurrent * doperand);
	}

	frame->slots[slot] = resultValue;
	DISPATCH();
}

OP_SET_LOCAL_PLUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, &frame->slots[slot], PEEK(currentModuleRecord, 0),
									OP_SET_LOCAL_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_MINUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, &frame->slots[slot], PEEK(currentModuleRecord, 0),
									OP_SET_LOCAL_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_SLASH: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_SLASH)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_STAR: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_STAR)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_PLUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MINUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_SLASH: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_SLASH, "/=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_STAR: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_STAR, "*=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_PLUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_PLUS, "+=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MINUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_MINUS, "-=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_TABLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectTable *table = new_object_table(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		Value value = pop(currentModuleRecord);
		Value key = pop(currentModuleRecord);
		if (IS_CRUX_HASHABLE(key)) {
			if (!object_table_set(vm, table, key, value)) {
				runtime_panic(currentModuleRecord, COLLECTION_SET, "Failed to set value in table.");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(currentModuleRecord, TYPE, "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	push(currentModuleRecord, OBJECT_VAL(table));
	DISPATCH();
}

OP_SET: {
	uint16_t elementCount = READ_SHORT();
	ObjectSet *set = new_set(vm, elementCount);
	push(currentModuleRecord, OBJECT_VAL(set));
	for (int i = elementCount - 1; i >= 0; i--) {
		(void)i;
		Value value = currentModuleRecord->stack_top[-2];
		currentModuleRecord->stack_top[-2] = currentModuleRecord->stack_top[-1];
		currentModuleRecord->stack_top--;
		if (!set_add_value(vm, set, value)) {
			pop(currentModuleRecord); // set
			runtime_panic(currentModuleRecord, TYPE, "All set elements must be hashable.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	DISPATCH();
}

OP_TUPLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectTuple *tuple = new_tuple(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		tuple->elements[i] = pop(currentModuleRecord);
	}
	push(currentModuleRecord, OBJECT_VAL(tuple));
	DISPATCH();
}

OP_RANGE: {
	Value end_value = pop(currentModuleRecord);
	Value step_value = pop(currentModuleRecord);
	Value start_value = pop(currentModuleRecord);

	if (!IS_INT(start_value) || !IS_INT(step_value) || !IS_INT(end_value)) {
		runtime_panic(currentModuleRecord, TYPE, "Range literal operands must be Int values.");
		return INTERPRET_RUNTIME_ERROR;
	}

	int32_t start = AS_INT(start_value);
	int32_t step = AS_INT(step_value);
	int32_t end = AS_INT(end_value);
	const char *error_message = NULL;
	if (!validate_range_values(start, step, end, &error_message)) {
		runtime_panic(currentModuleRecord, VALUE, "%s", error_message);
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectRange *range = new_range(vm, start, end, step);
	push(currentModuleRecord, OBJECT_VAL(range));
	DISPATCH();
}

OP_ANON_FUNCTION: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(currentModuleRecord, OBJECT_VAL(closure));
	for (int i = 0; i < closure->upvalue_count; i++) {
		uint16_t isLocal = READ_SHORT();
		uint16_t index = READ_SHORT();

		if (isLocal) {
			closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
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
	uint16_t slot = READ_SHORT();
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
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_INT_DIVIDE, "\\=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MODULUS: {
	ObjectString *name = READ_STRING();
	if (global_compound_operation(vm, name, OP_SET_GLOBAL_MODULUS, "%=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_INT_DIVIDE: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, &frame->slots[slot], PEEK(currentModuleRecord, 0),
									OP_SET_LOCAL_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}
OP_SET_LOCAL_MODULUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, &frame->slots[slot], PEEK(currentModuleRecord, 0),
									OP_SET_LOCAL_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_INT_DIVIDE: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MODULUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(currentModuleRecord, frame->closure->upvalues[slot]->location,
									PEEK(currentModuleRecord, 0), OP_SET_UPVALUE_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_USE_NATIVE: {
	uint16_t nameCount = READ_SHORT();
	ObjectString *names[UINT8_MAX] = {0};
	ObjectString *aliases[UINT8_MAX] = {0};

	for (uint16_t i = 0; i < nameCount; i++) {
		names[i] = READ_STRING();
	}
	for (uint16_t i = 0; i < nameCount; i++) {
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
		runtime_panic(currentModuleRecord, IMPORT, "Module '%s' not found.", moduleName->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	Table *moduleTable = vm->native_modules.modules[moduleIndex].names;
	for (int i = 0; i < nameCount; i++) {
		Value value;
		bool getSuccess = table_get(moduleTable, names[i], &value);
		if (!getSuccess) {
			runtime_panic(currentModuleRecord, IMPORT, "Failed to import '%s' from '%s'.", names[i]->chars,
						  moduleName->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		push(currentModuleRecord, OBJECT_VAL(value));
		bool setSuccess = table_set(vm, &vm->current_module_record->globals, aliases[i], value);

		if (!setSuccess) {
			runtime_panic(currentModuleRecord, IMPORT, "Failed to import '%s' from '%s'.", names[i]->chars,
						  moduleName->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		pop(currentModuleRecord);
	}

	DISPATCH();
}

OP_USE_MODULE: {
	// resolved by the compiler
	ObjectString *resolvedPath = READ_STRING();

	if (is_in_import_stack(vm, resolvedPath)) {
		runtime_panic(currentModuleRecord, IMPORT, "Circular dependency detected when importing: %s",
					  resolvedPath->chars);
		vm->current_module_record->state = STATE_ERROR;
		return INTERPRET_RUNTIME_ERROR;
	}

	Value cachedModule;
	if (table_get(&vm->module_cache, resolvedPath, &cachedModule)) {
		ObjectModuleRecord *module = AS_CRUX_MODULE_RECORD(cachedModule);

		if (module->state == STATE_LOADED) {
			push_import_stack(vm, resolvedPath);
			ObjectModuleRecord *previousModuleRecord = vm->current_module_record;
			vm->current_module_record = module;

			// Initialize runtime globals
			init_table(&module->globals);
			init_table(&module->publics);

			for (int i = 0; i < vm->core_fns.capacity; i++) {
				if (vm->core_fns.entries[i].key != NULL) {
					table_set(vm, &module->globals, vm->core_fns.entries[i].key, vm->core_fns.entries[i].value);
				}
			}

			// Execute the module code
			push(module, OBJECT_VAL(module->module_closure));
			call(module, module->module_closure, 0);

			InterpretResult result = run(vm, false);

			vm->current_module_record = previousModuleRecord;
			pop_import_stack(vm);

			if (result != INTERPRET_OK) {
				module->state = STATE_ERROR;
				return result;
			}
			module->state = STATE_EXECUTED;
		}
		push(currentModuleRecord, OBJECT_VAL(module));
		DISPATCH();
	}

	// dynamic import (dynuse)
	if (vm->import_count + 1 > IMPORT_MAX) {
		runtime_panic(currentModuleRecord, IMPORT, "Import limit reached");
		return INTERPRET_RUNTIME_ERROR;
	}
	vm->import_count++;

	FileResult file = read_file(resolvedPath->chars);
	if (file.error != NULL) {
		runtime_panic(currentModuleRecord, IO, file.error);
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectModuleRecord *module = new_object_module_record(vm, resolvedPath, false, false);
	module->enclosing_module = vm->current_module_record;
	reset_stack(module);
	push_import_stack(vm, resolvedPath);

	ObjectModuleRecord *previousModuleRecord = vm->current_module_record;
	vm->current_module_record = module;

	init_table(&module->globals);
	init_table(&module->publics);

	if (!initialize_std_lib(vm)) {
		module->state = STATE_ERROR;
		return INTERPRET_RUNTIME_ERROR;
	}

	Compiler compiler;
	ObjectFunction *function = compile(vm, &compiler, vm->main_compiler, file.content);
	free_file_result(file);

	if (function == NULL) {
		module->state = STATE_ERROR;
		return INTERPRET_COMPILE_ERROR;
	}

	ObjectClosure *closure = new_closure(vm, function);
	module->module_closure = closure;
	table_set(vm, &vm->module_cache, resolvedPath, OBJECT_VAL(module));

	push(module, OBJECT_VAL(closure));
	call(module, closure, 0);

	InterpretResult result = run(vm, false);

	vm->current_module_record = previousModuleRecord;
	pop_import_stack(vm);

	if (result != INTERPRET_OK) {
		module->state = STATE_ERROR;
		return result;
	}

	module->state = STATE_LOADED;
	push(currentModuleRecord, OBJECT_VAL(module));

	DISPATCH();
}

OP_FINISH_USE: {
	uint16_t nameCount = READ_SHORT();
	ObjectString *names[UINT8_MAX] = {0};
	ObjectString *aliases[UINT8_MAX] = {0};

	for (int i = 0; i < nameCount; i++)
		names[i] = READ_STRING();
	for (int i = 0; i < nameCount; i++)
		aliases[i] = READ_STRING();

	Value moduleValue = pop(currentModuleRecord);
	if (!IS_CRUX_MODULE_RECORD(moduleValue)) {
		runtime_panic(currentModuleRecord, RUNTIME, "Stack corrupted during import.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectModuleRecord *importedModule = AS_CRUX_MODULE_RECORD(moduleValue);

	if (importedModule->state == STATE_ERROR) {
		runtime_panic(currentModuleRecord, IMPORT, "Failed to import module from %s", importedModule->path->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	// copy exported names into the current module's globals
	for (uint16_t i = 0; i < nameCount; i++) {
		ObjectString *name = names[i];
		ObjectString *alias = aliases[i];

		Value value;
		if (!table_get(&importedModule->publics, name, &value)) {
			runtime_panic(currentModuleRecord, IMPORT, "'%s' is not an exported name.", name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}

		if (!table_set(vm, &vm->current_module_record->globals, alias, value)) {
			runtime_panic(currentModuleRecord, IMPORT, "Failed to import '%s'. Name already in use.", name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
	}

	// Only for dynamic imports
	if (vm->import_count > 0)
		vm->import_count--;

	DISPATCH();
}

OP_TYPEOF: {
	Value value = PEEK(currentModuleRecord, 0);
	Value typeValue = typeof_value(vm, value);
	pop(currentModuleRecord);
	push(currentModuleRecord, typeValue);
	DISPATCH();
}

OP_STRUCT: {
	ObjectStruct *structObject = AS_CRUX_STRUCT(READ_CONSTANT());
	push(currentModuleRecord, OBJECT_VAL(structObject));
	DISPATCH();
}

OP_STRUCT_INSTANCE_START: {
	Value value = PEEK(currentModuleRecord, 0);
	ObjectStruct *objectStruct = AS_CRUX_STRUCT(value);
	ObjectStructInstance *structInstance = new_struct_instance(vm, objectStruct, objectStruct->fields.count);
	pop(currentModuleRecord); // struct type
	if (!pushStructStack(vm, structInstance)) {
		runtime_panic(currentModuleRecord, RUNTIME, "Failed to push struct onto stack.");
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_STRUCT_NAMED_FIELD: {
	ObjectStructInstance *structInstance = peek_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(currentModuleRecord, RUNTIME, "Failed to get struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *fieldName = READ_STRING();

	ObjectStruct *structType = structInstance->struct_type;
	Value indexValue;
	if (!table_get(&structType->fields, fieldName, &indexValue)) {
		runtime_panic(currentModuleRecord, RUNTIME, "Field '%s' does not exist on strut type '%s'.", fieldName->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	uint16_t index = (uint16_t)AS_INT(indexValue);
	structInstance->fields[index] = pop(currentModuleRecord);
	DISPATCH();
}

OP_STRUCT_INSTANCE_END: {
	ObjectStructInstance *structInstance = pop_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(currentModuleRecord, RUNTIME, "Failed to pop struct from stack.");
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
	frame = &currentModuleRecord->frames[currentModuleRecord->frame_count - 1];

	if (is_anonymous_frame)
		return INTERPRET_OK;
	DISPATCH();
}

OP_UNWRAP: {
	Value value = pop(currentModuleRecord);
	if (!IS_CRUX_RESULT(value)) {
		runtime_panic(currentModuleRecord, TYPE, "Only the 'result' type supports unwrapping.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectResult *result = AS_CRUX_RESULT(value);
	if (result->is_ok) {
		push(currentModuleRecord, result->as.value);
	} else {
		ObjectError *error = result->as.error;
		runtime_panic(currentModuleRecord, RUNTIME, "Panic: %s", error->message->chars);
	}
	DISPATCH();
}

OP_PANIC: {
	Value value = pop(currentModuleRecord);
	ObjectString *message = to_string(vm, value);
	runtime_panic(vm->current_module_record, RUNTIME, "Panic --- %s", message->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_BITWISE_AND: {
	Value left = pop(currentModuleRecord);
	Value right = pop(currentModuleRecord);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(currentModuleRecord, TYPE, "Bitwise AND operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(currentModuleRecord, INT_VAL(INT_VAL(left) & INT_VAL(right)));
	DISPATCH();
}

OP_BITWISE_XOR: {
	Value left = pop(currentModuleRecord);
	Value right = pop(currentModuleRecord);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(currentModuleRecord, TYPE, "Bitwise XOR operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(currentModuleRecord, INT_VAL(INT_VAL(left) ^ INT_VAL(right)));
	DISPATCH();
}

OP_BITWISE_OR: {
	Value left = pop(currentModuleRecord);
	Value right = pop(currentModuleRecord);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(currentModuleRecord, TYPE, "Bitwise OR operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(currentModuleRecord, INT_VAL(INT_VAL(left) | INT_VAL(right)));
	DISPATCH();
}

OP_METHOD: {
	ObjectString *method_name = READ_STRING();
	Value method_closure = PEEK(currentModuleRecord, 0);
	Value struct_val = PEEK(currentModuleRecord, 1);

	ObjectStruct *struct_obj = AS_CRUX_STRUCT(struct_val);
	table_set(vm, &struct_obj->methods, method_name, method_closure);

	pop(currentModuleRecord); // closure
	DISPATCH();
}

OP_SET_PROPERTY_PLUS:
OP_SET_PROPERTY_MINUS:
OP_SET_PROPERTY_STAR:
OP_SET_PROPERTY_SLASH:
OP_SET_PROPERTY_INT_DIVIDE:
OP_SET_PROPERTY_MODULUS: {
	ObjectString *name = READ_STRING();
	Value operand = pop(currentModuleRecord);
	Value instance_val = PEEK(currentModuleRecord, 0);

	if (!IS_CRUX_STRUCT_INSTANCE(instance_val)) {
		runtime_panic(currentModuleRecord, TYPE, "Only instances have properties.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(instance_val);

	Value indexValue;
	if (table_get(&instance->struct_type->fields, name, &indexValue)) {
		uint16_t index = (uint16_t)AS_INT(indexValue);
		Value current_val = instance->fields[index];

		OpCode math_op;
		if (instruction == OP_SET_PROPERTY_PLUS)
			math_op = OP_SET_LOCAL_PLUS;
		else if (instruction == OP_SET_PROPERTY_MINUS)
			math_op = OP_SET_LOCAL_MINUS;
		else if (instruction == OP_SET_PROPERTY_STAR)
			math_op = OP_SET_LOCAL_STAR;
		else if (instruction == OP_SET_PROPERTY_SLASH)
			math_op = OP_SET_LOCAL_SLASH;
		else if (instruction == OP_SET_PROPERTY_INT_DIVIDE)
			math_op = OP_SET_LOCAL_INT_DIVIDE;
		else
			math_op = OP_SET_LOCAL_MODULUS;

		if (!handle_compound_assignment(currentModuleRecord, &current_val, operand, math_op)) {
			return INTERPRET_RUNTIME_ERROR;
		}

		instance->fields[index] = current_val;

		pop(currentModuleRecord);
		push(currentModuleRecord, current_val);
		DISPATCH();
	}

	runtime_panic(currentModuleRecord, NAME, "Undefined property '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_BITWISE_NOT: {
	Value value = pop(currentModuleRecord);
	if (!IS_INT(value)) {
		runtime_panic(currentModuleRecord, TYPE, "Bitwise NOT operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	int32_t int_val = AS_INT(value);
	push(currentModuleRecord, INT_VAL(~int_val));
	DISPATCH();
}

OP_TYPE_COERCE: {
	Value value = READ_CONSTANT();
	ObjectTypeRecord *type_record = AS_CRUX_TYPE_RECORD(value);
	Value query = PEEK(currentModuleRecord, 0);
	if (!runtime_types_compatible(type_record->base_type, query)) {
		char type_name[128];
		type_record_name(type_record, type_name, 128);
		runtime_panic(currentModuleRecord, TYPE, "Failed to perform type coercion. Expected type: '%s'.", type_name);
		return INTERPRET_RUNTIME_ERROR;
	}
	// if the type matched then continue on like nothing happened
	DISPATCH();
}

OP_GET_SLICE: {
	Value range_value = pop(currentModuleRecord);
	ObjectRange *range = AS_CRUX_RANGE(range_value);
	if (!IS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))) {
		runtime_panic(currentModuleRecord, TYPE, "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (AS_CRUX_OBJECT(PEEK(currentModuleRecord, 0))->type) {
	case OBJECT_ARRAY: {
		ObjectArray *array = AS_CRUX_ARRAY(PEEK(currentModuleRecord, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, array->size)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectArray *slice = new_array(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			// okay to not check return, we allocated enough capacity
			Value to_add = array->values[range->start + i * range->step];
			array_add_back(vm, slice, to_add);
		}
		pop_push(currentModuleRecord, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_STRING: {
		ObjectString *string = AS_CRUX_STRING(PEEK(currentModuleRecord, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, string->code_point_length)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		const utf8_int8_t **codepoint_starts = NULL;
		if (!collect_string_codepoint_starts(vm, string, &codepoint_starts)) {
			runtime_panic(currentModuleRecord, MEMORY, "Out of memory.");
			return INTERPRET_RUNTIME_ERROR;
		}

		char *buffer = ALLOCATE(vm, char, string->byte_length + 1);
		if (buffer == NULL) {
			FREE_ARRAY(vm, const utf8_int8_t *, codepoint_starts, string->code_point_length + 1);
			runtime_panic(currentModuleRecord, MEMORY, "Out of memory.");
			return INTERPRET_RUNTIME_ERROR;
		}

		uint32_t buffer_write_index = 0;
		int32_t index = range->start;
		for (uint32_t i = 0; i < len; i++, index += range->step) {
			const uint32_t current_index = (uint32_t)index;
			const uint32_t codepoint_size = (uint32_t)(codepoint_starts[current_index + 1] -
													   codepoint_starts[current_index]);
			memcpy(buffer + buffer_write_index, codepoint_starts[current_index], codepoint_size);
			buffer_write_index += codepoint_size;
		}
		buffer[buffer_write_index] = '\0';
		FREE_ARRAY(vm, const utf8_int8_t *, codepoint_starts, string->code_point_length + 1);

		char *resized = GROW_ARRAY(vm, char, buffer, string->byte_length + 1, buffer_write_index + 1);
		if (resized == NULL) {
			FREE_ARRAY(vm, char, buffer, string->byte_length + 1);
			runtime_panic(currentModuleRecord, MEMORY, "Out of memory.");
			return INTERPRET_RUNTIME_ERROR;
		}
		buffer = resized;

		ObjectString *slice = take_string(vm, buffer, buffer_write_index);
		pop_push(currentModuleRecord, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_TUPLE: {
		ObjectTuple *tuple = AS_CRUX_TUPLE(PEEK(currentModuleRecord, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, tuple->size)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectTuple *slice = new_tuple(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			slice->elements[i] = tuple->elements[range->start + i * range->step];
		}
		pop_push(currentModuleRecord, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_BUFFER: {
		ObjectBuffer *buffer = AS_CRUX_BUFFER(PEEK(currentModuleRecord, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, buffer->write_pos - buffer->read_pos)) {
			runtime_panic(currentModuleRecord, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectArray *slice = new_array(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			const uint32_t index = (uint32_t)(range->start + i * range->step);
			slice->values[i] = INT_VAL(buffer->data[buffer->read_pos + index]);
		}
		slice->size = len;
		pop_push(currentModuleRecord, OBJECT_VAL(slice));
		DISPATCH();
	}

	default: {
		runtime_panic(currentModuleRecord, TYPE, "Can only slice 'Array | String | Tuple | Buffer'");
		return INTERPRET_RUNTIME_ERROR;
	}
	}
	DISPATCH();
}

// value in collection
OP_IN: {
	Value right = pop(currentModuleRecord);
	Value left = pop(currentModuleRecord);

	ObjectType right_type = AS_CRUX_OBJECT(right)->type;
	ObjectType left_type = AS_CRUX_OBJECT(left)->type;

	if (IS_CRUX_OBJECT(right)) {
		switch (right_type) {
		case OBJECT_ARRAY: {
			ObjectArray *array = AS_CRUX_ARRAY(right);
			bool found = false;
			for (uint32_t i = 0; i < array->size; i++) {
				if (values_equal(left, array->values[i])) {
					found = true;
					break;
				}
			}
			push(currentModuleRecord, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_STRING: {
			ObjectString *str = AS_CRUX_STRING(right);
			if (!IS_CRUX_STRING(left)) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}
			ObjectString *goal = AS_CRUX_STRING(left);
			if (goal->byte_length == 0) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}

			bool found = utf8str(str->chars, goal->chars) != NULL;
			push(currentModuleRecord, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_TUPLE: {
			ObjectTuple *tuple = AS_CRUX_TUPLE(right);
			bool found = false;
			for (uint32_t i = 0; i < tuple->size; i++) {
				if (values_equal(left, tuple->elements[i])) {
					found = true;
					break;
				}
			}
			push(currentModuleRecord, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_BUFFER: {
			ObjectBuffer *buffer = AS_CRUX_BUFFER(right);
			if (!IS_INT(left)) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}
			bool found = false;
			uint8_t int_val = (uint8_t)AS_INT(left);
			for (uint32_t i = buffer->read_pos; i < buffer->write_pos; i++) {
				if (buffer->data[i] == int_val) {
					found = true;
					break;
				}
			}

			push(currentModuleRecord, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_SET: {
			ObjectSet *set = AS_CRUX_SET(right);
			if (object_table_contains_key(set->entries, left)) {
				push(currentModuleRecord, TRUE_VAL);
			} else {
				push(currentModuleRecord, FALSE_VAL);
			}
			break;
		}
		case OBJECT_RANGE: {
			ObjectRange *range = AS_CRUX_RANGE(right);
			if (!IS_INT(left)) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}
			int32_t int_val = AS_INT(left);
			push(currentModuleRecord, range_contains(range, int_val) ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_TABLE: {
			ObjectTable *table = AS_CRUX_TABLE(right);
			if (object_table_contains_key(table, left)) {
				push(currentModuleRecord, TRUE_VAL);
			} else {
				push(currentModuleRecord, FALSE_VAL);
			}
			break;
		}
		case OBJECT_VECTOR: {
			ObjectVector *vector = AS_CRUX_VECTOR(right);
			if (vector->dimensions == 0) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}
			if (!IS_INT(left) && !IS_FLOAT(left)) {
				push(currentModuleRecord, FALSE_VAL);
				break;
			}
			double double_val = TO_DOUBLE(left);
			if (vector->dimensions > STATIC_VECTOR_SIZE) {
				for (uint32_t i = 0; i < vector->dimensions; i++) {
					if (vector->as.h_components[i] != double_val) {
						push(currentModuleRecord, FALSE_VAL);
						break;
					}
				}
			} else {
				for (uint32_t i = 0; i < vector->dimensions; i++) {
					if (vector->as.s_components[i] != double_val) {
						push(currentModuleRecord, FALSE_VAL);
						break;
					}
				}
			}
			break;
		}
		default: {
			runtime_panic(currentModuleRecord, TYPE,
						  "Can only check 'in' for 'Array | String | Tuple | Buffer | Set | Range | Table'");
			return INTERPRET_RUNTIME_ERROR;
		}
		}
	}

	DISPATCH();
}

OP_OK: {
	Value value = PEEK(currentModuleRecord, 0);
	ObjectResult *result = new_ok_result(vm, value);
	pop(currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(result));
	DISPATCH();
}

OP_ERR: {
	Value err = PEEK(currentModuleRecord, 0);
	ObjectError *error = new_error(vm, to_string(vm, err), RUNTIME, false);
	push(currentModuleRecord, OBJECT_VAL(error));
	ObjectResult *result = new_error_result(vm, error);
	pop(currentModuleRecord);
	pop(currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(result));
	DISPATCH();
}

OP_SOME: {
	Value value = PEEK(currentModuleRecord, 0);
	ObjectOption *some = new_option(vm, value, true);
	pop(currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(some));
	DISPATCH();
}

OP_NONE: {
	ObjectOption *none = new_option(vm, NIL_VAL, false);
	push(currentModuleRecord, OBJECT_VAL(none));
	DISPATCH();
}

end: {
	printf("        ");
	for (Value *slot = currentModuleRecord->stack; slot < currentModuleRecord->stack_top; slot++) {
		printf("[");
		print_value(*slot, false);
		printf("]");
	}
	printf("\n");

	disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));

	instruction = READ_SHORT();
	goto *dispatchTable[instruction];
}

#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}
