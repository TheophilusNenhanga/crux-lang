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
	const Value to_add = args[1];

	if (!array_add(vm, array, to_add, array->size)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to add to array.", RUNTIME);
	}

	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *array_pop_method(VM *vm, int arg_count __attribute__((unused)),
			       const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (array->size == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot remove a value from an empty array.", BOUNDS);
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
		return MAKE_GC_SAFE_ERROR(vm, "<index> must be of type 'number'.", TYPE);
	}

	const Value toInsert = args[1];
	const uint32_t insert_at = AS_INT(args[2]);

	if (insert_at > array->size) {
		return MAKE_GC_SAFE_ERROR(vm, "<index> is out of bounds.", BOUNDS);
	}

	if (ensure_capacity(vm, array, array->size + 1)) {
		for (uint32_t i = array->size; i > insert_at; i--) {
			array->values[i] = array->values[i - 1];
		}

		array->values[insert_at] = toInsert;
		array->size++;
	} else {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate enough memory for new array.", MEMORY);
	}
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *array_remove_at_method(VM *vm,
				     int arg_count __attribute__((unused)),
				     const Value *args)
{
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<index> must be of type 'number'.", TYPE);
	}

	const uint32_t removeAt = AS_INT(args[1]);

	if (removeAt >= array->size) {
		return MAKE_GC_SAFE_ERROR(vm, "<index> is out of bounds.", BOUNDS);
	}

	const Value removed_element = array->values[removeAt];

	for (uint32_t i = removeAt; i < array->size - 1; i++) {
		array->values[i] = array->values[i + 1];
	}

	array->size--;
	return new_ok_result(vm, removed_element);
}

ObjectResult *array_concat_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_CRUX_ARRAY(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<target> must be of type 'array'.", TYPE);
	}

	const ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

	const uint32_t combined_size = targetArray->size + array->size;
	if (combined_size > MAX_ARRAY_SIZE) {
		return MAKE_GC_SAFE_ERROR(vm, "Size of resultant array out of bounds.", BOUNDS);
	}

	ObjectArray *resultArray = new_array(vm, combined_size, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(resultArray));

	for (uint32_t i = 0; i < combined_size; i++) {
		resultArray->values[i] =
			i < array->size ? array->values[i]
					: targetArray->values[i - array->size];
	}

	resultArray->size = combined_size;

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(resultArray));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *array_slice_method(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "<start_index> must be of type 'number'.", TYPE);
	}

	if (!IS_INT(args[2])) {
		return MAKE_GC_SAFE_ERROR(vm, "<end_index> must be of type 'number'.", TYPE);
	}

	const uint32_t start_index = AS_INT(args[1]);
	const uint32_t end_index = AS_INT(args[2]);

	if (start_index > array->size) {
		return MAKE_GC_SAFE_ERROR(vm, "<start_index> out of bounds.", BOUNDS);
	}

	if (end_index > array->size) {
		return MAKE_GC_SAFE_ERROR(vm, "<end_index> out of bounds.", BOUNDS);
	}

	if (end_index < start_index) {
		return MAKE_GC_SAFE_ERROR(vm, "indexes out of bounds.", BOUNDS);
	}

	const size_t sliceSize = end_index - start_index;
	ObjectArray *slicedArray = new_array(vm, sliceSize, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(slicedArray));

	for (size_t i = 0; i < sliceSize; i++) {
		slicedArray->values[i] = array->values[start_index + i];
		slicedArray->size += 1;
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(slicedArray));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *array_reverse_method(VM *vm, int arg_count __attribute__((unused)),
				   const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	Value *values = ALLOCATE(vm, Value, array->size);

	if (values == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to allocate memory when reversing array.", MEMORY);
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
	return MAKE_GC_SAFE_ERROR(vm, "Value could not be found in the array.", VALUE);
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
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;

	if (!IS_CRUX_CLOSURE(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected value of type 'callable' for <func> argument", TYPE);
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

	if (closure->function->arity != 1) {
		return MAKE_GC_SAFE_ERROR(vm, "<func> must take exactly 1 argument.", ARGUMENT_MISMATCH);
	}

	ObjectArray *resultArray = new_array(vm, array->size, vm->current_module_record);
	push(currentModuleRecord, OBJECT_VAL(resultArray));

	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];
		push(currentModuleRecord, arrayValue);
		InterpretResult res;
		ObjectResult *result = execute_user_function(vm, closure, 1, &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				pop(currentModuleRecord); // arrayValue
				pop(currentModuleRecord); // resultArray
				return result;
			}
		}

		if (result->is_ok) {
			array_add_back(vm, resultArray, result->as.value);
		} else {
			array_add_back(vm, resultArray, OBJECT_VAL(result->as.error));
		}
		pop(currentModuleRecord); // arrayValue
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(resultArray));
	pop(currentModuleRecord); // resultArray
	return res;
}

// arg0 - array
// arg1 - function
ObjectResult *array_filter_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_CRUX_CLOSURE(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected value of type 'callable' for <func> argument", TYPE);
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);

	if (closure->function->arity != 1) {
		return MAKE_GC_SAFE_ERROR(vm, "<func> must take exactly 1 argument.", ARGUMENT_MISMATCH);
	}

	ObjectArray *resultArray = new_array(vm, array->size, vm->current_module_record);
	push(currentModuleRecord, OBJECT_VAL(resultArray));

	uint32_t addCount = 0;
	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];
		push(currentModuleRecord, arrayValue);
		InterpretResult res;
		ObjectResult *result = execute_user_function(vm, closure, 1, &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				pop(currentModuleRecord); // arrayValue
				pop(currentModuleRecord); // resultArray
				return result;
			}
		}

		if (result->is_ok) {
			if (!is_falsy(result->as.value)) {
				array_add_back(vm, resultArray, arrayValue);
				addCount++;
			}
		}
		pop(currentModuleRecord); // arrayValue
	}

	resultArray->size = addCount;
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(resultArray));
	pop(currentModuleRecord); // resultArray
	return res;
}

// arg0 - array
// arg1 - function
// arg2 - initial value
ObjectResult *array_reduce_method(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args)
{
	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;

	if (!IS_CRUX_CLOSURE(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected value of type 'callable' for <func> argument", TYPE);
	}

	ObjectClosure *closure = AS_CRUX_CLOSURE(args[1]);
	if (closure->function->arity != 2) {
		return MAKE_GC_SAFE_ERROR(vm, "<func> must take exactly 2 arguments.", ARGUMENT_MISMATCH);
	}

	Value accumulator = args[2];

	for (uint32_t i = 0; i < array->size; i++) {
		const Value arrayValue = array->values[i];

		push(currentModuleRecord, arrayValue);
		push(currentModuleRecord, accumulator);

		InterpretResult res;

		ObjectResult *result = execute_user_function(vm, closure, 2, &res);

		if (res != INTERPRET_OK) {
			if (!result->is_ok) {
				pop(currentModuleRecord); // accumulator
				pop(currentModuleRecord); // arrayValue
				return result;
			}
		}

		if (result->is_ok) {
			accumulator = result->as.value;
		} else {
			pop(currentModuleRecord); // accumulator
			pop(currentModuleRecord); // arrayValue
			return result;
		}

		pop(currentModuleRecord); // accumulator
		pop(currentModuleRecord); // arrayValue
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
		return MAKE_GC_SAFE_ERROR(vm, "Array contains unsortable or mixed incompatible types", TYPE);
	}

	ObjectArray *sortedArray = new_array(vm, array->size, vm->current_module_record);
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	push(currentModuleRecord, OBJECT_VAL(sortedArray));

	for (uint32_t i = 0; i < array->size; i++) {
		sortedArray->values[i] = array->values[i];
	}
	sortedArray->size = array->size;

	quick_sort(sortedArray->values, 0, (int)sortedArray->size - 1);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(sortedArray));
	pop(currentModuleRecord);
	return res;
}

// arg0 - array
// arg1 - separator string
ObjectResult *array_join_method(VM *vm, int arg_count __attribute__((unused)),
				const Value *args)
{
	if (!IS_CRUX_STRING(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected arg <separator> to be of type 'string'.", TYPE);
	}

	const ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	const ObjectString *separator = AS_CRUX_STRING(args[1]);

	if (array->size == 0) {
		ObjectString *emptyResult = copy_string(vm, "", 0);
		push(vm->current_module_record, OBJECT_VAL(emptyResult));
		ObjectResult *res = new_ok_result(vm, OBJECT_VAL(emptyResult));
		pop(vm->current_module_record);
		return res;
	}

	// estimate: 3 chars per element + separators
	size_t bufferSize = array->size * 3 +
			    (array->size > 1
				     ? (array->size - 1) * separator->length
				     : 0);
	char *buffer = malloc(bufferSize);
	if (!buffer) {
		return MAKE_GC_SAFE_ERROR(vm, "Memory allocation failed", RUNTIME);
	}

	size_t actual_length = 0;

	for (uint32_t i = 0; i < array->size; i++) {
		ObjectString *element = to_string(vm, array->values[i]);
		push(vm->current_module_record, OBJECT_VAL(element));

		size_t neededSpace = element->length;
		if (i > 0) {
			neededSpace += separator->length;
		}

		if (actual_length + neededSpace > bufferSize) {
			// Grow the buffer by 50%
			const size_t newSize = (size_t)(((double)bufferSize *
							 1.5) +
							(double)neededSpace);
			char *newBuffer = realloc(buffer, newSize);
			if (!newBuffer) {
				free(buffer);
				pop(vm->current_module_record); // element
				return MAKE_GC_SAFE_ERROR(vm, "Memory reallocation failed", MEMORY);
			}
			buffer = newBuffer;
			bufferSize = newSize;
		}

		if (i > 0) {
			memcpy(buffer + actual_length, separator->chars,
			       separator->length);
			actual_length += separator->length;
		}

		memcpy(buffer + actual_length, element->chars, element->length);
		actual_length += element->length;

		pop(vm->current_module_record); // element
	}

	if (actual_length >= bufferSize) {
		char *newBuffer = realloc(buffer, actual_length + 1);
		if (!newBuffer) {
			free(buffer);
			return MAKE_GC_SAFE_ERROR(vm, "Memory reallocation failed", MEMORY);
		}
		buffer = newBuffer;
	}
	buffer[actual_length] = '\0';

	ObjectString *result = take_string(vm, buffer, (uint32_t)actual_length);
	push(vm->current_module_record, OBJECT_VAL(result));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result));
	pop(vm->current_module_record);
	return res;
}