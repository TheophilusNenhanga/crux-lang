#include "vectors.h"
#include <math.h>
#include <stdlib.h>

#include "../object.h"
#include "../panic.h"

#define EPSILON 1e-10

ObjectResult *new_vec2_function(VM *vm,
				const int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if ((!IS_INT(args[0]) && !IS_FLOAT(args[0])) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Parameters must be of type 'int' | 'float'.",
			TYPE);
	}

	const double x = IS_INT(args[0]) ? (double)AS_INT(args[0])
					 : AS_FLOAT(args[0]);
	const double y = IS_INT(args[1]) ? (double)AS_INT(args[1])
					 : AS_FLOAT(args[1]);

	ObjectVec2 *vec2 = new_vec2_unsafe(x, y);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(vec2));

	vec2 = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)vec2,
						      sizeof(ObjectVec2));
	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));
	return res;
}

ObjectResult *new_vec3_function(VM *vm,
				const int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if ((!IS_INT(args[0]) && !IS_FLOAT(args[0])) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1])) ||
	    (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Parameters must be of type 'int' | 'float'.",
			TYPE);
	}

	const double x = IS_INT(args[0]) ? (double)AS_INT(args[0])
					 : AS_FLOAT(args[0]);
	const double y = IS_INT(args[1]) ? (double)AS_INT(args[1])
					 : AS_FLOAT(args[1]);
	const double z = IS_INT(args[2]) ? (double)AS_INT(args[2])
					 : AS_FLOAT(args[2]);

	ObjectVec3 *vec3 = new_vec3_unsafe(x, y, z);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(vec3));

	vec3 = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)vec3,
						      sizeof(ObjectVec3));
	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));
	return res;
}

ObjectResult *vec2_dot_method(VM *vm,
			      const int arg_count __attribute__((unused)),
			      const Value *args)
{
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "dot method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	const double result = vec1->x * vec2->x + vec1->y * vec2->y;
	ObjectResult *res = new_ok_result(vm, FLOAT_VAL(result));
	return res;
}

ObjectResult *vec3_dot_method(VM *vm,
			      const int arg_count __attribute__((unused)),
			      const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "dot method can only be used on Vec3 objects.",
			TYPE);
	}
	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	const double result = vec1->x * vec2->x + vec1->y * vec2->y +
			      vec1->z * vec2->z;
	ObjectResult *res = new_ok_result(vm, FLOAT_VAL(result));
	return res;
}

ObjectResult *vec2_add_method(VM *vm,
			      const int arg_count __attribute__((unused)),
			      const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "add method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	ObjectVec2 *tmp = new_vec2_unsafe(vec1->x + vec2->x, vec1->y + vec2->y);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));
	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec3_add_method(VM *vm,
			      const int arg_count __attribute__((unused)),
			      const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "add method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	ObjectVec3 *tmp = new_vec3_unsafe(vec1->x + vec2->x, vec1->y + vec2->y,
					  vec1->z + vec2->z);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec2_subtract_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "subtract method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	ObjectVec2 *tmp = new_vec2_unsafe(vec1->x - vec2->x, vec1->y - vec2->y);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec3_subtract_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "subtract method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	ObjectVec3 *tmp = new_vec3_unsafe(vec1->x - vec2->x, vec1->y - vec2->y,
					  vec1->z - vec2->z);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec2_multiply_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "multiply method can only be used on "
					  "Vec2 objects and numbers.",
					  TYPE);
	}

	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
	const double scalar = IS_INT(args[1]) ? (double)AS_INT(args[1])
					      : AS_FLOAT(args[1]);

	ObjectVec2 *tmp = new_vec2_unsafe(vec->x * scalar, vec->y * scalar);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec3_multiply_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0]) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "multiply method can only be used on "
					  "Vec3 objects and numbers.",
					  TYPE);
	}

	const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);
	const double scalar = IS_INT(args[1]) ? (double)AS_INT(args[1])
					      : AS_FLOAT(args[1]);

	ObjectVec3 *tmp = new_vec3_unsafe(vec->x * scalar, vec->y * scalar,
					  vec->z * scalar);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec2_divide_method(VM *vm,
				 const int arg_count __attribute__((unused)),
				 const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "divide method can only be used on "
					  "Vec2 objects and numbers.",
					  TYPE);
	}

	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
	const double scalar = IS_INT(args[1]) ? (double)AS_INT(args[1])
					      : AS_FLOAT(args[1]);

	if (fabs(scalar) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot divide by zero.", MATH);
	}

	ObjectVec2 *tmp = new_vec2_unsafe(vec->x / scalar, vec->y / scalar);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec3_divide_method(VM *vm,
				 const int arg_count __attribute__((unused)),
				 const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0]) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "divide method can only be used on "
					  "Vec3 objects and numbers.",
					  TYPE);
	}

	const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);
	const double scalar = IS_INT(args[1]) ? (double)AS_INT(args[1])
					      : AS_FLOAT(args[1]);

	if (fabs(scalar) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot divide by zero.", MATH);
	}

	ObjectVec3 *tmp = new_vec3_unsafe(vec->x / scalar, vec->y / scalar,
					  vec->z / scalar);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec2_magnitude_method(VM *vm,
				    const int arg_count __attribute__((unused)),
				    const Value *args)
{
	if (!IS_CRUX_VEC2(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"magnitude method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
	const double result = sqrt(vec->x * vec->x + vec->y * vec->y);

	return new_ok_result(vm, FLOAT_VAL(result));
}

ObjectResult *vec3_magnitude_method(VM *vm,
				    const int arg_count __attribute__((unused)),
				    const Value *args)
{
	if (!IS_CRUX_VEC3(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"magnitude method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);
	const double result = sqrt(vec->x * vec->x + vec->y * vec->y +
				   vec->z * vec->z);

	return new_ok_result(vm, FLOAT_VAL(result));
}

ObjectResult *vec2_normalize_method(VM *vm,
				    const int arg_count __attribute__((unused)),
				    const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"normalize method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);

	const double magnitude = sqrt(vec->x * vec->x + vec->y * vec->y);

	if (fabs(magnitude) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot normalize a zero vector.",
					  MATH);
	}

	ObjectVec2 *tmp = new_vec2_unsafe(vec->x / magnitude,
					  vec->y / magnitude);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

ObjectResult *vec3_normalize_method(VM *vm,
				    const int arg_count __attribute__((unused)),
				    const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"normalize method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);

	const double magnitude = sqrt(vec->x * vec->x + vec->y * vec->y +
				      vec->z * vec->z);

	if (fabs(magnitude) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(vm, "Cannot normalize a zero vector.",
					  MATH);
	}

	ObjectVec3 *tmp = new_vec3_unsafe(vec->x / magnitude,
					  vec->y / magnitude,
					  vec->z / magnitude);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec2_distance_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "distance method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	const double dx = vec1->x - vec2->x;
	const double dy = vec1->y - vec2->y;
	const double result = sqrt(dx * dx + dy * dy);

	return new_ok_result(vm, FLOAT_VAL(result));
}

// arg_count: 1
ObjectResult *vec2_angle_method(VM *vm,
				const int arg_count __attribute__((unused)),
				const Value *args)
{
	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
	const double result = atan2(vec->y, vec->x);
	return new_ok_result(vm, FLOAT_VAL(result));
}

// arg_count: 2
ObjectResult *vec2_angle_between_method(VM *vm,
					const int arg_count
					__attribute__((unused)),
					const Value *args)
{
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"angleBetween method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	const double dot = vec1->x * vec2->x + vec1->y * vec2->y;
	const double mag1 = sqrt(vec1->x * vec1->x + vec1->y * vec1->y);
	const double mag2 = sqrt(vec2->x * vec2->x + vec2->y * vec2->y);

	if (fabs(mag1) < EPSILON || fabs(mag2) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot calculate angle with zero vector.", MATH);
	}

	const double cosTheta = dot / (mag1 * mag2);
	// Clamping to [-1, 1] to avoid numerical errors with acos
	const double clampedCos = fmax(-1.0, fmin(1.0, cosTheta));
	const double result = acos(clampedCos);

	return new_ok_result(vm, FLOAT_VAL(result));
}

// arg_count: 2
ObjectResult *vec2_rotate_method(VM *vm,
				 const int arg_count __attribute__((unused)),
				 const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) ||
	    (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "rotate method can only be used on "
					  "Vec2 objects with number.",
					  TYPE);
	}

	const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
	const double angle = IS_INT(args[1]) ? (double)AS_INT(args[1])
					     : AS_FLOAT(args[1]);

	const double cosA = cos(angle);
	const double sinA = sin(angle);

	const double newX = vec->x * cosA - vec->y * sinA;
	const double newY = vec->x * sinA + vec->y * cosA;

	ObjectVec2 *tmp = new_vec2_unsafe(newX, newY);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 3
ObjectResult *vec2_lerp_method(VM *vm,
			       const int arg_count __attribute__((unused)),
			       const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1]) ||
	    (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"lerp method requires two Vec2 objects and a number.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);
	const double t = IS_INT(args[2]) ? (double)AS_INT(args[2])
					 : AS_FLOAT(args[2]);

	const double newX = vec1->x + t * (vec2->x - vec1->x);
	const double newY = vec1->y + t * (vec2->y - vec1->y);

	ObjectVec2 *tmp = new_vec2_unsafe(newX, newY);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec2_reflect_method(VM *vm,
				  const int arg_count __attribute__((unused)),
				  const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "reflect method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *incident = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *normal = AS_CRUX_VEC2(args[1]);

	// Normalize the normal vector
	const double normalMag = sqrt(normal->x * normal->x +
				      normal->y * normal->y);
	if (fabs(normalMag) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot reflect with zero normal vector.", MATH);
	}

	const double nx = normal->x / normalMag;
	const double ny = normal->y / normalMag;

	// Calculate reflection: incident - 2 * (incident · normal) * normal
	const double dot = incident->x * nx + incident->y * ny;
	const double newX = incident->x - 2.0 * dot * nx;
	const double newY = incident->y - 2.0 * dot * ny;

	ObjectVec2 *tmp = new_vec2_unsafe(newX, newY);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec2 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec2));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec3_distance_method(VM *vm,
				   const int arg_count __attribute__((unused)),
				   const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "distance method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	const double dx = vec1->x - vec2->x;
	const double dy = vec1->y - vec2->y;
	const double dz = vec1->z - vec2->z;
	const double result = sqrt(dx * dx + dy * dy + dz * dz);

	return new_ok_result(vm, FLOAT_VAL(result));
}

// arg_count: 2
ObjectResult *vec3_cross_method(VM *vm,
				const int arg_count __attribute__((unused)),
				const Value *args)
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "cross method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	const double newX = vec1->y * vec2->z - vec1->z * vec2->y;
	const double newY = vec1->z * vec2->x - vec1->x * vec2->z;
	const double newZ = vec1->x * vec2->y - vec1->y * vec2->x;

	ObjectVec3 *tmp = new_vec3_unsafe(newX, newY, newZ);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec3_angle_between_method(VM *vm,
					const int arg_count
					__attribute__((unused)),
					const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"angleBetween method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

	const double dot = vec1->x * vec2->x + vec1->y * vec2->y +
			   vec1->z * vec2->z;
	const double mag1 = sqrt(vec1->x * vec1->x + vec1->y * vec1->y +
				 vec1->z * vec1->z);
	const double mag2 = sqrt(vec2->x * vec2->x + vec2->y * vec2->y +
				 vec2->z * vec2->z);

	if (fabs(mag1) < EPSILON || fabs(mag2) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot calculate angle with zero vector.", MATH);
	}

	const double cosTheta = dot / (mag1 * mag2);
	// Clamp to [-1, 1] to avoid numerical errors with acos
	const double clampedCos = fmax(-1.0, fmin(1.0, cosTheta));
	const double result = acos(clampedCos);

	return new_ok_result(vm, FLOAT_VAL(result));
}

// arg_count: 3
ObjectResult *vec3_lerp_method(VM *vm,
			       const int arg_count __attribute__((unused)),
			       const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1]) ||
	    (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
		return MAKE_GC_SAFE_ERROR(
			vm,
			"lerp method requires two Vec3 objects and a number.",
			TYPE);
	}

	ObjectModuleRecord *module_record = vm->current_module_record;

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);
	const double t = IS_INT(args[2]) ? (double)AS_INT(args[2])
					 : AS_FLOAT(args[2]);

	const double newX = vec1->x + t * (vec2->x - vec1->x);
	const double newY = vec1->y + t * (vec2->y - vec1->y);
	const double newZ = vec1->z + t * (vec2->z - vec1->z);

	ObjectVec3 *tmp = new_vec3_unsafe(newX, newY, newZ);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec3_reflect_method(VM *vm,
				  const int arg_count __attribute__((unused)),
				  const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "reflect method can only be used on Vec3 objects.",
			TYPE);
	}

	ObjectModuleRecord *module_record = vm->current_module_record;

	const ObjectVec3 *incident = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *normal = AS_CRUX_VEC3(args[1]);

	// Normalize the normal vector
	const double normalMag = sqrt(normal->x * normal->x +
				      normal->y * normal->y +
				      normal->z * normal->z);
	if (fabs(normalMag) < EPSILON) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Cannot reflect with zero normal vector.", MATH);
	}

	const double nx = normal->x / normalMag;
	const double ny = normal->y / normalMag;
	const double nz = normal->z / normalMag;

	// Calculate reflection: incident - 2 * (incident · normal) * normal
	const double dot = incident->x * nx + incident->y * ny +
			   incident->z * nz;
	const double newX = incident->x - 2.0 * dot * nx;
	const double newY = incident->y - 2.0 * dot * ny;
	const double newZ = incident->z - 2.0 * dot * nz;

	ObjectVec3 *tmp = new_vec3_unsafe(newX, newY, newZ);
	ObjectResult *res = new_ok_result_unsafe(OBJECT_VAL(tmp));

	tmp = (ObjectVec3 *)vm_take_object_ownership(vm, (Object *)tmp,
						     sizeof(ObjectVec3));

	res = (ObjectResult *)vm_take_object_ownership(vm, (Object *)res,
						       sizeof(ObjectResult));

	return res;
}

// arg_count: 2
ObjectResult *vec2_equals_method(VM *vm,
				 const int arg_count __attribute__((unused)),
				 const Value *args)
{
	if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "equals method can only be used on Vec2 objects.",
			TYPE);
	}

	const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
	const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

	const bool equal = (fabs(vec1->x - vec2->x) < EPSILON) &&
			   (fabs(vec1->y - vec2->y) < EPSILON);

	return new_ok_result(vm, BOOL_VAL(equal));
}

// arg_count: 2
ObjectResult *vec3_equals_method(VM *vm,
				 const int arg_count __attribute__((unused)),
				 const Value *args)
{
	if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "equals method can only be used on Vec3 objects.",
			TYPE);
	}

	const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
	const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);
	const bool equal = (fabs(vec1->x - vec2->x) < EPSILON) &&
			   (fabs(vec1->y - vec2->y) < EPSILON) &&
			   (fabs(vec1->z - vec2->z) < EPSILON);

	return new_ok_result(vm, BOOL_VAL(equal));
}

Value vec2_x_method(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	return FLOAT_VAL(AS_CRUX_VEC2(args[0])->x);
}

Value vec2_y_method(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	return FLOAT_VAL(AS_CRUX_VEC2(args[0])->y);
}

Value vec3_x_method(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	return FLOAT_VAL(AS_CRUX_VEC3(args[0])->x);
}

Value vec3_y_method(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	return FLOAT_VAL(AS_CRUX_VEC3(args[0])->y);
}

Value vec3_z_method(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	return FLOAT_VAL(AS_CRUX_VEC3(args[0])->z);
}
