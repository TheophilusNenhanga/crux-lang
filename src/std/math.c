#include "math.h"
#include <math.h>

static bool numberArgs(const Value *args, const int argCount) {
  for (int i = 0; i < argCount; i++) {
    if (!IS_INT(args[i]) && !IS_FLOAT(args[i])) {
      return false;
    }
  }
  return true;
}

ObjectResult *powFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(vm,
                 copyString(
                     vm, "Both arguments must be of type 'int' | 'float'.", 40),
                 TYPE, false));
  }

  const double base = AS_FLOAT(args[0]);
  const double exponent = AS_FLOAT(args[1]);

  return newOkResult(vm, FLOAT_VAL(pow(base, exponent)));
}

ObjectResult *sqrtFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  const double number = AS_FLOAT(args[0]);
  if (number < 0) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(vm, "Cannot calculate square root of a negative number.",
                       50),
            VALUE, false));
  }

  return newOkResult(vm, FLOAT_VAL(sqrt(number)));
}

static int32_t absoluteValue(const int32_t x) {
  if (x < 0) {
    return -x;
  }
  return x;
}

ObjectResult *absFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  if (IS_INT(args[0])) {
    return newOkResult(vm, INT_VAL(absoluteValue(AS_INT(args[0]))));
  }
  return newOkResult(vm, FLOAT_VAL(fabs(AS_FLOAT(args[0]))));
}

ObjectResult *sinFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(sin(FLOAT_VAL(args[0]))));
}

ObjectResult *cosFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(cos(AS_FLOAT(args[0]))));
}

ObjectResult *tanFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(tan(AS_FLOAT(args[0]))));
}

ObjectResult *asinFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }

  const double num = AS_FLOAT(args[0]);
  if (num < -1 || num > 1) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Argument must be between -1 and 1.", 34),
                 VALUE, false));
  }

  return newOkResult(vm, FLOAT_VAL(asin(num)));
}

ObjectResult *acosFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  const double num = AS_FLOAT(args[0]);
  if (num < -1 || num > 1) {
    return newErrorResult(
        vm,
        newError(vm, copyString(vm, "Argument must be between -1 and 1.", 34),
                 VALUE, false));
  }
  return newOkResult(vm, FLOAT_VAL(acos(num)));
}

ObjectResult *atanFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(atan(AS_FLOAT(args[0]))));
}

ObjectResult *expFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(exp(AS_FLOAT(args[0]))));
}

ObjectResult *lnFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }

  const double number = AS_FLOAT(args[0]);
  if (number < 0) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "Cannot calculate natural logarithm of non positive number.",
                58),
            VALUE, false));
  }
  return newOkResult(vm, FLOAT_VAL(log(AS_FLOAT(args[0]))));
}

ObjectResult *log10Function(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }

  const double number = AS_FLOAT(args[0]);
  if (number < 0) {
    return newErrorResult(
        vm,
        newError(
            vm,
            copyString(
                vm,
                "Cannot calculate base 10 logarithm of non positive number.",
                58),
            VALUE, false));
  }

  return newOkResult(vm, FLOAT_VAL(log10(AS_FLOAT(args[0]))));
}

ObjectResult *ceilFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(ceil(AS_FLOAT(args[0]))));
}

ObjectResult *floorFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(floor(AS_FLOAT(args[0]))));
}

ObjectResult *roundFunction(VM *vm, const int argCount, const Value *args) {
  if (!numberArgs(args, argCount)) {
    return newErrorResult(
        vm,
        newError(
            vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 34),
            TYPE, false));
  }
  return newOkResult(vm, FLOAT_VAL(round(AS_FLOAT(args[0]))));
}

Value piFunction(VM *vm, int argCount, const Value *args) {
  return FLOAT_VAL(3.14159265358979323846);
}

Value eFunction(VM *vm, int argCount, const Value *args) {
  return FLOAT_VAL(2.71828182845904523536);
}
