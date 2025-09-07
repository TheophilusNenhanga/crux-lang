#ifndef FS_H
#define FS_H

#include "../value.h"
#include "std.h"

ObjectResult *list_dir_function(VM *vm, int arg_count, const Value *args);
ObjectResult *is_file_function(VM *vm, int arg_count, const Value *args);
ObjectResult *is_dir_function(VM *vm, int arg_count, const Value *args);
ObjectResult *make_dir_function(VM *vm, int arg_count, const Value *args);
ObjectResult *delete_dir_function(VM *vm, int arg_count, const Value *args);
ObjectResult *path_exists_function(VM *vm, int arg_count, const Value *args);
ObjectResult *rename_function(VM *vm, int arg_count, const Value *args);
ObjectResult *copy_file_function(VM *vm, int arg_count, const Value *args);
ObjectResult *is_file_in_function(VM *vm, int arg_count, const Value *args);

#endif // FS_H
