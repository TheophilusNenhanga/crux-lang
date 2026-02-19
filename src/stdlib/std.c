#include "stdlib/std.h"

#include <string.h>

#include "garbage_collector.h"
#include "panic.h"
#include "stdlib/array.h"
#include "stdlib/complex.h"
#include "stdlib/core.h"
#include "stdlib/error.h"
#include "stdlib/fs.h"
#include "stdlib/io.h"
#include "stdlib/math.h"
#include "stdlib/matrix.h"
#include "stdlib/random.h"
#include "stdlib/string.h"
#include "stdlib/sys.h"
#include "stdlib/tables.h"
#include "stdlib/time.h"
#include "stdlib/vectors.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

static const Callable stringMethods[] = {
	{"first", string_first_method, 1},
	{"last", string_last_method, 1},
	{"get", string_get_method, 2},
	{"upper", string_upper_method, 1},
	{"lower", string_lower_method, 1},
	{"strip", string_strip_method, 1},
	{"starts_with", string_starts_with_method, 2},
	{"ends_with", string_ends_with_method, 2},
	{"contains", string_contains_method, 2},
	{"replace", string_replace_method, 3},
	{"split", string_split_method, 2},
	{"substring", string_substring_method, 3},
	{"concat", string_concat_method, 2},
	{"is_empty", string_is_empty_method, 1},
	{"is_alpha", string_is_alpha_method, 1},
	{"is_digit", string_is_digit_method, 1},
	{"is_lower", string_is_lower_method, 1},
	{"is_upper", string_is_upper_method, 1},
	{"is_space", string_is_space_method, 1},
	{"is_alnum", string_is_al_num_method, 1}};

static const Callable arrayMethods[] = {{"pop", array_pop_method, 1},
					{"push", array_push_method, 2},
					{"insert", array_insert_method, 3},
					{"remove", array_remove_at_method, 2},
					{"concat", array_concat_method, 2},
					{"slice", array_slice_method, 3},
					{"reverse", array_reverse_method, 1},
					{"index", array_index_of_method, 2},
					{"map", array_map_method, 2},
					{"filter", array_filter_method, 2},
					{"reduce", array_reduce_method, 3},
					{"sort", array_sort_method, 1},
					{"join", array_join_method, 2},
					{"contains", array_contains_method, 2},
					{"clear", array_clear_method, 1},
					{"equals", arrayEqualsMethod, 2}};

static const Callable tableMethods[] = {{"values", table_values_method, 1},
					{"keys", table_keys_method, 1},
					{"pairs", table_pairs_method, 1},
					{"remove", table_remove_method, 2},
					{"get", table_get_method, 2},
					{"has_key", table_has_key_method, 2},
					{"get_or_else",
					 table_get_or_else_method, 3}};

static const Callable errorMethods[] = {
	{"type", error_type_method, 1},
	{"message", error_message_method, 1},
};

static const Callable randomMethods[] = {{"seed", random_seed_method, 2},
					 {"int", random_int_method, 3},
					 {"double", random_double_method, 3},
					 {"bool", random_bool_method, 2},
					 {"choice", random_choice_method, 2},
					 {"_next", random_next_method, 1}};

static const Callable resultMethods[] = {{"_unwrap", unwrap_function, 1}};

static const Callable coreFunctions[] = {
	{"len", length_function, 1},	{"error", error_function, 1},
	{"assert", assert_function, 2}, {"err", error_function, 1},
	{"ok", ok_function, 1},		{"int", int_function, 1},
	{"float", float_function, 1},	{"string", string_function, 1},
	{"table", table_function, 1},	{"array", array_function, 1},
	{"format", format_function, 2}, {"_int", int_function_, 1},
	{"_float", float_function_, 1}, {"_string", string_function_, 1},
	{"_table", table_function_, 1}, {"_array", array_function_, 1}};

static const Callable mathFunctions[] = {
	{"pow", pow_function, 2},     {"sqrt", sqrt_function, 1},
	{"ceil", ceil_function, 1},   {"floor", floor_function, 1},
	{"abs", abs_function, 1},     {"sin", sin_function, 1},
	{"cos", cos_function, 1},     {"tan", tan_function, 1},
	{"atan", atan_function, 1},   {"acos", acos_function, 1},
	{"asin", asin_function, 1},   {"exp", exp_function, 1},
	{"ln", ln_function, 1},	      {"log", log10_function, 1},
	{"round", round_function, 1}, {"min", min_function, 2},
	{"max", max_function, 2},     {"e", e_function, 0},
	{"pi", pi_function, 0},	      {"nan", nan_function, 0},
	{"inf", inf_function, 0}};

static const Callable ioFunctions[] = {
	{"print", io_print_function, 1},
	{"println", io_println_function, 1},
	{"print_to", io_print_to_function, 2},
	{"println_to", io_println_to_function, 2},
	{"scan", io_scan_function, 0},
	{"scanln", io_scanln_function, 0},
	{"nscan", io_nscan_function, 1},
	{"scan_from", io_scan_from_function, 1},
	{"scanln_from", io_scanln_from_function, 1},
	{"nscan_from", io_nscan_from_function, 2}

};

static const Callable timeFunctions[] = {
	{"sleep_s", sleep_seconds_function, 1},
	{"sleep_ms", sleep_milliseconds_function, 1},
	{"_time_s", time_seconds_function_, 0},
	{"_time_ms", time_milliseconds_function_, 0},
	{"_year", year_function_, 0},
	{"_month", month_function_, 0},
	{"_day", day_function_, 0},
	{"_hour", hour_function_, 0},
	{"_minute", minute_function_, 0},
	{"_second", second_function_, 0},
	{"_weekday", weekday_function_, 0},
	{"_day_of_year", day_of_year_function_, 0},
};

static const Callable randomFunctions[] = {
	{"Random", random_init_function, 0},
};

static const Callable systemFunctions[] = {{"args", args_function, 0},
					   {"get_env", get_env_function, 1},
					   {"sleep", sleep_function, 1},
					   {"_platform", platform_function, 0},
					   {"_arch", arch_function, 0},
					   {"_pid", pid_function, 0},
					   {"_exit", exit_function, 1}};

static const Callable fileSystemFunctions[] = {
	{"open", fs_open_function, 2},
	{"remove", fs_remove_function, 1},
	{"size", fs_file_size_function, 1},
	{"copy_file", fs_copy_file_function, 2},
	{"mkdir", fs_mkdir_function, 1},
	{"read_file", fs_read_file_function, 1},
	{"write_file", fs_write_file_function, 2},
	{"append_file", fs_append_file_function, 2},
	{"exists", fs_exists_function, 1},
	{"is_file", fs_is_file_function, 1},
	{"is_dir", fs_is_dir_function, 1},
};

static const Callable fileSystemMethods[] = {
	{"close", fs_close_method, 1},
	{"flush", fs_flush_method, 1},
	{"read", fs_read_method, 2},
	{"readln", fs_readln_method, 1},
	{"read_all", fs_read_all_method, 1},
	{"read_lines", fs_read_lines_method, 1},
	{"write", fs_write_method, 1},
	{"writeln", fs_writeln_method, 2},
	{"seek", fs_seek_method, 3},
	{"tell", fs_tell_method, 1},
	{"is_open", fs_is_open_method, 1},
};

static const Callable complexFunctions[] = {
	{"Complex", new_complex_function, 2}};

static const Callable complexMethods[] = {
	{"add", add_complex_number_method, 2},
	{"sub", sub_complex_number_method, 2},
	{"mul", mul_complex_number_method, 2},
	{"div", div_complex_number_method, 2},
	{"scale", scale_complex_number_method, 2},
	{"real", complex_real_method, 1},
	{"imag", complex_imag_method, 1},
	{"conjugate", conjugate_complex_number_method, 1},
	{"mag", magnitude_complex_number_method, 1},
	{"square_mag", square_magnitude_complex_number_method, 1},
};

static const Callable vectorFunctions[] = {{"Vec", new_vector_function, 2}};

static const Callable vectorMethods[] = {
	{"dot", vector_dot_method, 2},
	{"add", vector_add_method, 2},
	{"subtract", vector_subtract_method, 2},
	{"multiply", vector_multiply_method, 2},
	{"divide", vector_divide_method, 2},
	{"magnitude", vector_magnitude_method, 1},
	{"normalize", vector_normalize_method, 1},
	{"distance", vector_distance_method, 2},
	{"angle_between", vector_angle_between_method, 2},
	{"cross", vector_cross_method, 2},
	{"lerp", vector_lerp_method, 3},
	{"reflect", vector_reflect_method, 2},
	{"equals", vector_equals_method, 2},
	{"x", vector_x_method, 1},
	{"y", vector_y_method, 1},
	{"z", vector_z_method, 1},
	{"w", vector_w_method, 1},
	{"dimension", vector_dimension_method, 1}};

static const Callable matrixFunctions[] = {
	{"Matrix", new_matrix_function, 2},
	{"IMatrix", new_matrix_identity_function, 1},
	{"AMatrix", new_matrix_from_array_function, 3},
};

static const Callable matrixMethods[] = {
	{"get", matrix_get_method, 3},
	{"set", matrix_set_method, 3},
	{"add", matrix_add_method, 2},
	{"sub", matrix_subtract_method, 2},
	{"mul", matrix_multiply_method, 2},
	{"scale", matrix_scale_method, 2},
	{"transpose", matrix_transpose_method, 1},
	{"determinant", matrix_determinant_method, 1},
	{"inverse", matrix_inverse_method, 1},
	{"trace", matrix_trace_method, 1},
	{"rank", matrix_rank_method, 1},
	{"row", matrix_row_method, 2},
	{"col", matrix_col_method, 2},
	{"equals", matrix_equals_method, 2},
	{"copy", matrix_copy_method, 1},
	{"to_array", matrix_to_array_method, 1},
	{"mul_vec", matrix_multiply_vector_method, 2},
	{"rows", matrix_rows_method, 1},
	{"cols", matrix_cols_method, 1},
};

bool register_native_method(VM *vm, Table *method_table,
			    const char *method_name,
			    const CruxCallable method_function, const int arity)
{
	ObjectString *name = copy_string(vm, method_name,
					 (int)strlen(method_name));
	table_set(vm, method_table, name,
		  OBJECT_VAL(
			  new_native_method(vm, method_function, arity, name)));
	return true;
}

static bool registerMethods(VM *vm, Table *methodTable, const Callable *methods,
			    const int count)
{
	for (int i = 0; i < count; i++) {
		const Callable method = methods[i];
		register_native_method(vm, methodTable, method.name,
				       method.function, method.arity);
	}
	return true;
}

static bool registerNativeFunction(VM *vm, Table *functionTable,
				   const char *functionName,
				   const CruxCallable function, const int arity)
{
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	ObjectString *name = copy_string(vm, functionName,
					 (int)strlen(functionName));
	push(currentModuleRecord, OBJECT_VAL(name));
	const Value func = OBJECT_VAL(
		new_native_function(vm, function, arity, name));
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
					    function.function,
					    function.arity)) {
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

	return true;
}
