#include "std.h"
#include "../memory.h"
#include "array.h"
#include "core.h"
#include "error.h"
#include "stl_io.h"
#include "stl_math.h"
#include "stl_time.h"
#include "string.h"
#include "tables.h"

Callable stringMethods[] = {
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
	{"substring", stringSubstringMethod, 3},
	{NULL, NULL, 0}
};

Callable arrayMethods[] = {
	{"pop", arrayPopMethod, 1},
	{"push", arrayPushMethod, 2},
	{"insert", arrayInsertMethod, 3},
	{"remove", arrayRemoveAtMethod, 2},
	{"concat", arrayConcatMethod, 2},
	{"slice", arraySliceMethod, 3},
	{"reverse", arrayReverseMethod, 1},
	{"index", arrayIndexOfMethod, 2},
	{NULL, NULL, 0}
};

InfallibleCallable arrayInfallibleMethods[] = {
	{"_contains", arrayContainsMethod, 2},
	{"_clear", arrayClearMethod, 1},
	{NULL, NULL, 0}
};

Callable tableMethods[] = {
	{"values", tableValuesMethod, 1}, {"keys", tableKeysMethod, 1}, {"pairs", tablePairsMethod, 1}, {NULL, NULL, 0}
};

Callable builtinCallables[] = {
	{"scanln", _scanln, 1}, {"panic", panicNative, 1}, {"len", lengthNative, 1},
	{"error", errorNative, 1}, {"assert", assertNative, 2}, {"err", _err, 1}, {"ok", _ok, 1}, {NULL, NULL, 0}
};

InfallibleCallable builtinInfallibleCallables[] = {
	{"_len", lengthNative_, 1},
	{"println", _println, 1},
	{"print", _print, 1},
	{"type", typeNative, 1},
	{NULL, NULL, 0}
};

Callable errorMethods[] = {
	{"message", errorMessageMethod, 1},
	{"type", errorTypeMethod, 1},
	{NULL, NULL, 0}
};

Callable mathFunctions[] = {
	{"pow", _pow, 2}, {"sqrt", _sqrt, 1}, {"ciel", _ceil, 1}, {"floor", _floor, 1},
	{"abs", _abs, 1}, {"sin", _sin, 1}, {"cos", _cos, 1}, {"tan", _tan, 1},
	{"atan", _atan, 1}, {"acos", _acos, 1}, {"asin", _asin, 1}, {"exp", _exp, 1},
	{"ln", _ln, 1}, {"log", _log10, 1}, {"round", _round, 1},
	{NULL, NULL, 0}
};

InfallibleCallable mathInfallibleFunctions[] = {
	{"_e", _e, 0},
	{"_pi", _pi, 0},
	{NULL, NULL, 0}
};

Callable ioFunctions[] = {
	{"print_to", _printTo, 2},
	{"scan", _scan, 0},
	{"scanln", _scanln, 0},
	{"scan_from", _scanFrom, 1},
	{"scanln_from", _scanlnFrom, 1},
	{"nscan", _nscan, 1},
	{"nscan_from", _nscanFrom, 2},
	{NULL, NULL, 0}
};

Callable timeFunctions[] = {
	{"sleep_s", _sleep_s, 1},
	{"sleep_ms", _sleep_ms, 1},
	{NULL, NULL, 0}
};

InfallibleCallable timeInfallibleFunctions[] = {
	{"_time_s", _time_s, 0}, {"_time_ms", _time_ms, 0},
	{"_year", _year, 0}, {"_month", _month, 0}, {"_day", _day, 0}, {"_hour", _hour, 0},
	{"_minute", _minute, 0}, {"_second", _second, 0}, {"_weekday", _weekday, 0}, {"_day_of_year", _day_of_year, 0},
	{NULL, NULL, 0}
};

bool defineNativeMethod(VM *vm, Table *methodTable, const char *methodName, StellaNativeCallable methodFunction,
                        int arity) {
	ObjectString *name = copyString(vm, methodName, (int) strlen(methodName));
	if (!tableSet(vm, methodTable, name, OBJECT_VAL(newNativeMethod(vm, methodFunction, arity, name)), false)) {
		return false;
	}
	return true;
}

bool defineNativeFunction(VM *vm, Table *functionTable, const char *functionName, StellaNativeCallable function,
                          int arity,
                          bool isPublic) {
	ObjectString *name = copyString(vm, functionName, (int) strlen(functionName));
	if (!tableSet(vm, functionTable, name, OBJECT_VAL(newNativeFunction(vm, function, arity, name)), isPublic)) {
		return false;
	}
	return true;
}

bool defineNativeInfallibleFunction(VM *vm, Table *functionTable, const char *functionName,
                                    StellaInfallibleCallable function, int arity,
                                    bool isPublic) {
	ObjectString *name = copyString(vm, functionName, (int) strlen(functionName));
	if (!tableSet(vm, functionTable, name, OBJECT_VAL(newNativeInfallibleFunction(vm, function, arity, name)),
	              isPublic)) {
		return false;
	}
	return true;
}

bool defineNativeInfallibleMethod(VM *vm, Table *methodTable, const char *methodName,
                                  StellaInfallibleCallable methodFunction,
                                  int arity) {
	ObjectString *name = copyString(vm, methodName, (int) strlen(methodName));
	if (!tableSet(vm, methodTable, name, OBJECT_VAL(newNativeInfallibleMethod(vm, methodFunction, arity, name)), false)) {
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

bool defineInfallibleMethods(VM *vm, Table *methodTable, InfallibleCallable *methods) {
	for (int i = 0; methods[i].name != NULL; i++) {
		InfallibleCallable method = methods[i];
		bool result = defineNativeInfallibleMethod(vm, methodTable, method.name, method.function, method.arity);
		if (!result) {
			return false;
		}
	}
	return true;
}

bool defineNativeFunctions(VM *vm, Table *callableTable) {
	for (int i = 0; builtinCallables[i].name != NULL; i++) {
		Callable function = builtinCallables[i];
		bool result = defineNativeFunction(vm, callableTable, function.name, function.function, function.arity, false);
		if (!result) {
			return false;
		}
	}
	return true;
}

bool defineNativeInfallibleFunctions(VM *vm, Table *callableTable, InfallibleCallable *infallibles) {
	for (int i = 0; infallibles[i].name != NULL; i++) {
		InfallibleCallable function = infallibles[i];
		bool result = defineNativeInfallibleFunction(vm, callableTable, function.name, function.function, function.arity,
		                                             false);
		if (!result) {
			return false;
		}
	}
	return true;
}

bool initNativeModule(VM *vm, Callable *globals, InfallibleCallable *infallibleCallables, char *moduleName) {
	Table *names = ALLOCATE(vm, Table, 1);
	initTable(names);

	for (int i = 0; globals[i].name != NULL; i++) {
		bool result = defineNativeFunction(vm, names, globals[i].name, globals[i].function, globals[i].arity, true);
		if (!result) {
			return false;
		}
	}

	if (infallibleCallables) {
		for (int i = 0; infallibleCallables[i].name != NULL; i++) {
			bool result = defineNativeInfallibleFunction(vm, names, infallibleCallables[i].name,
			                                             infallibleCallables[i].function, infallibleCallables[i].arity, true);
			if (!result) {
				return false;
			}
		}
	}

	if (vm->nativeModules.count + 1 > vm->nativeModules.capacity) {
		GROW_ARRAY(vm, NativeModule, vm->nativeModules.modules, vm->nativeModules.capacity, vm->nativeModules.capacity * 2);
		vm->nativeModules.capacity *= 2;
	}

	vm->nativeModules.modules[vm->nativeModules.count] = (NativeModule){.name = moduleName, .names = names};
	vm->nativeModules.count++;

	return true;
}

bool defineStandardLibrary(VM *vm) {
	bool r1 = initNativeModule(vm, mathFunctions, mathInfallibleFunctions, "math");
	if (!r1) {
		return false;
	}
	bool r2 = initNativeModule(vm, ioFunctions,NULL, "io");
	if (!r2) {
		return false;
	}
	bool r3 = initNativeModule(vm, timeFunctions, timeInfallibleFunctions, "time");
	if (!r3) {
		return false;
	}
	return true;
}
