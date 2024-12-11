#include "types.h"

#include "string.h"

Method stringMethods[] = {
	{"first", stringFirstMethod, 1},
	{"last", stringLastMethod, 1},
	{"get", stringGetMethod, 2},
	{"upper", stringUpperMethod, 1},
	{"lower", stringLowerMethod, 1},
	{"strip", stringStripMethod, 1},
	{"starts_with", stringStartsWithMethod, 2},
	{"ends_with", stringEndsWithMethod, 2},
	{"contains", stringContainsMethod, 2},
};

bool defineNativeMethod(Table* methodTable, const char * methodName, NativeFn methodFunction, int arity) {
	ObjectString* name = copyString(methodName, (int)strlen(methodName));
	if (!tableSet(methodTable, name, OBJECT_VAL(newNative(methodFunction, arity)))) {
		return false;
	}
	return true;
}

bool defineStringMethods(Table* methodTable) {
	int size = sizeof(stringMethods) / sizeof(stringMethods[0]);
	for (int i = 0; i < size; i++) {
		Method method = stringMethods[i];
		bool result = defineNativeMethod(methodTable, method.name, method.function, method.arity);
		if (!result) {
			return false;
		}
	}
	return true;
}