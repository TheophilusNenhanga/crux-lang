#include "sys.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "../panic.h"
#include "../vm/vm_helpers.h"

#ifndef _WIN32
#include <unistd.h>
#endif

ObjectResult *argsFunction(VM *vm, int argCount __attribute__((unused)),
			   const Value *args __attribute__((unused)))
{
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
	ObjectArray *resultArray = newArray(vm, 2, currentModuleRecord);
	ObjectArray *argvArray = newArray(vm, vm->args.argc,
					  currentModuleRecord);
	PUSH(currentModuleRecord, OBJECT_VAL(resultArray));
	PUSH(currentModuleRecord, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char *arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			POP(currentModuleRecord);
			POP(currentModuleRecord);
			return newErrorResult(
				vm, newError(vm,
					     copyString(vm,
							"Failed to allocate "
							"memory for argument.",
							39),
					     MEMORY, false));
		}
		ObjectString *argvString = takeString(vm, arg, strlen(arg));
		arrayAddBack(vm, argvArray, OBJECT_VAL(argvString));
	}

	arrayAddBack(vm, resultArray, INT_VAL(vm->args.argc));
	arrayAddBack(vm, resultArray, OBJECT_VAL(argvArray));

	POP(currentModuleRecord);
	POP(currentModuleRecord);

	return newOkResult(vm, OBJECT_VAL(resultArray));
}

Value platformFunction(VM *vm, int argCount __attribute__((unused)),
		       const Value *args __attribute__((unused)))
{
#ifdef _WIN32
	return OBJECT_VAL(copyString(vm, "windows", 7));
#endif
#ifdef __linux__
	return OBJECT_VAL(copyString(vm, "linux", 5));
#endif
#ifdef __APPLE__
	return OBJECT_VAL(copyString(vm, "apple", 5));
#endif
}

Value archFunction(VM *vm, int argCount __attribute__((unused)),
		   const Value *args __attribute__((unused)))
{
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

Value pidFunction(VM *vm __attribute__((unused)),
		  int argCount __attribute__((unused)),
		  const Value *args __attribute__((unused)))
{
#ifdef _WIN32
	return INT_VAL(GetCurrentProcessId());
#else
	return INT_VAL(getpid());
#endif
}

ObjectResult *getEnvFunction(VM *vm, int argCount __attribute__((unused)),
			     const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Argument <name> must be of "
						"type 'string'.",
						41),
				     TYPE, false));
	}
	const char *value = getenv(AS_C_STRING(args[0]));
	if (value == NULL) {
		return newErrorResult(
			vm,
			newError(vm,
				 copyString(vm,
					    "Environment variable not found.",
					    31),
				 RUNTIME, false));
	}

	char *newValue = strdup(value);
	if (newValue == NULL) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Failed to allocate memory for "
						"environment variable value.",
						57),
				     MEMORY, false));
	}

	ObjectString *valueString = takeString(vm, newValue, strlen(value));
	return newOkResult(vm, OBJECT_VAL(valueString));
}

ObjectResult *sleepFunction(VM *vm, int argCount __attribute__((unused)),
			    const Value *args)
{
	if (!IS_INT(args[0])) {
		return newErrorResult(
			vm, newError(vm,
				     copyString(vm,
						"Argument <seconds> must be of "
						"type 'int'.",
						41),
				     MEMORY, false));
	}

#ifdef _WIN32
	Sleep(AS_INT(args[0]));
#else
	sleep(AS_INT(args[0]));
#endif
	return newOkResult(vm, BOOL_VAL(true));
}

Value exitFunction(VM *vm __attribute__((unused)),
		   int argCount __attribute__((unused)), const Value *args)
{
	if (!IS_INT(args[0])) {
		exit(1);
	}
	exit(AS_INT(args[0]));
	__builtin_unreachable();
	return BOOL_VAL(true);
}
