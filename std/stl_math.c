#include "stl_math.h"
#include <math.h>

NativeReturn power(VM *vm, int argCount, Value *args) {
	if (argCount != 2) {
		return makeNativeReturn(vm, 0);
	}

	if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
		return makeNativeReturn(vm, 0);
	}

	double base = AS_NUMBER(args[0]);
	double exponent = AS_NUMBER(args[1]);

	NativeReturn result = makeNativeReturn(vm, 1);
	result.values[0] = NUMBER_VAL(pow(base, exponent));
	return result;
}