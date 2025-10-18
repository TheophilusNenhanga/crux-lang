#ifndef VECTORS_H
#define VECTORS_H

#include "../value.h"
#include "std.h"

ObjectResult *new_vector_function(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_dot_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_add_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_subtract_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_multiply_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_divide_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_magnitude_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_normalize_method(VM *vm, int arg_count, const Value *args);

ObjectResult *vector_distance_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vector_cross_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vector_angle_between_method(VM *vm, int arg_count,
					const Value *args);
ObjectResult *vector_lerp_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vector_reflect_method(VM *vm, int arg_count, const Value *args);
ObjectResult *vector_equals_method(VM *vm, int arg_count, const Value *args);

Value vector_x_method(VM *vm, int arg_count, const Value *args);
Value vector_y_method(VM *vm, int arg_count, const Value *args);
Value vector_z_method(VM *vm, int arg_count, const Value *args);
Value vector_w_method(VM *vm, int arg_count, const Value *args);
Value vector_dimension_method(VM *vm, int arg_count, const Value *args);

#endif // VECTORS_H
