#include "stdlib/fs.h"

ObjectResult *list_dir_function(VM *vm, int arg_count __attribute__((unused)),
				const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_file_function(VM *vm, int arg_count __attribute__((unused)),
			       const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_dir_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *make_dir_function(VM *vm, int arg_count __attribute__((unused)),
				const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *delete_dir_function(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *path_exists_function(VM *vm,
				   int arg_count __attribute__((unused)),
				   const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *rename_function(VM *vm, int arg_count __attribute__((unused)),
			      const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *copy_file_function(VM *vm, int arg_count __attribute__((unused)),
				 const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}

ObjectResult *is_file_in_function(VM *vm, int arg_count __attribute__((unused)),
				  const Value *args __attribute__((unused)))
{
	return new_ok_result(vm, NIL_VAL);
}
