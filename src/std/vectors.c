#include "vectors.h"
#include "../object.h"
#include "../panic.h"
#include <math.h>

#define EPSILON 1e-10

ObjectResult *newVec2Function(VM *vm,
                              const int argCount __attribute__((unused)),
                              const Value *args) {
  if ((!IS_INT(args[0]) && !IS_FLOAT(args[0])) ||
      (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm, newError(vm,
                     copyString(
                         vm, "Parameters must be of type 'int' | 'float'.", 43),
                     TYPE, false));
  }

  const double x =
      IS_INT(args[0]) ? (double)AS_INT(args[0]) : AS_FLOAT(args[0]);
  const double y =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  return newOkResult(vm, OBJECT_VAL(newVec2(vm, x, y)));
}

ObjectResult *newVec3Function(VM *vm,
                              const int argCount __attribute__((unused)),
                              const Value *args) {
  if ((!IS_INT(args[0]) && !IS_FLOAT(args[0])) ||
      (!IS_INT(args[1]) && !IS_FLOAT(args[1])) ||
      (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
    return newErrorResult(
        vm, newError(vm,
                     copyString(
                         vm, "Parameters must be of type 'int' | 'float'.", 43),
                     TYPE, false));
  }

  const double x =
      IS_INT(args[0]) ? (double)AS_INT(args[0]) : AS_FLOAT(args[0]);
  const double y =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);
  const double z =
      IS_INT(args[2]) ? (double)AS_INT(args[2]) : AS_FLOAT(args[2]);

  return newOkResult(vm, OBJECT_VAL(newVec3(vm, x, y, z))); // still to fix this
}

ObjectResult *vec2DotMethod(VM *vm, const int argCount __attribute__((unused)),
                            const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "dot method can only be used on Vec2 objects.", 44),
            TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  const double result = vec1->x * vec2->x + vec1->y * vec2->y;

  return newOkResult(vm, FLOAT_VAL(result));
}

ObjectResult *vec3DotMethod(VM *vm, const int argCount __attribute__((unused)),
                            const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "dot method can only be used on Vec3 objects.", 44),
            TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  const double result =
      vec1->x * vec2->x + vec1->y * vec2->y + vec1->z * vec2->z;

  return newOkResult(vm, FLOAT_VAL(result));
}

ObjectResult *vec2AddMethod(VM *vm, const int argCount __attribute__((unused)),
                            const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "add method can only be used on Vec2 objects.", 44),
            TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  ObjectVec2 *result = newVec2(vm, vec1->x + vec2->x, vec1->y + vec2->y);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec3AddMethod(VM *vm, const int argCount __attribute__((unused)),
                            const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "add method can only be used on Vec3 objects.", 44),
            TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  ObjectVec3 *result =
      newVec3(vm, vec1->x + vec2->x, vec1->y + vec2->y, vec1->z + vec2->z);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec2SubtractMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm,
                            "subtract method can only be used on Vec2 objects.",
                            49),
                 TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  ObjectVec2 *result = newVec2(vm, vec1->x - vec2->x, vec1->y - vec2->y);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec3SubtractMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm,
                            "subtract method can only be used on Vec3 objects.",
                            49),
                 TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  ObjectVec3 *result =
      newVec3(vm, vec1->x - vec2->x, vec1->y - vec2->y, vec1->z - vec2->z);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec2MultiplyMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "multiply method can only be used on Vec2 objects and numbers.",
                60),
            TYPE, false));
  }

  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
  const double scalar =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  ObjectVec2 *result = newVec2(vm, vec->x * scalar, vec->y * scalar);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec3MultiplyMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {

  if (!IS_CRUX_VEC3(args[0]) || (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "multiply method can only be used on Vec3 objects and numbers.",
                60),
            TYPE, false));
  }

  const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);
  const double scalar =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  ObjectVec3 *result =
      newVec3(vm, vec->x * scalar, vec->y * scalar, vec->z * scalar);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec2DivideMethod(VM *vm,
                               const int argCount __attribute__((unused)),
                               const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "divide method can only be used on Vec2 objects and numbers.",
                58),
            TYPE, false));
  }

  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
  const double scalar =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  if (fabs(scalar) < 1e-10) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot divide by zero.", 22), MATH,
                     false));
  }

  ObjectVec2 *result = newVec2(vm, vec->x / scalar, vec->y / scalar);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec3DivideMethod(VM *vm,
                               const int argCount __attribute__((unused)),
                               const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "divide method can only be used on Vec3 objects and numbers.",
                58),
            TYPE, false));
  }

  const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);
  const double scalar =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  if (fabs(scalar) < 1e-10) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot divide by zero.", 22), MATH,
                     false));
  }

  ObjectVec3 *result =
      newVec3(vm, vec->x / scalar, vec->y / scalar, vec->z / scalar);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec2MagnitudeMethod(VM *vm,
                                  const int argCount __attribute__((unused)),
                                  const Value *args) {

  if (!IS_CRUX_VEC2(args[0])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "magnitude method can only be used on Vec2 objects.",
                       50),
            TYPE, false));
  }

  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);

  const double result = sqrt(vec->x * vec->x + vec->y * vec->y);

  return newOkResult(vm, FLOAT_VAL(result));
}

ObjectResult *vec3MagnitudeMethod(VM *vm,
                                  const int argCount __attribute__((unused)),
                                  const Value *args) {

  if (!IS_CRUX_VEC3(args[0])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "magnitude method can only be used on Vec3 objects.",
                       50),
            TYPE, false));
  }

  const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);

  const double result =
      sqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);

  return newOkResult(vm, FLOAT_VAL(result));
}

ObjectResult *vec2NormalizeMethod(VM *vm,
                                  const int argCount __attribute__((unused)),
                                  const Value *args) {

  if (!IS_CRUX_VEC2(args[0])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "normalize method can only be used on Vec2 objects.",
                       51),
            TYPE, false));
  }

  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);

  const double magnitude = sqrt(vec->x * vec->x + vec->y * vec->y);

  if (fabs(magnitude) < 1e-10) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot normalize a zero vector.", 30),
                     MATH, false));
  }

  ObjectVec2 *result = newVec2(vm, vec->x / magnitude, vec->y / magnitude);

  return newOkResult(vm, OBJECT_VAL(result));
}

ObjectResult *vec3NormalizeMethod(VM *vm,
                                  const int argCount __attribute__((unused)),
                                  const Value *args) {
  if (!IS_CRUX_VEC3(args[0])) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "normalize method can only be used on Vec3 objects.",
                       51),
            TYPE, false));
  }

  const ObjectVec3 *vec = AS_CRUX_VEC3(args[0]);

  const double magnitude =
      sqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);

  if (fabs(magnitude) < 1e-10) {
    return newErrorResult(
        vm, newError(vm, copyString(vm, "Cannot normalize a zero vector.", 30),
                     MATH, false));
  }

  ObjectVec3 *result =
      newVec3(vm, vec->x / magnitude, vec->y / magnitude, vec->z / magnitude);

  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec2DistanceMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {

  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm,
                            "distance method can only be used on Vec2 objects.",
                            49),
                 TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  const double dx = vec1->x - vec2->x;
  const double dy = vec1->y - vec2->y;
  const double result = sqrt(dx * dx + dy * dy);

  return newOkResult(vm, FLOAT_VAL(result));
}

// argCount: 1
ObjectResult *vec2AngleMethod(VM *vm,
                              const int argCount __attribute__((unused)),
                              const Value *args) {
  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
  const double result = atan2(vec->y, vec->x);
  return newOkResult(vm, FLOAT_VAL(result));
}

// argCount: 2
ObjectResult *vec2AngleBetweenMethod(VM *vm,
                                     const int argCount __attribute__((unused)),
                                     const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "angleBetween method can only be used on Vec2 objects.",
                    53),
                TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  const double dot = vec1->x * vec2->x + vec1->y * vec2->y;
  const double mag1 = sqrt(vec1->x * vec1->x + vec1->y * vec1->y);
  const double mag2 = sqrt(vec2->x * vec2->x + vec2->y * vec2->y);

  if (fabs(mag1) < 1e-10 || fabs(mag2) < 1e-10) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "Cannot calculate angle with zero vector.", 39),
                 MATH, false));
  }

  const double cosTheta = dot / (mag1 * mag2);
  // Clamp to [-1, 1] to avoid numerical errors with acos
  const double clampedCos = fmax(-1.0, fmin(1.0, cosTheta));
  const double result = acos(clampedCos);

  return newOkResult(vm, FLOAT_VAL(result));
}

// argCount: 2
ObjectResult *vec2RotateMethod(VM *vm,
                               const int argCount __attribute__((unused)),
                               const Value *args) {

  if (!IS_CRUX_VEC2(args[0]) || (!IS_INT(args[1]) && !IS_FLOAT(args[1]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "rotate method can only be used on Vec2 objects with number.",
                59),
            TYPE, false));
  }

  const ObjectVec2 *vec = AS_CRUX_VEC2(args[0]);
  const double angle =
      IS_INT(args[1]) ? (double)AS_INT(args[1]) : AS_FLOAT(args[1]);

  const double cosA = cos(angle);
  const double sinA = sin(angle);

  const double newX = vec->x * cosA - vec->y * sinA;
  const double newY = vec->x * sinA + vec->y * cosA;

  ObjectVec2 *result = newVec2(vm, newX, newY);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 3
ObjectResult *vec2LerpMethod(VM *vm, const int argCount __attribute__((unused)),
                             const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1]) ||
      (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm, "lerp method requires two Vec2 objects and a number.", 51),
            TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);
  const double t =
      IS_INT(args[2]) ? (double)AS_INT(args[2]) : AS_FLOAT(args[2]);

  const double newX = vec1->x + t * (vec2->x - vec1->x);
  const double newY = vec1->y + t * (vec2->y - vec1->y);

  ObjectVec2 *result = newVec2(vm, newX, newY);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec2ReflectMethod(VM *vm,
                                const int argCount __attribute__((unused)),
                                const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "reflect method can only be used on Vec2 objects.", 48),
                TYPE, false));
  }

  const ObjectVec2 *incident = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *normal = AS_CRUX_VEC2(args[1]);

  // Normalize the normal vector
  const double normalMag = sqrt(normal->x * normal->x + normal->y * normal->y);
  if (fabs(normalMag) < EPSILON) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "Cannot reflect with zero normal vector.", 38),
                 MATH, false));
  }

  const double nx = normal->x / normalMag;
  const double ny = normal->y / normalMag;

  // Calculate reflection: incident - 2 * (incident · normal) * normal
  const double dot = incident->x * nx + incident->y * ny;
  const double newX = incident->x - 2.0 * dot * nx;
  const double newY = incident->y - 2.0 * dot * ny;

  ObjectVec2 *result = newVec2(vm, newX, newY);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec3DistanceMethod(VM *vm,
                                 const int argCount __attribute__((unused)),
                                 const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm,
                            "distance method can only be used on Vec3 objects.",
                            49),
                 TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  const double dx = vec1->x - vec2->x;
  const double dy = vec1->y - vec2->y;
  const double dz = vec1->z - vec2->z;
  const double result = sqrt(dx * dx + dy * dy + dz * dz);

  return newOkResult(vm, FLOAT_VAL(result));
}

// argCount: 2
ObjectResult *vec3CrossMethod(VM *vm,
                              const int argCount __attribute__((unused)),
                              const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "cross method can only be used on Vec3 objects.", 46),
                 TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  const double newX = vec1->y * vec2->z - vec1->z * vec2->y;
  const double newY = vec1->z * vec2->x - vec1->x * vec2->z;
  const double newZ = vec1->x * vec2->y - vec1->y * vec2->x;

  ObjectVec3 *result = newVec3(vm, newX, newY, newZ);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec3AngleBetweenMethod(VM *vm,
                                     const int argCount __attribute__((unused)),
                                     const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "angleBetween method can only be used on Vec3 objects.",
                    53),
                TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);

  const double dot = vec1->x * vec2->x + vec1->y * vec2->y + vec1->z * vec2->z;
  const double mag1 =
      sqrt(vec1->x * vec1->x + vec1->y * vec1->y + vec1->z * vec1->z);
  const double mag2 =
      sqrt(vec2->x * vec2->x + vec2->y * vec2->y + vec2->z * vec2->z);

  if (fabs(mag1) < EPSILON || fabs(mag2) < EPSILON) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "Cannot calculate angle with zero vector.", 39),
                 MATH, false));
  }

  const double cosTheta = dot / (mag1 * mag2);
  // Clamp to [-1, 1] to avoid numerical errors with acos
  const double clampedCos = fmax(-1.0, fmin(1.0, cosTheta));
  const double result = acos(clampedCos);

  return newOkResult(vm, FLOAT_VAL(result));
}

// argCount: 3
ObjectResult *vec3LerpMethod(VM *vm, const int argCount __attribute__((unused)),
                             const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1]) ||
      (!IS_INT(args[2]) && !IS_FLOAT(args[2]))) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm, "lerp method requires two Vec3 objects and a number.", 51),
            TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);
  const double t =
      IS_INT(args[2]) ? (double)AS_INT(args[2]) : AS_FLOAT(args[2]);

  const double newX = vec1->x + t * (vec2->x - vec1->x);
  const double newY = vec1->y + t * (vec2->y - vec1->y);
  const double newZ = vec1->z + t * (vec2->z - vec1->z);

  ObjectVec3 *result = newVec3(vm, newX, newY, newZ);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec3ReflectMethod(VM *vm,
                                const int argCount __attribute__((unused)),
                                const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm, newError(
                vm,
                copyString(
                    vm, "reflect method can only be used on Vec3 objects.", 48),
                TYPE, false));
  }

  const ObjectVec3 *incident = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *normal = AS_CRUX_VEC3(args[1]);

  // Normalize the normal vector
  const double normalMag = sqrt(normal->x * normal->x + normal->y * normal->y +
                                normal->z * normal->z);
  if (fabs(normalMag) < EPSILON) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(vm, "Cannot reflect with zero normal vector.", 39),
                 MATH, false));
  }

  const double nx = normal->x / normalMag;
  const double ny = normal->y / normalMag;
  const double nz = normal->z / normalMag;

  // Calculate reflection: incident - 2 * (incident · normal) * normal
  const double dot = incident->x * nx + incident->y * ny + incident->z * nz;
  const double newX = incident->x - 2.0 * dot * nx;
  const double newY = incident->y - 2.0 * dot * ny;
  const double newZ = incident->z - 2.0 * dot * nz;

  ObjectVec3 *result = newVec3(vm, newX, newY, newZ);
  return newOkResult(vm, OBJECT_VAL(result));
}

// argCount: 2
ObjectResult *vec2EqualsMethod(VM *vm,
                               const int argCount __attribute__((unused)),
                               const Value *args) {
  if (!IS_CRUX_VEC2(args[0]) || !IS_CRUX_VEC2(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "equals method can only be used on Vec2 objects.", 47),
                 TYPE, false));
  }

  const ObjectVec2 *vec1 = AS_CRUX_VEC2(args[0]);
  const ObjectVec2 *vec2 = AS_CRUX_VEC2(args[1]);

  const bool equal = (fabs(vec1->x - vec2->x) < EPSILON) &&
                     (fabs(vec1->y - vec2->y) < EPSILON);

  return newOkResult(vm, BOOL_VAL(equal));
}

// argCount: 2
ObjectResult *vec3EqualsMethod(VM *vm,
                               const int argCount __attribute__((unused)),
                               const Value *args) {
  if (!IS_CRUX_VEC3(args[0]) || !IS_CRUX_VEC3(args[1])) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "equals method can only be used on Vec3 objects.", 47),
                 TYPE, false));
  }

  const ObjectVec3 *vec1 = AS_CRUX_VEC3(args[0]);
  const ObjectVec3 *vec2 = AS_CRUX_VEC3(args[1]);
  const bool equal = (fabs(vec1->x - vec2->x) < EPSILON) &&
                     (fabs(vec1->y - vec2->y) < EPSILON) &&
                     (fabs(vec1->z - vec2->z) < EPSILON);

  return newOkResult(vm, BOOL_VAL(equal));
}

Value vec2XMethod(VM *vm __attribute__((unused)),
                  int argCount __attribute__((unused)), const Value *args) {
  return FLOAT_VAL(AS_CRUX_VEC2(args[0])->x);
}

Value vec2YMethod(VM *vm __attribute__((unused)),
                  int argCount __attribute__((unused)), const Value *args) {
  return FLOAT_VAL(AS_CRUX_VEC2(args[0])->y);
}

Value vec3XMethod(VM *vm __attribute__((unused)),
                  int argCount __attribute__((unused)), const Value *args) {
  return FLOAT_VAL(AS_CRUX_VEC3(args[0])->x);
}

Value vec3YMethod(VM *vm __attribute__((unused)),
                  int argCount __attribute__((unused)), const Value *args) {
  return FLOAT_VAL(AS_CRUX_VEC3(args[0])->y);
}

Value vec3ZMethod(VM *vm __attribute__((unused)),
                  int argCount __attribute__((unused)), const Value *args) {
  return FLOAT_VAL(AS_CRUX_VEC3(args[0])->z);
}
