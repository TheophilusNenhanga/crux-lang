#include "string.h"

#include <stdlib.h>


NativeReturn stringFirstMethod(int argCount, Value *args) {
	Value value =  args[0];
	ObjectString* string = AS_STRING(value);
	NativeReturn returnValue =  makeNativeReturn(2);

	if (string->length == 0) {
		returnValue.values[0] = NIL_VAL;
		returnValue.values[1] = OBJECT_VAL(newError("String must have at least one character to get the first character.", VALUE, STELLA));
		return returnValue;
	}

	returnValue.values[0] = OBJECT_VAL(copyString(&string->chars[0], 1));
	returnValue.values[1] = NIL_VAL;
	return returnValue;
}