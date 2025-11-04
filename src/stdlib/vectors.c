#include <math.h>
#include <stdlib.h>

#include "object.h"
#include "panic.h"
#include "stdlib/vectors.h"

#define EPSILON 1e-10
#define STATIC_VECTOR_SIZE 4

#define TO_DOUBLE(value)                                                       \
	IS_INT((value)) ? (double)AS_INT((value)) : AS_FLOAT((value))

#define IS_ZERO_SCALAR(scalar) ((scalar) < EPSILON && (scalar) > -EPSILON)

static double compute_magnitude(const double *restrict components,
				const uint32_t dimensions)
{
	double sum = 0.0;
	for (uint32_t i = 0; i < dimensions; i++) {
		sum += components[i] * components[i];
	}
	return sqrt(sum);
}

static double compute_dot_product(const double *restrict comp1,
				  const double *restrict comp2,
				  const uint32_t dimensions)
{
	double result = 0.0;
	for (uint32_t i = 0; i < dimensions; i++) {
		result += comp1[i] * comp2[i];
	}
	return result;
}

static void compute_vector_add(double *restrict result,
			       const double *restrict comp1,
			       const double *restrict comp2,
			       const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = comp1[i] + comp2[i];
	}
}

static void compute_vector_subtract(double *restrict result,
				    const double *restrict comp1,
				    const double *restrict comp2,
				    const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = comp1[i] - comp2[i];
	}
}

static void compute_scalar_multiply(double *restrict result,
				    const double *restrict components,
				    const double scalar,
				    const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = components[i] * scalar;
	}
}

static void compute_scalar_divide(double *restrict result,
				  const double *restrict components,
				  const double scalar,
				  const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = components[i] / scalar;
	}
}

static void compute_normalize(double *restrict result,
			      const double *restrict components,
			      const double magnitude, const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = components[i] / magnitude;
	}
}

static double compute_distance(const double *restrict comp1,
			       const double *restrict comp2,
			       const uint32_t dimensions)
{
	double sum = 0.0;
	for (uint32_t i = 0; i < dimensions; i++) {
		const double diff = comp1[i] - comp2[i];
		sum += diff * diff;
	}
	return sqrt(sum);
}

static void compute_lerp(double *restrict result, const double *restrict comp1,
			 const double *restrict comp2, const double t,
			 const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = comp1[i] + t * (comp2[i] - comp1[i]);
	}
}

static void compute_reflect(double *restrict result,
			    const double *restrict incident,
			    const double *restrict normal,
			    const double normal_mag, const uint32_t dimensions)
{
	double dot = 0.0;
	for (uint32_t i = 0; i < dimensions; i++) {
		dot += incident[i] * (normal[i] / normal_mag);
	}

	for (uint32_t i = 0; i < dimensions; i++) {
		result[i] = incident[i] - 2.0 * dot * (normal[i] / normal_mag);
	}
}

static bool compute_equals(const double *restrict comp1,
			   const double *restrict comp2,
			   const uint32_t dimensions)
{
	for (uint32_t i = 0; i < dimensions; i++) {
		if (fabs(comp1[i] - comp2[i]) >= EPSILON) {
			return false;
		}
	}
	return true;
}

static double vector_magnitude(const ObjectVector *vec)
{
	const double *components = VECTOR_COMPONENTS(vec);
	return compute_magnitude(components, vec->dimensions);
}

/* arg_count = 2; arg0 -> dimension, arg1 -> components array */
ObjectResult *new_vector_function(VM *vm, const int arg_count,
				  const Value *args)
{
	(void)arg_count;

	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "<dimension> must be of type 'int'.",
					  TYPE);
	}
	if (!IS_CRUX_ARRAY(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<components> must be of type 'array'.", TYPE);
	}

	const ObjectArray *array = AS_CRUX_ARRAY(args[1]);

	for (uint32_t i = 0; i < array->size; i++) {
		if (!IS_NUMERIC(array->values[i])) {
			return MAKE_GC_SAFE_ERROR(
				vm,
				"elements of <components> must be of type "
				"'int' | 'float'",
				TYPE);
		}
	}

	const uint32_t dimensions = AS_INT(args[0]);
	const uint32_t copy_count = (array->size < dimensions) ? array->size
							       : dimensions;

	ObjectVector *vector = new_vector(vm, dimensions);
	push(vm->current_module_record, OBJECT_VAL(vector));

	double *components = VECTOR_COMPONENTS(vector);
	const Value *array_values = array->values;

	for (uint32_t i = 0; i < copy_count; i++) {
		components[i] = TO_DOUBLE(array_values[i]);
	}

	for (uint32_t i = copy_count; i < dimensions; i++) {
		components[i] = 0.0;
	}

	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(vector));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *vector_dot_method(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "dot method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for dot product.",
			TYPE);
	}

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);

	const double result = compute_dot_product(comp1, comp2,
						  vec1->dimensions);

	return new_ok_result(vm, FLOAT_VAL(result));
}

ObjectResult *vector_add_method(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "add method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for addition.",
			TYPE);
	}

	ObjectVector *result_vector = new_vector(vm, vec1->dimensions);
	push(vm->current_module_record, OBJECT_VAL(result_vector));

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_vector_add(result_comp, comp1, comp2, vec1->dimensions);

	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_vector));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *vector_subtract_method(VM *vm, const int arg_count,
				     const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"subtract method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for subtraction.",
			TYPE);
	}

	ObjectVector *result_vector = new_vector(vm, vec1->dimensions);
	push(vm->current_module_record, OBJECT_VAL(result_vector));

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_vector_subtract(result_comp, comp1, comp2, vec1->dimensions);

	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_vector));
	pop(vm->current_module_record);
	return result;
}

ObjectResult *vector_multiply_method(VM *vm, const int arg_count,
				     const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_NUMERIC(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "multiply method can only be used on "
					  "Vector objects and numbers.",
					  TYPE);
	}

	const ObjectVector *vec = AS_CRUX_VECTOR(args[0]);
	const double scalar = TO_DOUBLE(args[1]);

	ObjectVector *result_vector = new_vector(vm, vec->dimensions);
	push(vm->current_module_record, OBJECT_VAL(result_vector));

	const double *comp = VECTOR_COMPONENTS(vec);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_scalar_multiply(result_comp, comp, scalar, vec->dimensions);

	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(result_vector));
	pop(vm->current_module_record);

	return result;
}

ObjectResult *vector_divide_method(VM *vm, const int arg_count,
				   const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_NUMERIC(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "divide method can only be used on "
					  "Vector objects and numbers.",
					  TYPE);
	}

	const ObjectVector *vec = AS_CRUX_VECTOR(args[0]);
	const double scalar = TO_DOUBLE(args[1]);

	if (IS_ZERO_SCALAR(scalar)) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot divide by zero.", MATH);
	}

	ObjectVector *result_vector = new_vector(vm, vec->dimensions);

	const double *comp = VECTOR_COMPONENTS(vec);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_scalar_divide(result_comp, comp, scalar, vec->dimensions);

	return new_ok_result(vm, OBJECT_VAL(result_vector));
}

ObjectResult *vector_magnitude_method(VM *vm, const int arg_count,
				      const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"magnitude method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec = AS_CRUX_VECTOR(args[0]);

	const double magnitude = vector_magnitude(vec);

	return new_ok_result(vm, FLOAT_VAL(magnitude));
}

ObjectResult *vector_normalize_method(VM *vm, const int arg_count,
				      const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"normalize method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec = AS_CRUX_VECTOR(args[0]);

	const double magnitude = vector_magnitude(vec);

	if (IS_ZERO_SCALAR(magnitude)) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot normalize a zero vector.",
					  MATH);
	}

	ObjectVector *result_vector = new_vector(vm, vec->dimensions);

	const double *comp = VECTOR_COMPONENTS(vec);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_normalize(result_comp, comp, magnitude, vec->dimensions);

	return new_ok_result(vm, OBJECT_VAL(result_vector));
}

ObjectResult *vector_distance_method(VM *vm, const int arg_count,
				     const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"distance method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Vectors must have the same "
					  "dimension for distance calculation.",
					  TYPE);
	}

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);

	const double distance = compute_distance(comp1, comp2,
						 vec1->dimensions);

	return new_ok_result(vm, FLOAT_VAL(distance));
}

ObjectResult *vector_cross_method(VM *vm, const int arg_count,
				  const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "cross method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != 3 || vec2->dimensions != 3) {
		return MAKE_GC_SAFE_ERROR(
			vm, "cross product is only defined for 3D vectors.",
			TYPE);
	}

	ObjectVector *tmp = new_vector(vm, 3);
	push(vm->current_module_record, OBJECT_VAL(tmp));

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);
	double *result_comp = VECTOR_COMPONENTS(tmp);

	result_comp[0] = comp1[1] * comp2[2] - comp1[2] * comp2[1];
	result_comp[1] = comp1[2] * comp2[0] - comp1[0] * comp2[2];
	result_comp[2] = comp1[0] * comp2[1] - comp1[1] * comp2[0];

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(tmp));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *vector_angle_between_method(VM *vm, const int arg_count,
					  const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "angleBetween method can only be "
					  "used on Vector objects.",
					  TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Vectors must have the same dimension.", TYPE);
	}

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);

	const double dot = compute_dot_product(comp1, comp2, vec1->dimensions);
	const double mag1 = compute_magnitude(comp1, vec1->dimensions);
	const double mag2 = compute_magnitude(comp2, vec2->dimensions);

	if (fabs(mag1) < EPSILON || fabs(mag2) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot calculate angle with zero vector.", MATH);
	}

	const double cosTheta = dot / (mag1 * mag2);
	const double clampedCos = fmax(-1.0, fmin(1.0, cosTheta));
	const double result = acos(clampedCos);

	return new_ok_result(vm, FLOAT_VAL(result));
}

ObjectResult *vector_lerp_method(VM *vm, const int arg_count, const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1]) ||
	    !IS_NUMERIC(args[2])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"lerp method requires two Vector objects and a number.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);
	const double t = TO_DOUBLE(args[2]);

	if (vec1->dimensions != vec2->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Vectors must have the same dimension for lerp.",
			TYPE);
	}

	ObjectVector *result_vector = new_vector(vm, vec1->dimensions);
	push(vm->current_module_record, OBJECT_VAL(result_vector));

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_lerp(result_comp, comp1, comp2, t, vec1->dimensions);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_vector));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *vector_reflect_method(VM *vm, const int arg_count,
				    const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"reflect method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *incident = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *normal = AS_CRUX_VECTOR(args[1]);

	if (incident->dimensions != normal->dimensions) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for reflection.",
			TYPE);
	}

	const double normal_mag = vector_magnitude(normal);

	if (fabs(normal_mag) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot reflect with zero normal vector.", MATH);
	}

	ObjectVector *result_vector = new_vector(vm, incident->dimensions);
	push(vm->current_module_record, OBJECT_VAL(result_vector));

	const double *inc_comp = VECTOR_COMPONENTS(incident);
	const double *norm_comp = VECTOR_COMPONENTS(normal);
	double *result_comp = VECTOR_COMPONENTS(result_vector);

	compute_reflect(result_comp, inc_comp, norm_comp, normal_mag,
			incident->dimensions);

	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(result_vector));
	pop(vm->current_module_record);
	return res;
}

ObjectResult *vector_equals_method(VM *vm, const int arg_count,
				   const Value *args)
{
	(void)arg_count;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "equals method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimensions != vec2->dimensions) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	const double *comp1 = VECTOR_COMPONENTS(vec1);
	const double *comp2 = VECTOR_COMPONENTS(vec2);

	const bool equal = compute_equals(comp1, comp2, vec1->dimensions);

	return new_ok_result(vm, BOOL_VAL(equal));
}

Value vector_x_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimensions >= 1) {
		const double *comp = VECTOR_COMPONENTS(vector);
		return FLOAT_VAL(comp[0]);
	}
	return NIL_VAL;
}

Value vector_y_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimensions >= 2) {
		const double *comp = VECTOR_COMPONENTS(vector);
		return FLOAT_VAL(comp[1]);
	}
	return NIL_VAL;
}

Value vector_z_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimensions >= 3) {
		const double *comp = VECTOR_COMPONENTS(vector);
		return FLOAT_VAL(comp[2]);
	}
	return NIL_VAL;
}

Value vector_w_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimensions >= 4) {
		const double *comp = VECTOR_COMPONENTS(vector);
		return FLOAT_VAL(comp[3]);
	}
	return NIL_VAL;
}

Value vector_dimension_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	return INT_VAL(vector->dimensions);
}
