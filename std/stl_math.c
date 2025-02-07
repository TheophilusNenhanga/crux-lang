#include "stl_math.h"
#include <math.h>

static bool numberArgs(Value* args, int argCount) {
	for (int i = 0; i < argCount; i++) {
		if (!IS_NUMBER(args[i])) {
			return false;
		}
	}
	return true;
}

NativeReturn _pow(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Both arguments must be of type 'number'.", 40), TYPE, STELLA));
		return nativeReturn;
	}

	double base = AS_NUMBER(args[0]);
	double exponent = AS_NUMBER(args[1]);

	nativeReturn.values[0] = NUMBER_VAL(pow(base, exponent));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _sqrt(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm,
				copyString(vm, "Cannot calculate square root of a negative number.", 50),
				VALUE,
				STELLA));
		return nativeReturn;
	}

	nativeReturn.values[0] = NUMBER_VAL(sqrt(number));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _abs(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	double num = AS_NUMBER(args[0]);
	nativeReturn.values[0] = NUMBER_VAL(fabs(num));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _sin(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(sin(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _cos(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(cos(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _tan(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(tan(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _asin(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}

	double num = AS_NUMBER(args[0]);
	if (num < -1 || num > 1) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm,
				copyString(vm, "Argument must be between -1 and 1.", 34),
				VALUE,
				STELLA));
		return nativeReturn;
	}

	nativeReturn.values[0] = NUMBER_VAL(asin(num));
	nativeReturn.values[1] = NIL_VAL;

	return nativeReturn;

}

NativeReturn _acos(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	double num = AS_NUMBER(args[0]);
	if (num < -1 || num > 1) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm,
				copyString(vm, "Argument must be between -1 and 1.", 34),
				VALUE,
				STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(acos(num));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _atan(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(atan(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _exp(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(exp(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _ln(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}

	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Cannot calculate natural logarithm of non positive number.", 58), VALUE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(log(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _log10(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}

	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Cannot calculate base 10 logarithm of non positive number.", 58), VALUE, STELLA));
		return nativeReturn;
	}

	nativeReturn.values[0] = NUMBER_VAL(log10(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _ceil(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(ceil(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _floor(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(floor(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;
}

NativeReturn _round(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	if (!numberArgs(args, argCount)) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, STELLA));
		return nativeReturn;
	}
	nativeReturn.values[0] = NUMBER_VAL(round(AS_NUMBER(args[0])));
	nativeReturn.values[1] = NIL_VAL;
	return nativeReturn;

}

NativeReturn _pi(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NUMBER_VAL(3.14159265358979323846);
	return nativeReturn;
}

NativeReturn _e(VM *vm, int argCount, Value *args){
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NUMBER_VAL(2.71828182845904523536);
	return nativeReturn;
}