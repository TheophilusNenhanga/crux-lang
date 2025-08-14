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

ObjectResult *args_function(VM *vm, int arg_count __attribute__((unused)),
			    const Value *args __attribute__((unused)))
{
	ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
	ObjectArray *resultArray = new_array(vm, 2, currentModuleRecord);
	ObjectArray *argvArray = new_array(vm, vm->args.argc,
					  currentModuleRecord);
	push(currentModuleRecord, OBJECT_VAL(resultArray));
	push(currentModuleRecord, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char *arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			pop(currentModuleRecord);
			pop(currentModuleRecord);
			return new_error_result(
				vm, new_error(vm,
					     copy_string(vm,
							"Failed to allocate "
							"memory for argument.",
							39),
					     MEMORY, false));
		}
		ObjectString *argvString = take_string(vm, arg, strlen(arg));
		array_add_back(vm, argvArray, OBJECT_VAL(argvString));
	}

	array_add_back(vm, resultArray, INT_VAL(vm->args.argc));
	array_add_back(vm, resultArray, OBJECT_VAL(argvArray));

	pop(currentModuleRecord);
	pop(currentModuleRecord);

	return new_ok_result(vm, OBJECT_VAL(resultArray));
}

Value platform_function(VM *vm, int arg_count __attribute__((unused)),
			const Value *args __attribute__((unused)))
{
#ifdef _WIN32
	return OBJECT_VAL(copy_string(vm, "windows", 7));
#endif
#ifdef __linux__
	return OBJECT_VAL(copy_string(vm, "linux", 5));
#endif
#ifdef __APPLE__
	return OBJECT_VAL(copy_string(vm, "apple", 5));
#endif
}

Value arch_function(VM *vm, int arg_count __attribute__((unused)),
		    const Value *args __attribute__((unused)))
{
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

Value pid_function(VM *vm __attribute__((unused)),
		   int arg_count __attribute__((unused)),
		   const Value *args __attribute__((unused)))
{
#ifdef _WIN32
	return INT_VAL(GetCurrentProcessId());
#else
	return INT_VAL(getpid());
#endif
}

ObjectResult *get_env_function(VM *vm, int arg_count __attribute__((unused)),
			       const Value *args)
{
	if (!IS_CRUX_STRING(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Argument <name> must be of "
						"type 'string'.",
						41),
				     TYPE, false));
	}
	const char *value = getenv(AS_C_STRING(args[0]));
	if (value == NULL) {
		return new_error_result(
			vm,
			new_error(vm,
				 copy_string(vm,
					    "Environment variable not found.",
					    31),
				 RUNTIME, false));
	}

	char *newValue = strdup(value);
	if (newValue == NULL) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
						"Failed to allocate memory for "
						"environment variable value.",
						57),
				     MEMORY, false));
	}

	ObjectString *valueString = take_string(vm, newValue, strlen(value));
	return new_ok_result(vm, OBJECT_VAL(valueString));
}

ObjectResult *sleep_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	if (!IS_INT(args[0])) {
		return new_error_result(
			vm, new_error(vm,
				     copy_string(vm,
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
	return new_ok_result(vm, BOOL_VAL(true));
}

Value exit_function(VM *vm __attribute__((unused)),
		    int arg_count __attribute__((unused)), const Value *args)
{
	if (!IS_INT(args[0])) {
		exit(1);
	}
	exit(AS_INT(args[0]));
	__builtin_unreachable();
	return BOOL_VAL(true);
}
