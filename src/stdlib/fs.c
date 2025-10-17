#include "stdlib/fs.h"

ObjectResult *list_dir_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_file_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_dir_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *make_dir_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *delete_dir_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *path_exists_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *rename_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *copy_file_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_file_in_function(VM *vm, int arg_count, const Value *args)
{
	(void)arg_count;
	(void)args;
	return new_ok_result(vm, NIL_VAL);
}
