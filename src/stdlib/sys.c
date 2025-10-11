#include "stdlib/sys.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "vm/vm_helpers.h"
#include "panic.h"

#ifndef _WIN32
#include <unistd.h>
#endif

ObjectResult *args_function(VM *vm, int arg_count __attribute__((unused)),
			    const Value *args __attribute__((unused)))
{
	ObjectModuleRecord *module_record = vm->current_module_record;
	ObjectArray *resultArray = new_array(vm, 2, module_record);
	ObjectArray *argvArray = new_array(vm, vm->args.argc, module_record);
	push(module_record, OBJECT_VAL(resultArray));
	push(module_record, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char *arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			ObjectResult *error_result = MAKE_GC_SAFE_ERROR(
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

	return new_ok_result(vm, OBJECT_VAL(resultArray));
}

Value platform_function(VM *vm, int arg_count __attribute__((unused)),
			const Value *args __attribute__((unused)))
{
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
		return MAKE_GC_SAFE_ERROR(
			vm, "Argument <name> must be of type 'string'.", TYPE);
	}
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
	return res;
}

ObjectResult *sleep_function(VM *vm, int arg_count __attribute__((unused)),
			     const Value *args)
{
	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(
			vm, "Argument <seconds> must be of type 'int'.", TYPE);
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
