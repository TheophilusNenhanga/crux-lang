#include "fs.h"

ObjectResult *listDirFunction(VM *vm, int argCount __attribute__((unused)),
			      const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *isFileFunction(VM *vm, int argCount __attribute__((unused)),
			     const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *isDirFunction(VM *vm, int argCount __attribute__((unused)),
			    const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *makeDirFunction(VM *vm, int argCount __attribute__((unused)),
			      const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *deleteDirFunction(VM *vm, int argCount __attribute__((unused)),
				const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *pathExistsFunction(VM *vm, int argCount __attribute__((unused)),
				 const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *renameFunction(VM *vm, int argCount __attribute__((unused)),
			     const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *copyFileFunction(VM *vm, int argCount __attribute__((unused)),
			       const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}

ObjectResult *isFileInFunction(VM *vm, int argCount __attribute__((unused)),
			       const Value *args __attribute__((unused)))
{
	return newOkResult(vm, NIL_VAL);
}
