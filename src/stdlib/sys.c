#include "stdlib/sys.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "panic.h"

#ifndef _WIN32
#include <unistd.h>
#endif

/**
 * Returns the command line arguments as an array
 * TODO:CHNANGE TYPE TO {} (no arguments)
 * Returns Result<Array>
 */
Value args_function(VM *vm, const Value *args)
{
	(void)args;
	ObjectModuleRecord *module_record = vm->current_module_record;
	ObjectArray *resultArray = new_array(vm, 2);
	ObjectArray *argvArray = new_array(vm, vm->args.argc);
	push(module_record, OBJECT_VAL(resultArray));
	push(module_record, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char *arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			Value error_result = MAKE_GC_SAFE_ERROR(
				vm, "Failed to allocate memory for argument.",
				MEMORY);
			pop(module_record);
			pop(module_record);
			return error_result;
		}
		ObjectString *argv_string = take_string(vm, arg, strlen(arg));
		push(module_record, OBJECT_VAL(argv_string));
		array_add_back(vm, argvArray, OBJECT_VAL(argv_string));
		pop(module_record);
	}

	array_add_back(vm, resultArray, INT_VAL(vm->args.argc));
	array_add_back(vm, resultArray, OBJECT_VAL(argvArray));

	pop(module_record);
	pop(module_record);

	return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(resultArray)));
}

/**
 * Returns the current operating system platform
 * TODO:CHNAGE TYPE TO {} (no arguments)
 * Returns String
 */
Value platform_function(VM *vm, const Value *args)
{
	(void)args;
#ifdef _WIN32
	return OBJECT_VAL(copy_string(vm, "windows", 7));
#elif __linux__
	return OBJECT_VAL(copy_string(vm, "linux", 5));
#elif __APPLE__
	return OBJECT_VAL(copy_string(vm, "apple", 5));
#else
	return OBJECT_VAL(copy_string(vm, "unknown", 7));
#endif
}

/**
 * Returns the CPU architecture
 * TODO:CHNAGE TYPE TO {} (no arguments)
 * Returns String
 */
Value arch_function(VM *vm, const Value *args)
{
	(void)args;
#if defined(__x86_64__) || defined(_M_X64)
	return OBJECT_VAL(copy_string(vm, "x86_64", 6));
#elif defined(__i386) || defined(_M_IX86)
	return OBJECT_VAL(copy_string(vm, "x86", 3));
#elif defined(__aarch64__) || defined(_M_ARM64)
	return OBJECT_VAL(copy_string(vm, "arm64", 5));
#elif defined(__arm__) || defined(_M_ARM)
	return OBJECT_VAL(copy_string(vm, "arm", 3));
#elif defined(__powerpc64__) || defined(__ppc64__)
	return OBJECT_VAL(copy_string(vm, "ppc64", 5));
#elif defined(__powerpc__) || defined(__ppc__)
	return OBJECT_VAL(copy_string(vm, "ppc", 3));
#elif defined(__riscv)
#if __riscv_xlen == 64
	return OBJECT_VAL(copy_string(vm, "riscv64", 7));
#else
	return OBJECT_VAL(copy_string(vm, "riscv", 5));
#endif
#elif defined(__s390x__)
	return OBJECT_VAL(copy_string(vm, "s390x", 5));
#elif defined(__mips__)
#if defined(_MIPS_SIM_ABI64)
	return OBJECT_VAL(copy_string(vm, "mips64", 6));
#else
	return OBJECT_VAL(copy_string(vm, "mips", 4));
#endif
#else
	return OBJECT_VAL(copy_string(vm, "unknown", 7));
#endif
}

/**
 * Returns the current process ID
 * TODO:CHNAGE TYPE TO {} (no arguments)
 * Returns Int
 */
Value pid_function(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;
#ifdef _WIN32
	return INT_VAL(GetCurrentProcessId());
#else
	return INT_VAL(getpid());
#endif
}

/**
 * Gets the value of an environment variable
 * arg0 -> name: String
 * Returns Result<String>
 */
Value get_env_function(VM *vm, const Value *args)
{
	const char *value = getenv(AS_C_STRING(args[0]));
	if (value == NULL) {
		return MAKE_GC_SAFE_ERROR(vm, "Environment variable not found.",
					  RUNTIME);
	}

	char *newValue = strdup(value);
	if (newValue == NULL) {
		return MAKE_GC_SAFE_ERROR(vm,
					  "Failed to allocate memory for "
					  "environment variable value.",
					  MEMORY);
	}

	ObjectString *valueString = take_string(vm, newValue, strlen(value));
	push(vm->current_module_record, OBJECT_VAL(valueString));
	ObjectResult *res = new_ok_result(vm, OBJECT_VAL(valueString));
	pop(vm->current_module_record);
	return OBJECT_VAL(res);
}

/**
 * Pauses execution for the specified number of seconds
 * arg0 -> seconds: Int
 * Returns Nil
 */
Value sleep_function(VM *vm, const Value *args)
{
#ifdef _WIN32
	Sleep(AS_INT(args[0]));
#else
	sleep(AS_INT(args[0]));
#endif
	return NIL_TYPE;
}

/**
 * Exits the program with the given exit code
 * arg0 -> code: Int
 * Returns (never returns)
 */
Value exit_function(VM *vm, const Value *args)
{
	(void)vm;

	/* TODO: exit the vm gracefully */
	if (!IS_INT(args[0])) {
		exit(1);
	}
	exit(AS_INT(args[0]));
	return BOOL_VAL(true);
}
