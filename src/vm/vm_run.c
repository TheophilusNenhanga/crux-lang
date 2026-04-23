#include <math.h>
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
#include "stdlib/complex.h"
#include "stdlib/matrix.h"
#include "stdlib/range.h"
#include "stdlib/set.h"

#ifdef DEBUG_TRACE_EXECUTION
#define DISPATCH() goto *dispatchTable[endIndex]
#else
#define DISPATCH()                                                                                                     \
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
	register ObjectModuleRecord *current_module_record = vm->current_module_record;
	register CallFrame *frame = &current_module_record->frames[current_module_record->frame_count - 1];

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
									&&OP_GET_PROPERTY_INDEX,
									&&OP_SET_PROPERTY_INDEX,
									&&OP_SET_PROPERTY_PLUS_INDEX,
									&&OP_SET_PROPERTY_MINUS_INDEX,
									&&OP_SET_PROPERTY_STAR_INDEX,
									&&OP_SET_PROPERTY_SLASH_INDEX,
									&&OP_SET_PROPERTY_INT_DIVIDE_INDEX,
									&&OP_SET_PROPERTY_MODULUS_INDEX,
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
									&&OP_OPTION_MATCH_SOME,
									&&OP_OPTION_MATCH_NONE,
									&&OP_TYPE_MATCH,
									&&OP_ADD_INT,
									&&OP_ADD_NUM,
									&&OP_SUBTRACT_INT,
									&&OP_SUBTRACT_NUM,
									&&OP_MULTIPLY_INT,
									&&OP_MULTIPLY_NUM,
									&&OP_DIVIDE_NUM,
									&&OP_INT_DIVIDE_INT,
									&&OP_MODULUS_INT,
									&&OP_POWER_INT,
									&&OP_POWER_NUM,
									&&OP_ADD_VECTOR_VECTOR,
									&&OP_SUBTRACT_VECTOR_VECTOR,
									&&OP_MULTIPLY_VECTOR_VECTOR,
									&&OP_DIVIDE_VECTOR_VECTOR,
									&&OP_MULTIPLY_VECTOR_SCALAR,
									&&OP_MULTIPLY_SCALAR_VECTOR,
									&&OP_DIVIDE_VECTOR_SCALAR,
									&&OP_ADD_COMPLEX_COMPLEX,
									&&OP_SUBTRACT_COMPLEX_COMPLEX,
									&&OP_MULTIPLY_COMPLEX_COMPLEX,
									&&OP_DIVIDE_COMPLEX_COMPLEX,
									&&OP_MULTIPLY_COMPLEX_SCALAR,
									&&OP_MULTIPLY_SCALAR_COMPLEX,
									&&OP_DIVIDE_COMPLEX_SCALAR,
									&&OP_ADD_MATRIX_MATRIX,
									&&OP_SUBTRACT_MATRIX_MATRIX,
									&&OP_ADD_MATRIX_SCALAR,
									&&OP_ADD_SCALAR_MATRIX,
									&&OP_SUBTRACT_MATRIX_SCALAR,
									&&OP_SUBTRACT_SCALAR_MATRIX,
									&&OP_MULTIPLY_MATRIX_MATRIX,
									&&OP_MULTIPLY_MATRIX_SCALAR,
									&&OP_MULTIPLY_SCALAR_MATRIX,
									&&OP_DIVIDE_MATRIX_SCALAR,
									&&OP_INVOKE_STDLIB,
									&&OP_INVOKE_STDLIB_UNWRAP,
									&&OP_POP_N,
									&&OP_DEFINE_PUB_GLOBAL,
									&&OP_0_INT,
									&&OP_1_INT,
									&&OP_2_INT,
									&&OP_0_FLOAT,
									&&OP_1_FLOAT,
									&&OP_2_FLOAT,
									&&end};

	register uint16_t instruction;
#ifdef DEBUG_TRACE_EXECUTION
	static uint16_t endIndex = sizeof(dispatchTable) / sizeof(dispatchTable[0]) - 1;
#endif
	DISPATCH();
OP_RETURN: {
	Value result = pop(current_module_record);
	close_upvalues(current_module_record, frame->slots);
	current_module_record->frame_count--;
	if (current_module_record->frame_count == 0) {
		pop(current_module_record);
		return INTERPRET_OK;
	}
	current_module_record->stack_top = frame->slots;
	push(current_module_record, result);
	frame = &current_module_record->frames[current_module_record->frame_count - 1];

	if (is_anonymous_frame)
		return INTERPRET_OK;
	DISPATCH();
}

OP_CONSTANT: {
	Value constant = READ_CONSTANT();
	push(current_module_record, constant);
	DISPATCH();
}

OP_NIL: {
	push(current_module_record, NIL_VAL);
	DISPATCH();
}

OP_TRUE: {
	push(current_module_record, BOOL_VAL(true));
	DISPATCH();
}

OP_FALSE: {
	push(current_module_record, BOOL_VAL(false));
	DISPATCH();
}

OP_NEGATE: {
	Value operand = PEEK(current_module_record, 0);
	if (IS_INT(operand)) {
		int32_t iVal = AS_INT(operand);
		if (iVal == INT32_MIN) {
			pop_push(current_module_record, FLOAT_VAL(-(double)INT32_MIN));
		} else {
			pop_push(current_module_record, INT_VAL(-iVal));
		}
	} else if (IS_FLOAT(operand)) {
		pop_push(current_module_record, FLOAT_VAL(-AS_FLOAT(operand)));
	} else {
		pop(current_module_record);
		runtime_panic(current_module_record, TYPE, type_error_message(vm, operand, "int' | 'float"));
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_EQUAL: {
	Value b = pop(current_module_record);
	Value a = pop(current_module_record);
	push(current_module_record, BOOL_VAL(values_equal(a, b)));
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
	Value b = pop(current_module_record);
	Value a = pop(current_module_record);
	push(current_module_record, BOOL_VAL(!values_equal(a, b)));
	DISPATCH();
}

OP_ADD: {
	if (IS_CRUX_STRING(PEEK(current_module_record, 0)) && IS_CRUX_STRING(PEEK(current_module_record, 1))) {
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
	push(current_module_record, BOOL_VAL(is_falsy(pop(current_module_record))));
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
	pop(current_module_record);
	DISPATCH();
}

// uses module record from function to resolve names from imported modules
OP_DEFINE_GLOBAL: {
	uint16_t index = READ_SHORT();
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	frame_module_record->globals[index] = PEEK(current_module_record, 0);
	pop(current_module_record);
	DISPATCH();
}

OP_GET_GLOBAL: {
	uint16_t index = READ_SHORT();
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	Value value = frame_module_record->globals[index];
	push(current_module_record, value);
	DISPATCH();
}

OP_SET_GLOBAL: {
	uint16_t index = READ_SHORT();
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	frame_module_record->globals[index] = PEEK(current_module_record, 0);
	DISPATCH();
}

OP_GET_LOCAL: {
	uint16_t slot = READ_SHORT();
	push(current_module_record, frame->slots[slot]);
	DISPATCH();
}

OP_SET_LOCAL: {
	uint16_t slot = READ_SHORT();
	frame->slots[slot] = PEEK(current_module_record, 0);
	DISPATCH();
}

OP_ITER_INIT: {
	const Value iterable = PEEK(current_module_record, 0);
	Value iterator;
	if (!get_iterator_from_value(vm, iterable, &iterator)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top[-1] = iterator;
	DISPATCH();
}

OP_ITER_NEXT: {
	uint16_t offset = READ_SHORT();
	Value option;
	if (!get_next_option_from_iterator(vm, PEEK(current_module_record, 0), &option)) {
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectOption *next_option = AS_CRUX_OPTION(option);
	if (!next_option->is_some) {
		pop(current_module_record);
		frame->ip += offset; // jump to after the loop
		DISPATCH();
	}

	current_module_record->stack_top[-1] = next_option->value; // set the value to the next item
	DISPATCH();
}

OP_JUMP_IF_FALSE: {
	uint16_t offset = READ_SHORT();
	if (is_falsy(PEEK(current_module_record, 0))) {
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
	if (!call_value(vm, PEEK(current_module_record, arg_count), arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &current_module_record->frames[current_module_record->frame_count - 1];
	DISPATCH();
}

OP_CLOSURE: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(current_module_record, OBJECT_VAL(closure));

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
	push(current_module_record, *frame->closure->upvalues[slot]->location);
	DISPATCH();
}

OP_SET_UPVALUE: {
	uint16_t slot = READ_SHORT();
	*frame->closure->upvalues[slot]->location = PEEK(current_module_record, 0);
	DISPATCH();
}

OP_CLOSE_UPVALUE: {
	close_upvalues(current_module_record, current_module_record->stack_top - 1);
	pop(current_module_record);
	DISPATCH();
}

OP_GET_PROPERTY: {
	Value receiver = pop(current_module_record);
	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		runtime_panic(current_module_record, TYPE, "Cannot get property on non 'struct instance' type.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *name = READ_STRING();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	ObjectStruct *structType = instance->struct_type;

	Value indexValue;
	if (!table_get(&structType->fields, name, &indexValue)) {
		runtime_panic(current_module_record, NAME, "Property '%s' does not exist on struct '%s'.", name->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	push(current_module_record, instance->fields[(uint16_t)AS_INT(indexValue)]);
	DISPATCH();
}

OP_SET_PROPERTY: {
	Value valueToSet = pop(current_module_record);
	Value receiver = pop(current_module_record);

	if (!IS_CRUX_STRUCT_INSTANCE(receiver)) {
		ObjectString *name = READ_STRING();
		runtime_panic(current_module_record, TYPE,
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
		runtime_panic(current_module_record, NAME, "Property '%s' does not exist on struct '%s'.", name->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	instance->fields[(uint16_t)AS_INT(indexValue)] = valueToSet;
	push(current_module_record, valueToSet);

	DISPATCH();
}

OP_INVOKE: {
	ObjectString *methodName = READ_STRING();
	int arg_count = READ_SHORT();
	if (!invoke(vm, methodName, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	frame = &current_module_record->frames[current_module_record->frame_count - 1];
	DISPATCH();
}

OP_ARRAY: {
	uint16_t elementCount = READ_SHORT();
	ObjectArray *array = new_array(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		array_add(vm, array, pop(current_module_record), i);
	}
	push(current_module_record, OBJECT_VAL(array));
	DISPATCH();
}

OP_GET_COLLECTION: {
	Value indexValue = pop(current_module_record);
	if (!IS_CRUX_OBJECT(PEEK(current_module_record, 0))) {
		runtime_panic(current_module_record, TYPE, "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (AS_CRUX_OBJECT(PEEK(current_module_record, 0))->type) {
	case OBJECT_TABLE: {
		if (IS_CRUX_HASHABLE(indexValue)) {
			ObjectTable *table = AS_CRUX_TABLE(PEEK(current_module_record, 0));
			Value value;
			if (!object_table_get(table->entries, table->size, table->capacity, indexValue, &value)) {
				runtime_panic(current_module_record, COLLECTION_GET, "Failed to get value from table");
				return INTERPRET_RUNTIME_ERROR;
			}
			pop_push(current_module_record, value);
		} else {
			runtime_panic(current_module_record, TYPE, "Key cannot be hashed.", READ_STRING());
			return INTERPRET_RUNTIME_ERROR;
		}
		DISPATCH();
	}
	case OBJECT_ARRAY: {
		if (!IS_INT(indexValue)) {
			char buf[64];
			type_mask_name(get_type_mask(indexValue), buf, sizeof(buf));
			runtime_panic(current_module_record, TYPE,
						  "Index must be of type 'Int' but "
						  "got type '%s'.",
						  buf);
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectArray *array = AS_CRUX_ARRAY(PEEK(current_module_record, 0));

		if (index >= array->size) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		Value value = array->values[index];

		pop_push(current_module_record,
				 value); // pop the array off the stack // push the
						 // value onto the stack
		DISPATCH();
	}
	case OBJECT_STRING: {
		if (!IS_INT(indexValue)) {
			runtime_panic(current_module_record, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectString *string = AS_CRUX_STRING(PEEK(current_module_record, 0));
		ObjectString *ch;
		if (index >= string->code_point_length) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		// Only single character indexing
		ch = copy_string(vm, string->chars + index, 1);
		pop_push(current_module_record,
				 OBJECT_VAL(ch)); // pop the string off the stack, push the character onto the stack
		DISPATCH();
	}
	case OBJECT_TUPLE: {
		if (!IS_INT(indexValue)) {
			runtime_panic(current_module_record, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectTuple *tuple = AS_CRUX_TUPLE(PEEK(current_module_record, 0));
		if (index >= tuple->size) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		Value value = tuple->elements[index];
		pop_push(current_module_record, value); // pop the tuple off the stack, push the value onto the stack
		DISPATCH();
	}
	case OBJECT_BUFFER: {
		if (!IS_INT(indexValue)) {
			runtime_panic(current_module_record, TYPE, "Index must be of type 'int'.");
			return INTERPRET_RUNTIME_ERROR;
		}
		uint32_t index = (uint32_t)AS_INT(indexValue);
		ObjectBuffer *buffer = AS_CRUX_BUFFER(PEEK(current_module_record, 0));
		if (index >= (buffer->write_pos - buffer->read_pos)) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}
		Value value = AS_INT(buffer->data[index + buffer->read_pos]);
		pop_push(current_module_record, value); // pop the buffer off the stack, push the value onto the stack
		DISPATCH();
	}

	default: {
		runtime_panic(current_module_record, TYPE, "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	}
	DISPATCH();
}

OP_SET_COLLECTION: {
	Value value = pop(current_module_record);
	Value indexValue = PEEK(current_module_record, 0);
	Value collection = PEEK(current_module_record, 1);

	switch (AS_CRUX_OBJECT(collection)->type) {
	case OBJECT_TABLE: {
		ObjectTable *table = AS_CRUX_TABLE(collection);
		if (IS_INT(indexValue) || IS_CRUX_STRING(indexValue)) {
			if (!object_table_set(vm, table, indexValue, value)) {
				runtime_panic(current_module_record, COLLECTION_GET, "Failed to set value in table");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(current_module_record, TYPE, "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}

	case OBJECT_ARRAY: {
		ObjectArray *array = AS_CRUX_ARRAY(collection);
		int index = AS_INT(indexValue);
		if (!array_set(vm, array, index, value)) {
			runtime_panic(current_module_record, BOUNDS, "Cannot set a value in an empty array.");
			return INTERPRET_RUNTIME_ERROR;
		}
		break;
	}
	default: {
		runtime_panic(current_module_record, TYPE, "Value is not a mutable collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	}

	pop_two(current_module_record); // indexValue and collection
	push(current_module_record, indexValue);
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
	Value operandValue = PEEK(current_module_record, 0); // Right-hand side

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
		runtime_panic(current_module_record, TYPE, "Operands for '/=' must be numbers.");
		return INTERPRET_RUNTIME_ERROR;
	}

	Value resultValue;

	double dcurrent = currentIsFloat ? AS_FLOAT(currentValue) : (double)AS_INT(currentValue);
	double doperand = operandIsFloat ? AS_FLOAT(operandValue) : (double)AS_INT(operandValue);

	if (doperand == 0.0) {
		runtime_panic(current_module_record, MATH, "Division by zero in '/=' assignment.");
		return INTERPRET_RUNTIME_ERROR;
	}

	resultValue = FLOAT_VAL(dcurrent / doperand);
	frame->slots[slot] = resultValue;
	DISPATCH();
}

OP_SET_LOCAL_STAR: {
	uint16_t slot = READ_SHORT();
	Value currentValue = frame->slots[slot];
	Value operandValue = PEEK(current_module_record, 0);

	bool currentIsInt = IS_INT(currentValue);
	bool currentIsFloat = IS_FLOAT(currentValue);
	bool operandIsInt = IS_INT(operandValue);
	bool operandIsFloat = IS_FLOAT(operandValue);

	if (!((currentIsInt || currentIsFloat) && (operandIsInt || operandIsFloat))) {
		runtime_panic(current_module_record, TYPE, "Operands for '*=' must be numbers.");
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
	if (!handle_compound_assignment(current_module_record, &frame->slots[slot], PEEK(current_module_record, 0),
									OP_SET_LOCAL_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_MINUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, &frame->slots[slot], PEEK(current_module_record, 0),
									OP_SET_LOCAL_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_SLASH: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_SLASH)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_STAR: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_STAR)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_PLUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_PLUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MINUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_MINUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_SLASH: {
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_SLASH, "/=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_STAR: {
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_STAR, "*=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_PLUS: {
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_PLUS, "+=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MINUS: {
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_MINUS, "-=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_TABLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectTable *table = new_object_table(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		Value value = pop(current_module_record);
		Value key = pop(current_module_record);
		if (IS_CRUX_HASHABLE(key)) {
			if (!object_table_set(vm, table, key, value)) {
				runtime_panic(current_module_record, COLLECTION_SET, "Failed to set value in table.");
				return INTERPRET_RUNTIME_ERROR;
			}
		} else {
			runtime_panic(current_module_record, TYPE, "Key cannot be hashed.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	push(current_module_record, OBJECT_VAL(table));
	DISPATCH();
}

OP_SET: {
	uint16_t elementCount = READ_SHORT();
	ObjectSet *set = new_set(vm, elementCount);
	push(current_module_record, OBJECT_VAL(set));
	for (int i = elementCount - 1; i >= 0; i--) {
		(void)i;
		Value value = current_module_record->stack_top[-2];
		current_module_record->stack_top[-2] = current_module_record->stack_top[-1];
		current_module_record->stack_top--;
		if (!set_add_value(vm, set, value)) {
			pop(current_module_record); // set
			runtime_panic(current_module_record, TYPE, "All set elements must be hashable.");
			return INTERPRET_RUNTIME_ERROR;
		}
	}
	DISPATCH();
}

OP_TUPLE: {
	uint16_t elementCount = READ_SHORT();
	ObjectTuple *tuple = new_tuple(vm, elementCount);
	for (int i = elementCount - 1; i >= 0; i--) {
		tuple->elements[i] = pop(current_module_record);
	}
	push(current_module_record, OBJECT_VAL(tuple));
	DISPATCH();
}

OP_RANGE: {
	Value end_value = pop(current_module_record);
	Value step_value = pop(current_module_record);
	Value start_value = pop(current_module_record);

	if (!IS_INT(start_value) || !IS_INT(step_value) || !IS_INT(end_value)) {
		runtime_panic(current_module_record, TYPE, "Range literal operands must be Int values.");
		return INTERPRET_RUNTIME_ERROR;
	}

	int32_t start = AS_INT(start_value);
	int32_t step = AS_INT(step_value);
	int32_t end = AS_INT(end_value);
	const char *error_message = NULL;
	if (!validate_range_values(start, step, end, &error_message)) {
		runtime_panic(current_module_record, VALUE, "%s", error_message);
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectRange *range = new_range(vm, start, end, step);
	push(current_module_record, OBJECT_VAL(range));
	DISPATCH();
}

OP_ANON_FUNCTION: {
	ObjectFunction *function = AS_CRUX_FUNCTION(READ_CONSTANT());
	ObjectClosure *closure = new_closure(vm, function);
	push(current_module_record, OBJECT_VAL(closure));
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
	Value target = PEEK(current_module_record, 0);
	if (vm->match_handler_stack.count >= vm->match_handler_stack.capacity) {
		runtime_panic(current_module_record, RUNTIME, "Match nesting depth exceeded.");
		return INTERPRET_RUNTIME_ERROR;
	}
	vm->match_handler_stack.count++;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].match_target = target;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].is_match_target = true;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].match_bind = NIL_VAL;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].is_match_bind = false;
	DISPATCH();
}

OP_MATCH_JUMP: {
	uint16_t offset = READ_SHORT();
	Value pattern = pop(current_module_record);
	Value target = PEEK(current_module_record, 0);
	if (!values_equal(pattern, target)) {
		frame->ip += offset;
	}
	DISPATCH();
}

OP_MATCH_END: {
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].match_target = NIL_VAL;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].match_bind = NIL_VAL;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].is_match_bind = false;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].is_match_target = false;
	vm->match_handler_stack.count--;
	DISPATCH();
}

OP_RESULT_MATCH_OK: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(current_module_record, 0);
	if (!IS_CRUX_RESULT(target) || !AS_CRUX_RESULT(target)->is_ok) {
		frame->ip += offset;
	} else {
		Value value = AS_CRUX_RESULT(target)->as.value;
		pop_push(current_module_record, value);
	}
	DISPATCH();
}

OP_RESULT_MATCH_ERR: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(current_module_record, 0);
	if (!IS_CRUX_RESULT(target) || AS_CRUX_RESULT(target)->is_ok) {
		frame->ip += offset;
	} else {
		Value error = OBJECT_VAL(AS_CRUX_RESULT(target)->as.error);
		pop_push(current_module_record, error);
	}
	DISPATCH();
}

OP_RESULT_BIND: {
	uint16_t slot = READ_SHORT();
	Value bind = PEEK(current_module_record, 0);
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].match_bind = bind;
	vm->match_handler_stack.handlers[vm->match_handler_stack.count - 1].is_match_bind = true;
	frame->slots[slot] = bind;
	DISPATCH();
}

OP_OPTION_MATCH_SOME: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(current_module_record, 0);
	if (!IS_CRUX_OPTION(target) || !AS_CRUX_OPTION(target)->is_some) {
		frame->ip += offset;
	} else {
		Value value = AS_CRUX_OPTION(target)->value;
		pop_push(current_module_record, value);
	}
	DISPATCH();
}

OP_OPTION_MATCH_NONE: {
	uint16_t offset = READ_SHORT();
	Value target = PEEK(current_module_record, 0);
	if (!IS_CRUX_OPTION(target) || AS_CRUX_OPTION(target)->is_some) {
		frame->ip += offset;
	}
	DISPATCH();
}

OP_TYPE_MATCH: {
	TypeMask expected = (TypeMask)READ_SHORT();
	uint16_t offset = READ_SHORT();
	Value target = PEEK(current_module_record, 0);
	if (!runtime_types_compatible(expected, target)) {
		frame->ip += offset;
	}
	DISPATCH();
}

OP_GIVE: {
	Value result = pop(current_module_record);
	pop_push(current_module_record, result);
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
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_INT_DIVIDE, "\\=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_GLOBAL_MODULUS: {
	uint16_t index = READ_SHORT();
	if (global_compound_operation(vm, index, OP_SET_GLOBAL_MODULUS, "%=") == INTERPRET_RUNTIME_ERROR) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_LOCAL_INT_DIVIDE: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, &frame->slots[slot], PEEK(current_module_record, 0),
									OP_SET_LOCAL_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}
OP_SET_LOCAL_MODULUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, &frame->slots[slot], PEEK(current_module_record, 0),
									OP_SET_LOCAL_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_INT_DIVIDE: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_INT_DIVIDE)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_SET_UPVALUE_MODULUS: {
	uint16_t slot = READ_SHORT();
	if (!handle_compound_assignment(current_module_record, frame->closure->upvalues[slot]->location,
									PEEK(current_module_record, 0), OP_SET_UPVALUE_MODULUS)) {
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_USE_MODULE: {
	// resolved by the compiler
	ObjectString *resolvedPath = READ_STRING();

	if (is_in_import_stack(vm, resolvedPath)) {
		runtime_panic(current_module_record, IMPORT, "Circular dependency detected when importing: %s",
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

			// Allocate globals for the module
			if (module->global_count > 0 && module->globals == NULL) {
				module->globals = malloc(sizeof(Value) * module->global_count);
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
		push(current_module_record, OBJECT_VAL(module));
		DISPATCH();
	}

	// IF WE REACH HERE, IT MEANS THE COMPILER FAILED TO CACHE IT!
	runtime_panic(current_module_record, RUNTIME, "FATAL: Module '%s' was not statically compiled!",
				  resolvedPath->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_FINISH_USE: {
	uint16_t nameCount = READ_SHORT();

	Value moduleValue = pop(current_module_record);
	if (!IS_CRUX_MODULE_RECORD(moduleValue)) {
		runtime_panic(current_module_record, RUNTIME, "Stack corrupted during import.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectModuleRecord *importedModule = AS_CRUX_MODULE_RECORD(moduleValue);

	if (importedModule->state == STATE_ERROR) {
		runtime_panic(current_module_record, IMPORT, "Failed to import module from %s", importedModule->path->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	// Unpack the exported values
	for (uint16_t i = 0; i < nameCount; i++) {
		ObjectString *export_name = READ_STRING();
		uint16_t global_index = READ_SHORT();

		Value value;
		if (!table_get(&importedModule->publics, export_name, &value)) {
			runtime_panic(current_module_record, IMPORT, "'%s' is not an exported name.", export_name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}

		// 0xFFFF is a sentinel - SKIP FOR LOCAL ASSIGNMENT
		if (global_index == 0xFFFF) {
			push(current_module_record, value);
		} else {
			current_module_record->globals[global_index] = value;
		}
	}

	if (vm->import_count > 0)
		vm->import_count--;

	DISPATCH();
}

OP_TYPEOF: {
	Value value = PEEK(current_module_record, 0);
	Value typeValue = typeof_value(vm, value);
	pop(current_module_record);
	push(current_module_record, typeValue);
	DISPATCH();
}

OP_STRUCT: {
	ObjectStruct *structObject = AS_CRUX_STRUCT(READ_CONSTANT());
	push(current_module_record, OBJECT_VAL(structObject));
	DISPATCH();
}

OP_STRUCT_INSTANCE_START: {
	Value value = PEEK(current_module_record, 0);
	ObjectStruct *objectStruct = AS_CRUX_STRUCT(value);
	ObjectStructInstance *structInstance = new_struct_instance(vm, objectStruct, objectStruct->fields.count);
	pop(current_module_record); // struct type
	if (!pushStructStack(vm, structInstance)) {
		runtime_panic(current_module_record, RUNTIME, "Failed to push struct onto stack.");
		return INTERPRET_RUNTIME_ERROR;
	}
	DISPATCH();
}

OP_STRUCT_NAMED_FIELD: {
	ObjectStructInstance *structInstance = peek_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(current_module_record, RUNTIME, "Failed to get struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectString *fieldName = READ_STRING();

	ObjectStruct *structType = structInstance->struct_type;
	Value indexValue;
	if (!table_get(&structType->fields, fieldName, &indexValue)) {
		runtime_panic(current_module_record, RUNTIME, "Field '%s' does not exist on strut type '%s'.", fieldName->chars,
					  structType->name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}

	uint16_t index = (uint16_t)AS_INT(indexValue);
	structInstance->fields[index] = pop(current_module_record);
	DISPATCH();
}

OP_STRUCT_INSTANCE_END: {
	ObjectStructInstance *structInstance = pop_struct_stack(vm);
	if (structInstance == NULL) {
		runtime_panic(current_module_record, RUNTIME, "Failed to pop struct from stack.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(current_module_record, OBJECT_VAL(structInstance));
	DISPATCH();
}

OP_NIL_RETURN: {
	close_upvalues(current_module_record, frame->slots);
	current_module_record->frame_count--;
	if (current_module_record->frame_count == 0) {
		pop(current_module_record);
		return INTERPRET_OK;
	}
	current_module_record->stack_top = frame->slots;
	push(current_module_record, NIL_VAL);
	frame = &current_module_record->frames[current_module_record->frame_count - 1];

	if (is_anonymous_frame)
		return INTERPRET_OK;
	DISPATCH();
}

OP_UNWRAP: {
	Value value = pop(current_module_record);
	if (!IS_CRUX_RESULT(value)) {
		runtime_panic(current_module_record, TYPE, "Only the 'result' type supports unwrapping.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectResult *result = AS_CRUX_RESULT(value);
	if (result->is_ok) {
		push(current_module_record, result->as.value);
	} else {
		ObjectError *error = result->as.error;
		runtime_panic(current_module_record, RUNTIME, "Panic: %s", error->message->chars);
	}
	DISPATCH();
}

OP_PANIC: {
	Value value = pop(current_module_record);
	ObjectString *message = to_string(vm, value);
	runtime_panic(vm->current_module_record, RUNTIME, "Panic --- %s", message->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_BITWISE_AND: {
	Value left = pop(current_module_record);
	Value right = pop(current_module_record);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(current_module_record, TYPE, "Bitwise AND operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(current_module_record, INT_VAL(INT_VAL(left) & INT_VAL(right)));
	DISPATCH();
}

OP_BITWISE_XOR: {
	Value left = pop(current_module_record);
	Value right = pop(current_module_record);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(current_module_record, TYPE, "Bitwise XOR operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(current_module_record, INT_VAL(INT_VAL(left) ^ INT_VAL(right)));
	DISPATCH();
}

OP_BITWISE_OR: {
	Value left = pop(current_module_record);
	Value right = pop(current_module_record);
	if (!IS_INT(left) || !IS_INT(right)) {
		runtime_panic(current_module_record, TYPE, "Bitwise OR operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	push(current_module_record, INT_VAL(INT_VAL(left) | INT_VAL(right)));
	DISPATCH();
}

OP_METHOD: {
	ObjectString *method_name = READ_STRING();
	Value method_closure = PEEK(current_module_record, 0);
	Value struct_val = PEEK(current_module_record, 1);

	ObjectStruct *struct_obj = AS_CRUX_STRUCT(struct_val);
	table_set(vm, &struct_obj->methods, method_name, method_closure);

	pop(current_module_record); // closure
	DISPATCH();
}

OP_SET_PROPERTY_PLUS:
OP_SET_PROPERTY_MINUS:
OP_SET_PROPERTY_STAR:
OP_SET_PROPERTY_SLASH:
OP_SET_PROPERTY_INT_DIVIDE:
OP_SET_PROPERTY_MODULUS: {
	ObjectString *name = READ_STRING();
	Value operand = pop(current_module_record);
	Value instance_val = PEEK(current_module_record, 0);

	if (!IS_CRUX_STRUCT_INSTANCE(instance_val)) {
		runtime_panic(current_module_record, TYPE, "Only instances have properties.");
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

		if (!handle_compound_assignment(current_module_record, &current_val, operand, math_op)) {
			return INTERPRET_RUNTIME_ERROR;
		}

		instance->fields[index] = current_val;

		pop(current_module_record);
		push(current_module_record, current_val);
		DISPATCH();
	}

	runtime_panic(current_module_record, NAME, "Undefined property '%s'.", name->chars);
	return INTERPRET_RUNTIME_ERROR;
}

OP_GET_PROPERTY_INDEX: {
	Value receiver = pop(current_module_record);
	uint16_t index = READ_SHORT();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	push(current_module_record, instance->fields[index]);
	DISPATCH();
}

OP_SET_PROPERTY_INDEX: {
	Value valueToSet = pop(current_module_record);
	Value receiver = pop(current_module_record);
	uint16_t index = READ_SHORT();
	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(receiver);
	instance->fields[index] = valueToSet;
	push(current_module_record, valueToSet);
	DISPATCH();
}

OP_SET_PROPERTY_PLUS_INDEX:
OP_SET_PROPERTY_MINUS_INDEX:
OP_SET_PROPERTY_STAR_INDEX:
OP_SET_PROPERTY_SLASH_INDEX:
OP_SET_PROPERTY_INT_DIVIDE_INDEX:
OP_SET_PROPERTY_MODULUS_INDEX: {
	uint16_t index = READ_SHORT();
	Value operand = pop(current_module_record);
	Value instance_val = PEEK(current_module_record, 0);

	ObjectStructInstance *instance = AS_CRUX_STRUCT_INSTANCE(instance_val);
	Value current_val = instance->fields[index];

	OpCode math_op;
	if (instruction == OP_SET_PROPERTY_PLUS_INDEX)
		math_op = OP_SET_LOCAL_PLUS;
	else if (instruction == OP_SET_PROPERTY_MINUS_INDEX)
		math_op = OP_SET_LOCAL_MINUS;
	else if (instruction == OP_SET_PROPERTY_STAR_INDEX)
		math_op = OP_SET_LOCAL_STAR;
	else if (instruction == OP_SET_PROPERTY_SLASH_INDEX)
		math_op = OP_SET_LOCAL_SLASH;
	else if (instruction == OP_SET_PROPERTY_INT_DIVIDE_INDEX)
		math_op = OP_SET_LOCAL_INT_DIVIDE;
	else
		math_op = OP_SET_LOCAL_MODULUS;

	if (!handle_compound_assignment(current_module_record, &current_val, operand, math_op)) {
		return INTERPRET_RUNTIME_ERROR;
	}

	instance->fields[index] = current_val;

	pop(current_module_record);
	push(current_module_record, current_val);
	DISPATCH();
}

OP_BITWISE_NOT: {
	Value value = pop(current_module_record);
	if (!IS_INT(value)) {
		runtime_panic(current_module_record, TYPE, "Bitwise NOT operation requires type 'Int'.");
		return INTERPRET_RUNTIME_ERROR;
	}
	int32_t int_val = AS_INT(value);
	push(current_module_record, INT_VAL(~int_val));
	DISPATCH();
}

OP_TYPE_COERCE: {
	Value value = READ_CONSTANT();
	ObjectTypeRecord *type_record = AS_CRUX_TYPE_RECORD(value);
	Value query = PEEK(current_module_record, 0);
	if (!runtime_types_compatible(type_record->base_type, query)) {
		char type_name[128];
		type_record_name(type_record, type_name, 128);
		runtime_panic(current_module_record, TYPE, "Failed to perform type coercion. Expected type: '%s'.", type_name);
		return INTERPRET_RUNTIME_ERROR;
	}
	// if the type matched then continue on like nothing happened
	DISPATCH();
}

OP_GET_SLICE: {
	Value range_value = pop(current_module_record);
	ObjectRange *range = AS_CRUX_RANGE(range_value);
	if (!IS_CRUX_OBJECT(PEEK(current_module_record, 0))) {
		runtime_panic(current_module_record, TYPE, "Cannot get from a non-collection type.");
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (AS_CRUX_OBJECT(PEEK(current_module_record, 0))->type) {
	case OBJECT_ARRAY: {
		ObjectArray *array = AS_CRUX_ARRAY(PEEK(current_module_record, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, array->size)) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectArray *slice = new_array(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			// okay to not check return, we allocated enough capacity
			Value to_add = array->values[range->start + i * range->step];
			array_add_back(vm, slice, to_add);
		}
		pop_push(current_module_record, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_STRING: {
		ObjectString *string = AS_CRUX_STRING(PEEK(current_module_record, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, string->code_point_length)) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		const utf8_int8_t **codepoint_starts = NULL;
		if (!collect_string_codepoint_starts(vm, string, &codepoint_starts)) {
			runtime_panic(current_module_record, MEMORY, "Out of memory.");
			return INTERPRET_RUNTIME_ERROR;
		}

		char *buffer = ALLOCATE(vm, char, string->byte_length + 1);
		if (buffer == NULL) {
			FREE_ARRAY(vm, const utf8_int8_t *, codepoint_starts, string->code_point_length + 1);
			runtime_panic(current_module_record, MEMORY, "Out of memory.");
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
			runtime_panic(current_module_record, MEMORY, "Out of memory.");
			return INTERPRET_RUNTIME_ERROR;
		}
		buffer = resized;

		ObjectString *slice = take_string(vm, buffer, buffer_write_index);
		pop_push(current_module_record, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_TUPLE: {
		ObjectTuple *tuple = AS_CRUX_TUPLE(PEEK(current_module_record, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, tuple->size)) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectTuple *slice = new_tuple(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			slice->elements[i] = tuple->elements[range->start + i * range->step];
		}
		pop_push(current_module_record, OBJECT_VAL(slice));
		DISPATCH();
	}
	case OBJECT_BUFFER: {
		ObjectBuffer *buffer = AS_CRUX_BUFFER(PEEK(current_module_record, 0));
		uint32_t len = range_len(range);

		if (!range_indices_in_bounds(range, buffer->write_pos - buffer->read_pos)) {
			runtime_panic(current_module_record, BOUNDS, "Index out of bounds.");
			return INTERPRET_RUNTIME_ERROR;
		}

		ObjectArray *slice = new_array(vm, len);
		for (uint32_t i = 0; i < len; i++) {
			const uint32_t index = (uint32_t)(range->start + i * range->step);
			slice->values[i] = INT_VAL(buffer->data[buffer->read_pos + index]);
		}
		slice->size = len;
		pop_push(current_module_record, OBJECT_VAL(slice));
		DISPATCH();
	}

	default: {
		runtime_panic(current_module_record, TYPE, "Can only slice 'Array | String | Tuple | Buffer'");
		return INTERPRET_RUNTIME_ERROR;
	}
	}
	DISPATCH();
}

// value in collection
OP_IN: {
	Value right = pop(current_module_record);
	Value left = pop(current_module_record);

	ObjectType right_type = AS_CRUX_OBJECT(right)->type;

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
			push(current_module_record, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_STRING: {
			ObjectString *str = AS_CRUX_STRING(right);
			if (!IS_CRUX_STRING(left)) {
				push(current_module_record, FALSE_VAL);
				break;
			}
			ObjectString *goal = AS_CRUX_STRING(left);
			if (goal->byte_length == 0) {
				push(current_module_record, FALSE_VAL);
				break;
			}

			bool found = utf8str(str->chars, goal->chars) != NULL;
			push(current_module_record, found ? TRUE_VAL : FALSE_VAL);
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
			push(current_module_record, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_BUFFER: {
			ObjectBuffer *buffer = AS_CRUX_BUFFER(right);
			if (!IS_INT(left)) {
				push(current_module_record, FALSE_VAL);
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

			push(current_module_record, found ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_SET: {
			ObjectSet *set = AS_CRUX_SET(right);
			if (object_table_contains_key(set->entries, left)) {
				push(current_module_record, TRUE_VAL);
			} else {
				push(current_module_record, FALSE_VAL);
			}
			break;
		}
		case OBJECT_RANGE: {
			ObjectRange *range = AS_CRUX_RANGE(right);
			if (!IS_INT(left)) {
				push(current_module_record, FALSE_VAL);
				break;
			}
			int32_t int_val = AS_INT(left);
			push(current_module_record, range_contains(range, int_val) ? TRUE_VAL : FALSE_VAL);
			break;
		}
		case OBJECT_TABLE: {
			ObjectTable *table = AS_CRUX_TABLE(right);
			if (object_table_contains_key(table, left)) {
				push(current_module_record, TRUE_VAL);
			} else {
				push(current_module_record, FALSE_VAL);
			}
			break;
		}
		case OBJECT_VECTOR: {
			ObjectVector *vector = AS_CRUX_VECTOR(right);
			if (vector->dimensions == 0) {
				push(current_module_record, FALSE_VAL);
				break;
			}
			if (!IS_INT(left) && !IS_FLOAT(left)) {
				push(current_module_record, FALSE_VAL);
				break;
			}
			double double_val = TO_DOUBLE(left);
			if (vector->dimensions > STATIC_VECTOR_SIZE) {
				for (uint32_t i = 0; i < vector->dimensions; i++) {
					if (vector->as.h_components[i] != double_val) {
						push(current_module_record, FALSE_VAL);
						break;
					}
				}
			} else {
				for (uint32_t i = 0; i < vector->dimensions; i++) {
					if (vector->as.s_components[i] != double_val) {
						push(current_module_record, FALSE_VAL);
						break;
					}
				}
			}
			break;
		}
		default: {
			runtime_panic(current_module_record, TYPE,
						  "Can only check 'in' for 'Array | String | Tuple | Buffer | Set | Range | Table'");
			return INTERPRET_RUNTIME_ERROR;
		}
		}
	}

	DISPATCH();
}

OP_OK: {
	Value value = PEEK(current_module_record, 0);
	ObjectResult *result = new_ok_result(vm, value);
	pop(current_module_record);
	push(current_module_record, OBJECT_VAL(result));
	DISPATCH();
}

OP_ERR: {
	Value err = PEEK(current_module_record, 0);
	ObjectError *error = new_error(vm, to_string(vm, err), RUNTIME, false);
	push(current_module_record, OBJECT_VAL(error));
	ObjectResult *result = new_error_result(vm, error);
	pop(current_module_record);
	pop(current_module_record);
	push(current_module_record, OBJECT_VAL(result));
	DISPATCH();
}

OP_SOME: {
	Value value = PEEK(current_module_record, 0);
	ObjectOption *some = new_option(vm, value, true);
	pop(current_module_record);
	push(current_module_record, OBJECT_VAL(some));
	DISPATCH();
}

OP_NONE: {
	ObjectOption *none = new_option(vm, NIL_VAL, false);
	push(current_module_record, OBJECT_VAL(none));
	DISPATCH();
}

OP_ADD_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	const int64_t result = (int64_t)a + (int64_t)b;
	current_module_record->stack_top--;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		current_module_record->stack_top[-1] = INT_VAL((int32_t)result);
	} else {
		current_module_record->stack_top[-1] = FLOAT_VAL((double)result);
	}
	DISPATCH();
}

OP_ADD_NUM: {
	double b = TO_DOUBLE(current_module_record->stack_top[-1]);
	double a = TO_DOUBLE(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(a + b);
	DISPATCH();
}

OP_SUBTRACT_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	const int64_t result = (int64_t)a - (int64_t)b;
	current_module_record->stack_top--;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		current_module_record->stack_top[-1] = INT_VAL((int32_t)result);
	} else {
		current_module_record->stack_top[-1] = FLOAT_VAL((double)result);
	}
	DISPATCH();
}

OP_SUBTRACT_NUM: {
	double b = TO_DOUBLE(current_module_record->stack_top[-1]);
	double a = TO_DOUBLE(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(a - b);
	DISPATCH();
}

OP_MULTIPLY_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	const int64_t result = (int64_t)a * (int64_t)b;
	current_module_record->stack_top--;
	if (result >= INT32_MIN && result <= INT32_MAX) {
		current_module_record->stack_top[-1] = INT_VAL((int32_t)result);
	} else {
		current_module_record->stack_top[-1] = FLOAT_VAL((double)result);
	}
	DISPATCH();
}

OP_MULTIPLY_NUM: {
	double b = TO_DOUBLE(current_module_record->stack_top[-1]);
	double a = TO_DOUBLE(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(a * b);
	DISPATCH();
}

OP_DIVIDE_NUM: {
	double b = TO_DOUBLE(current_module_record->stack_top[-1]);
	if (b == 0.0) {
		runtime_panic(current_module_record, MATH, "Division by zero.");
		return INTERPRET_RUNTIME_ERROR;
	}
	double a = TO_DOUBLE(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(a / b);
	DISPATCH();
}

OP_INT_DIVIDE_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	if (b == 0) {
		runtime_panic(current_module_record, MATH, "Integer division by zero.");
		return INTERPRET_RUNTIME_ERROR;
	}
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;

	if (a == INT32_MIN && b == -1) {
		current_module_record->stack_top[-1] = FLOAT_VAL(-(double)INT32_MIN);
	} else {
		current_module_record->stack_top[-1] = INT_VAL(a / b);
	}
	DISPATCH();
}

OP_MODULUS_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	if (b == 0) {
		runtime_panic(current_module_record, MATH, "Modulo by zero.");
		return INTERPRET_RUNTIME_ERROR;
	}
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;

	if (a == INT32_MIN && b == -1) {
		current_module_record->stack_top[-1] = INT_VAL(0);
	} else {
		current_module_record->stack_top[-1] = INT_VAL(a % b);
	}
	DISPATCH();
}

OP_POWER_INT: {
	int32_t b = AS_INT(current_module_record->stack_top[-1]);
	int32_t a = AS_INT(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(pow((double)a, (double)b));
	DISPATCH();
}

OP_POWER_NUM: {
	double b = TO_DOUBLE(current_module_record->stack_top[-1]);
	double a = TO_DOUBLE(current_module_record->stack_top[-2]);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = FLOAT_VAL(pow(a, b));
	DISPATCH();
}

OP_ADD_VECTOR_VECTOR: {
	ObjectVector *right = AS_CRUX_VECTOR(current_module_record->stack_top[-1]);
	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);

	if (left->dimensions != right->dimensions) {
		runtime_panic(current_module_record, MATH, "Vectors must have same dimensions to add.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectVector *res_vec = new_vector(vm, left->dimensions);
	double *l_data = VECTOR_COMPONENTS(left);
	double *r_data = VECTOR_COMPONENTS(right);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < left->dimensions; i++) {
		out_data[i] = l_data[i] + r_data[i];
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_SUBTRACT_VECTOR_VECTOR: {
	ObjectVector *right = AS_CRUX_VECTOR(current_module_record->stack_top[-1]);
	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);

	if (left->dimensions != right->dimensions) {
		runtime_panic(current_module_record, MATH, "Vectors must have same dimensions to subtract.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectVector *res_vec = new_vector(vm, left->dimensions);
	double *l_data = VECTOR_COMPONENTS(left);
	double *r_data = VECTOR_COMPONENTS(right);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < left->dimensions; i++) {
		out_data[i] = l_data[i] - r_data[i];
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_MULTIPLY_VECTOR_VECTOR: { // Cross Product
	ObjectVector *right = AS_CRUX_VECTOR(current_module_record->stack_top[-1]);
	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);

	if (left->dimensions != 3 || right->dimensions != 3) {
		runtime_panic(current_module_record, MATH, "Cross product requires 3D vectors.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectVector *res_vec = new_vector(vm, 3);
	double *l_data = VECTOR_COMPONENTS(left);
	double *r_data = VECTOR_COMPONENTS(right);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	out_data[0] = l_data[1] * r_data[2] - l_data[2] * r_data[1];
	out_data[1] = l_data[2] * r_data[0] - l_data[0] * r_data[2];
	out_data[2] = l_data[0] * r_data[1] - l_data[1] * r_data[0];

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_DIVIDE_VECTOR_VECTOR: { // Component-wise division
	ObjectVector *right = AS_CRUX_VECTOR(current_module_record->stack_top[-1]);
	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);

	if (left->dimensions != right->dimensions) {
		runtime_panic(current_module_record, MATH, "Vectors must have same dimensions to divide.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectVector *res_vec = new_vector(vm, left->dimensions);
	double *l_data = VECTOR_COMPONENTS(left);
	double *r_data = VECTOR_COMPONENTS(right);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < left->dimensions; i++) {
		if (r_data[i] == 0.0) {
			runtime_panic(current_module_record, MATH, "Vector component division by zero.");
			return INTERPRET_RUNTIME_ERROR;
		}
		out_data[i] = l_data[i] / r_data[i];
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_MULTIPLY_VECTOR_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);

	ObjectVector *res_vec = new_vector(vm, left->dimensions);
	double *l_data = VECTOR_COMPONENTS(left);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < left->dimensions; i++) {
		out_data[i] = l_data[i] * scalar;
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_MULTIPLY_SCALAR_VECTOR: {
	ObjectVector *right = AS_CRUX_VECTOR(current_module_record->stack_top[-1]);
	double scalar = TO_DOUBLE(current_module_record->stack_top[-2]);

	ObjectVector *res_vec = new_vector(vm, right->dimensions);
	double *r_data = VECTOR_COMPONENTS(right);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < right->dimensions; i++) {
		out_data[i] = scalar * r_data[i];
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_DIVIDE_VECTOR_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	if (scalar == 0.0) {
		runtime_panic(current_module_record, MATH, "Vector division by zero scalar.");
		return INTERPRET_RUNTIME_ERROR;
	}

	ObjectVector *left = AS_CRUX_VECTOR(current_module_record->stack_top[-2]);
	ObjectVector *res_vec = new_vector(vm, left->dimensions);
	double *l_data = VECTOR_COMPONENTS(left);
	double *out_data = VECTOR_COMPONENTS(res_vec);

	for (uint32_t i = 0; i < left->dimensions; i++) {
		out_data[i] = l_data[i] / scalar;
	}

	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = OBJECT_VAL(res_vec);
	DISPATCH();
}

OP_ADD_COMPLEX_COMPLEX: {
	ObjectComplex *right = AS_CRUX_COMPLEX(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	Value result = complex_add_value(vm, left, right);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_SUBTRACT_COMPLEX_COMPLEX: {
	ObjectComplex *right = AS_CRUX_COMPLEX(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	Value result = complex_subtract_value(vm, left, right);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_MULTIPLY_COMPLEX_COMPLEX: {
	ObjectComplex *right = AS_CRUX_COMPLEX(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	Value result = complex_multiply_value(vm, left, right);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_DIVIDE_COMPLEX_COMPLEX: {
	ObjectComplex *right = AS_CRUX_COMPLEX(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	Value result = complex_divide_value(vm, left, right);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_MULTIPLY_COMPLEX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	Value result = complex_scalar_multiply_value(vm, left, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_MULTIPLY_SCALAR_COMPLEX: {
	ObjectComplex *right = AS_CRUX_COMPLEX(current_module_record->stack_top[-1]);
	double scalar = TO_DOUBLE(current_module_record->stack_top[-2]);
	Value result = complex_scalar_multiply_value(vm, right, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_DIVIDE_COMPLEX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectComplex *left = AS_CRUX_COMPLEX(current_module_record->stack_top[-2]);
	ObjectResult *res = AS_CRUX_RESULT(complex_scalar_divide_value(vm, left, scalar));
	if (!res->is_ok) {
		runtime_panic(current_module_record, res->as.error->type, "%s", res->as.error->message->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = res->as.value;
	DISPATCH();
}

OP_ADD_MATRIX_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	ObjectResult *res = AS_CRUX_RESULT(matrix_add_value(vm, left, right));
	if (!res->is_ok) {
		runtime_panic(current_module_record, res->as.error->type, "%s", res->as.error->message->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = res->as.value;
	DISPATCH();
}

OP_SUBTRACT_MATRIX_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	ObjectResult *res = AS_CRUX_RESULT(matrix_subtract_value(vm, left, right));
	if (!res->is_ok) {
		runtime_panic(current_module_record, res->as.error->type, "%s", res->as.error->message->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = res->as.value;
	DISPATCH();
}

OP_ADD_MATRIX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	Value result = matrix_scalar_add_value(vm, left, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_ADD_SCALAR_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	double scalar = TO_DOUBLE(current_module_record->stack_top[-2]);
	Value result = matrix_scalar_add_value(vm, right, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_SUBTRACT_MATRIX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	Value result = matrix_scalar_subtract_value(vm, left, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_SUBTRACT_SCALAR_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	double scalar = TO_DOUBLE(current_module_record->stack_top[-2]);
	Value result = scalar_matrix_subtract_value(vm, scalar, right);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_MULTIPLY_MATRIX_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	ObjectResult *res = AS_CRUX_RESULT(matrix_multiply_value(vm, left, right));
	if (!res->is_ok) {
		runtime_panic(current_module_record, res->as.error->type, "%s", res->as.error->message->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = res->as.value;
	DISPATCH();
}

OP_MULTIPLY_MATRIX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	Value result = matrix_scale_value(vm, left, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_MULTIPLY_SCALAR_MATRIX: {
	ObjectMatrix *right = AS_CRUX_MATRIX(current_module_record->stack_top[-1]);
	double scalar = TO_DOUBLE(current_module_record->stack_top[-2]);
	Value result = matrix_scale_value(vm, right, scalar);
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = result;
	DISPATCH();
}

OP_DIVIDE_MATRIX_SCALAR: {
	double scalar = TO_DOUBLE(current_module_record->stack_top[-1]);
	ObjectMatrix *left = AS_CRUX_MATRIX(current_module_record->stack_top[-2]);
	ObjectResult *res = AS_CRUX_RESULT(matrix_scalar_divide_value(vm, left, scalar));
	if (!res->is_ok) {
		runtime_panic(current_module_record, res->as.error->type, "%s", res->as.error->message->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	current_module_record->stack_top--;
	current_module_record->stack_top[-1] = res->as.value;
	DISPATCH();
}

OP_INVOKE_STDLIB: {
	Value callable = READ_CONSTANT();
	int arg_count = READ_SHORT();

	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const Value receiver = PEEK(current_module_record, arg_count);
	const Value original = PEEK(current_module_record,
								arg_count + 1); // Store the original caller

	if (!IS_CRUX_OBJECT(receiver)) {
		runtime_panic(current_module_record, TYPE, "Only instances have methods");
		return INTERPRET_RUNTIME_ERROR;
	}

	arg_count++; // for the value that the method will act on

	mark_value(vm, original);

	// Save original stack order
	current_module_record->stack_top[-arg_count - 1] = callable;
	current_module_record->stack_top[-arg_count] = receiver;

	if (!call_value(vm, callable, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}

	// restore the caller and put the result in the right place
	const Value result = pop(current_module_record);
	push(current_module_record, original);
	push(current_module_record, result);

	frame = &current_module_record->frames[current_module_record->frame_count - 1];

	DISPATCH();
}

OP_INVOKE_STDLIB_UNWRAP: {
	Value callable = READ_CONSTANT();
	int arg_count = READ_SHORT();

	ObjectModuleRecord *current_module_record = vm->current_module_record;
	const Value receiver = PEEK(current_module_record, arg_count);
	const Value original = PEEK(current_module_record,
								arg_count + 1); // Store the original caller

	if (!IS_CRUX_OBJECT(receiver)) {
		runtime_panic(current_module_record, TYPE, "Only instances have methods");
		return INTERPRET_RUNTIME_ERROR;
	}

	arg_count++; // for the value that the method will act on

	mark_value(vm, original);

	// Save original stack order
	current_module_record->stack_top[-arg_count - 1] = callable;
	current_module_record->stack_top[-arg_count] = receiver;

	if (!call_value(vm, callable, arg_count)) {
		return INTERPRET_RUNTIME_ERROR;
	}

	// restore the caller and put the result in the right place
	const Value result = pop(current_module_record);
	push(current_module_record, original);

	if (!IS_CRUX_RESULT(result)) {
		runtime_panic(current_module_record, TYPE, "Only the 'result' type supports unwrapping.");
		return INTERPRET_RUNTIME_ERROR;
	}
	ObjectResult *result_obj = AS_CRUX_RESULT(result);
	if (result_obj->is_ok) {
		push(current_module_record, result_obj->as.value);
	} else {
		ObjectError *error = result_obj->as.error;
		runtime_panic(current_module_record, RUNTIME, "Panic: %s", error->message->chars);
	}

	frame = &current_module_record->frames[current_module_record->frame_count - 1];

	DISPATCH();
}

OP_POP_N: {
	uint16_t pop_count = READ_SHORT();
#ifdef STACK_SAFETY
	for (uint16_t i = 0; i < pop_count; i++) {
		pop(current_module_record);
	}
#else
	current_module_record->stack_top -= pop_count;
#endif
	DISPATCH();
}

OP_DEFINE_PUB_GLOBAL: {
	uint16_t index = READ_SHORT();
	ObjectString *name = READ_STRING();
	ObjectModuleRecord *frame_module_record = frame->closure->function->module_record;
	frame_module_record->globals[index] = PEEK(current_module_record, 0);
	table_set(vm, &frame_module_record->publics, name, PEEK(current_module_record, 0));

	pop(current_module_record);
	DISPATCH();
}

OP_0_INT: {
	push(current_module_record, INT_VAL(0));
	DISPATCH();
}
OP_1_INT: {
	push(current_module_record, INT_VAL(1));
	DISPATCH();
}
OP_2_INT: {
	push(current_module_record, INT_VAL(2));
	DISPATCH();
}
OP_0_FLOAT: {
	push(current_module_record, FLOAT_VAL(0));
	DISPATCH();
}
OP_1_FLOAT: {
	push(current_module_record, FLOAT_VAL(1));
	DISPATCH();
}
OP_2_FLOAT: {
	push(current_module_record, FLOAT_VAL(2));
	DISPATCH();
}

end: {
	printf("        ");
	for (Value *slot = current_module_record->stack; slot < current_module_record->stack_top; slot++) {
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
