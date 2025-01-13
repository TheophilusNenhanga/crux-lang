#include "std.h"
#include "array.h"
#include "collections.h"
#include "error.h"
#include "io.h"
#include "stl_time.h"
#include "string.h"
#include "tables.h"

Callable stringMethods[] = {{"first", stringFirstMethod, 1},
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
														{"substring", stringSubstringMethod, 3},
														{NULL, NULL, 0}};

Callable arrayMethods[] = {{"pop", arrayPopMethod, 1},
													 {"push", arrayPushMethod, 2},
													 {"insert", arrayInsertMethod, 3},
													 {"remove", arrayRemoveAtMethod, 2},
													 {"concat", arrayConcatMethod, 2},
													 {"slice", arraySliceMethod, 3},
													 {"reverse", arrayReverseMethod, 1},
													 {"index", arrayIndexOfMethod, 2},
													 {"contains", arrayContainsMethod, 2},
													 {"clear", arrayClearMethod, 1},
													 {NULL, NULL, 0}};

Callable tableMethods[] = {
		{"values", tableValuesMethod, 1}, {"keys", tableKeysMethod, 1}, {"pairs", tablePairsMethod, 1}, {NULL, NULL, 0}};


Callable nativeCallables[] = {
		{"time_s", currentTimeSeconds, 0}, {"time_ms", currentTimeMillis, 0}, {"print", printNative, 1},
		{"println", printlnNative, 1},		 {"len", lengthNative, 1},					{"panic", panicNative, 1},
		{"error", errorNative, 1},				 {"assert", assertNative, 2},				{NULL, NULL, 0}};

Callable errorMethods[] = {{"message", errorMessageMethod, 1},
													 {"creator", errorCreatorMethod, 1},
													 {"type", errorTypeMethod, 1},
													 {NULL, NULL, 0}};

bool defineNativeMethod(VM *vm, Table *methodTable, const char *methodName, NativeFn methodFunction, int arity) {
	ObjectString *name = copyString(vm, methodName, (int) strlen(methodName));
	if (!tableSet(vm, methodTable, name, OBJECT_VAL(newNativeMethod(vm, methodFunction, arity, name)), false)) {
		return false;
	}
	return true;
}

bool defineNativeFunction(VM *vm, Table *functionTable, const char *functionName, NativeFn function, int arity) {
	ObjectString *name = copyString(vm, functionName, (int) strlen(functionName));
	if (!tableSet(vm, functionTable, name, OBJECT_VAL(newNativeFunction(vm, function, arity, name)), false)) {
		return false;
	}
	return true;
}

bool defineMethods(VM *vm, Table *methodTable, Callable *methods) {
	for (int i = 0; methods[i].name != NULL; i++) {
		Callable method = methods[i];
		bool result = defineNativeMethod(vm, methodTable, method.name, method.function, method.arity);
		if (!result) {
			return false;
		}
	}
	return true;
}


bool defineNativeFunctions(VM *vm, Table *callableTable) {
	for (int i = 0; nativeCallables[i].name != NULL; i++) {
		Callable function = nativeCallables[i];
		bool result = defineNativeFunction(vm, callableTable, function.name, function.function, function.arity);
		if (!result) {
			return false;
		}
	}
	return true;
}
