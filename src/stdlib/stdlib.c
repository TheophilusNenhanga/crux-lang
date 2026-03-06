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

#define SA (vm->type_arena)

#define REC(t) new_type_rec(SA, (t))
#define ARR(elem) new_array_type_rec(SA, (elem))
#define TUP(elem) new_tuple_type_rec(SA, (elem))
#define TBL(k, v) new_table_type_rec(SA, (k), (v))
#define RES(ok) new_result_type_rec(SA, (ok))
#define SET_OF(elem) new_set_type_rec(SA, (elem))

// builds a heap allocated type record array.
// The array's lifetime is owned by ObjectNativeCallable
static TypeRecord **make_args(VM *vm, TypeRecord **src, int count)
{
	if (count == 0)
		return NULL;
	TypeRecord **dst = ALLOCATE(vm, TypeRecord *, count);
	memcpy(dst, src, sizeof(TypeRecord *) * count);
	return dst;
}

#define ARGS(...)                                                              \
	make_args(vm, (TypeRecord *[]){__VA_ARGS__},                           \
		  (int)(sizeof((TypeRecord *[]){__VA_ARGS__}) /                \
			sizeof(TypeRecord *)))

#define ARGS0 NULL

bool register_native_method(VM *vm, Table *method_table,
			    const char *method_name,
			    const CruxCallable method_function, const int arity,
			    TypeRecord **arg_types, TypeRecord *return_type)
{
	ObjectString *name = copy_string(vm, method_name,
					 (int)strlen(method_name));
	table_set(vm, method_table, name,
		  OBJECT_VAL(new_native_callable(vm, method_function, arity,
						 name, arg_types,
						 return_type)));
	return true;
}

static bool register_native_function(VM *vm, Table *function_table,
				     const char *function_name,
				     const CruxCallable function,
				     const int arity, TypeRecord **arg_types,
				     TypeRecord *return_type)
{
	ObjectModuleRecord *mod = vm->current_module_record;
	ObjectString *name = copy_string(vm, function_name,
					 (int)strlen(function_name));
	push(mod, OBJECT_VAL(name));
	const Value func = OBJECT_VAL(new_native_callable(vm, function, arity,
							  name, arg_types,
							  return_type));
	push(mod, func);
	const bool ok = table_set(vm, function_table, name, func);
	pop(mod);
	pop(mod);
	return ok;
}

static bool register_native_methods(VM *vm, Table *method_table,
				    const Callable *methods, int count)
{
	for (int i = 0; i < count; i++) {
		if (!register_native_method(vm, method_table, methods[i].name,
					    methods[i].function,
					    methods[i].arity,
					    methods[i].arg_types,
					    methods[i].return_type))
			return false;
	}
	return true;
}

static bool register_native_functions(VM *vm, Table *function_table,
				      const Callable *functions, int count)
{
	for (int i = 0; i < count; i++) {
		if (!register_native_function(
			    vm, function_table, functions[i].name,
			    functions[i].function, functions[i].arity,
			    functions[i].arg_types, functions[i].return_type))
			return false;
	}
	return true;
}

static bool init_module(VM *vm, const char *module_name,
			const Callable *functions, int count)
{
	Table *module_table = ALLOCATE(vm, Table, 1);
	if (!module_table)
		return false;
	init_table(module_table);

	if (functions &&
	    !register_native_functions(vm, module_table, functions, count))
		return false;

	if (vm->native_modules.count + 1 > vm->native_modules.capacity) {
		const int new_cap = vm->native_modules.capacity == 0
					    ? 8
					    : vm->native_modules.capacity * 2;
		vm->native_modules.modules =
			GROW_ARRAY(vm, NativeModule, vm->native_modules.modules,
				   vm->native_modules.capacity, new_cap);
		vm->native_modules.capacity = new_cap;
	}

	ObjectString *name_copy = copy_string(vm, module_name,
					      (int)strlen(module_name));
	vm->native_modules.modules[vm->native_modules.count++] =
		(NativeModule){.name = name_copy, .names = module_table};
	return true;
}

static bool init_type_method_table(VM *vm, Table *method_table,
				   const Callable *methods, int count)
{
	return methods ? register_native_methods(vm, method_table, methods,
						 count)
		       : true;
}

bool initialize_std_lib(VM *vm)
{
	TypeRecord *t_nil = REC(NIL_TYPE);
	TypeRecord *t_bool = REC(BOOL_TYPE);
	TypeRecord *t_int = REC(INT_TYPE);
	TypeRecord *t_flt = REC(FLOAT_TYPE);
	TypeRecord *t_str = REC(STRING_TYPE);
	TypeRecord *t_any = REC(ANY_TYPE);
	TypeRecord *t_num = REC(NUMERIC_TYPE);
	TypeRecord *t_hash = REC(HASHABLE_TYPE);
	TypeRecord *t_tbl = REC(TABLE_TYPE);
	TypeRecord *t_fn = REC(FUNCTION_TYPE);
	TypeRecord *t_err = REC(ERROR_TYPE);
	TypeRecord *t_res = REC(RESULT_TYPE);
	TypeRecord *t_rnd = REC(RANDOM_TYPE);
	TypeRecord *t_file = REC(FILE_TYPE);
	TypeRecord *t_vec = REC(VECTOR_TYPE);
	TypeRecord *t_mat = REC(MATRIX_TYPE);
	TypeRecord *t_cmpl = REC(COMPLEX_TYPE);
	TypeRecord *t_rang = REC(RANGE_TYPE);
	TypeRecord *t_tup = REC(TUPLE_TYPE);
	TypeRecord *t_buf = REC(BUFFER_TYPE);
	TypeRecord *t_set = REC(SET_TYPE);

	// Compound types used across multiple groups.
	TypeRecord *res_nil = RES(t_nil);
	TypeRecord *res_any = RES(t_any);
	TypeRecord *res_int = RES(t_int);
	TypeRecord *res_str = RES(t_str);
	TypeRecord *res_flt = RES(t_flt);
	TypeRecord *arr_str = ARR(t_str);
	TypeRecord *arr_any = ARR(t_any);
	TypeRecord* arr_int = ARR(t_int);
	TypeRecord* arr_flt = ARR(t_flt);
	TypeRecord* arr_num = ARR(t_num);

	TypeRecord* res_arr_any = RES(arr_any);

	// ---------------------------------------------------------------
	// Core globals  (always in scope, no import required)
	// ---------------------------------------------------------------
	{
		Callable fns[] = {
			{"len", length_function, 1, ARGS(t_any), t_int},
			{"error", error_function, 1, ARGS(t_any), t_err},
			{"assert", assert_function, 2, ARGS(t_bool, t_str),
			 t_nil},
			{"err", error_function, 1, ARGS(t_str), t_err},
			{"ok", ok_function, 1, ARGS(t_any), RES(t_any)},
			{"int", int_function, 1, ARGS(t_any), RES(t_int)},
			{"float", float_function, 1, ARGS(t_any), RES(t_int)},
			{"string", string_function, 1, ARGS(t_any), t_str},
			{"table", table_function, 1, ARGS(t_any), t_tbl},
			{"array", array_function, 1, ARGS(t_any), arr_any},
			{"format", format_function, 2,
			 ARGS(t_str, TBL(t_str, t_any)), t_str},
		};
		if (!register_native_functions(
			    vm, &vm->current_module_record->globals, fns,
			    ARRAY_COUNT(fns)))
			return false;
		if (!register_native_functions(
				vm, &vm->core_fns, fns,
				ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// String methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"first", string_first_method, 1, ARGS(t_str), t_str},
			{"last", string_last_method, 1, ARGS(t_str), t_str},
			{"get", string_get_method, 2, ARGS(t_str, t_int),
			 res_str},
			{"upper", string_upper_method, 1, ARGS(t_str), t_str},
			{"lower", string_lower_method, 1, ARGS(t_str), t_str},
			{"strip", string_strip_method, 1, ARGS(t_str), t_str},
			{"starts_with", string_starts_with_method, 2,
			 ARGS(t_str, t_str), t_bool},
			{"ends_with", string_ends_with_method, 2,
			 ARGS(t_str, t_str), t_bool},
			{"contains", string_contains_method, 2,
			 ARGS(t_str, t_str), t_bool},
			{"replace", string_replace_method, 3,
			 ARGS(t_str, t_str, t_str), t_str},
			{"split", string_split_method, 2, ARGS(t_str, t_str),
			 arr_str},
			{"substring", string_substring_method, 3,
			 ARGS(t_str, t_int, t_int), res_str},
			{"concat", string_concat_method, 2, ARGS(t_str, t_str),
			 t_str},
			{"is_empty", string_is_empty_method, 1, ARGS(t_str),
			 t_bool},
			{"is_alpha", string_is_alpha_method, 1, ARGS(t_str),
			 t_bool},
			{"is_digit", string_is_digit_method, 1, ARGS(t_str),
			 t_bool},
			{"is_lower", string_is_lower_method, 1, ARGS(t_str),
			 t_bool},
			{"is_upper", string_is_upper_method, 1, ARGS(t_str),
			 t_bool},
			{"is_space", string_is_space_method, 1, ARGS(t_str),
			 t_bool},
			{"is_alnum", string_is_al_num_method, 1, ARGS(t_str),
			 t_bool},
		};
		init_type_method_table(vm, &vm->string_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// Array methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"push", array_push_method, 2, ARGS(arr_any, t_any),
			 res_nil},
			{"pop", array_pop_method, 1, ARGS(arr_any), res_any},
			{"insert", array_insert_method, 3,
			 ARGS(arr_any, t_int, t_any), res_nil},
			{"remove", array_remove_at_method, 2,
			 ARGS(arr_any, t_int), res_any},
			{"concat", array_concat_method, 2, ARGS(arr_any, arr_any),
			 res_arr_any},
			{"slice", array_slice_method, 3,
			 ARGS(arr_any, t_int, t_int), res_arr_any},
			{"reverse", array_reverse_method, 1, ARGS(arr_any),
			 res_arr_any},
			{"index", array_index_of_method, 2, ARGS(arr_any, t_any),
			 res_int},
			{"map", array_map_method, 2, ARGS(arr_any, t_fn),
			 res_arr_any},
			{"filter", array_filter_method, 2, ARGS(arr_any, t_fn),
			 res_arr_any},
			{"reduce", array_reduce_method, 3,
			 ARGS(arr_any, t_fn, t_any), res_any},
			{"sort", array_sort_method, 1, ARGS(arr_any), res_arr_any},
			{"join", array_join_method, 2, ARGS(arr_any, t_str),
			 res_str},
			{"contains", array_contains_method, 2,
			 ARGS(arr_any, t_any), t_bool},
			{"clear", array_clear_method, 1, ARGS(arr_any), t_nil},
			{"equals", arrayEqualsMethod, 2, ARGS(arr_any, arr_any),
			 t_bool},
		};
		init_type_method_table(vm, &vm->array_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// Table methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"values", table_values_method, 1, ARGS(t_tbl),
			 arr_any},
			{"keys", table_keys_method, 1, ARGS(t_tbl), arr_any},
			{"pairs", table_pairs_method, 1, ARGS(t_tbl), arr_any},
			{"remove", table_remove_method, 2, ARGS(t_tbl, t_hash),
			 res_any},
			{"get", table_get_method, 2, ARGS(t_tbl, t_hash),
			 res_any},
			{"has_key", table_has_key_method, 2,
			 ARGS(t_tbl, t_hash), t_bool},
			{"get_or_else", table_get_or_else_method, 3,
			 ARGS(t_tbl, t_hash, t_any), t_any},
		};
		init_type_method_table(vm, &vm->table_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// Error methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"type", error_type_method, 1, ARGS(t_err), t_str},
			{"message", error_message_method, 1, ARGS(t_err),
			 t_str},
		};
		init_type_method_table(vm, &vm->error_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// Result methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"_unwrap", unwrap_function, 1, ARGS(t_res), t_any},
		};
		init_type_method_table(vm, &vm->result_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// File methods
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"close", fs_close_method, 1, ARGS(t_file), res_nil},
			{"flush", fs_flush_method, 1, ARGS(t_file), res_nil},
			{"read", fs_read_method, 2, ARGS(t_file, t_int),
			 res_str},
			{"readln", fs_readln_method, 1, ARGS(t_file), res_str},
			{"read_all", fs_read_all_method, 1, ARGS(t_file),
			 res_str},
			{"read_lines", fs_read_lines_method, 1, ARGS(t_file),
			 RES(arr_str)},
			{"write", fs_write_method, 2, ARGS(t_file, t_str),
			 res_nil},
			{"writeln", fs_writeln_method, 2, ARGS(t_file, t_str),
			 res_nil},
			{"seek", fs_seek_method, 3, ARGS(t_file, t_int, t_str),
			 res_nil},
			{"tell", fs_tell_method, 1, ARGS(t_file), res_int},
			{"is_open", fs_is_open_method, 1, ARGS(t_file), t_bool},
		};
		init_type_method_table(vm, &vm->file_type, methods,
				       ARRAY_COUNT(methods));
	}

	// ---------------------------------------------------------------
	// Random methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"seed", random_seed_method, 2, ARGS(t_rnd, t_int),
			 t_nil},
			{"int", random_int_method, 3, ARGS(t_rnd, t_int, t_int),
			 t_int},
			{"float", random_float_method, 3,
			 ARGS(t_rnd, t_num, t_num), t_flt},
			{"bool", random_bool_method, 2, ARGS(t_rnd, t_num),
			 t_bool},
			{"choice", random_choice_method, 2, ARGS(t_rnd, arr_any),
			 res_any},
			{"_next", random_next_method, 1, ARGS(t_rnd), t_int},
		};
		init_type_method_table(vm, &vm->random_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Random", random_init_function, 0, ARGS0, t_rnd},
		};
		if (!init_module(vm, "random", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Vector methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"dot", vector_dot_method, 2, ARGS(t_vec, t_vec),
			 t_flt},
			{"add", vector_add_method, 2, ARGS(t_vec, t_vec),
			 t_vec},
			{"subtract", vector_subtract_method, 2,
			 ARGS(t_vec, t_vec), t_vec},
			{"multiply", vector_multiply_method, 2,
			 ARGS(t_vec, t_num), t_vec},
			{"divide", vector_divide_method, 2, ARGS(t_vec, t_num),
			 t_vec},
			{"magnitude", vector_magnitude_method, 1, ARGS(t_vec),
			 t_flt},
			{"normalize", vector_normalize_method, 1, ARGS(t_vec),
			 t_vec},
			{"distance", vector_distance_method, 2,
			 ARGS(t_vec, t_vec), t_flt},
			{"angle_between", vector_angle_between_method, 2,
			 ARGS(t_vec, t_vec), t_flt},
			{"cross", vector_cross_method, 2, ARGS(t_vec, t_vec),
			 t_vec},
			{"lerp", vector_lerp_method, 3,
			 ARGS(t_vec, t_vec, t_num), t_vec},
			{"reflect", vector_reflect_method, 2,
			 ARGS(t_vec, t_vec), t_vec},
			{"equals", vector_equals_method, 2, ARGS(t_vec, t_vec),
			 t_bool},
			{"x", vector_x_method, 1, ARGS(t_vec), t_flt},
			{"y", vector_y_method, 1, ARGS(t_vec), t_flt},
			{"z", vector_z_method, 1, ARGS(t_vec), t_flt},
			{"w", vector_w_method, 1, ARGS(t_vec), t_flt},
			{"dimension", vector_dimension_method, 1, ARGS(t_vec),
			 t_int},
		};
		init_type_method_table(vm, &vm->vector_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Vec", new_vector_function, 2, ARGS(t_int, arr_num),
			 t_vec},
		};
		if (!init_module(vm, "vector", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Complex methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"add", add_complex_number_method, 2,
			 ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"sub", sub_complex_number_method, 2,
			 ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"mul", mul_complex_number_method, 2,
			 ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"div", div_complex_number_method, 2,
			 ARGS(t_cmpl, t_cmpl), t_cmpl},
			{"scale", scale_complex_number_method, 2,
			 ARGS(t_cmpl, t_num), t_cmpl},
			{"real", complex_real_method, 1, ARGS(t_cmpl), t_flt},
			{"imag", complex_imag_method, 1, ARGS(t_cmpl), t_flt},
			{"conjugate", conjugate_complex_number_method, 1,
			 ARGS(t_cmpl), t_cmpl},
			{"mag", magnitude_complex_number_method, 1,
			 ARGS(t_cmpl), t_flt},
			{"square_mag", square_magnitude_complex_number_method,
			 1, ARGS(t_cmpl), t_flt},
		};
		init_type_method_table(vm, &vm->complex_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Complex", new_complex_function, 2, ARGS(t_num, t_num),
			 t_cmpl},
		};
		if (!init_module(vm, "complex", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Matrix methods  +  module constructors
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"get", matrix_get_method, 3, ARGS(t_mat, t_int, t_int),
			 t_flt},
			{"set", matrix_set_method, 4,
			 ARGS(t_mat, t_int, t_int, t_num), t_nil},
			{"add", matrix_add_method, 2, ARGS(t_mat, t_mat),
			 t_mat},
			{"sub", matrix_subtract_method, 2, ARGS(t_mat, t_mat),
			 t_mat},
			{"mul", matrix_multiply_method, 2, ARGS(t_mat, t_mat),
			 t_mat},
			{"scale", matrix_scale_method, 2, ARGS(t_mat, t_num),
			 t_mat},
			{"transpose", matrix_transpose_method, 1, ARGS(t_mat),
			 t_mat},
			{"determinant", matrix_determinant_method, 1,
			 ARGS(t_mat), t_flt},
			{"inverse", matrix_inverse_method, 1, ARGS(t_mat),
			 RES(t_mat)},
			{"trace", matrix_trace_method, 1, ARGS(t_mat), t_flt},
			{"rank", matrix_rank_method, 1, ARGS(t_mat), t_int},
			{"row", matrix_row_method, 2, ARGS(t_mat, t_int),
			 t_vec},
			{"col", matrix_col_method, 2, ARGS(t_mat, t_int),
			 t_vec},
			{"equals", matrix_equals_method, 2, ARGS(t_mat, t_mat),
			 t_bool},
			{"copy", matrix_copy_method, 1, ARGS(t_mat), t_mat},
			{"to_array", matrix_to_array_method, 1, ARGS(t_mat),
			 arr_any},
			{"mul_vec", matrix_multiply_vector_method, 2,
			 ARGS(t_mat, t_vec), t_vec},
			{"rows", matrix_rows_method, 1, ARGS(t_mat), t_int},
			{"cols", matrix_cols_method, 1, ARGS(t_mat), t_int},
		};
		init_type_method_table(vm, &vm->matrix_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Matrix", new_matrix_function, 2, ARGS(t_int, t_int),
			 t_mat},
			{"IMatrix", new_matrix_identity_function, 1,
			 ARGS(t_int), t_mat},
			{"AMatrix", new_matrix_from_array_function, 3,
			 ARGS(t_int, t_int, arr_num), t_mat},
		};
		if (!init_module(vm, "matrix", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Range methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"contains", contains_range_method, 2,
			 ARGS(t_rang, t_int), t_bool},
			{"to_array", to_array_range_method, 1, ARGS(t_rang),
			 arr_any},
			{"start", start_range_method, 1, ARGS(t_rang), t_int},
			{"end", end_range_method, 1, ARGS(t_rang), t_int},
			{"step", step_range_method, 1, ARGS(t_rang), t_int},
			{"is_empty", is_empty_range_method, 1, ARGS(t_rang),
			 t_bool},
			{"reversed", reversed_range_method, 1, ARGS(t_rang),
			 t_rang},
		};
		init_type_method_table(vm, &vm->range_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Range", new_range_function, 3,
			 ARGS(t_int, t_int, t_int), t_rang},
		};
		if (!init_module(vm, "range", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Tuple methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"get", get_tuple_method, 2, ARGS(t_tup, t_int),
			 res_any},
			{"slice", slice_tuple_method, 3,
			 ARGS(t_tup, t_int, t_int), t_tup},
			{"index", index_tuple_method, 2, ARGS(t_tup, t_any),
			 res_int},
			{"is_empty", is_empty_tuple_method, 1, ARGS(t_tup),
			 t_bool},
			{"to_array", to_array_tuple_method, 1, ARGS(t_tup),
			 arr_any},
			{"first", first_tuple_method, 1, ARGS(t_tup), res_any},
			{"last", last_tuple_method, 1, ARGS(t_tup), res_any},
			{"contains", contains_tuple_method, 2,
			 ARGS(t_tup, t_any), t_bool},
			{"equals", equals_tuple_method, 2, ARGS(t_tup, t_tup),
			 t_bool},
		};
		init_type_method_table(vm, &vm->tuple_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Tuple", new_tuple_function, 1, ARGS(arr_any), t_tup},
		};
		if (!init_module(vm, "tuple", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Set methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"add", add_set_method, 2, ARGS(t_set, t_hash),
			 res_nil},
			{"remove", remove_set_method, 2, ARGS(t_set, t_hash),
			 res_nil},
			{"discard", discard_set_method, 2, ARGS(t_set, t_hash),
			 t_nil},
			{"union", union_set_method, 2, ARGS(t_set, t_set),
			 t_set},
			{"intersection", intersection_set_method, 2,
			 ARGS(t_set, t_set), t_set},
			{"difference", difference_set_method, 2,
			 ARGS(t_set, t_set), t_set},
			{"sym_difference", sym_difference_set_method, 2,
			 ARGS(t_set, t_set), t_set},
			{"is_subset", is_subset_set_method, 2,
			 ARGS(t_set, t_set), t_bool},
			{"is_superset", is_superset_set_method, 2,
			 ARGS(t_set, t_set), t_bool},
			{"is_disjoint", is_disjoint_set_method, 2,
			 ARGS(t_set, t_set), t_bool},
			{"contains", contains_set_method, 2,
			 ARGS(t_set, t_hash), t_bool},
			{"is_empty", is_empty_set_method, 1, ARGS(t_set),
			 t_bool},
			{"to_array", to_array_set_method, 1, ARGS(t_set),
			 arr_any},
			{"clone", clone_set_method, 1, ARGS(t_set), t_set},
		};
		init_type_method_table(vm, &vm->set_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Set", new_set_function, 1, ARGS(arr_any), t_set},
		};
		if (!init_module(vm, "set", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Buffer methods  +  module constructor
	// ---------------------------------------------------------------
	{
		Callable methods[] = {
			{"write_byte", write_byte_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"write_int16_le", write_int16_le_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"write_int32_le", write_int32_le_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"write_float32_le", write_float32_le_buffer_method, 2,
			 ARGS(t_buf, t_flt), res_nil},
			{"write_float64_le", write_float64_le_buffer_method, 2,
			 ARGS(t_buf, t_flt), res_nil},
			{"write_int16_be", write_int16_be_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"write_int32_be", write_int32_be_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"write_float32_be", write_float32_be_buffer_method, 2,
			 ARGS(t_buf, t_flt), res_nil},
			{"write_float64_be", write_float64_be_buffer_method, 2,
			 ARGS(t_buf, t_flt), res_nil},
			{"write_string", write_string_buffer_method, 2,
			 ARGS(t_buf, t_str), res_nil},
			{"write_buffer", write_buffer_buffer_method, 2,
			 ARGS(t_buf, t_buf), res_nil},
			{"read_byte", read_byte_buffer_method, 1, ARGS(t_buf),
			 res_int},
			{"read_string", read_string_buffer_method, 2,
			 ARGS(t_buf, t_int), res_str},
			{"read_line", read_line_buffer_method, 1, ARGS(t_buf),
			 res_str},
			{"read_all", read_all_buffer_method, 1, ARGS(t_buf),
			 res_str},
			{"read_int16_le", read_int16_le_buffer_method, 1,
			 ARGS(t_buf), res_int},
			{"read_int32_le", read_int32_le_buffer_method, 1,
			 ARGS(t_buf), res_int},
			{"read_float32_le", read_float32_le_buffer_method, 1,
			 ARGS(t_buf), res_flt},
			{"read_float64_le", read_float64_le_buffer_method, 1,
			 ARGS(t_buf), res_flt},
			{"read_int16_be", read_int16_be_buffer_method, 1,
			 ARGS(t_buf), res_int},
			{"read_int32_be", read_int32_be_buffer_method, 1,
			 ARGS(t_buf), res_int},
			{"read_float32_be", read_float32_be_buffer_method, 1,
			 ARGS(t_buf), res_flt},
			{"read_float64_be", read_float64_be_buffer_method, 1,
			 ARGS(t_buf), res_flt},
			{"capacity", capacity_buffer_method, 1, ARGS(t_buf),
			 t_int},
			{"is_empty", is_empty_buffer_method, 1, ARGS(t_buf),
			 t_bool},
			{"clear", clear_buffer_method, 1, ARGS(t_buf), t_nil},
			{"peek_byte", peek_byte_buffer_method, 1, ARGS(t_buf),
			 res_int},
			{"skip_bytes", skip_bytes_buffer_method, 2,
			 ARGS(t_buf, t_int), res_nil},
			{"clone", clone_buffer_method, 1, ARGS(t_buf), t_buf},
			{"compact", compact_buffer_method, 1, ARGS(t_buf),
			 t_buf},
		};
		init_type_method_table(vm, &vm->buffer_type, methods,
				       ARRAY_COUNT(methods));

		Callable fns[] = {
			{"Buffer", new_buffer_function, 0, ARGS0, t_buf},
		};
		if (!init_module(vm, "buffer", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Math module
	// ---------------------------------------------------------------
	{
		Callable fns[] = {
			{"pow", pow_function, 2, ARGS(t_num, t_num), t_flt},
			{"sqrt", sqrt_function, 1, ARGS(t_num), t_flt},
			{"ceil", ceil_function, 1, ARGS(t_num), t_int},
			{"floor", floor_function, 1, ARGS(t_num), t_int},
			{"abs", abs_function, 1, ARGS(t_num), t_num},
			{"sin", sin_function, 1, ARGS(t_num), t_flt},
			{"cos", cos_function, 1, ARGS(t_num), t_flt},
			{"tan", tan_function, 1, ARGS(t_num), t_flt},
			{"atan", atan_function, 1, ARGS(t_num), t_flt},
			{"acos", acos_function, 1, ARGS(t_num), t_flt},
			{"asin", asin_function, 1, ARGS(t_num), t_flt},
			{"exp", exp_function, 1, ARGS(t_num), t_flt},
			{"ln", ln_function, 1, ARGS(t_num), t_flt},
			{"log", log10_function, 1, ARGS(t_num), t_flt},
			{"round", round_function, 1, ARGS(t_num), t_int},
			{"min", min_function, 2, ARGS(t_num, t_num), t_num},
			{"max", max_function, 2, ARGS(t_num, t_num), t_num},
			{"e", e_function, 0, ARGS0, t_flt},
			{"pi", pi_function, 0, ARGS0, t_flt},
			{"nan", nan_function, 0, ARGS0, t_flt},
			{"inf", inf_function, 0, ARGS0, t_flt},
		};
		if (!init_module(vm, "math", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// IO module
	// ---------------------------------------------------------------
	{
		Callable fns[] = {
			{"print", io_print_function, 1, ARGS(t_any), t_nil},
			{"println", io_println_function, 1, ARGS(t_any), t_nil},
			{"print_to", io_print_to_function, 2,
			 ARGS(t_str, t_any), t_nil},
			{"println_to", io_println_to_function, 2,
			 ARGS(t_str, t_any), t_nil},
			{"scan", io_scan_function, 0, ARGS0, res_str},
			{"scanln", io_scanln_function, 0, ARGS0, res_str},
			{"nscan", io_nscan_function, 1, ARGS(t_int), res_str},
			{"scan_from", io_scan_from_function, 1, ARGS(t_str),
			 res_str},
			{"scanln_from", io_scanln_from_function, 1, ARGS(t_str),
			 res_str},
			{"nscan_from", io_nscan_from_function, 2,
			 ARGS(t_str, t_int), res_str},
		};
		if (!init_module(vm, "io", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Time module
	// ---------------------------------------------------------------
	{
		Callable fns[] = {
			{"sleep_s", sleep_seconds_function, 1, ARGS(t_num),
			 t_nil},
			{"sleep_ms", sleep_milliseconds_function, 1,
			 ARGS(t_num), t_nil},
			{"_time_s", time_seconds_function_, 0, ARGS0, t_flt},
			{"_time_ms", time_milliseconds_function_, 0, ARGS0,
			 t_flt},
			{"_year", year_function_, 0, ARGS0, t_int},
			{"_month", month_function_, 0, ARGS0, t_int},
			{"_day", day_function_, 0, ARGS0, t_int},
			{"_hour", hour_function_, 0, ARGS0, t_int},
			{"_minute", minute_function_, 0, ARGS0, t_int},
			{"_second", second_function_, 0, ARGS0, t_int},
			{"_weekday", weekday_function_, 0, ARGS0, t_int},
			{"_day_of_year", day_of_year_function_, 0, ARGS0,
			 t_int},
		};
		if (!init_module(vm, "time", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// System module
	// ---------------------------------------------------------------
	{
		Callable fns[] = {
			{"args", args_function, 0, ARGS0, arr_str},
			{"get_env", get_env_function, 1, ARGS(t_str), res_str},
			{"sleep", sleep_function, 1, ARGS(t_int), t_nil},
			{"_platform", platform_function, 0, ARGS0, t_str},
			{"_arch", arch_function, 0, ARGS0, t_str},
			{"_pid", pid_function, 0, ARGS0, t_int},
			{"exit", exit_function, 1, ARGS(t_int), t_nil},
		};
		if (!init_module(vm, "sys", fns, ARRAY_COUNT(fns)))
			return false;
	}

	// ---------------------------------------------------------------
	// Filesystem module
	// ---------------------------------------------------------------
	{
		TypeRecord *res_file = RES(t_file);

		Callable fns[] = {
			{"open", fs_open_function, 2, ARGS(t_str, t_str),
			 res_file},
			{"remove", fs_remove_function, 1, ARGS(t_str), res_nil},
			{"size", fs_file_size_function, 1, ARGS(t_str),
			 res_int},
			{"copy_file", fs_copy_file_function, 2,
			 ARGS(t_str, t_str), res_nil},
			{"mkdir", fs_mkdir_function, 1, ARGS(t_str), res_nil},
			{"read_file", fs_read_file_function, 1, ARGS(t_str),
			 res_str},
			{"write_file", fs_write_file_function, 2,
			 ARGS(t_str, t_str), res_nil},
			{"append_file", fs_append_file_function, 2,
			 ARGS(t_str, t_str), res_nil},
			{"exists", fs_exists_function, 1, ARGS(t_str), t_bool},
			{"is_file", fs_is_file_function, 1, ARGS(t_str),
			 t_bool},
			{"is_dir", fs_is_dir_function, 1, ARGS(t_str), t_bool},
		};
		if (!init_module(vm, "fs", fns, ARRAY_COUNT(fns)))
			return false;
	}

	return true;
}
