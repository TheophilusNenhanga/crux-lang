#ifndef CRUX_STDLIB_TERM_H
#define CRUX_STDLIB_TERM_H

#include "value.h"

Value term_init_function(VM *vm, const Value *args);
Value term_exit_function(VM *vm, const Value *args);
Value term_size_function(VM *vm, const Value *args);
Value term_width_function(VM *vm, const Value *args);
Value term_height_function(VM *vm, const Value *args);
Value term_read_event_function(VM *vm, const Value *args);
Value term_read_event_timeout_function(VM *vm, const Value *args);
Value term_poll_event_function(VM *vm, const Value *args);
Value term_mouse_enable_function(VM *vm, const Value *args);
Value term_mouse_disable_function(VM *vm, const Value *args);

#endif
