#include <string.h>

#include "garbage_collector.h"
#include "panic.h"
#include "stdlib/array.h"
#include "stdlib/buffer.h"
#include "stdlib/complex.h"
#include "stdlib/core.h"
#include "stdlib/error.h"
#include "stdlib/fs.h"
#include "stdlib/io.h"
#include "stdlib/math.h"
#include "stdlib/matrix.h"
#include "stdlib/random.h"
#include "stdlib/range.h"
#include "stdlib/stdlib.h"
#include "stdlib/string.h"
#include "stdlib/sys.h"
#include "stdlib/tables.h"
#include "stdlib/time.h"
#include "stdlib/tuple.h"
#include "stdlib/vectors.h"
#include "value.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

static const Callable stringMethods[] = {
	{"first", string_first_method, 1, {STRING_TYPE}},
	{"last", string_last_method, 1, {STRING_TYPE}},
	{"get", string_get_method, 2, {STRING_TYPE, INT_TYPE}},
	{"upper", string_upper_method, 1, {STRING_TYPE}},
	{"lower", string_lower_method, 1, {STRING_TYPE}},
	{"strip", string_strip_method, 1, {STRING_TYPE}},
	{"starts_with",
	 string_starts_with_method,
	 2,
	 {STRING_TYPE, STRING_TYPE}},
	{"ends_with", string_ends_with_method, 2, {STRING_TYPE, STRING_TYPE}},
	{"contains", string_contains_method, 2, {STRING_TYPE, STRING_TYPE}},
	{"replace",
	 string_replace_method,
	 3,
	 {STRING_TYPE, STRING_TYPE, STRING_TYPE}},
	{"split", string_split_method, 2, {STRING_TYPE, INT_TYPE}},
	{"substring",
	 string_substring_method,
	 3,
	 {STRING_TYPE, INT_TYPE, INT_TYPE}},
	{"concat", string_concat_method, 2, {STRING_TYPE, STRING_TYPE}},
	{"is_empty", string_is_empty_method, 1, {STRING_TYPE}},
	{"is_alpha", string_is_alpha_method, 1, {STRING_TYPE}},
	{"is_digit", string_is_digit_method, 1, {STRING_TYPE}},
	{"is_lower", string_is_lower_method, 1, {STRING_TYPE}},
	{"is_upper", string_is_upper_method, 1, {STRING_TYPE}},
	{"is_space", string_is_space_method, 1, {STRING_TYPE}},
	{"is_alnum", string_is_al_num_method, 1, {STRING_TYPE}}};

static const Callable arrayMethods[] = {
	{"pop", array_pop_method, 1, {ARRAY_TYPE}},
	{"push", array_push_method, 2, {ARRAY_TYPE, ANY_TYPE}},
	{"insert", array_insert_method, 3, {ARRAY_TYPE, INT_TYPE, ANY_TYPE}},
	{"remove", array_remove_at_method, 2, {ARRAY_TYPE, INT_TYPE}},
	{"concat", array_concat_method, 2, {ARRAY_TYPE, ARRAY_TYPE}},
	{"slice", array_slice_method, 3, {ARRAY_TYPE, INT_TYPE, INT_TYPE}},
	{"reverse", array_reverse_method, 1, {ARRAY_TYPE}},
	{"index", array_index_of_method, 2, {ARRAY_TYPE, ANY_TYPE}},
	{"map", array_map_method, 2, {ARRAY_TYPE, FUNCTION_TYPE}},
	{"filter", array_filter_method, 2, {ARRAY_TYPE, FUNCTION_TYPE}},
	{"reduce",
	 array_reduce_method,
	 3,
	 {ARRAY_TYPE, FUNCTION_TYPE, ANY_TYPE}},
	{"sort", array_sort_method, 1, {ARRAY_TYPE}},
	{"join", array_join_method, 2, {ARRAY_TYPE, STRING_TYPE}},
	{"contains", array_contains_method, 2, {ARRAY_TYPE, ANY_TYPE}},
	{"clear", array_clear_method, 1, {ARRAY_TYPE}},
	{"equals", arrayEqualsMethod, 2, {ARRAY_TYPE, ARRAY_TYPE}}};

static const Callable tableMethods[] = {
	// TODO: Add hashable type which consists of Int, Float, String, Bool
	{"values", table_values_method, 1, {TABLE_TYPE}},
	{"keys", table_keys_method, 1, {TABLE_TYPE}},
	{"pairs", table_pairs_method, 1, {TABLE_TYPE}},
	{"remove", table_remove_method, 2, {TABLE_TYPE, ANY_TYPE}},
	{"get", table_get_method, 2, {TABLE_TYPE, ANY_TYPE}},
	{"has_key", table_has_key_method, 2, {TABLE_TYPE, ANY_TYPE}},
	{"get_or_else",
	 table_get_or_else_method,
	 3,
	 {TABLE_TYPE, ANY_TYPE, ANY_TYPE}}};

static const Callable errorMethods[] = {
	{"type", error_type_method, 1, {ERROR_TYPE}},
	{"message", error_message_method, 1, {ERROR_TYPE}},
};

static const Callable randomMethods[] = {
	{"seed", random_seed_method, 2, {RANDOM_TYPE, INT_TYPE}},
	{"int", random_int_method, 3, {RANDOM_TYPE, INT_TYPE, INT_TYPE}},
	{"float",
	 random_float_method,
	 3,
	 {RANDOM_TYPE, NUMERIC_TYPE, NUMERIC_TYPE}},
	{"bool", random_bool_method, 2, {RANDOM_TYPE, NUMERIC_TYPE}},
	{"choice", random_choice_method, 2, {RANDOM_TYPE, ARRAY_TYPE}},
	{"_next", random_next_method, 1, {RANDOM_TYPE}}};

static const Callable resultMethods[] = {
	{"_unwrap", unwrap_function, 1, {RESULT_TYPE}}};

static const Callable coreFunctions[] = {
	{"len", length_function, 1, {ANY_TYPE}},
	{"error", error_function, 1, {ANY_TYPE}},
	{"assert", assert_function, 2, {BOOL_TYPE, STRING_TYPE}},
	{"err", error_function, 1, {STRING_TYPE}},
	{"ok", ok_function, 1, {ANY_TYPE}},
	{"int", int_function, 1, {ANY_TYPE}},
	{"float", float_function, 1, {ANY_TYPE}},
	{"string", string_function, 1, {ANY_TYPE}},
	{"table", table_function, 1, {ANY_TYPE}},
	{"array", array_function, 1, {ANY_TYPE}},
	{"format", format_function, 2, {STRING_TYPE, TABLE_TYPE}},
};

static const Callable mathFunctions[] = {
	{"pow", pow_function, 2, {NUMERIC_TYPE, NUMERIC_TYPE}},
	{"sqrt", sqrt_function, 1, {NUMERIC_TYPE}},
	{"ceil", ceil_function, 1, {NUMERIC_TYPE}},
	{"floor", floor_function, 1, {NUMERIC_TYPE}},
	{"abs", abs_function, 1, {NUMERIC_TYPE}},
	{"sin", sin_function, 1, {NUMERIC_TYPE}},
	{"cos", cos_function, 1, {NUMERIC_TYPE}},
	{"tan", tan_function, 1, {NUMERIC_TYPE}},
	{"atan", atan_function, 1, {NUMERIC_TYPE}},
	{"acos", acos_function, 1, {NUMERIC_TYPE}},
	{"asin", asin_function, 1, {NUMERIC_TYPE}},
	{"exp", exp_function, 1, {NUMERIC_TYPE}},
	{"ln", ln_function, 1, {NUMERIC_TYPE}},
	{"log", log10_function, 1, {NUMERIC_TYPE}},
	{"round", round_function, 1, {NUMERIC_TYPE}},
	{"min", min_function, 2, {NUMERIC_TYPE, NUMERIC_TYPE}},
	{"max", max_function, 2, {NUMERIC_TYPE, NUMERIC_TYPE}},
	{"e", e_function, 0, {}},
	{"pi", pi_function, 0, {}},
	{"nan", nan_function, 0, {}},
	{"inf", inf_function, 0, {}}};

static const Callable ioFunctions[] = {
	{"print", io_print_function, 1, {ANY_TYPE}},
	{"println", io_println_function, 1, {ANY_TYPE}},
	{"print_to", io_print_to_function, 2, {STRING_TYPE, ANY_TYPE}},
	{"println_to", io_println_to_function, 2, {STRING_TYPE, ANY_TYPE}},
	{"scan", io_scan_function, 0, {}},
	{"scanln", io_scanln_function, 0, {}},
	{"nscan", io_nscan_function, 1, {INT_TYPE}},
	{"scan_from", io_scan_from_function, 1, {STRING_TYPE}},
	{"scanln_from", io_scanln_from_function, 1, {STRING_TYPE}},
	{"nscan_from", io_nscan_from_function, 2, {STRING_TYPE, INT_TYPE}},

};

static const Callable timeFunctions[] = {
	{"sleep_s", sleep_seconds_function, 1, {NUMERIC_TYPE}},
	{"sleep_ms", sleep_milliseconds_function, 1, {NUMERIC_TYPE}},
	{"_time_s", time_seconds_function_, 0, {}},
	{"_time_ms", time_milliseconds_function_, 0, {}},
	{"_year", year_function_, 0, {}},
	{"_month", month_function_, 0, {}},
	{"_day", day_function_, 0, {}},
	{"_hour", hour_function_, 0, {}},
	{"_minute", minute_function_, 0, {}},
	{"_second", second_function_, 0, {}},
	{"_weekday", weekday_function_, 0, {}},
	{"_day_of_year", day_of_year_function_, 0, {}},
};

static const Callable randomFunctions[] = {
	{"Random", random_init_function, 0, {}},
};

static const Callable systemFunctions[] = {
	{"args", args_function, 0, {}},
	{"get_env", get_env_function, 1, {STRING_TYPE}},
	{"sleep", sleep_function, 1, {INT_TYPE}},
	{"_platform", platform_function, 0, {}},
	{"_arch", arch_function, 0, {}},
	{"_pid", pid_function, 0, {}},
	{"exit", exit_function, 1, {INT_TYPE}}};

static const Callable fileSystemFunctions[] = {
	{"open", fs_open_function, 2, {STRING_TYPE, STRING_TYPE}},
	{"remove", fs_remove_function, 1, {STRING_TYPE}},
	{"size", fs_file_size_function, 1, {STRING_TYPE}},
	{"copy_file", fs_copy_file_function, 2, {STRING_TYPE, STRING_TYPE}},
	{"mkdir", fs_mkdir_function, 1, {STRING_TYPE}},
	{"read_file", fs_read_file_function, 1, {STRING_TYPE}},
	{"write_file", fs_write_file_function, 2, {STRING_TYPE, STRING_TYPE}},
	{"append_file", fs_append_file_function, 2, {STRING_TYPE, STRING_TYPE}},
	{"exists", fs_exists_function, 1, {STRING_TYPE}},
	{"is_file", fs_is_file_function, 1, {STRING_TYPE}},
	{"is_dir", fs_is_dir_function, 1, {STRING_TYPE}},
};

static const Callable fileSystemMethods[] = {
	{"close", fs_close_method, 1, {FILE_TYPE}},
	{"flush", fs_flush_method, 1, {FILE_TYPE}},
	{"read", fs_read_method, 2, {FILE_TYPE, INT_TYPE}},
	{"readln", fs_readln_method, 1, {FILE_TYPE}},
	{"read_all", fs_read_all_method, 1, {FILE_TYPE}},
	{"read_lines", fs_read_lines_method, 1, {FILE_TYPE}},
	{"write", fs_write_method, 2, {FILE_TYPE, STRING_TYPE}},
	{"writeln", fs_writeln_method, 2, {FILE_TYPE, STRING_TYPE}},
	{"seek", fs_seek_method, 3, {FILE_TYPE, INT_TYPE, STRING_TYPE}},
	{"tell", fs_tell_method, 1, {FILE_TYPE}},
	{"is_open", fs_is_open_method, 1, {FILE_TYPE}},
};

static const Callable complexFunctions[] = {
	{"Complex", new_complex_function, 2, {NUMERIC_TYPE, NUMERIC_TYPE}}};

static const Callable complexMethods[] = {
	{"add", add_complex_number_method, 2, {COMPLEX_TYPE, COMPLEX_TYPE}},
	{"sub", sub_complex_number_method, 2, {COMPLEX_TYPE, COMPLEX_TYPE}},
	{"mul", mul_complex_number_method, 2, {COMPLEX_TYPE, COMPLEX_TYPE}},
	{"div", div_complex_number_method, 2, {COMPLEX_TYPE, COMPLEX_TYPE}},
	{"scale", scale_complex_number_method, 2, {COMPLEX_TYPE, NUMERIC_TYPE}},
	{"real", complex_real_method, 1, {COMPLEX_TYPE}},
	{"imag", complex_imag_method, 1, {COMPLEX_TYPE}},
	{"conjugate", conjugate_complex_number_method, 1, {COMPLEX_TYPE}},
	{"mag", magnitude_complex_number_method, 1, {COMPLEX_TYPE}},
	{"square_mag",
	 square_magnitude_complex_number_method,
	 1,
	 {COMPLEX_TYPE}},
};

static const Callable vectorFunctions[] = {
	{"Vec", new_vector_function, 2, {INT_TYPE, ARRAY_TYPE}}};

static const Callable vectorMethods[] = {
	{"dot", vector_dot_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"add", vector_add_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"subtract", vector_subtract_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"multiply", vector_multiply_method, 2, {VECTOR_TYPE, NUMERIC_TYPE}},
	{"divide", vector_divide_method, 2, {VECTOR_TYPE, NUMERIC_TYPE}},
	{"magnitude", vector_magnitude_method, 1, {VECTOR_TYPE}},
	{"normalize", vector_normalize_method, 1, {VECTOR_TYPE}},
	{"distance", vector_distance_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"angle_between",
	 vector_angle_between_method,
	 2,
	 {VECTOR_TYPE, VECTOR_TYPE}},
	{"cross", vector_cross_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"lerp",
	 vector_lerp_method,
	 3,
	 {VECTOR_TYPE, VECTOR_TYPE, NUMERIC_TYPE}},
	{"reflect", vector_reflect_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"equals", vector_equals_method, 2, {VECTOR_TYPE, VECTOR_TYPE}},
	{"x", vector_x_method, 1, {VECTOR_TYPE}},
	{"y", vector_y_method, 1, {VECTOR_TYPE}},
	{"z", vector_z_method, 1, {VECTOR_TYPE}},
	{"w", vector_w_method, 1, {VECTOR_TYPE}},
	{"dimension", vector_dimension_method, 1, {VECTOR_TYPE}}};

static const Callable matrixFunctions[] = {
	{"Matrix", new_matrix_function, 2, {INT_TYPE, INT_TYPE}},
	{"IMatrix", new_matrix_identity_function, 1, {INT_TYPE}},
	{"AMatrix",
	 new_matrix_from_array_function,
	 3,
	 {INT_TYPE, INT_TYPE, ARRAY_TYPE}},
};

static const Callable matrixMethods[] = {
	{"get", matrix_get_method, 3, {MATRIX_TYPE, INT_TYPE, INT_TYPE}},
	{"set", matrix_set_method, 3, {MATRIX_TYPE, INT_TYPE, INT_TYPE}},
	{"add", matrix_add_method, 2, {MATRIX_TYPE, MATRIX_TYPE}},
	{"sub", matrix_subtract_method, 2, {MATRIX_TYPE, MATRIX_TYPE}},
	{"mul", matrix_multiply_method, 2, {MATRIX_TYPE, MATRIX_TYPE}},
	{"scale", matrix_scale_method, 2, {MATRIX_TYPE, NUMERIC_TYPE}},
	{"transpose", matrix_transpose_method, 1, {MATRIX_TYPE}},
	{"determinant", matrix_determinant_method, 1, {MATRIX_TYPE}},
	{"inverse", matrix_inverse_method, 1, {MATRIX_TYPE}},
	{"trace", matrix_trace_method, 1, {MATRIX_TYPE}},
	{"rank", matrix_rank_method, 1, {MATRIX_TYPE}},
	{"row", matrix_row_method, 2, {MATRIX_TYPE, INT_TYPE}},
	{"col", matrix_col_method, 2, {MATRIX_TYPE, INT_TYPE}},
	{"equals", matrix_equals_method, 2, {MATRIX_TYPE, MATRIX_TYPE}},
	{"copy", matrix_copy_method, 1, {MATRIX_TYPE}},
	{"to_array", matrix_to_array_method, 1, {MATRIX_TYPE}},
	{"mul_vec",
	 matrix_multiply_vector_method,
	 2,
	 {MATRIX_TYPE, VECTOR_TYPE}},
	{"rows", matrix_rows_method, 1, {MATRIX_TYPE}},
	{"cols", matrix_cols_method, 1, {MATRIX_TYPE}},
};

static const Callable range_functions[] = {
	{"Range", new_range_function, 3, {INT_TYPE, INT_TYPE, INT_TYPE}}};

static const Callable range_methods[] = {
	{"contains", contains_range_method, 2, {RANGE_TYPE, INT_TYPE}},
	{"to_array", to_array_range_method, 1, {RANGE_TYPE}},
	{"start", start_range_method, 1, {RANGE_TYPE}},
	{"end", end_range_method, 1, {RANGE_TYPE}},
	{"step", step_range_method, 1, {RANGE_TYPE}},
	{"is_empty", is_empty_range_method, 1, {RANGE_TYPE}},
	{"reversed", reversed_range_method, 1, {RANGE_TYPE}},
};

static const Callable tuple_functions[] = {
	{"Tuple", new_tuple_function, 1, {ARRAY_TYPE}},
};

static const Callable tuple_methods[] = {
	{"get", get_tuple_method, 2, {TUPLE_TYPE, INT_TYPE}},
	{"slice", slice_tuple_method, 3, {TUPLE_TYPE, INT_TYPE, INT_TYPE}},
	{"index", index_tuple_method, 2, {TUPLE_TYPE, ANY_TYPE}},
	{"is_empty", is_empty_tuple_method, 1, {TUPLE_TYPE}},
	{"to_array", to_array_tuple_method, 1, {TUPLE_TYPE}},
	{"first", first_tuple_method, 1, {TUPLE_TYPE}},
	{"last", last_tuple_method, 1, {TUPLE_TYPE}},
	{"contains", contains_tuple_method, 2, {TUPLE_TYPE, ANY_TYPE}},
	{"equals", equals_tuple_method, 2, {TUPLE_TYPE, TUPLE_TYPE}},
};

static const Callable buffer_functions[] = {
	{"Buffer", new_buffer_function, 0, {}},
};

static const Callable buffer_methods[] = {
	{"write_byte", write_byte_buffer_method, 2, {BUFFER_TYPE, INT_TYPE}},
	{"write_int16_le",
	 write_int16_le_buffer_method,
	 2,
	 {BUFFER_TYPE, INT_TYPE}},
	{"write_int32_le",
	 write_int32_le_buffer_method,
	 2,
	 {BUFFER_TYPE, INT_TYPE}},
	{"write_float32_le",
	 write_float32_le_buffer_method,
	 2,
	 {BUFFER_TYPE, FLOAT_TYPE}},
	{"write_float64_le",
	 write_float64_le_buffer_method,
	 2,
	 {BUFFER_TYPE, FLOAT_TYPE}},
	{"write_int16_be",
	 write_int16_be_buffer_method,
	 2,
	 {BUFFER_TYPE, INT_TYPE}},
	{"write_int32_be",
	 write_int32_be_buffer_method,
	 2,
	 {BUFFER_TYPE, INT_TYPE}},
	{"write_float32_be",
	 write_float32_be_buffer_method,
	 2,
	 {BUFFER_TYPE, FLOAT_TYPE}},
	{"write_float64_be",
	 write_float64_be_buffer_method,
	 2,
	 {BUFFER_TYPE, FLOAT_TYPE}},
	{"write_string",
	 write_string_buffer_method,
	 2,
	 {BUFFER_TYPE, STRING_TYPE}},
	{"write_buffer",
	 write_buffer_buffer_method,
	 2,
	 {BUFFER_TYPE, BUFFER_TYPE}},
	{"read_byte", read_byte_buffer_method, 1, {BUFFER_TYPE}},
	{"read_string", read_string_buffer_method, 1, {BUFFER_TYPE}},
	{"read_line", read_line_buffer_method, 1, {BUFFER_TYPE, BUFFER_TYPE}},
	{"read_all", read_all_buffer_method, 1, {BUFFER_TYPE}},
	{"read_int16_le", read_int16_le_buffer_method, 1, {BUFFER_TYPE}},
	{"read_int32_le", read_int32_le_buffer_method, 1, {BUFFER_TYPE}},
	{"read_float32_le", read_float32_le_buffer_method, 1, {BUFFER_TYPE}},
	{"read_float64_le", read_float64_le_buffer_method, 1, {BUFFER_TYPE}},
	{"read_int16_be", read_int16_be_buffer_method, 1, {BUFFER_TYPE}},
	{"read_int32_be", read_int32_be_buffer_method, 1, {BUFFER_TYPE}},
	{"read_float32_be", read_float32_be_buffer_method, 1, {BUFFER_TYPE}},
	{"read_float64_be", read_float64_be_buffer_method, 1, {BUFFER_TYPE}},
	{"capacity", capacity_buffer_method, 1, {BUFFER_TYPE}},
	{"is_empty", is_empty_buffer_method, 1, {BUFFER_TYPE}},
	{"clear", clear_buffer_method, 1, {BUFFER_TYPE}},
	{"peek_byte", peek_byte_buffer_method, 1, {BUFFER_TYPE}},
	{"skip_bytes", skip_bytes_buffer_method, 2, {BUFFER_TYPE, INT_TYPE}},
	{"clone", clone_buffer_method, 1, {BUFFER_TYPE}},
	{"compact", compact_buffer_method, 1, {BUFFER_TYPE}},
};

bool register_native_method(VM *vm, Table *method_table,
			    const char *method_name,
			    const CruxCallable method_function, const int arity,
			    const TypeMask *arg_types)
{
	ObjectString *name = copy_string(vm, method_name,
					 (int)strlen(method_name));
	table_set(vm, method_table, name,
		  OBJECT_VAL(new_native_callable(vm, method_function, arity,
						 name, arg_types)));
	return true;
}

static bool registerMethods(VM *vm, Table *methodTable, const Callable *methods,
			    const int count)
{
	for (int i = 0; i < count; i++) {
		const Callable method = methods[i];
		register_native_method(vm, methodTable, method.name,
				       method.function, method.arity,
				       method.arg_types);
	}
	return true;
}

static bool registerNativeFunction(VM *vm, Table *functionTable,
				   const char *functionName,
				   const CruxCallable function, const int arity,
				   const TypeMask *arg_types)
{
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	ObjectString *name = copy_string(vm, functionName,
					 (int)strlen(functionName));
	push(currentModuleRecord, OBJECT_VAL(name));
	const Value func = OBJECT_VAL(
		new_native_callable(vm, function, arity, name, arg_types));
	push(currentModuleRecord, func);
	const bool success = table_set(vm, functionTable, name, func);

	pop(currentModuleRecord);
	pop(currentModuleRecord);

	if (!success) {
		return false;
	}
	return true;
}

static bool registerNativeFunctions(VM *vm, Table *functionTable,
				    const Callable *functions, const int count)
{
	for (int i = 0; i < count; i++) {
		const Callable function = functions[i];
		if (!registerNativeFunction(vm, functionTable, function.name,
					    function.function, function.arity,
					    function.arg_types)) {
			return false;
		}
	}
	return true;
}

static bool initModule(VM *vm, const char *moduleName,
		       const Callable *functions, const int functionsCount)
{
	Table *moduleTable = ALLOCATE(vm, Table, 1);
	if (moduleTable == NULL)
		return false;

	init_table(moduleTable);

	if (functions != NULL) {
		if (!registerNativeFunctions(vm, moduleTable, functions,
					     functionsCount)) {
			return false;
		}
	}

	if (vm->native_modules.count + 1 > vm->native_modules.capacity) {
		const int newCapacity = vm->native_modules.capacity == 0
						? 8
						: vm->native_modules.capacity *
							  2;
		NativeModule *newModules =
			GROW_ARRAY(vm, NativeModule, vm->native_modules.modules,
				   vm->native_modules.capacity, newCapacity);
		vm->native_modules.modules = newModules;
		vm->native_modules.capacity = newCapacity;
	}

	ObjectString *nameCopy = copy_string(vm, moduleName,
					     (int)strlen(moduleName));

	vm->native_modules.modules[vm->native_modules.count] =
		(NativeModule){.name = nameCopy, .names = moduleTable};
	vm->native_modules.count++;

	return true;
}

static bool initTypeMethodTable(VM *vm, Table *methodTable,
				const Callable *methods, const int methodCount)
{
	if (methods != NULL) {
		registerMethods(vm, methodTable, methods, methodCount);
	}

	return true;
}

bool initialize_std_lib(VM *vm)
{
	if (!registerNativeFunctions(vm, &vm->current_module_record->globals,
				     coreFunctions,
				     ARRAY_COUNT(coreFunctions))) {
		return false;
	}

	initTypeMethodTable(vm, &vm->string_type, stringMethods,
			    ARRAY_COUNT(stringMethods));

	initTypeMethodTable(vm, &vm->array_type, arrayMethods,
			    ARRAY_COUNT(arrayMethods));

	initTypeMethodTable(vm, &vm->table_type, tableMethods,
			    ARRAY_COUNT(tableMethods));

	initTypeMethodTable(vm, &vm->error_type, errorMethods,
			    ARRAY_COUNT(errorMethods));

	initTypeMethodTable(vm, &vm->random_type, randomMethods,
			    ARRAY_COUNT(randomMethods));

	initTypeMethodTable(vm, &vm->file_type, fileSystemMethods,
			    ARRAY_COUNT(fileSystemMethods));

	initTypeMethodTable(vm, &vm->result_type, resultMethods,
			    ARRAY_COUNT(resultMethods));

	initTypeMethodTable(vm, &vm->vector_type, vectorMethods,
			    ARRAY_COUNT(vectorMethods));

	initTypeMethodTable(vm, &vm->complex_type, complexMethods,
			    ARRAY_COUNT(complexMethods));

	initTypeMethodTable(vm, &vm->matrix_type, matrixMethods,
			    ARRAY_COUNT(matrixMethods));

	initTypeMethodTable(vm, &vm->range_type, range_methods,
			    ARRAY_COUNT(range_methods));

	initTypeMethodTable(vm, &vm->tuple_type, tuple_methods,
			    ARRAY_COUNT(tuple_methods));

	// Initialize standard library modules
	if (!initModule(vm, "math", mathFunctions,
			ARRAY_COUNT(mathFunctions))) {
		return false;
	}

	if (!initModule(vm, "fs", fileSystemFunctions,
			ARRAY_COUNT(fileSystemFunctions))) {
		return false;
	}

	if (!initModule(vm, "io", ioFunctions, ARRAY_COUNT(ioFunctions))) {
		return false;
	}

	if (!initModule(vm, "time", timeFunctions,
			ARRAY_COUNT(timeFunctions))) {
		return false;
	}

	if (!initModule(vm, "random", randomFunctions,
			ARRAY_COUNT(randomFunctions))) {
		return false;
	}

	if (!initModule(vm, "sys", systemFunctions,
			ARRAY_COUNT(systemFunctions))) {
		return false;
	}

	if (!initModule(vm, "vectors", vectorFunctions,
			ARRAY_COUNT(vectorFunctions))) {
		return false;
	}

	if (!initModule(vm, "complex", complexFunctions,
			ARRAY_COUNT(complexFunctions))) {
		return false;
	}

	if (!initModule(vm, "matrix", matrixFunctions,
			ARRAY_COUNT(matrixFunctions))) {
		return false;
	}

	if (!initModule(vm, "range", range_functions,
			ARRAY_COUNT(range_functions))) {
		return false;
	}

	if (!initModule(vm, "tuple", tuple_functions,
			ARRAY_COUNT(tuple_functions))) {
		return false;
	}

	return true;
}
