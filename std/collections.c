#include "collections.h"

#include "../memory.h"
#include "../object.h"


NativeReturn lengthNative(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 2);

	Value value = args[0];
	if (IS_ARRAY(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_ARRAY(value)->size);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	if (IS_STRING(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_STRING(value)->length);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	if (IS_TABLE(value)) {
		nativeReturn.values[0] = NUMBER_VAL(AS_TABLE(value)->size);
		nativeReturn.values[1] = NIL_VAL;
		return nativeReturn;
	}
	ObjectError *error = newError(vm, takeString(vm, "Expected either collection type.", 32), TYPE, STELLA);
	nativeReturn.values[0] = NIL_VAL;
	nativeReturn.values[1] = OBJECT_VAL(error);
	return nativeReturn;
}
