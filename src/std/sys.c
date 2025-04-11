#include "sys.h"

ObjectResult* argsFunction(VM *vm, int argCount, Value *args) {
	ObjectArray* resultArray = newArray(vm, 2);
	ObjectArray* argvArray = newArray(vm, vm->args.argc);
	push(vm, OBJECT_VAL(resultArray));
	push(vm, OBJECT_VAL(argvArray));

	for (int i = 0; i < vm->args.argc; i++) {
		char* arg = strdup(vm->args.argv[i]);
		if (arg == NULL) {
			pop(vm);
			pop(vm);
			return newErrorResult(vm, newError(vm, copyString(vm, "Failed to allocate memory for argument.", 39), MEMORY, false));
		}
		ObjectString* argvString = takeString(vm, arg, strlen(arg));
		arrayAddBack(vm, argvArray, OBJECT_VAL(argvString));
	}

	arrayAddBack(vm, resultArray, INT_VAL(vm->args.argc));
	arrayAddBack(vm, resultArray, OBJECT_VAL(argvArray));

	pop(vm);
	pop(vm);

	return newOkResult(vm, OBJECT_VAL(resultArray));
}