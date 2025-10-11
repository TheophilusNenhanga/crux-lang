#include <string.h>

#include "stdlib/std.h"
#include "memory.h"
#include "panic.h"
#include "stdlib/array.h"
#include "stdlib/core.h"
#include "stdlib/error.h"
#include "stdlib/fs.h"
#include "stdlib/io.h"
#include "stdlib/math.h"
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
	{"substring", string_substring_method, 3}};

static const InfallibleCallable stringInfallibleMethods[] = {
	{"_is_empty", string_is_empty_method, 1},
	{"_is_alpha", string_is_alpha_method, 1},
	{"_is_digit", string_is_digit_method, 1},
	{"_is_lower", string_is_lower_method, 1},
	{"_is_upper", string_is_upper_method, 1},
	{"_is_space", string_is_space_method, 1},
	{"_is_alnum", string_is_al_num_method, 1}};

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
					{"join", array_join_method, 2}};

static const InfallibleCallable arrayInfallibleMethods[] = {
	{"_contains", array_contains_method, 2},
	{"_clear", array_clear_method, 1},
	{"_equals", arrayEqualsMethod, 2}};

static const Callable tableMethods[] = {{"values", table_values_method, 1},
					{"keys", table_keys_method, 1},
					{"pairs", table_pairs_method, 1},
					{"remove", table_remove_method, 2},
					{"get", table_get_method, 2}};

static const InfallibleCallable tableInfallibleMethods[] = {
	{"_has_key", table_has_key_method, 2},
	{"_get_or_else", table_get_or_else_method, 3}};

static const Callable errorMethods[] = {
	{"type", error_type_method, 1},
};

static const InfallibleCallable errorInfallibleMethods[] = {
	{"message", error_message_method, 1},
};

static const Callable randomMethods[] = {{"seed", random_seed_method, 2},
					 {"int", random_int_method, 3},
					 {"double", random_double_method, 3},
					 {"bool", random_bool_method, 2},
					 {"choice", random_choice_method, 2}};

static const InfallibleCallable randomInfallibleMethods[] = {
	{"_next", random_next_method, 1}};

static const Callable fileMethods[] = {{"readln", readln_file_method, 1},
				       {"read_all", read_all_file_method, 1},
				       {"write", write_file_method, 2},
				       {"writeln", writeln_file_method, 2},
				       {"close", close_file_method, 1}};

static const InfallibleCallable resultInfallibleMethods[] = {
	{"_unwrap", unwrap_function, 1}};

static const Callable coreFunctions[] = {
	{"scanln", scanln_function, 0}, {"panic", panic_function, 1},
	{"len", length_function, 1},	{"error", error_function, 1},
	{"assert", assert_function, 2}, {"err", error_function, 1},
	{"ok", ok_function, 1},		{"int", int_function, 1},
	{"float", float_function, 1},	{"string", string_function, 1},
	{"table", table_function, 1},	{"array", array_function, 1},
};

static const InfallibleCallable coreInfallibleFunctions[] = {
	{"_len", length_function_, 1},	{"println", println_function, 1},
	{"_print", print_function, 1},	{"_int", int_function_, 1},
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
	{"max", max_function, 2}};

static const InfallibleCallable mathInfallibleFunctions[] =
	{{"_e", e_function, 0}, {"_pi", pi_function, 0}};

static const Callable ioFunctions[] = {
	{"print_to", print_to_function, 2},
	{"scan", scan_function, 0},
	{"scanln", scanln_function, 0},
	{"scan_from", scan_from_function, 1},
	{"scanln_from", scanln_from_function, 1},
	{"nscan", nscan_function, 1},
	{"nscan_from", nscan_from_function, 2},
	{"open_file", open_file_function, 2},
};

static const Callable timeFunctions[] = {
	{"sleep_s", sleep_seconds_function, 1},
	{"sleep_ms", sleep_milliseconds_function, 1},
};

static const InfallibleCallable timeInfallibleFunctions[] = {
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

static const InfallibleCallable randomInfallibleFunctions[] = {
	{"Random", random_init_function, 0},
};

static const Callable systemFunctions[] = {
	{"args", args_function, 0},
	{"get_env", get_env_function, 1},
	{"sleep", sleep_function, 1},
};

static const InfallibleCallable systemInfallibleFunctions[] = {
	{"_platform", platform_function, 0},
	{"_arch", arch_function, 0},
	{"_pid", pid_function, 0},
	{"_exit", exit_function, 1}};

static const Callable fileSystemFunctions[] = {
	{"lisr_dir", list_dir_function, 1},
	{"is_file", is_file_function, 1},
	{"is_dir", is_dir_function, 1},
	{"make_dir", make_dir_function, 1},
	{"delete_dir", make_dir_function, 1},
	{"path_exists", path_exists_function, 1},
	{"rename", rename_function, 2},
	{"copy_file", copy_file_function, 2},
	{"is_file_in", is_file_in_function, 2}};

static const Callable vectorFunctions[] = {{"Vec2", new_vec2_function, 2},
					   {"Vec3", new_vec3_function, 3}};

static const Callable vec2Methods[] = {
	{"dot", vec2_dot_method, 2},
	{"add", vec2_add_method, 2},
	{"subtract", vec2_subtract_method, 2},
	{"multiply", vec2_multiply_method, 2},
	{"divide", vec2_divide_method, 2},
	{"magnitude", vec2_magnitude_method, 1},
	{"normalize", vec2_normalize_method, 1},
	{"distance", vec2_distance_method, 2},
	{"angle", vec2_angle_method, 1},
	{"rotate", vec2_rotate_method, 2},
	{"lerp", vec2_lerp_method, 3},
	{"reflect", vec2_reflect_method, 2},
	{"equals", vec2_equals_method, 2},
};

static const InfallibleCallable vec2InfallibleMethods[] = {
	{"x", vec2_x_method, 1},
	{"y", vec2_y_method, 1},
};

static const Callable vec3Methods[] = {
	{"dot", vec3_dot_method, 2},
	{"add", vec3_add_method, 2},
	{"subtract", vec3_subtract_method, 2},
	{"multiply", vec3_multiply_method, 2},
	{"divide", vec3_divide_method, 2},
	{"magnitude", vec3_magnitude_method, 1},
	{"normalize", vec3_normalize_method, 1},
	{"distance", vec3_distance_method, 2},
	{"angle_between", vec3_angle_between_method, 2},
	{"cross", vec3_cross_method, 2},
	{"lerp", vec3_lerp_method, 3},
	{"reflect", vec3_reflect_method, 2},
	{"equals", vec3_equals_method, 2},
};

static const InfallibleCallable vec3InfallibleMethods[] = {
	{"x", vec3_x_method, 1},
	{"y", vec3_y_method, 1},
	{"z", vec3_z_method, 1},
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

bool register_native_infallible_method(
	VM *vm, Table *method_table, const char *method_name,
	const CruxInfallibleCallable method_function, const int arity)
{
	ObjectString *name = copy_string(vm, method_name,
					 (int)strlen(method_name));
	table_set(vm, method_table, name,
		  OBJECT_VAL(new_native_infallible_method(vm, method_function,
							  arity, name)));
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

static bool registerInfallibleMethods(VM *vm, Table *methodTable,
				      const InfallibleCallable *methods,
				      const int count)
{
	for (int i = 0; i < count; i++) {
		const InfallibleCallable method = methods[i];
		register_native_infallible_method(vm, methodTable, method.name,
						  method.function,
						  method.arity);
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

static bool registerNativeInfallibleFunction(
	VM *vm, Table *functionTable, const char *functionName,
	const CruxInfallibleCallable function, const int arity)
{
	ObjectModuleRecord *currentModuleRecord = vm->current_module_record;
	ObjectString *name = copy_string(vm, functionName,
					 (int)strlen(functionName));
	push(currentModuleRecord, OBJECT_VAL(name));
	const Value func = OBJECT_VAL(
		new_native_infallible_function(vm, function, arity, name));
	push(currentModuleRecord, func);

	const bool success = table_set(vm, functionTable, name, func);
	if (!success) {
		return false;
	}
	pop(currentModuleRecord);
	pop(currentModuleRecord);

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

static bool
registerNativeInfallibleFunctions(VM *vm, Table *functionTable,
				  const InfallibleCallable *functions,
				  const int count)
{
	for (int i = 0; i < count; i++) {
		const InfallibleCallable function = functions[i];
		if (!registerNativeInfallibleFunction(vm, functionTable,
						      function.name,
						      function.function,
						      function.arity)) {
			return false;
		}
	}
	return true;
}

static bool initModule(VM *vm, const char *moduleName,
		       const Callable *functions, const int functionsCount,
		       const InfallibleCallable *infallibles,
		       const int infalliblesCount)
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

	if (infallibles != NULL) {
		if (!registerNativeInfallibleFunctions(vm, moduleTable,
						       infallibles,
						       infalliblesCount)) {
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
				const Callable *methods, const int methodCount,
				const InfallibleCallable *infallibleMethods,
				const int infallibleCount)
{
	if (methods != NULL) {
		registerMethods(vm, methodTable, methods, methodCount);
	}

	if (infallibleMethods != NULL) {
		registerInfallibleMethods(vm, methodTable, infallibleMethods,
					  infallibleCount);
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

	if (!registerNativeInfallibleFunctions(
		    vm, &vm->current_module_record->globals,
		    coreInfallibleFunctions,
		    ARRAY_COUNT(coreInfallibleFunctions))) {
		return false;
	}

	initTypeMethodTable(vm, &vm->string_type, stringMethods,
			    ARRAY_COUNT(stringMethods), stringInfallibleMethods,
			    ARRAY_COUNT(stringInfallibleMethods));

	initTypeMethodTable(vm, &vm->array_type, arrayMethods,
			    ARRAY_COUNT(arrayMethods), arrayInfallibleMethods,
			    ARRAY_COUNT(arrayInfallibleMethods));

	initTypeMethodTable(vm, &vm->table_type, tableMethods,
			    ARRAY_COUNT(tableMethods), tableInfallibleMethods,
			    ARRAY_COUNT(tableInfallibleMethods));

	initTypeMethodTable(vm, &vm->error_type, errorMethods,
			    ARRAY_COUNT(errorMethods), errorInfallibleMethods,
			    ARRAY_COUNT(errorInfallibleMethods));

	initTypeMethodTable(vm, &vm->random_type, randomMethods,
			    ARRAY_COUNT(randomMethods), randomInfallibleMethods,
			    ARRAY_COUNT(randomInfallibleMethods));

	initTypeMethodTable(vm, &vm->file_type, fileMethods,
			    ARRAY_COUNT(fileMethods), NULL, 0);

	initTypeMethodTable(vm, &vm->result_type, NULL, 0,
			    resultInfallibleMethods,
			    ARRAY_COUNT(resultInfallibleMethods));

	initTypeMethodTable(vm, &vm->vec2_type, vec2Methods,
			    ARRAY_COUNT(vec2Methods), vec2InfallibleMethods,
			    ARRAY_COUNT(vec2InfallibleMethods));
	initTypeMethodTable(vm, &vm->vec3_type, vec3Methods,
			    ARRAY_COUNT(vec3Methods), vec3InfallibleMethods,
			    ARRAY_COUNT(vec3InfallibleMethods));

	// Initialize standard library modules
	if (!initModule(vm, "math", mathFunctions, ARRAY_COUNT(mathFunctions),
			mathInfallibleFunctions,
			ARRAY_COUNT(mathInfallibleFunctions))) {
		return false;
	}

	if (!initModule(vm, "io", ioFunctions, ARRAY_COUNT(ioFunctions), NULL,
			0)) {
		return false;
	}

	if (!initModule(vm, "time", timeFunctions, ARRAY_COUNT(timeFunctions),
			timeInfallibleFunctions,
			ARRAY_COUNT(timeInfallibleFunctions))) {
		return false;
	}

	if (!initModule(vm, "random", NULL, 0, randomInfallibleFunctions,
			ARRAY_COUNT(randomInfallibleFunctions))) {
		return false;
	}

	if (!initModule(vm, "sys", systemFunctions,
			ARRAY_COUNT(systemFunctions), systemInfallibleFunctions,
			ARRAY_COUNT(systemInfallibleFunctions))) {
		return false;
	}

	if (!initModule(vm, "fs", fileSystemFunctions,
			ARRAY_COUNT(fileSystemFunctions), NULL, 0)) {
		return false;
	}

	if (!initModule(vm, "vectors", vectorFunctions,
			ARRAY_COUNT(vectorFunctions), NULL, 0)) {
		return false;
	}

	return true;
}
