#include <time.h>

#include "stl_time.h"

#include "../memory.h"


NativeReturn currentTimeMillis(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NUMBER_VAL((double) clock());
	return nativeReturn;
}

NativeReturn currentTimeSeconds(VM *vm, int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
	return nativeReturn;
}
