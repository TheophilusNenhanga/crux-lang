#include <string.h>

#include "garbage_collector.h"
#include "object.h"
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
#include "stdlib/set.h"
#include "stdlib/stdlib.h"
#include "stdlib/string.h"
#include "stdlib/sys.h"
#include "stdlib/tables.h"
#include "stdlib/time.h"
#include "stdlib/tuple.h"
#include "stdlib/vectors.h"
#include "type_system.h"
#include "value.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SA vm

static ObjectTypeRecord **make_args(VM *vm, ObjectTypeRecord **src, int count)
{
	if (count == 0)
		return NULL;
	ObjectTypeRecord **dst = ALLOCATE(vm, ObjectTypeRecord *, count);
	memcpy(dst, src, sizeof(ObjectTypeRecord *) * count);
	return dst;
}

static ObjectString **make_names(VM *vm, ObjectString **src, int count)
{
	if (count == 0)
		return NULL;
	ObjectString **dst = ALLOCATE(vm, ObjectString *, count);
	memcpy(dst, src, sizeof(ObjectString *) * count);
	return dst;
}

#define NAMES(...)                                                                                                     \
	make_names(vm, (ObjectString *[]){__VA_ARGS__},                                                                    \
			   (int)(sizeof((ObjectString *[]){__VA_ARGS__}) / sizeof(ObjectString *)))

#define ARGS(...)                                                                                                      \
	make_args(vm, (ObjectTypeRecord *[]){__VA_ARGS__},                                                                 \
			  (int)(sizeof((ObjectTypeRecord *[]){__VA_ARGS__}) / sizeof(ObjectTypeRecord *)))

#define ARGS0 NULL

#define name_int copy_string(SA, "Int", sizeof("Int"))
#define name_float copy_string(SA, "Float", sizeof("Float"))
#define name_string copy_string(SA, "String", sizeof("String"))
#define name_nil copy_string(SA, "Nil", sizeof("Nil"))
#define name_bool copy_string(SA, "Bool", sizeof("Bool"))

#define REC(t) new_type_rec(SA, (t))
#define ARR(elem) new_array_type_rec(SA, (elem))
#define TUP_ANY new_tuple_type_rec(SA, ARGS(t_any), -1)
#define SET_ANY new_set_type_rec(SA, t_any)
#define TBL(k, v) new_table_type_rec(SA, (k), (v))
#define RES(ok) new_result_type_rec(SA, (ok))
#define SET_OF(elem) new_set_type_rec(SA, (elem))
#define TABLE_OF(key, value) new_table_type_rec(SA, (key), (value))

#define VEC(dim) new_vector_type_rec(SA, (dim))
#define MAT(row, col) new_matrix_type_rec(SA, (row), (col))
#define UNI(args, names, count) new_union_type_rec(SA, (args), (names), (count))
#define FUNC(args, count, return_type) new_function_type_rec(SA, (args), (count), (return_type))

#define t_nil REC(NIL_TYPE)
#define t_bool REC(BOOL_TYPE)
#define t_int REC(INT_TYPE)
#define t_flt REC(FLOAT_TYPE)
#define t_str REC(STRING_TYPE)
#define t_any REC(ANY_TYPE)
#define t_err REC(ERROR_TYPE)
#define t_rnd REC(RANDOM_TYPE)
#define t_file REC(FILE_TYPE)
#define t_cmpl REC(COMPLEX_TYPE)
#define t_rang REC(RANGE_TYPE)
#define t_buf REC(BUFFER_TYPE)
#define t_never REC(NEVER_TYPE)

#define hashable                                                                                                       \
	UNI(ARGS(t_nil, t_int, t_flt, t_bool, t_str), NAMES(name_nil, name_int, name_float, name_bool, name_string), 5)
#define numeric UNI(ARGS(t_int, t_flt), NAMES(name_int, name_float), 2)

// Compound types
#define res_nil RES(t_nil)
#define res_any RES(t_any)
#define res_int RES(t_int)
#define res_str RES(t_str)
#define res_flt RES(t_flt)
#define res_bool RES(t_bool)

#define arr_str ARR(t_str)
#define arr_any ARR(t_any)
#define arr_int ARR(t_int)
#define arr_flt ARR(t_flt)
#define arr_num ARR(numeric)

#define set_any SET_OF(t_any)
#define vec_any VEC(-1)
#define mat_any MAT(-1, -1)
#define tbl_any TABLE_OF(t_any, t_any)

bool register_native_method(VM *vm, Table *method_table, const char *method_name, const CruxCallable method_function,
							const int arity, ObjectTypeRecord **arg_types, ObjectTypeRecord *return_type)
{
	ObjectString *name = copy_string(vm, method_name, (int)strlen(method_name));
	table_set(vm, method_table, name,
			  OBJECT_VAL(new_native_callable(vm, method_function, arity, name, arg_types, return_type)));
	return true;
}

static bool register_native_function(VM *vm, Table *function_table, const char *function_name,
									 const CruxCallable function, const int arity, ObjectTypeRecord **arg_types,
									 ObjectTypeRecord *return_type)
{
	ObjectModuleRecord *mod = vm->current_module_record;
	ObjectString *name = copy_string(vm, function_name, (int)strlen(function_name));
	push(mod, OBJECT_VAL(name));
	const Value func = OBJECT_VAL(new_native_callable(vm, function, arity, name, arg_types, return_type));
	push(mod, func);
	const bool ok = table_set(vm, function_table, name, func);
	pop(mod);
	pop(mod);
	return ok;
}

static bool register_native_methods(VM *vm, Table *method_table, const Callable *methods, int count)
{
	for (int i = 0; i < count; i++) {
		if (!register_native_method(vm, method_table, methods[i].name, methods[i].function, methods[i].arity,
									methods[i].arg_types, methods[i].return_type))
			return false;
	}
	return true;
}

static bool register_native_functions(VM *vm, Table *function_table, const Callable *functions, int count)
{
	for (int i = 0; i < count; i++) {
		if (!register_native_function(vm, function_table, functions[i].name, functions[i].function, functions[i].arity,
									  functions[i].arg_types, functions[i].return_type))
			return false;
	}
	return true;
}

static bool init_module(VM *vm, const char *module_name, const Callable *functions, int count)
{
	Table *module_table = ALLOCATE(vm, Table, 1);
	if (!module_table)
		return false;
	init_table(module_table);

	if (functions && !register_native_functions(vm, module_table, functions, count))
		return false;

	if (vm->native_modules.count + 1 > vm->native_modules.capacity) {
		const int new_cap = vm->native_modules.capacity == 0 ? 8 : vm->native_modules.capacity * 2;
		vm->native_modules.modules = GROW_ARRAY(vm, NativeModule, vm->native_modules.modules,
												vm->native_modules.capacity, new_cap);
		vm->native_modules.capacity = new_cap;
	}

	ObjectString *name_copy = copy_string(vm, module_name, (int)strlen(module_name));
	vm->native_modules.modules[vm->native_modules.count++] = (NativeModule){.name = name_copy, .names = module_table};
	return true;
}

static bool init_type_method_table(VM *vm, Table *method_table, const Callable *methods, int count)
{
	return methods ? register_native_methods(vm, method_table, methods, count) : true;
}

bool initialize_std_lib(VM *vm)
{
	// core functions
	{
		const Callable fns[] = {{"len", length_function, 1, ARGS(t_any), t_int},
								{"error", error_function, 1, ARGS(t_any), t_err},
								{"assert", assert_function, 2, ARGS(t_bool, t_str), t_nil},
								{"err", err_function, 1, ARGS(t_str), RES(t_any)},
								{"ok", ok_function, 1, ARGS(t_any), RES(t_any)},
								{"int", int_function, 1, ARGS(t_any), RES(t_int)},
								{"float", float_function, 1, ARGS(t_any), RES(t_flt)},
								{"string", string_function, 1, ARGS(t_any), t_str},
								{"table", table_function, 1, ARGS(t_any), tbl_any},
								{"array", array_function, 1, ARGS(t_any), RES(arr_any)},
								{"format", format_function, 2, ARGS(t_str, TBL(t_str, t_any)), res_nil},
								{"println", io_println_function, 1, ARGS(t_any), t_nil}};

		if (!register_native_functions(vm, &vm->core_fns, fns, ARRAY_COUNT(fns)))
			return false;

		for (int i = 0; i < vm->core_fns.capacity; i++) {
			if (vm->core_fns.entries[i].key != NULL) {
				table_set(vm, &vm->current_module_record->globals, vm->core_fns.entries[i].key,
						  vm->core_fns.entries[i].value);
			}
		}
	}

	// string methods
	{
		Callable methods[] = {
			{"first", string_first_method, 1, ARGS(t_str), t_str},
			{"last", string_last_method, 1, ARGS(t_str), t_str},
			{"get", string_get_method, 2, ARGS(t_str, t_int), res_str},
			{"upper", string_upper_method, 1, ARGS(t_str), res_str},
			{"lower", string_lower_method, 1, ARGS(t_str), res_str},
			{"strip", string_strip_method, 1, ARGS(t_str), t_str},
			{"starts_with", string_starts_with_method, 2, ARGS(t_str, t_str), res_bool},
			{"ends_with", string_ends_with_method, 2, ARGS(t_str, t_str), t_bool},
			{"contains", string_contains_method, 2, ARGS(t_str, t_str), t_bool},
			{"replace", string_replace_method, 3, ARGS(t_str, t_str, t_str), res_str},
			{"split", string_split_method, 2, ARGS(t_str, t_str), RES(ARR(t_str))},
			{"substring", string_substring_method, 3, ARGS(t_str, t_int, t_int), res_str},
			{"concat", string_concat_method, 2, ARGS(t_str, t_str), res_str},
			{"is_empty", string_is_empty_method, 1, ARGS(t_str), t_bool},
			{"is_alpha", string_is_alpha_method, 1, ARGS(t_str), t_bool},
			{"is_digit", string_is_digit_method, 1, ARGS(t_str), t_bool},
			{"is_lower", string_is_lower_method, 1, ARGS(t_str), t_bool},
			{"is_upper", string_is_upper_method, 1, ARGS(t_str), t_bool},
			{"is_space", string_is_space_method, 1, ARGS(t_str), t_bool},
			{"is_alnum", string_is_al_num_method, 1, ARGS(t_str), t_bool},
		};
		init_type_method_table(vm, &vm->string_type, methods, ARRAY_COUNT(methods));
	}

	// array methods
	{
		const Callable methods[] = {
			{"push", array_push_method, 2, ARGS(arr_any, t_any), res_nil},
			{"pop", array_pop_method, 1, ARGS(arr_any), res_any},
			{"insert", array_insert_method, 3, ARGS(arr_any, t_int, t_any), res_nil},
			{"remove", array_remove_at_method, 2, ARGS(arr_any, t_int), res_any},
			{"concat", array_concat_method, 2, ARGS(arr_any, arr_any), RES(arr_any)},
			{"slice", array_slice_method, 3, ARGS(arr_any, t_int, t_int), RES(arr_any)},
			{"reverse", array_reverse_method, 1, ARGS(arr_any), res_nil},
			{"index", array_index_of_method, 2, ARGS(arr_any, t_any), res_int},
			{"map", array_map_method, 2, ARGS(arr_any, FUNC(ARGS(t_any), 1, t_any)), RES(arr_any)},
			{"filter", array_filter_method, 2, ARGS(arr_any, FUNC(ARGS(t_any), 1, t_any)), RES(arr_any)},
			{"reduce", array_reduce_method, 3, ARGS(arr_any, FUNC(ARGS(t_any, t_any), 2, t_any), t_any), res_any},
			{"sort", array_sort_method, 1, ARGS(arr_any), RES(arr_any)},
			{"join", array_join_method, 2, ARGS(arr_any, t_str), res_str},
			{"contains", array_contains_method, 2, ARGS(arr_any, t_any), t_bool},
			{"clear", array_clear_method, 1, ARGS(arr_any), t_nil},
			{"equals", arrayEqualsMethod, 2, ARGS(arr_any, arr_any), t_bool},
		};
		init_type_method_table(vm, &vm->array_type, methods, ARRAY_COUNT(methods));
	}

	// Table methods
	{
		const Callable methods[] = {
			{"values", table_values_method, 1, ARGS(tbl_any), arr_any},
			{"keys", table_keys_method, 1, ARGS(tbl_any), arr_any},
			{"pairs", table_pairs_method, 1, ARGS(tbl_any), arr_any},
			{"remove", table_remove_method, 2, ARGS(tbl_any, hashable), res_any},
			{"get", table_get_method, 2, ARGS(tbl_any, hashable), res_any},
			{"has_key", table_has_key_method, 2, ARGS(tbl_any, hashable), t_bool},
			{"get_or_else", table_get_or_else_method, 3, ARGS(tbl_any, hashable, t_any), t_any},
		};
		init_type_method_table(vm, &vm->table_type, methods, ARRAY_COUNT(methods));
	}

	// Error methods
	{
		const Callable methods[] = {
			{"type", error_type_method, 1, ARGS(t_err), t_str},
			{"message", error_message_method, 1, ARGS(t_err), t_str},
		};
		init_type_method_table(vm, &vm->error_type, methods, ARRAY_COUNT(methods));
	}

	// Result methods
	{
		const Callable methods[] = {
			{"unwrap", unwrap_function, 1, ARGS(res_any), t_any},
		};
		init_type_method_table(vm, &vm->result_type, methods, ARRAY_COUNT(methods));
	}

	// File methods
	{
		const Callable methods[] = {
			{"close", fs_close_method, 1, ARGS(t_file), res_nil},
			{"flush", fs_flush_method, 1, ARGS(t_file), res_nil},
			{"read", fs_read_method, 2, ARGS(t_file, t_int), res_str},
			{"readln", fs_readln_method, 1, ARGS(t_file), res_str},
			{"read_all", fs_read_all_method, 1, ARGS(t_file), res_str},
			{"read_lines", fs_read_lines_method, 1, ARGS(t_file), RES(arr_str)},
			{"write", fs_write_method, 2, ARGS(t_file, t_str), res_nil},
			{"writeln", fs_writeln_method, 2, ARGS(t_file, t_str), res_nil},
			{"seek", fs_seek_method, 3, ARGS(t_file, t_int, t_str), res_nil},
			{"tell", fs_tell_method, 1, ARGS(t_file), res_int},
			{"is_open", fs_is_open_method, 1, ARGS(t_file), t_bool},
		};
		init_type_method_table(vm, &vm->file_type, methods, ARRAY_COUNT(methods));
	}

	// Random methods  +  module constructor
	{
		const Callable methods[] = {
			{"seed", random_seed_method, 2, ARGS(t_rnd, t_int), t_nil},
			{"int", random_int_method, 3, ARGS(t_rnd, t_int, t_int), RES(t_int)},
			{"float", random_float_method, 3, ARGS(t_rnd, numeric, numeric), RES(t_flt)},
			{"bool", random_bool_method, 2, ARGS(t_rnd, numeric), RES(t_bool)},
			{"choice", random_choice_method, 2, ARGS(t_rnd, arr_any), RES(res_any)},
			{"next", random_next_method, 1, ARGS(t_rnd), t_flt},
		};
		init_type_method_table(vm, &vm->random_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Random", random_init_function, 0, ARGS0, t_rnd},
		};
		if (!init_module(vm, "random", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Vector methods  +  module constructor
	{
		const Callable methods[] = {
			{"dot", vector_dot_method, 2, ARGS(vec_any, vec_any), RES(t_flt)},
			{"add", vector_add_method, 2, ARGS(vec_any, vec_any), RES(vec_any)},
			{"subtract", vector_subtract_method, 2, ARGS(vec_any, vec_any), RES(vec_any)},
			{"multiply", vector_multiply_method, 2, ARGS(vec_any, numeric), RES(vec_any)},
			{"divide", vector_divide_method, 2, ARGS(vec_any, numeric), RES(vec_any)},
			{"magnitude", vector_magnitude_method, 1, ARGS(vec_any), t_flt},
			{"normalize", vector_normalize_method, 1, ARGS(vec_any), RES(vec_any)},
			{"distance", vector_distance_method, 2, ARGS(vec_any, vec_any), RES(t_flt)},
			{"angle_between", vector_angle_between_method, 2, ARGS(vec_any, vec_any), RES(t_flt)},
			{"cross", vector_cross_method, 2, ARGS(vec_any, vec_any), RES(vec_any)},
			{"lerp", vector_lerp_method, 3, ARGS(vec_any, vec_any, numeric), RES(vec_any)},
			{"reflect", vector_reflect_method, 2, ARGS(vec_any, vec_any), RES(vec_any)},
			{"equals", vector_equals_method, 2, ARGS(vec_any, vec_any), t_bool},
			{"x", vector_x_method, 1, ARGS(vec_any), t_flt},
			{"y", vector_y_method, 1, ARGS(vec_any), t_flt},
			{"z", vector_z_method, 1, ARGS(vec_any), t_flt},
			{"w", vector_w_method, 1, ARGS(vec_any), t_flt},
			{"dimension", vector_dimension_method, 1, ARGS(vec_any), t_int},
		};
		init_type_method_table(vm, &vm->vector_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Vec", new_vector_function, 2, ARGS(t_int, arr_num), RES(vec_any)},
		};
		if (!init_module(vm, "vector", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Complex methods  +  module constructor
	{
		const Callable methods[] = {
			{"add", add_complex_number_method, 2, ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"sub", sub_complex_number_method, 2, ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"mul", mul_complex_number_method, 2, ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"div", div_complex_number_method, 2, ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"scale", scale_complex_number_method, 2, ARGS(t_cmpl, numeric), t_cmpl},
			{"real", complex_real_method, 1, ARGS(t_cmpl), t_flt},
			{"imag", complex_imag_method, 1, ARGS(t_cmpl), t_flt},
			{"conjugate", conjugate_complex_number_method, 1, ARGS(t_cmpl), t_cmpl},
			{"mag", magnitude_complex_number_method, 1, ARGS(t_cmpl), t_flt},
			{"square_mag", square_magnitude_complex_number_method, 1, ARGS(t_cmpl), t_flt},
		};
		init_type_method_table(vm, &vm->complex_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Complex", new_complex_function, 2, ARGS(numeric, numeric), t_cmpl},
		};
		if (!init_module(vm, "complex", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Matrix methods  +  module constructors
	{
		const Callable methods[] = {
			{"get", matrix_get_method, 3, ARGS(mat_any, t_int, t_int), RES(t_flt)},
			{"set", matrix_set_method, 4, ARGS(mat_any, t_int, t_int, numeric), RES(t_nil)},
			{"add", matrix_add_method, 2, ARGS(mat_any, mat_any), RES(mat_any)},
			{"sub", matrix_subtract_method, 2, ARGS(mat_any, mat_any), RES(mat_any)},
			{"mul", matrix_multiply_method, 2, ARGS(mat_any, mat_any), RES(mat_any)},
			{"scale", matrix_scale_method, 2, ARGS(mat_any, numeric), mat_any},
			{"transpose", matrix_transpose_method, 1, ARGS(mat_any), mat_any},
			{"determinant", matrix_determinant_method, 1, ARGS(mat_any), RES(t_flt)},
			{"inverse", matrix_inverse_method, 1, ARGS(mat_any), RES(mat_any)},
			{"trace", matrix_trace_method, 1, ARGS(mat_any), RES(t_flt)},
			{"rank", matrix_rank_method, 1, ARGS(mat_any), RES(t_int)},
			{"row", matrix_row_method, 2, ARGS(mat_any, t_int), RES(vec_any)},
			{"col", matrix_col_method, 2, ARGS(mat_any, t_int), RES(vec_any)},
			{"equals", matrix_equals_method, 2, ARGS(mat_any, mat_any), t_bool},
			{"copy", matrix_copy_method, 1, ARGS(mat_any), mat_any},
			{"to_array", matrix_to_array_method, 1, ARGS(mat_any), RES(arr_any)},
			{"mul_vec", matrix_multiply_vector_method, 2, ARGS(mat_any, vec_any), RES(vec_any)},
			{"rows", matrix_rows_method, 1, ARGS(mat_any), t_int},
			{"cols", matrix_cols_method, 1, ARGS(mat_any), t_int},
		};
		init_type_method_table(vm, &vm->matrix_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Matrix", new_matrix_function, 2, ARGS(t_int, t_int), RES(mat_any)},
			{"IMatrix", new_matrix_identity_function, 1, ARGS(t_int), RES(mat_any)},
			{"AMatrix", new_matrix_from_array_function, 3, ARGS(t_int, t_int, arr_num), RES(mat_any)},
		};
		if (!init_module(vm, "matrix", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Range methods  +  module constructor
	{
		const Callable methods[] = {
			{"contains", contains_range_method, 2, ARGS(t_rang, t_int), t_bool},
			{"to_array", to_array_range_method, 1, ARGS(t_rang), arr_any},
			{"start", start_range_method, 1, ARGS(t_rang), t_int},
			{"end", end_range_method, 1, ARGS(t_rang), t_int},
			{"step", step_range_method, 1, ARGS(t_rang), t_int},
			{"is_empty", is_empty_range_method, 1, ARGS(t_rang), t_bool},
			{"reversed", reversed_range_method, 1, ARGS(t_rang), t_rang},
		};
		init_type_method_table(vm, &vm->range_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Range", new_range_function, 3, ARGS(t_int, t_int, t_int), RES(t_rang)},
		};
		if (!init_module(vm, "range", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Tuple methods  +  module constructor
	{
		const Callable methods[] = {
			{"get", get_tuple_method, 2, ARGS(TUP_ANY, t_int), res_any},
			{"slice", slice_tuple_method, 3, ARGS(TUP_ANY, t_int, t_int), TUP_ANY},
			{"index", index_tuple_method, 2, ARGS(TUP_ANY, t_any), res_int},
			{"is_empty", is_empty_tuple_method, 1, ARGS(TUP_ANY), t_bool},
			{"to_array", to_array_tuple_method, 1, ARGS(TUP_ANY), arr_any},
			{"first", first_tuple_method, 1, ARGS(TUP_ANY), res_any},
			{"last", last_tuple_method, 1, ARGS(TUP_ANY), res_any},
			{"contains", contains_tuple_method, 2, ARGS(TUP_ANY, t_any), t_bool},
			{"equals", equals_tuple_method, 2, ARGS(TUP_ANY, TUP_ANY), t_bool},
		};
		init_type_method_table(vm, &vm->tuple_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Tuple", new_tuple_function, 1, ARGS(arr_any), TUP_ANY},
		};
		if (!init_module(vm, "tuple", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Set methods  +  module constructor
	{
		const Callable methods[] = {
			{"add", add_set_method, 2, ARGS(set_any, hashable), res_nil},
			{"remove", remove_set_method, 2, ARGS(set_any, hashable), res_nil},
			{"discard", discard_set_method, 2, ARGS(set_any, hashable), t_nil},
			{"union", union_set_method, 2, ARGS(set_any, set_any), set_any},
			{"intersection", intersection_set_method, 2, ARGS(set_any, set_any), set_any},
			{"difference", difference_set_method, 2, ARGS(set_any, set_any), set_any},
			{"sym_difference", sym_difference_set_method, 2, ARGS(set_any, set_any), set_any},
			{"is_subset", is_subset_set_method, 2, ARGS(set_any, set_any), t_bool},
			{"is_superset", is_superset_set_method, 2, ARGS(set_any, set_any), t_bool},
			{"is_disjoint", is_disjoint_set_method, 2, ARGS(set_any, set_any), t_bool},
			{"contains", contains_set_method, 2, ARGS(set_any, hashable), t_bool},
			{"is_empty", is_empty_set_method, 1, ARGS(set_any), t_bool},
			{"to_array", to_array_set_method, 1, ARGS(set_any), arr_any},
			{"clone", clone_set_method, 1, ARGS(set_any), set_any},
		};
		init_type_method_table(vm, &vm->set_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Set", new_set_function, 1, ARGS(arr_any), SET_ANY},
		};
		if (!init_module(vm, "set", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Buffer methods  +  module constructor
	{
		const Callable methods[] = {
			{"write_byte", write_byte_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"write_int16_le", write_int16_le_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"write_int32_le", write_int32_le_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"write_float32_le", write_float32_le_buffer_method, 2, ARGS(t_buf, t_flt), res_nil},
			{"write_float64_le", write_float64_le_buffer_method, 2, ARGS(t_buf, t_flt), res_nil},
			{"write_int16_be", write_int16_be_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"write_int32_be", write_int32_be_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"write_float32_be", write_float32_be_buffer_method, 2, ARGS(t_buf, t_flt), res_nil},
			{"write_float64_be", write_float64_be_buffer_method, 2, ARGS(t_buf, t_flt), res_nil},
			{"write_string", write_string_buffer_method, 2, ARGS(t_buf, t_str), res_nil},
			{"write_buffer", write_buffer_buffer_method, 2, ARGS(t_buf, t_buf), res_nil},
			{"read_byte", read_byte_buffer_method, 1, ARGS(t_buf), res_int},
			{"read_string", read_string_buffer_method, 2, ARGS(t_buf, t_int), res_str},
			{"read_line", read_line_buffer_method, 1, ARGS(t_buf), res_str},
			{"read_all", read_all_buffer_method, 1, ARGS(t_buf), res_str},
			{"read_int16_le", read_int16_le_buffer_method, 1, ARGS(t_buf), res_int},
			{"read_int32_le", read_int32_le_buffer_method, 1, ARGS(t_buf), res_int},
			{"read_float32_le", read_float32_le_buffer_method, 1, ARGS(t_buf), res_flt},
			{"read_float64_le", read_float64_le_buffer_method, 1, ARGS(t_buf), res_flt},
			{"read_int16_be", read_int16_be_buffer_method, 1, ARGS(t_buf), res_int},
			{"read_int32_be", read_int32_be_buffer_method, 1, ARGS(t_buf), res_int},
			{"read_float32_be", read_float32_be_buffer_method, 1, ARGS(t_buf), res_flt},
			{"read_float64_be", read_float64_be_buffer_method, 1, ARGS(t_buf), res_flt},
			{"capacity", capacity_buffer_method, 1, ARGS(t_buf), t_int},
			{"is_empty", is_empty_buffer_method, 1, ARGS(t_buf), t_bool},
			{"clear", clear_buffer_method, 1, ARGS(t_buf), t_nil},
			{"peek_byte", peek_byte_buffer_method, 1, ARGS(t_buf), res_int},
			{"skip_bytes", skip_bytes_buffer_method, 2, ARGS(t_buf, t_int), res_nil},
			{"clone", clone_buffer_method, 1, ARGS(t_buf), t_buf},
			{"compact", compact_buffer_method, 1, ARGS(t_buf), t_buf},
		};
		init_type_method_table(vm, &vm->buffer_type, methods, ARRAY_COUNT(methods));

		const Callable fns[] = {
			{"Buffer", new_buffer_function, 0, ARGS0, t_buf},
		};
		if (!init_module(vm, "buffer", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Math module
	{
		const Callable fns[] = {
			{"pow", pow_function, 2, ARGS(numeric, numeric), t_flt},
			{"sqrt", sqrt_function, 1, ARGS(numeric), RES(t_flt)},
			{"ceil", ceil_function, 1, ARGS(numeric), t_int},
			{"floor", floor_function, 1, ARGS(numeric), t_int},
			{"abs", abs_function, 1, ARGS(numeric), numeric},
			{"sin", sin_function, 1, ARGS(numeric), t_flt},
			{"cos", cos_function, 1, ARGS(numeric), t_flt},
			{"tan", tan_function, 1, ARGS(numeric), t_flt},
			{"atan", atan_function, 1, ARGS(numeric), t_flt},
			{"acos", acos_function, 1, ARGS(numeric), t_flt},
			{"asin", asin_function, 1, ARGS(numeric), t_flt},
			{"exp", exp_function, 1, ARGS(numeric), t_flt},
			{"ln", ln_function, 1, ARGS(numeric), t_flt},
			{"log", log10_function, 1, ARGS(numeric), t_flt},
			{"round", round_function, 1, ARGS(numeric), t_int},
			{"min", min_function, 2, ARGS(numeric, numeric), numeric},
			{"max", max_function, 2, ARGS(numeric, numeric), numeric},
			{"e", e_function, 0, ARGS0, t_flt},
			{"pi", pi_function, 0, ARGS0, t_flt},
			{"nan", nan_function, 0, ARGS0, t_flt},
			{"inf", inf_function, 0, ARGS0, t_flt},
		};
		if (!init_module(vm, "math", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// IO module
	{
		const Callable fns[] = {
			{"print", io_print_function, 1, ARGS(t_any), t_nil},
			{"print_to", io_print_to_function, 2, ARGS(t_str, t_any), t_nil},
			{"println_to", io_println_to_function, 2, ARGS(t_str, t_any), t_nil},
			{"scan", io_scan_function, 0, ARGS0, res_str},
			{"scanln", io_scanln_function, 0, ARGS0, res_str},
			{"nscan", io_nscan_function, 1, ARGS(t_int), res_str},
			{"scan_from", io_scan_from_function, 1, ARGS(t_str), res_str},
			{"scanln_from", io_scanln_from_function, 1, ARGS(t_str), res_str},
			{"nscan_from", io_nscan_from_function, 2, ARGS(t_str, t_int), res_str},
		};
		if (!init_module(vm, "io", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Time module
	{
		const Callable fns[] = {
			{"sleep_s", sleep_seconds_function, 1, ARGS(numeric), t_nil},
			{"sleep_ms", sleep_milliseconds_function, 1, ARGS(numeric), t_nil},
			{"time_s", time_seconds_function_, 0, ARGS0, t_flt},
			{"time_ms", time_milliseconds_function_, 0, ARGS0, t_flt},
			{"year", year_function_, 0, ARGS0, t_int},
			{"month", month_function_, 0, ARGS0, t_int},
			{"day", day_function_, 0, ARGS0, t_int},
			{"hour", hour_function_, 0, ARGS0, t_int},
			{"minute", minute_function_, 0, ARGS0, t_int},
			{"second", second_function_, 0, ARGS0, t_int},
			{"weekday", weekday_function_, 0, ARGS0, t_int},
			{"day_of_year", day_of_year_function_, 0, ARGS0, t_int},
		};
		if (!init_module(vm, "time", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// System module
	{
		const Callable fns[] = {
			{"args", args_function, 0, ARGS0, arr_str},		  {"get_env", get_env_function, 1, ARGS(t_str), res_str},
			{"sleep", sleep_function, 1, ARGS(t_int), t_nil}, {"platform", platform_function, 0, ARGS0, t_str},
			{"arch", arch_function, 0, ARGS0, t_str},		  {"pid", pid_function, 0, ARGS0, t_int},
			{"exit", exit_function, 1, ARGS(t_int), t_never},
		};
		if (!init_module(vm, "sys", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// Filesystem module
	{
		ObjectTypeRecord *res_file = RES(t_file);

		const Callable fns[] = {
			{"open", fs_open_function, 2, ARGS(t_str, t_str), res_file},
			{"remove", fs_remove_function, 1, ARGS(t_str), res_nil},
			{"size", fs_file_size_function, 1, ARGS(t_str), res_int},
			{"copy_file", fs_copy_file_function, 2, ARGS(t_str, t_str), res_nil},
			{"mkdir", fs_mkdir_function, 1, ARGS(t_str), res_nil},
			{"read_file", fs_read_file_function, 1, ARGS(t_str), res_str},
			{"write_file", fs_write_file_function, 2, ARGS(t_str, t_str), res_nil},
			{"append_file", fs_append_file_function, 2, ARGS(t_str, t_str), res_nil},
			{"exists", fs_exists_function, 1, ARGS(t_str), t_bool},
			{"is_file", fs_is_file_function, 1, ARGS(t_str), t_bool},
			{"is_dir", fs_is_dir_function, 1, ARGS(t_str), t_bool},
		};
		if (!init_module(vm, "fs", fns, ARRAY_COUNT(fns)))
			return false;
	}

	return true;
}
