#include "types.h"
#include "string.h"
#include "array.h"

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
	{"replace", stringReplaceMethod, 3},
	{"split", stringSplitMethod, 2},
	{"substring", stringSubstringMethod, 3}
};

Method arrayMethods[] = {
	{"pop", arrayPopMethod, 1},
	{"push", arrayPushMethod, 2},
	{"insert", arrayInsertMethod, 3}, 
	{"remove_at", arrayRemoveAtMethod, 2},
	{"concat", arrayConcatMethod, 3}, 
	{"slice", arraySliceMethod, 3}, 
	{"revserse", arrayReverseMethod, 1}, 
	{"index", arrayIndexOfMethod, 2}, 
	{"contains", arrayContainsMethod, 2}, 
	{"clear", arrayClearMethod, 1}
};

bool defineNativeMethod(Table* methodTable, const char * methodName, NativeFn methodFunction, int arity) {
	ObjectString* name = copyString(methodName, (int)strlen(methodName));
	if (!tableSet(methodTable, name, OBJECT_VAL(newNative(methodFunction, arity)))) {
		return false;
	}
	return true;
}

bool defineMethods(Table *methodTable, Method* methods) {
	int size = sizeof(*methods) / sizeof(methods[0]);
	for (int i = 0; i < size; i++) {
		Method method = methods[i];
		bool result = defineNativeMethod(methodTable, method.name, method.function, method.arity);
		if (!result) {
			return false;
		}
	}
	return true;
}