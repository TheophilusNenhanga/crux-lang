#ifndef VECTORS_H
#define VECTORS_H

#include "../value.h"
#include "std.h"

ObjectResult *new_vec2_function(VM *vm, int arg_count, const Value *args);
ObjectResult *new_vec3_function(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_dot_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_dot_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_add_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_add_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_subtract_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_subtract_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_multiply_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_multiply_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_divide_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_divide_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_magnitude_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_magnitude_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_normalize_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_normalize_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec2_distance_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec2_angle_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec2_rotate_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec2_lerp_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec2_reflect_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec2_equals_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vec3_distance_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_cross_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_angle_between_method(VM *vm, int arg_count,
					const Value *args);
ObjectResult *vec3_lerp_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_reflect_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vec3_equals_method(VM *vm, int arg_count, const Value *args);

Value vec2_x_method(VM *vm, int arg_count, const Value *args);
Value vec2_y_method(VM *vm, int arg_count, const Value *args);

Value vec3_x_method(VM *vm, int arg_count, const Value *args);
Value vec3_y_method(VM *vm, int arg_count, const Value *args);
Value vec3_z_method(VM *vm, int arg_count, const Value *args);

#endif // VECTORS_H
