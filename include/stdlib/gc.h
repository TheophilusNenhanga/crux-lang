#ifndef CRUX_GC_H
#define CRUX_GC_H

#include "object.h"

Value gc_off_function(VM *vm, const Value *args);
Value gc_on_function(VM *vm, const Value *args);
Value gc_set_heap_growth_function(VM *vm, const Value *args);
Value gc_set_min_heap_function(VM *vm, const Value *args);
Value gc_set_min_growth_function(VM *vm, const Value *args);
Value gc_collect_function(VM *vm, const Value *args);
Value gc_heap_used_function(VM *vm, const Value *args);
Value gc_heap_capacity_function(VM *vm, const Value *args);
Value gc_is_on_function(VM *vm, const Value *args);
Value gc_stats_function(VM *vm, const Value *args);

#endif
