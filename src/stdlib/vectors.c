#include <math.h>
#include <stdlib.h>

#include "object.h"
#include "panic.h"
#include "stdlib/vectors.h"

#define EPSILON 1e-10

#define TO_DOUBLE(value)                                                       \
	IS_INT((value)) ? (double)AS_INT((value)) : AS_FLOAT((value))

#define IS_ZERO_SCALAR(scalar) ((scalar) < EPSILON && (scalar) > -EPSILON)

static double vector_magnitude(const ObjectVector *vec)
{
	double sum = 0.0;
	for (uint32_t i = 0; i < vec->dimension; i++) {
		sum += vec->components[i] * vec->components[i];
	}
	return sqrt(sum);
}

/* arg_count = 2; arg0 ->  */
ObjectResult *new_vector_function(VM *vm, const int arg_count,
				  const Value *args)
{
	(void) arg_count;
	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "<dimension> must be of type 'int'.", TYPE);
	}
	if (!IS_CRUX_ARRAY(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "<components> must be of type 'array'.", TYPE);
	}

	const ObjectArray *array = AS_CRUX_ARRAY(args[1]);
	
	
	for (uint32_t i = 0; i < array->size ; i++) {
		if (!IS_NUMERIC(array->values[i])) {
			return MAKE_GC_SAFE_ERROR(vm, "elements of <components> must be of type 'int' | 'float'", TYPE);
		}
	}

	const uint32_t dimension = AS_INT(args[0]);
	const ObjectVector *vector = new_vector(vm, dimension);

	for (uint32_t i = 0; i < dimension; i++) {
		if (i >= array->size) {
			vector->components[i] = 0;
		}
		vector->components[i] = TO_DOUBLE(array->values[i]);
	}
	return new_ok_result(vm, OBJECT_VAL(vector));
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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for dot product.",
			TYPE);
	}

	double result = 0.0;
	for (uint32_t i = 0; i < vec1->dimension; i++) {
		result += vec1->components[i] * vec2->components[i];
	}

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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for addition.",
			TYPE);
	}

	ObjectVector *result_vector = new_vector(vm, vec1->dimension);

	for (uint32_t i = 0; i < vec1->dimension; i++) {
		result_vector->components[i] = vec1->components[i] + vec2->components[i];
	}

	return new_ok_result(vm, OBJECT_VAL(result_vector));
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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for subtraction.",
			TYPE);
	}

	const ObjectVector *result_vector = new_vector(vm, vec1->dimension);

	for (uint32_t i = 0; i < vec1->dimension; i++) {
		result_vector->components[i] = vec1->components[i] - vec2->components[i];
	}

	return new_ok_result(vm, OBJECT_VAL(result_vector));
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

	ObjectVector *result_vector = new_vector(vm, vec->dimension);

	for (uint32_t i = 0; i < vec->dimension; i++) {
		result_vector->components[i] = vec->components[i] * scalar;
	}

	return new_ok_result(vm, OBJECT_VAL(result_vector));
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

	ObjectVector *result_vector = new_vector(vm, vec->dimension);

	for (uint32_t i = 0; i < vec->dimension; i++) {
		result_vector->components[i] = vec->components[i] / scalar;
	}

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

	ObjectVector *result_vector = new_vector(vm, vec->dimension);

	for (uint32_t i = 0; i < vec->dimension; i++) {
		result_vector->components[i] = vec->components[i] / magnitude;
	}

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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Vectors must have the same "
					  "dimension for distance calculation.",
					  TYPE);
	}

	double sum = 0.0;
	for (uint32_t i = 0; i < vec1->dimension; i++) {
		const double diff = vec1->components[i] - vec2->components[i];
		sum += diff * diff;
	}

	return new_ok_result(vm, FLOAT_VAL(sqrt(sum)));
}

ObjectResult *vector_cross_method(VM *vm, const int arg_count,
				  const Value *args)
{
	(void)arg_count;
	ObjectModuleRecord *module_record = vm->current_module_record;

	if (!IS_CRUX_VECTOR(args[0]) || !IS_CRUX_VECTOR(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "cross method can only be used on Vector objects.",
			TYPE);
	}

	const ObjectVector *vec1 = AS_CRUX_VECTOR(args[0]);
	const ObjectVector *vec2 = AS_CRUX_VECTOR(args[1]);

	if (vec1->dimension != 3 || vec2->dimension != 3) {
		return MAKE_GC_SAFE_ERROR(
			vm, "cross product is only defined for 3D vectors.",
			TYPE);
	}

	ObjectVector *tmp = new_vector(vm, 3);

	double components[3];
	components[0] = vec1->components[1] * vec2->components[2] -
			vec1->components[2] * vec2->components[1];
	components[1] = vec1->components[2] * vec2->components[0] -
			vec1->components[0] * vec2->components[2];
	components[2] = vec1->components[0] * vec2->components[1] -
			vec1->components[1] * vec2->components[0];

	tmp->components[0] = components[0];
	tmp->components[1] = components[1];
	tmp->components[2] = components[2];

	push(module_record, OBJECT_VAL(tmp));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(tmp));
	pop(module_record);
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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Vectors must have the same dimension.", TYPE);
	}

	double dot = 0.0;
	for (uint32_t i = 0; i < vec1->dimension; i++) {
		dot += vec1->components[i] * vec2->components[i];
	}

	const double mag1 = vector_magnitude(vec1);
	const double mag2 = vector_magnitude(vec2);

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

	if (vec1->dimension != vec2->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Vectors must have the same dimension for lerp.",
			TYPE);
	}

	ObjectVector* result_vector = new_vector(vm, vec1->dimension);

	for (uint32_t i = 0; i < vec1->dimension; i++) {
		result_vector->components[i] = vec1->components[i] +
				t * (vec2->components[i] - vec1->components[i]);
	}
	push(vm->current_module_record, OBJECT_VAL(result_vector));
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

	if (incident->dimension != normal->dimension) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"Vectors must have the same dimension for reflection.",
			TYPE);
	}

	const double normalMag = vector_magnitude(normal);
	if (fabs(normalMag) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot reflect with zero normal vector.", MATH);
	}

	// Calculate dot product: incident · normalized_normal
	double dot = 0.0;
	for (uint32_t i = 0; i < incident->dimension; i++) {
		dot += incident->components[i] *
		       (normal->components[i] / normalMag);
	}

	ObjectVector *result_vector = new_vector(vm, incident->dimension);

	for (uint32_t i = 0; i < incident->dimension; i++) {
		result_vector->components[i] = incident->components[i] -
				2.0 * dot * (normal->components[i] / normalMag);
	}

	push(vm->current_module_record, OBJECT_VAL(result_vector));
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

	if (vec1->dimension != vec2->dimension) {
		return new_ok_result(vm, BOOL_VAL(false));
	}

	bool equal = true;
	for (uint32_t i = 0; i < vec1->dimension; i++) {
		if (fabs(vec1->components[i] - vec2->components[i]) >=
		    EPSILON) {
			equal = false;
			break;
		}
	}

	return new_ok_result(vm, BOOL_VAL(equal));
}

Value vector_x_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimension >= 1) {
		return FLOAT_VAL(vector->components[0]);
	}
	return NIL_VAL;
}

Value vector_y_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const	ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimension >= 2) {
		return FLOAT_VAL(vector->components[1]);
	}
	return NIL_VAL;
}

Value vector_z_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimension >= 3) {
		return FLOAT_VAL(vector->components[2]);
	}
	return NIL_VAL;
}

Value vector_w_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	if (vector->dimension >= 4) {
		return FLOAT_VAL(vector->components[3]);
	}
	return NIL_VAL;
}

Value vector_dimension_method(VM *vm, const int arg_count, const Value *args)
{
	(void)vm;
	(void)arg_count;
	const ObjectVector *vector = AS_CRUX_VECTOR(args[0]);
	return INT_VAL(vector->dimension);
}
