#include <time.h>

#include "stl_time.h"

#include "../memory.h"


NativeReturn currentTimeMillis(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(1);
	nativeReturn.values[0] = NUMBER_VAL((double) clock());
	return nativeReturn;
}

NativeReturn currentTimeSeconds(int argCount, Value *args) {
	NativeReturn nativeReturn = makeNativeReturn(1);
	nativeReturn.values[0] = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
	return nativeReturn;
}
