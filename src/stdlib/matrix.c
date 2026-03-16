#include <math.h>
#include <string.h>

#include "object.h"
#include "panic.h"
#include "stdlib/matrix.h"

#include "garbage_collector.h"

#define EPSILON 1e-10
#define MATRIX_AT(m, i, j) ((m)->data[(i) * (m)->col_dim + (j)])

/* Validate that two matrices have the same shape. */
#define REQUIRE_SAME_SHAPE(m1, m2, op)                                                                                 \
	if ((m1)->row_dim != (m2)->row_dim || (m1)->col_dim != (m2)->col_dim) {                                            \
		return MAKE_GC_SAFE_ERROR(vm, "Matrices must have the same dimensions for " op ".", TYPE);                     \
	}

/*
 * Deep-copy src into a freshly allocated ObjectMatrix.
 * Returns NULL on allocation failure (the VM will have already seen the OOM).
 */
static ObjectMatrix *matrix_copy_internal(VM *vm, const ObjectMatrix *src)
{
	ObjectMatrix *dst = new_matrix(vm, src->row_dim, src->col_dim);
	memcpy(dst->data, src->data, sizeof(double) * src->row_dim * src->col_dim);
	return dst;
}

/*
 * LU decomposition with partial pivoting.
 * Operates in-place on `m` (a square matrix), fills `perm` with the row
 * permutation, and returns the sign of the permutation (+1 or -1) so
 * the caller can adjust the determinant.
 * Returns 0 if the matrix is singular.
 */
static int lu_decompose(double *restrict m, const uint16_t n, uint16_t *perm)
{
	int sign = 1;
	for (uint16_t i = 0; i < n; i++) {
		perm[i] = i;
	}

	for (uint16_t col = 0; col < n; col++) {
		/* Find pivot row */
		uint16_t pivot = col;
		double max_val = fabs(m[col * n + col]);
		for (uint16_t row = col + 1; row < n; row++) {
			const double v = fabs(m[row * n + col]);
			if (v > max_val) {
				max_val = v;
				pivot = row;
			}
		}

		if (max_val < EPSILON) {
			return 0; /* singular */
		}

		if (pivot != col) {
			/* Swap rows */
			for (uint16_t k = 0; k < n; k++) {
				const double tmp = m[col * n + k];
				m[col * n + k] = m[pivot * n + k];
				m[pivot * n + k] = tmp;
			}
			const uint16_t tmp_p = perm[col];
			perm[col] = perm[pivot];
			perm[pivot] = tmp_p;
			sign = -sign;
		}

		const double diag = m[col * n + col];
		for (uint16_t row = col + 1; row < n; row++) {
			m[row * n + col] /= diag;
			for (uint16_t k = col + 1; k < n; k++) {
				m[row * n + k] -= m[row * n + col] * m[col * n + k];
			}
		}
	}
	return sign;
}

/* ── Construction ────────────────────────────────────────────────────────────
 */

/**
 * Creates a new matrix with the specified dimensions (zero-initialized)
 * arg0 -> rows: Int
 * arg1 -> cols: Int
 * Returns Result<Matrix>
 */
Value new_matrix_function(VM *vm, const Value *args)
{
	const int32_t rows = AS_INT(args[0]);
	const int32_t cols = AS_INT(args[1]);

	if (rows <= 0 || cols <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Matrix dimensions must be positive integers.", VALUE);
	}

	ObjectMatrix *mat = new_matrix(vm, (uint16_t)rows, (uint16_t)cols);
	push(vm->current_module_record, OBJECT_VAL(mat));

	memset(mat->data, 0, sizeof(double) * rows * cols);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(mat));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Creates an n×n identity matrix
 * arg0 -> n: Int
 * Returns Result<Matrix>
 */
Value new_matrix_identity_function(VM *vm, const Value *args)
{
	const int32_t n = AS_INT(args[0]);
	if (n <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Identity matrix size must be a positive integer.", VALUE);
	}

	ObjectMatrix *mat = new_matrix(vm, (uint16_t)n, (uint16_t)n);
	push(vm->current_module_record, OBJECT_VAL(mat));

	memset(mat->data, 0, sizeof(double) * n * n);
	for (int32_t i = 0; i < n; i++) {
		MATRIX_AT(mat, i, i) = 1.0;
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(mat));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Creates a matrix from a flat array of numbers (row-major order)
 * arg0 -> rows: Int
 * arg1 -> cols: Int
 * arg2 -> data: Array
 * Returns Result<Matrix>
 */
Value new_matrix_from_array_function(VM *vm, const Value *args)
{
	const int32_t rows = AS_INT(args[0]);
	const int32_t cols = AS_INT(args[1]);

	if (rows <= 0 || cols <= 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Matrix dimensions must be positive integers.", VALUE);
	}

	const ObjectArray *arr = AS_CRUX_ARRAY(args[2]);
	const uint32_t total = (uint32_t)(rows * cols);

	for (uint32_t i = 0; i < arr->size && i < total; i++) {
		if (!IS_NUMERIC(arr->values[i])) {
			return MAKE_GC_SAFE_ERROR(vm,
									  "All elements of <data> must be of type "
									  "'int' | 'float'.",
									  TYPE);
		}
	}

	ObjectMatrix *mat = new_matrix(vm, (uint16_t)rows, (uint16_t)cols);
	push(vm->current_module_record, OBJECT_VAL(mat));

	const uint32_t copy_count = arr->size < total ? arr->size : total;
	for (uint32_t i = 0; i < copy_count; i++) {
		mat->data[i] = TO_DOUBLE(arr->values[i]);
	}
	for (uint32_t i = copy_count; i < total; i++) {
		mat->data[i] = 0.0;
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(mat));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/* ── Element access ──────────────────────────────────────────────────────────
 */

/**
 * Gets the element at the specified row and column (0-indexed)
 * arg0 -> matrix: Matrix
 * arg1 -> row: Int
 * arg2 -> col: Int
 * Returns Result<Float>
 */
Value matrix_get_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const int32_t row = AS_INT(args[1]);
	const int32_t col = AS_INT(args[2]);

	if (row < 0 || (uint16_t)row >= mat->row_dim || col < 0 || (uint16_t)col >= mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Matrix index out of bounds.", BOUNDS);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(MATRIX_AT(mat, row, col))));
}

/**
 * Sets the element at the specified row and column
 * arg0 -> matrix: Matrix
 * arg1 -> row: Int
 * arg2 -> col: Int
 * arg3 -> value: Float
 * Returns Result<Nil>
 */
Value matrix_set_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const int32_t row = AS_INT(args[1]);
	const int32_t col = AS_INT(args[2]);

	if (row < 0 || (uint16_t)row >= mat->row_dim || col < 0 || (uint16_t)col >= mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Matrix index out of bounds.", BOUNDS);
	}

	MATRIX_AT(mat, row, col) = TO_DOUBLE(args[3]);
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/* ── Infallible property accessors ───────────────────────────────────────────
 */

/**
 * Returns the number of rows in the matrix
 * arg0 -> matrix: Matrix
 * Returns Int
 */
Value matrix_rows_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	return INT_VAL(mat->row_dim);
}

/**
 * Returns the number of columns in the matrix
 * arg0 -> matrix: Matrix
 * Returns Int
 */
Value matrix_cols_method(VM *vm, const Value *args)
{
	(void)vm;

	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	return INT_VAL(mat->col_dim);
}

/* ── Arithmetic ──────────────────────────────────────────────────────────────
 */

/**
 * Adds another matrix element-wise (matrices must have the same dimensions)
 * arg0 -> matrix: Matrix
 * arg1 -> other: Matrix
 * Returns Result<Matrix>
 */
Value matrix_add_method(VM *vm, const Value *args)
{
	const ObjectMatrix *a = AS_CRUX_MATRIX(args[0]);
	const ObjectMatrix *b = AS_CRUX_MATRIX(args[1]);
	REQUIRE_SAME_SHAPE(a, b, "addition");

	ObjectMatrix *result = new_matrix(vm, a->row_dim, a->col_dim);
	push(vm->current_module_record, OBJECT_VAL(result));

	const uint32_t total = (uint32_t)a->row_dim * a->col_dim;
	for (uint32_t i = 0; i < total; i++) {
		result->data[i] = a->data[i] + b->data[i];
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Subtracts another matrix element-wise (matrices must have the same
 * dimensions) arg0 -> matrix: Matrix arg1 -> other: Matrix Returns
 * Result<Matrix>
 */
Value matrix_subtract_method(VM *vm, const Value *args)
{
	const ObjectMatrix *a = AS_CRUX_MATRIX(args[0]);
	const ObjectMatrix *b = AS_CRUX_MATRIX(args[1]);
	REQUIRE_SAME_SHAPE(a, b, "subtraction");

	ObjectMatrix *result = new_matrix(vm, a->row_dim, a->col_dim);
	push(vm->current_module_record, OBJECT_VAL(result));

	const uint32_t total = (uint32_t)a->row_dim * a->col_dim;
	for (uint32_t i = 0; i < total; i++) {
		result->data[i] = a->data[i] - b->data[i];
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Performs standard matrix multiplication (first matrix cols must equal second
 * matrix rows) arg0 -> matrix: Matrix arg1 -> other: Matrix Returns
 * Result<Matrix>
 */
Value matrix_multiply_method(VM *vm, const Value *args)
{
	const ObjectMatrix *a = AS_CRUX_MATRIX(args[0]);
	const ObjectMatrix *b = AS_CRUX_MATRIX(args[1]);

	if (a->col_dim != b->row_dim) {
		return MAKE_GC_SAFE_ERROR(vm,
								  "Number of columns in first matrix must equal "
								  "number of rows in second matrix for multiplication.",
								  TYPE);
	}

	ObjectMatrix *result = new_matrix(vm, a->row_dim, b->col_dim);
	push(vm->current_module_record, OBJECT_VAL(result));

	memset(result->data, 0, sizeof(double) * a->row_dim * b->col_dim);

	for (uint16_t i = 0; i < a->row_dim; i++) {
		for (uint16_t k = 0; k < a->col_dim; k++) {
			const double aik = MATRIX_AT(a, i, k);
			if (fabs(aik) < EPSILON)
				continue; /* skip zero rows */
			for (uint16_t j = 0; j < b->col_dim; j++) {
				MATRIX_AT(result, i, j) += aik * MATRIX_AT(b, k, j);
			}
		}
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Multiplies every element in the matrix by a scalar value
 * arg0 -> matrix: Matrix
 * arg1 -> scalar: Float
 * Returns Matrix
 */
Value matrix_scale_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const double scalar = TO_DOUBLE(args[1]);

	ObjectMatrix *result = new_matrix(vm, mat->row_dim, mat->col_dim);

	const uint32_t total = (uint32_t)mat->row_dim * mat->col_dim;
	for (uint32_t i = 0; i < total; i++) {
		result->data[i] = mat->data[i] * scalar;
	}
	return OBJECT_VAL(result);
}

/* ── Linear-algebra operations ───────────────────────────────────────────────
 */

/**
 * Returns the transpose of the matrix (rows become columns)
 * arg0 -> matrix: Matrix
 * Returns Result<Matrix>
 */
Value matrix_transpose_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	ObjectMatrix *result = new_matrix(vm, mat->col_dim, mat->row_dim);

	for (uint16_t i = 0; i < mat->row_dim; i++) {
		for (uint16_t j = 0; j < mat->col_dim; j++) {
			MATRIX_AT(result, j, i) = MATRIX_AT(mat, i, j);
		}
	}
	return OBJECT_VAL(result);
}

/**
 * Computes the determinant of the matrix (only defined for square matrices)
 * arg0 -> matrix: Matrix
 * Returns Result<Float>
 */
Value matrix_determinant_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	if (mat->row_dim != mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Determinant is only defined for square matrices.", TYPE);
	}

	const uint16_t n = mat->row_dim;

	/* Work on a copy so we don't mutate the original */
	double *tmp = ALLOCATE(vm, double, (uint32_t)n *n);
	memcpy(tmp, mat->data, sizeof(double) * n * n);

	uint16_t *perm = ALLOCATE(vm, uint16_t, n);
	const int sign = lu_decompose(tmp, n, perm);

	double det = (double)sign;
	if (sign != 0) {
		for (uint16_t i = 0; i < n; i++) {
			det *= tmp[i * n + i];
		}
	}

	FREE_ARRAY(vm, double, tmp, (uint32_t)n *n);
	FREE_ARRAY(vm, uint16_t, perm, n);

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(det)));
}

/**
 * Computes the inverse of the matrix (only defined for square, non-singular
 * matrices) arg0 -> matrix: Matrix Returns Result<Matrix>
 */
Value matrix_inverse_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	if (mat->row_dim != mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Inverse is only defined for square matrices.", TYPE);
	}

	const uint16_t n = mat->row_dim;
	const uint32_t n2 = (uint32_t)n * n;

	/* Build augmented matrix [A | I] stored as two separate buffers */
	double *aug = ALLOCATE(vm, double, n2);
	memcpy(aug, mat->data, sizeof(double) * n2);

	/* Allocate result and initialise as identity */
	ObjectMatrix *result = new_matrix(vm, n, n);
	push(vm->current_module_record, OBJECT_VAL(result));
	memset(result->data, 0, sizeof(double) * n2);
	for (uint16_t i = 0; i < n; i++) {
		MATRIX_AT(result, i, i) = 1.0;
	}

	/* Gauss-Jordan with partial pivoting */
	for (uint16_t col = 0; col < n; col++) {
		/* Find pivot */
		uint16_t pivot = col;
		double max_val = fabs(aug[col * n + col]);
		for (uint16_t row = col + 1; row < n; row++) {
			double v = fabs(aug[row * n + col]);
			if (v > max_val) {
				max_val = v;
				pivot = row;
			}
		}

		if (max_val < EPSILON) {
			FREE_ARRAY(vm, double, aug, n2);
			pop(vm->current_module_record); /* result */
			return MAKE_GC_SAFE_ERROR(vm, "Matrix is singular and cannot be inverted.", MATH);
		}

		if (pivot != col) {
			for (uint16_t k = 0; k < n; k++) {
				double t = aug[col * n + k];
				aug[col * n + k] = aug[pivot * n + k];
				aug[pivot * n + k] = t;

				t = MATRIX_AT(result, col, k);
				MATRIX_AT(result, col, k) = MATRIX_AT(result, pivot, k);
				MATRIX_AT(result, pivot, k) = t;
			}
		}

		const double diag = aug[col * n + col];
		for (uint16_t k = 0; k < n; k++) {
			aug[col * n + k] /= diag;
			MATRIX_AT(result, col, k) /= diag;
		}

		for (uint16_t row = 0; row < n; row++) {
			if (row == col)
				continue;
			const double factor = aug[row * n + col];
			for (uint16_t k = 0; k < n; k++) {
				aug[row * n + k] -= factor * aug[col * n + k];
				MATRIX_AT(result, row, k) -= factor * MATRIX_AT(result, col, k);
			}
		}
	}

	FREE_ARRAY(vm, double, aug, n2);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Returns the trace of the matrix (sum of main diagonal elements, square
 * matrices only) arg0 -> matrix: Matrix Returns Result<Float>
 */
Value matrix_trace_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	if (mat->row_dim != mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Trace is only defined for square matrices.", TYPE);
	}

	double trace = 0.0;
	for (uint16_t i = 0; i < mat->row_dim; i++) {
		trace += MATRIX_AT(mat, i, i);
	}

	return OBJECT_VAL(new_ok_result(vm, FLOAT_VAL(trace)));
}

/**
 * Computes the rank of the matrix
 * arg0 -> matrix: Matrix
 * Returns Result<Int>
 */
Value matrix_rank_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const uint16_t rows = mat->row_dim;
	const uint16_t cols = mat->col_dim;
	const uint32_t total = (uint32_t)rows * cols;

	double *tmp = ALLOCATE(vm, double, total);
	memcpy(tmp, mat->data, sizeof(double) * total);

	uint16_t rank = 0;
	uint16_t pivot_row = 0;

	for (uint16_t col = 0; col < cols && pivot_row < rows; col++) {
		/* Find a non-zero element in this column at or below pivot_row
		 */
		uint16_t found = UINT16_MAX;
		for (uint16_t row = pivot_row; row < rows; row++) {
			if (fabs(tmp[row * cols + col]) >= EPSILON) {
				found = row;
				break;
			}
		}
		if (found == UINT16_MAX)
			continue;

		/* Swap found row to pivot_row */
		if (found != pivot_row) {
			for (uint16_t k = 0; k < cols; k++) {
				double t = tmp[pivot_row * cols + k];
				tmp[pivot_row * cols + k] = tmp[found * cols + k];
				tmp[found * cols + k] = t;
			}
		}

		/* Eliminate below */
		const double diag = tmp[pivot_row * cols + col];
		for (uint16_t row = pivot_row + 1; row < rows; row++) {
			const double factor = tmp[row * cols + col] / diag;
			for (uint16_t k = col; k < cols; k++) {
				tmp[row * cols + k] -= factor * tmp[pivot_row * cols + k];
			}
		}

		rank++;
		pivot_row++;
	}

	FREE_ARRAY(vm, double, tmp, total);
	return OBJECT_VAL(new_ok_result(vm, INT_VAL((int32_t)rank)));
}

/* ── Row / column extraction ─────────────────────────────────────────────────
 */

/**
 * Returns the specified row as an array
 * arg0 -> matrix: Matrix
 * arg1 -> row: Int
 * Returns Result<Array>
 */
Value matrix_row_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const int32_t row = AS_INT(args[1]);

	if (row < 0 || (uint16_t)row >= mat->row_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Row index out of bounds.", BOUNDS);
	}

	ObjectArray *arr = new_array(vm, row);
	push(vm->current_module_record, OBJECT_VAL(arr));

	for (uint16_t j = 0; j < mat->col_dim; j++) {
		const Value v = FLOAT_VAL(MATRIX_AT(mat, row, j));
		array_add_back(vm, arr, v);
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(arr));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Returns the specified column as an array
 * arg0 -> matrix: Matrix
 * arg1 -> col: Int
 * Returns Result<Array>
 */
Value matrix_col_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const int32_t col = AS_INT(args[1]);

	if (col < 0 || (uint16_t)col >= mat->col_dim) {
		return MAKE_GC_SAFE_ERROR(vm, "Column index out of bounds.", BOUNDS);
	}

	ObjectArray *arr = new_array(vm, col);
	push(vm->current_module_record, OBJECT_VAL(arr));

	for (uint16_t i = 0; i < mat->row_dim; i++) {
		Value v = FLOAT_VAL(MATRIX_AT(mat, i, col));
		array_add_back(vm, arr, v);
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(arr));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/* ── Utilities ───────────────────────────────────────────────────────────────
 */

/**
 * Checks if two matrices are equal (element-wise comparison with epsilon
 * tolerance) arg0 -> matrix: Matrix arg1 -> other: Matrix Returns Result<Bool>
 */
Value matrix_equals_method(VM *vm, const Value *args)
{
	const ObjectMatrix *a = AS_CRUX_MATRIX(args[0]);
	const ObjectMatrix *b = AS_CRUX_MATRIX(args[1]);

	if (a->row_dim != b->row_dim || a->col_dim != b->col_dim) {
		return BOOL_VAL(false);
	}

	const uint32_t total = (uint32_t)a->row_dim * a->col_dim;
	for (uint32_t i = 0; i < total; i++) {
		if (fabs(a->data[i] - b->data[i]) >= EPSILON) {
			return BOOL_VAL(false);
		}
	}

	return BOOL_VAL(true);
}

/**
 * Returns a deep copy of the matrix
 * arg0 -> matrix: Matrix
 * Returns Result<Matrix>
 */
Value matrix_copy_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	ObjectMatrix *dst = matrix_copy_internal(vm, mat);
	return OBJECT_VAL(dst);
}

/**
 * Converts the matrix to an array of row arrays
 * arg0 -> matrix: Matrix
 * Returns Result<Array>
 */
Value matrix_to_array_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);

	ObjectArray *outer = new_array(vm, mat->row_dim);
	push(vm->current_module_record, OBJECT_VAL(outer));

	for (uint16_t i = 0; i < mat->row_dim; i++) {
		ObjectArray *row_arr = new_array(vm, mat->col_dim);
		push(vm->current_module_record, OBJECT_VAL(row_arr));

		for (uint16_t j = 0; j < mat->col_dim; j++) {
			bool success = array_add_back(vm, row_arr, FLOAT_VAL(MATRIX_AT(mat, i, j)));
			if (!success) {
				pop(vm->current_module_record); /* row_arr */
				pop(vm->current_module_record); /* outer */
				return MAKE_GC_SAFE_ERROR(vm, "Failed to add element to row array", MEMORY);
			}
		}

		array_add_back(vm, outer, OBJECT_VAL(row_arr));
		pop(vm->current_module_record); /* row_arr */
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(outer));
	pop(vm->current_module_record); /* outer */
	return OBJECT_VAL(res);
}

/* ── Vector interop ──────────────────────────────────────────────────────────
 */

/**
 * Multiplies the matrix by a vector (matrix columns must equal vector
 * dimension) arg0 -> matrix: Matrix arg1 -> vector: Vector Returns
 * Result<Vector>
 */
Value matrix_multiply_vector_method(VM *vm, const Value *args)
{
	const ObjectMatrix *mat = AS_CRUX_MATRIX(args[0]);
	const ObjectVector *vec = AS_CRUX_VECTOR(args[1]);

	if (mat->col_dim != vec->dimensions) {
		return MAKE_GC_SAFE_ERROR(vm,
								  "Matrix column count must match vector dimension "
								  "for matrix-vector multiplication.",
								  TYPE);
	}

	ObjectVector *result_vec = new_vector(vm, mat->row_dim);
	push(vm->current_module_record, OBJECT_VAL(result_vec));

	const double *v_comp = VECTOR_COMPONENTS(vec);
	double *r_comp = VECTOR_COMPONENTS(result_vec);

	for (uint16_t i = 0; i < mat->row_dim; i++) {
		double sum = 0.0;
		for (uint16_t j = 0; j < mat->col_dim; j++) {
			sum += MATRIX_AT(mat, i, j) * v_comp[j];
		}
		r_comp[i] = sum;
	}

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_vec));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}
