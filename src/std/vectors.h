#ifndef VECTORS_H
#define VECTORS_H

#include "../value.h"
#include "std.h"

ObjectResult *newVec2Function(VM *vm, int argCount, const Value *args);
ObjectResult *newVec3Function(VM *vm, int argCount, const Value *args);

ObjectResult *vec2DotMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3DotMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2AddMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3AddMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2SubtractMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3SubtractMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2MultiplyMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3MultiplyMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2DivideMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3DivideMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2MagnitudeMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3MagnitudeMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2NormalizeMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3NormalizeMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec2DistanceMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec2AngleMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec2RotateMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec2LerpMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec2ReflectMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec2EqualsMethod(VM *vm, int argCount, const Value *args);

ObjectResult *vec3DistanceMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3CrossMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3AngleBetweenMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3LerpMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3ReflectMethod(VM *vm, int argCount, const Value *args);
ObjectResult *vec3EqualsMethod(VM *vm, int argCount, const Value *args);

Value vec2XMethod(VM *vm, int argCount, const Value *args);
Value vec2YMethod(VM *vm, int argCount, const Value *args);

Value vec3XMethod(VM *vm, int argCount, const Value *args);
Value vec3YMethod(VM *vm, int argCount, const Value *args);
Value vec3ZMethod(VM *vm, int argCount, const Value *args);

#endif // VECTORS_H
