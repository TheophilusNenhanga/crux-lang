#ifndef CRUX_LANG_MATRIX_H
#define CRUX_LANG_MATRIX_H

#include "object.h"
#include "vm.h"

/* ── Construction ──────────────────────────────────────────────────────────── */

/* new_matrix_function(rows, cols) -> Result<Matrix>
 * new_matrix_identity_function(n)  -> Result<Matrix>   (n×n identity)
 * new_matrix_from_array_function(rows, cols, array) -> Result<Matrix>
 */
Value new_matrix_function(VM *vm, int arg_count, const Value *args);
Value new_matrix_identity_function(VM *vm, int arg_count, const Value *args);
Value new_matrix_from_array_function(VM *vm, int arg_count, const Value *args);

/* ── Element access ────────────────────────────────────────────────────────── */

/* get(row, col) -> Result<float>   set(row, col, val) -> Result<nil> */
Value matrix_get_method(VM *vm, int arg_count, const Value *args);
Value matrix_set_method(VM *vm, int arg_count, const Value *args);

/* ── Infallible properties ─────────────────────────────────────────────────── */

Value matrix_rows_method(VM *vm, int arg_count, const Value *args);
Value matrix_cols_method(VM *vm, int arg_count, const Value *args);

/* ── Arithmetic ────────────────────────────────────────────────────────────── */

Value matrix_add_method(VM *vm, int arg_count, const Value *args);
Value matrix_subtract_method(VM *vm, int arg_count, const Value *args);
Value matrix_multiply_method(VM *vm, int arg_count, const Value *args);   /* mat×mat or mat×scalar */
Value matrix_scale_method(VM *vm, int arg_count, const Value *args);      /* mat × scalar */

/* ── Linear-algebra operations ─────────────────────────────────────────────── */

Value matrix_transpose_method(VM *vm, int arg_count, const Value *args);
Value matrix_determinant_method(VM *vm, int arg_count, const Value *args);
Value matrix_inverse_method(VM *vm, int arg_count, const Value *args);
Value matrix_trace_method(VM *vm, int arg_count, const Value *args);
Value matrix_rank_method(VM *vm, int arg_count, const Value *args);

/* ── Row operations (return new matrix) ────────────────────────────────────── */

Value matrix_row_method(VM *vm, int arg_count, const Value *args);   /* row(i) -> Array */
Value matrix_col_method(VM *vm, int arg_count, const Value *args);   /* col(j) -> Array */

/* ── Utilities ─────────────────────────────────────────────────────────────── */

Value matrix_equals_method(VM *vm, int arg_count, const Value *args);
Value matrix_copy_method(VM *vm, int arg_count, const Value *args);
Value matrix_to_array_method(VM *vm, int arg_count, const Value *args); /* -> Array of Arrays */

/* ── Vector interop ────────────────────────────────────────────────────────── */

Value matrix_multiply_vector_method(VM *vm, int arg_count, const Value *args); /* M × v -> Vector */

#endif // CRUX_LANG_MATRIX_H
