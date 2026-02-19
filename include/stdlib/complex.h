#ifndef CRUX_LANG_COMPLEX_H
#define CRUX_LANG_COMPLEX_H

#include "value.h"
#include "vm.h"

Value complex_real_method(VM *vm, int arg_count, const Value *args);
Value complex_imag_method(VM *vm, int arg_count, const Value *args);
Value conjugate_complex_number_method(VM *vm, int arg_count, const Value* args);
Value magnitude_complex_number_method(VM *vm, int arg_count, const Value* args);
Value square_magnitude_complex_number_method(VM *vm, int arg_count, const Value* args);

Value new_complex_function(VM *vm, int arg_count, const Value* args);

Value add_complex_number_method(VM *vm, int arg_count, const Value* args);
Value sub_complex_number_method(VM *vm, int arg_count, const Value* args);
Value mul_complex_number_method(VM *vm, int arg_count, const Value* args);
Value div_complex_number_method(VM *vm, int arg_count, const Value* args);
Value scale_complex_number_method(VM *vm, int arg_count, const Value* args);

#endif // CRUX_LANG_COMPLEX_H
