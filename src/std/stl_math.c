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

ObjectResult* _pow(VM *vm, int argCount, Value *args) {

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Both arguments must be of type 'number'.", 40), TYPE, false));
	}

	double base = AS_NUMBER(args[0]);
	double exponent = AS_NUMBER(args[1]);

	return stellaOk(vm, NUMBER_VAL(pow(base, exponent)));
}

ObjectResult* _sqrt(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm,
				copyString(vm, "Cannot calculate square root of a negative number.", 50),
				VALUE,
				false));
	}

	return stellaOk(vm, NUMBER_VAL(sqrt(number)));

}

ObjectResult* _abs(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	double num = AS_NUMBER(args[0]);
	return stellaOk(vm, NUMBER_VAL(fabs(num)));

}

ObjectResult* _sin(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(sin(AS_NUMBER(args[0]))));

}

ObjectResult* _cos(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(cos(AS_NUMBER(args[0]))));

}

ObjectResult* _tan(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(tan(AS_NUMBER(args[0]))));

}

ObjectResult* _asin(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double num = AS_NUMBER(args[0]);
	if (num < -1 || num > 1) {
		return stellaErr(vm, newError(vm,
				copyString(vm, "Argument must be between -1 and 1.", 34),
				VALUE,
				false));
	}

	return stellaOk(vm, NUMBER_VAL(asin(num)));
}

ObjectResult* _acos(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	double num = AS_NUMBER(args[0]);
	if (num < -1 || num > 1) {
		return stellaErr(vm, newError(vm,
				copyString(vm, "Argument must be between -1 and 1.", 34),
				VALUE,
				false));
	}
	return stellaOk(vm, NUMBER_VAL(acos(num)));

}

ObjectResult* _atan(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(atan(AS_NUMBER(args[0]))));

}

ObjectResult* _exp(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(exp(AS_NUMBER(args[0]))));
}

ObjectResult* _ln(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot calculate natural logarithm of non positive number.", 58), VALUE, false));
	}
	return stellaOk(vm, NUMBER_VAL(log(AS_NUMBER(args[0]))));

}

ObjectResult* _log10(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double number = AS_NUMBER(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot calculate base 10 logarithm of non positive number.", 58), VALUE, false));
	}

	return stellaOk(vm, NUMBER_VAL(log10(AS_NUMBER(args[0]))));

}

ObjectResult* _ceil(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(ceil(AS_NUMBER(args[0]))));

}

ObjectResult* _floor(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(floor(AS_NUMBER(args[0]))));
}

ObjectResult* _round(VM *vm, int argCount, Value *args){

	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, NUMBER_VAL(round(AS_NUMBER(args[0]))));

}

ObjectResult* _pi(VM *vm, int argCount, Value *args){
	return stellaOk(vm, NUMBER_VAL(3.14159265358979323846));
}

ObjectResult* _e(VM *vm, int argCount, Value *args){
	return stellaOk(vm, NUMBER_VAL(2.71828182845904523536));
}