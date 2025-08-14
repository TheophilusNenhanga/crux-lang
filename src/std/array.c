#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../memory.h"
#include "../panic.h"
#include "../vm/vm_helpers.h"
#include "array.h"

ObjectResult *array_push_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const Value toAdd = args[1];

	if (!array_add(vm, array, toAdd, array->size)) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Failed to add to array.", 23),
				 RUNTIME, false));
	}

	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *array_pop_method(VM *vm, int arg_count __attribute__((unused)),
			       const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (array->size == 0) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Cannot remove a value from an "
						"empty array.",
						42),
				     BOUNDS, false));
	}

	const Value popped = array->values[array->size - 1];
	array->values[array->size - 1] = NIL_VAL;
	array->size--;

	return new_ok_result(vm, popped);
}

ObjectResult *array_insert_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[2])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "<index> must be of type 'number'.",
					    33),
				 TYPE, false));
	}

	const Value toInsert = args[1];
	const uint32_t insertAt = AS_INT(args[2]);

	if (insertAt > array->size) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "<index> is out of bounds.",
						25),
				     BOUNDS, false));
	}

	if (ensure_capacity(vm, array, array->size + 1)) {
		for (uint32_t i = array->size; i > insertAt; i--) {
			array->values[i] = array->values[i - 1];
		}

		array->values[insertAt] = toInsert;
		array->size++;
	} else {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate enough "
						"memory for new array.",
						47),
				     MEMORY, false));
	}
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *array_remove_at_method(VM *vm,
				     int arg_count __attribute__((unused)),
				     const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "<index> must be of type 'number'.",
					    33),
				 TYPE, false));
	}

	const uint32_t removeAt = AS_INT(args[1]);

	if (removeAt >= array->size) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm, "<index> is out of bounds.",
						25),
				     BOUNDS, false));
	}

	const Value removedElement = array->values[removeAt];

	for (uint32_t i = removeAt; i < array->size - 1; i++) {
		array->values[i] = array->values[i + 1];
	}

	array->size--;
	return new_ok_result(vm, removedElement);
}

ObjectResult *array_concat_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_CRUX_ARRAY(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "<target> must be of type 'array'.",
					    33),
				 TYPE, false));
	}

	const ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

	const uint32_t combinedSize = targetArray->size + array->size;
	if (combinedSize > MAX_ARRAY_SIZE) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Size of resultant array out "
						"of bounds.",
						38),
				     BOUNDS, false));
	}

	ObjectArray *resultArray = new_array(vm, combinedSize,
					    vm->currentModuleRecord);

	for (uint32_t i = 0; i < combinedSize; i++) {
		resultArray->values[i] =
			i < array->size ? array->values[i]
					: targetArray->values[i - array->size];
	}

	resultArray->size = combinedSize;

	return new_ok_result(vm, OBJECT_VAL(resultArray));
}

ObjectResult *array_slice_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"<start_index> must be of type "
						"'number'.",
						39),
				     TYPE, false));
	}

	if (!IS_INT(args[2])) {
		return new_error_result(
			vm,
			new_error(
				vm,
				copy_string(
					vm,
					"<end_index> must be of type 'number'.",
					37),
				TYPE, false));
	}

	const uint32_t startIndex = IS_INT(args[1]);
	const uint32_t endIndex = IS_INT(args[2]);

	if (startIndex > array->size) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "<start_index> out of bounds.",
					    28),
				 BOUNDS, false));
	}

	if (endIndex > array->size) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "<end_index> out of bounds.",
					    26),
				 BOUNDS, false));
	}

	if (endIndex < startIndex) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "indexes out of bounds.", 22),
				 BOUNDS, false));
	}

	const size_t sliceSize = endIndex - startIndex;
	ObjectArray *slicedArray = new_array(vm, sliceSize,
					    vm->currentModuleRecord);

	for (size_t i = 0; i < sliceSize; i++) {
		slicedArray->values[i] = array->values[startIndex + i];
		slicedArray->size += 1;
	}

	return new_ok_result(vm, OBJECT_VAL(slicedArray));
}

ObjectResult *array_reverse_method(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	Value *values = ALLOCATE(vm, Value, array->size);

	if (values == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory "
						"when reversing array.",
						47),
				     MEMORY, false));
	}

	for (uint32_t i = 0; i < array->size; i++) {
		values[i] = array->values[i];
	}

	for (uint32_t i = 0; i < array->size; i++) {
		array->values[i] = values[array->size - 1 - i];
	}

	free(values);

	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *array_index_of_method(VM *vm,
				    int arg_count __attribute__((unused)),
				    const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const Value target = args[1];

	for (uint32_t i = 0; i < array->size; i++) {
		if (values_equal(target, array->values[i])) {
			return new_ok_result(vm, INT_VAL(i));
		}
	}
	return new_error_result(
		vm,
		new_error(vm,
			 copy_string(vm,
				    "Value could not be found in the array.",
				    38),
			 VALUE, false));
}

Value array_contains_method(VM *vm __attribute__((unused)),
			    int arg_count __attribute__((unused)),
			    const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const Value target = args[1];

	for (uint32_t i = 0; i < array->size; i++) {
		if (values_equal(target, array->values[i])) {
			return BOOL_VAL(true);
		}
	}
	return BOOL_VAL(false);
}

Value array_clear_method(VM *vm __attribute__((unused)),
			 int arg_count __attribute__((unused)),
			 const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	for (uint32_t i = 0; i < array->size; i++) {
		array->values[i] = NIL_VAL;
	}
	array->size = 0;

	return NIL_VAL;
}

Value arrayEqualsMethod(VM *vm __attribute__((unused)),
			int arg_count __attribute__((unused)), const Value *args)
{
	if (!IS_CRUX_ARRAY(args[1])) {
		return BOOL_VAL(false);
	}

	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

	if (array->size != targetArray->size) {
		return BOOL_VAL(false);
	}

	for (uint32_t i = 0; i < array->size; i++) {
		if (!values_equal(array->values[i], targetArray->values[i])) {
			return BOOL_VAL(false);
		}
	}

	return BOOL_VAL(true);
}

// arg0 - array
// arg1 - function
ObjectResult *array_map_method(VM *vm, int arg_count __attribute__((unused)),
			       const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;

	if (!IS_CRUX_CLOSURE(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Expected value of type 'callable' "
					    "for <func> argument",
					    53),
				 TYPE, false));
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

	if (closure->function->arity != 1) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "<func> must take exactly 1 argument.",
					 36),
				 ARGUMENT_MISMATCH, true));
	}

	ObjectArray *resultArray = new_array(vm, array->size,
					    vm->currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(resultArray));
	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];
		push(currentModuleRecord, arrayValue);
		InterpretResult res;
		ObjectResult *result = execute_user_function(vm, closure, 1,
							   &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				return result;
			}
		}

		if (result->is_ok) {
			array_add_back(vm, resultArray, result->as.value);
		} else {
			array_add_back(vm, resultArray,
				     OBJECT_VAL(result->as.error));
		}
		pop(currentModuleRecord);
	}
	pop(currentModuleRecord);
	return new_ok_result(vm, OBJECT_VAL(resultArray));
}

// arg0 - array
// arg1 - function
ObjectResult *array_filter_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_CRUX_CLOSURE(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Expected value of type 'callable' "
					    "for <func> argument",
					    53),
				 TYPE, false));
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

	if (closure->function->arity != 1) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(
					 vm,
					 "<func> must take exactly 1 argument.",
					 36),
				 ARGUMENT_MISMATCH, true));
	}

	ObjectArray *resultArray = new_array(vm, array->size,
					    vm->currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(resultArray));
	uint32_t addCount = 0;
	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];
		push(currentModuleRecord, arrayValue);
		InterpretResult res;
		ObjectResult *result = execute_user_function(vm, closure, 1,
							   &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				return result;
			}
		}

		if (result->is_ok) {
			if (!is_falsy(result->as.value)) {
				array_add_back(vm, resultArray, arrayValue);
				addCount++;
			}
		}
		pop(currentModuleRecord);
	}
	pop(currentModuleRecord);
	resultArray->size = addCount;
	return new_ok_result(vm, OBJECT_VAL(resultArray));
}

// arg0 - array
// arg1 - function
// arg2 - initial value
ObjectResult *array_reduce_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;

	if (!IS_CRUX_CLOSURE(args[1])) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Expected value of type 'callable' "
					    "for <func> argument",
					    53),
				 TYPE, false));
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);
	if (closure->function->arity != 2) {
		return new_error_result(
			vm,
			new_error(
				vm,
				copy_string(
					vm,
					"<func> must take exactly 2 arguments.",
					37),
				ARGUMENT_MISMATCH, true));
	}

	Value accumulator = args[2];

	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];

		push(currentModuleRecord, arrayValue);
		push(currentModuleRecord, accumulator);

		InterpretResult res;

		ObjectResult *result = execute_user_function(vm, closure, 2,
							   &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				pop(currentModuleRecord); // arrayValue
				pop(currentModuleRecord); // accumulator
				return result;
			}
		}

		if (result->is_ok) {
			accumulator = result->as.value;
		} else {
			pop(currentModuleRecord); // arrayValue
			pop(currentModuleRecord); // accumulator
			return result;
		}

		pop(currentModuleRecord); // arrayValue
		pop(currentModuleRecord); // accumulator
	}

	return new_ok_result(vm, accumulator);
}

static int compare_values(const Value a, const Value b)
{
	if (IS_INT(a) && IS_INT(b)) {
		const int64_t aVal = AS_INT(a);
		const int64_t bVal = AS_INT(b);
		if (aVal < bVal)
			return -1;
		if (aVal > bVal)
			return 1;
		return 0;
	}

	if (IS_FLOAT(a) && IS_FLOAT(b)) {
		const double aVal = AS_FLOAT(a);
		const double bVal = AS_FLOAT(b);
		if (aVal < bVal)
			return -1;
		if (aVal > bVal)
			return 1;
		return 0;
	}

	if ((IS_INT(a) || IS_FLOAT(a)) && (IS_INT(b) || IS_FLOAT(b))) {
		const double aVal = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
		const double bVal = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
		if (aVal < bVal)
			return -1;
		if (aVal > bVal)
			return 1;
		return 0;
	}

	if (IS_CRUX_STRING(a) && IS_CRUX_STRING(b)) {
		const ObjectString *aStr = AS_CRUX_STRING(a);
		const ObjectString *bStr = AS_CRUX_STRING(b);
		return strcmp(aStr->chars, bStr->chars);
	}

	// If types don't match or aren't comparable
	return 0;
}

static bool are_all_elements_sortable(const ObjectArray *array)
{
	if (array->size == 0)
		return true;

	bool hasInt = false, hasFloat = false, hasString = false;

	for (uint32_t i = 0; i < array->size; i++) {
		const Value val = array->values[i];
		if (IS_INT(val)) {
			hasInt = true;
		} else if (IS_FLOAT(val)) {
			hasFloat = true;
		} else if (IS_CRUX_STRING(val)) {
			hasString = true;
		} else {
			return false;
		}
	}

	if (hasString && (hasInt || hasFloat)) {
		return false;
	}

	return true;
}

static int partition(Value *arr, const int low, const int high)
{
	const Value pivot = arr[high];
	int i = (low - 1);

	for (int j = low; j <= high - 1; j++) {
		if (compare_values(arr[j], pivot) <= 0) {
			i++;
			const Value temp = arr[i];
			arr[i] = arr[j];
			arr[j] = temp;
		}
	}
	const Value temp = arr[i + 1];
	arr[i + 1] = arr[high];
	arr[high] = temp;
	return (i + 1);
}

static void quick_sort(Value *arr, const int low, const int high)
{
	if (low < high) {
		const int pi = partition(arr, low, high);
		quick_sort(arr, low, pi - 1);
		quick_sort(arr, pi + 1, high);
	}
}

// arg0 - array
ObjectResult *array_sort_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (array->size == 0) {
		return new_ok_result(vm, args[0]);
	}

	if (!are_all_elements_sortable(array)) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Array contains unsortable or "
						"mixed incompatible types",
						53),
				     TYPE, false));
	}

	ObjectArray *sortedArray = new_array(vm, array->size,
					    vm->currentModuleRecord);
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
	push(currentModuleRecord, OBJECT_VAL(sortedArray));

	for (uint32_t i = 0; i < array->size; i++) {
		sortedArray->values[i] = array->values[i];
	}
	sortedArray->size = array->size;

	quick_sort(sortedArray->values, 0, (int)sortedArray->size - 1);

	pop(currentModuleRecord);
	return new_ok_result(vm, OBJECT_VAL(sortedArray));
}

// arg0 - array
// arg1 - separator string
ObjectResult *array_join_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	if (!IS_CRUX_STRING(args[1])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Expected arg <separator> to "
						"be of type 'string'.",
						48),
				     TYPE, false));
	}

	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const ObjectString *separator = AS_CRUX_STRING(args[1]);

	if (array->size == 0) {
		ObjectString *emptyResult = copy_string(vm, "", 0);
		return new_ok_result(vm, OBJECT_VAL(emptyResult));
	}

	// estimate: 3 chars per element + separators
	size_t bufferSize = array->size * 3 +
			    (array->size > 1
				     ? (array->size - 1) * separator->length
				     : 0);
	char *buffer = malloc(bufferSize);
	if (!buffer) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm, "Memory allocation failed", 24),
				 RUNTIME, false));
	}

	size_t actualLength = 0;

	for (uint32_t i = 0; i < array->size; i++) {
		const ObjectString *element = to_string(vm, array->values[i]);

		size_t neededSpace = element->length;
		if (i > 0) {
			neededSpace += separator->length;
		}

		if (actualLength + neededSpace > bufferSize) {
			// Grow the buffer by 50%
			const size_t newSize = (size_t)(((double)bufferSize *
							 1.5) +
							(double)neededSpace);
			char *newBuffer = realloc(buffer, newSize);
			if (!newBuffer) {
				free(buffer);
				return new_error_result(
					vm,
					new_error(vm,
						 copy_string(
							 vm,
							 "Memory reallocation "
							 "failed",
							 26),
						 MEMORY, false));
			}
			buffer = newBuffer;
			bufferSize = newSize;
		}

		if (i > 0) {
			memcpy(buffer + actualLength, separator->chars,
			       separator->length);
			actualLength += separator->length;
		}

		memcpy(buffer + actualLength, element->chars, element->length);
		actualLength += element->length;
	}

	if (actualLength >= bufferSize) {
		char *newBuffer = realloc(buffer, actualLength + 1);
		if (!newBuffer) {
			free(buffer);
			return new_error_result(
				vm,
				new_error(
					vm,
					copy_string(vm,
						   "Memory reallocation failed",
						   26),
					MEMORY, false));
		}
		buffer = newBuffer;
	}
	buffer[actualLength] = '\0';

	ObjectString *result = take_string(vm, buffer, (uint32_t)actualLength);
	return new_ok_result(vm, OBJECT_VAL(result));
}
