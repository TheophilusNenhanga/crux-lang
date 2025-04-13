#include "sys.h"

#include <windows.h>
#include <unistd.h>

ObjectResult *argsFunction(VM *vm, int argCount, Value *args) {
	ObjectArray *resultArray = newArray(vm, 2);
	ObjectArray *argvArray = newArray(vm, vm->args.argc);
	push(vm, OBJECT_VAL(resultArray));
	push(vm, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char *arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			pop(vm);
			pop(vm);
			return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate memory for argument.", 39), MEMORY,
			                                   false));
		}
		ObjectString *argvString = takeString(vm, arg, strlen(arg));
		arrayAddBack(vm, argvArray, OBJECT_VAL(argvString));
	}

	arrayAddBack(vm, resultArray, INT_VAL(vm->args.argc));
	arrayAddBack(vm, resultArray, OBJECT_VAL(argvArray));

	pop(vm);
	pop(vm);

	return newOkResult(vm, OBJECT_VAL(resultArray));
}

Value platformFunction(VM *vm, int argCount, Value *args) {
#ifdef _WIN32
	return OBJECT_VAL(copyString(vm, "windows", 7));
#endif
#ifdef __linux__
	return OBJECT_VAL(copyString(vm, "linux",5));
#endif
#ifdef __APPLE__
	return OBJECT_VAL(copyString(vm, "apple", 5));
#endif
}

Value archFunction(VM *vm, int argCount, Value *args) {
#if defined(__x86_64__) || defined(_M_X64)
	return OBJECT_VAL(copyString(vm, "x86_64", 6));
#elif defined(__i386) || defined(_M_IX86)
	return OBJECT_VAL(copyString(vm, "x86", 3));
#elif defined(__aarch64__) || defined(_M_ARM64)
	return OBJECT_VAL(copyString(vm, "arm64", 5));
#elif defined(__arm__) || defined(_M_ARM)
	return OBJECT_VAL(copyString(vm, "arm", 3));
#elif defined(__powerpc64__) || defined(__ppc64__)
	return OBJECT_VAL(copyString(vm, "ppc64", 5));
#elif defined(__powerpc__) || defined(__ppc__)
	return OBJECT_VAL(copyString(vm, "ppc", 3));
#elif defined(__riscv)
#if __riscv_xlen == 64
	return OBJECT_VAL(copyString(vm, "riscv64", 7));
#else
	return OBJECT_VAL(copyString(vm, "riscv", 5));
#endif
#elif defined(__s390x__)
	return OBJECT_VAL(copyString(vm, "s390x", 5));
#elif defined(__mips__)
#if defined(_MIPS_SIM_ABI64)
	return OBJECT_VAL(copyString(vm, "mips64", 6));
#else
	return OBJECT_VAL(copyString(vm, "mips", 4));
#endif
#else
	return OBJECT_VAL(copyString(vm, "unknown", 7));
#endif
}

Value pidFunction(VM *vm, int argCount, Value *args) {
#ifdef _WIN32
	return INT_VAL(_getpid());
#else
	return INT_VAL(getpid());
#endif
}

ObjectResult *getEnvFunction(VM *vm, int argCount, Value *args) {
	if (!IS_CRUX_STRING(args[0])) {
		return newErrorResult(
			vm, newError(vm, copyString(vm, "Argument <name> must be of type 'string'.", 41), TYPE, false));
	}
	const char *value = getenv(AS_C_STRING(args[0]));
	if (value == NULL) {
		return newErrorResult(vm, newError(vm, copyString(vm, "Environment variable not found.", 31), RUNTIME, false));
	}

	char *newValue = strdup(value);
	if (newValue == NULL) {
		return newErrorResult(vm, newError(
			                      vm, copyString(vm, "Failed to allocate memory for environment variable value.", 57), MEMORY,
			                      false));
	}

	ObjectString *valueString = takeString(vm, newValue, strlen(value));
	return newOkResult(vm, OBJECT_VAL(valueString));
}

ObjectResult *sleepFunction(VM *vm, int argCount, Value *args) {
	if (!IS_INT(args[0])) {
		return newErrorResult(vm, newError(vm, copyString(vm, "Argument <seconds> must be of type 'int'.", 41), MEMORY,
		                                   false));
	}

#ifdef _WIN32
	Sleep(AS_INT(args[0]));
#else
	sleep(AS_INT(args[0]));
#endif
	return newOkResult(vm, BOOL_VAL(true));
}

Value exitFunction(VM *vm, int argCount, Value *args) {
	if (!IS_INT(args[0])) {
		exit(1);
	} else {
		exit(AS_INT(args[0]));
	}
	return BOOL_VAL(true);
}
