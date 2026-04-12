#include <string.h>

#include "stdlib/gc.h"
#include "common.h"
#include "garbage_collector.h"
#include "panic.h"
#include "value.h"

static void add_gc_stat(VM *vm, ObjectTable *table, const char *name, const Value value)
{
	ObjectString *key = copy_string(vm, name, (uint32_t)strlen(name));
	object_table_set(vm, table, OBJECT_VAL(key), value);
}

static size_t compute_next_gc_threshold(const VM *vm)
{
	const size_t growth_target = (size_t)((double)vm->bytes_allocated * vm->heap_growth_factor);
	const size_t delta_target = vm->bytes_allocated + vm->min_gc_growth_delta;

	size_t next_gc = growth_target > delta_target ? growth_target : delta_target;
	if (next_gc < vm->min_gc_heap_size) {
		next_gc = vm->min_gc_heap_size;
	}
	return next_gc;
}

/**
 * Turns the garbage collector off
 * Returns Nil
 */
Value gc_off_function(VM *vm, const Value *args)
{
	(void)args;
	vm->gc_status = PAUSED;
	return NIL_VAL;
}

/**
 * Turns the garbage collector on
 * Returns Nil
 */
Value gc_on_function(VM *vm, const Value *args)
{
	(void)args;
	vm->gc_status = RUNNING;
	return NIL_VAL;
}

/**
 * Sets the heap growth factor
 * arg0 -> growthFactor: Float | Int (Must be greater than 1.0)
 * Returns Result<Nil>
 */
Value gc_set_heap_growth_function(VM *vm, const Value *args)
{
	double growth_factor = TO_DOUBLE(args[0]);
	if (growth_factor <= 1.0) {
		return MAKE_GC_SAFE_ERROR(vm, "Growth factor must be greater than 1.0.", RUNTIME);
	}
	vm->heap_growth_factor = growth_factor;
	vm->next_gc = compute_next_gc_threshold(vm);
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

Value gc_set_min_heap_function(VM *vm, const Value *args)
{
	const double min_heap = TO_DOUBLE(args[0]);
	if (min_heap < 0.0) {
		return MAKE_GC_SAFE_ERROR(vm, "Minimum heap size must be non-negative.", RUNTIME);
	}

	vm->min_gc_heap_size = (size_t)min_heap;
	vm->next_gc = compute_next_gc_threshold(vm);
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

Value gc_set_min_growth_function(VM *vm, const Value *args)
{
	const double min_growth = TO_DOUBLE(args[0]);
	if (min_growth < 0.0) {
		return MAKE_GC_SAFE_ERROR(vm, "Minimum growth delta must be non-negative.", RUNTIME);
	}

	vm->min_gc_growth_delta = (size_t)min_growth;
	vm->next_gc = compute_next_gc_threshold(vm);
	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

/**
 * Collect garbage
 * Returns Nil
 */
Value gc_collect_function(VM *vm, const Value *args)
{
	(void)args;
	collect_garbage(vm);
	return NIL_VAL;
}

/**
 * Returns the number of bytes allocated
 * Returns Float
 */
Value gc_heap_used_function(VM *vm, const Value *args)
{
	(void)args;
	return FLOAT_VAL((double)vm->bytes_allocated);
}

/**
 * Returns the current GC heap capacity
 * Returns Float
 */
Value gc_heap_capacity_function(VM *vm, const Value *args)
{
	(void)args;
	return FLOAT_VAL((double)vm->next_gc);
}

/**
 * Returns whether the GC is currently on
 * Returns Bool
 */
Value gc_is_on_function(VM *vm, const Value *args)
{
	(void)args;
	return BOOL_VAL(vm->gc_status == RUNNING);
}

Value gc_stats_function(VM *vm, const Value *args)
{
	(void)args;

	ObjectTable *stats = new_object_table(vm, 20);
	push(vm->current_module_record, OBJECT_VAL(stats));

	add_gc_stat(vm, stats, "collections", FLOAT_VAL((double)vm->gc_collections));
	add_gc_stat(vm, stats, "total_ns", FLOAT_VAL((double)vm->gc_total_ns));
	add_gc_stat(vm, stats, "mark_roots_ns", FLOAT_VAL((double)vm->gc_mark_roots_ns));
	add_gc_stat(vm, stats, "trace_ns", FLOAT_VAL((double)vm->gc_trace_ns));
	add_gc_stat(vm, stats, "remove_white_ns", FLOAT_VAL((double)vm->gc_remove_white_ns));
	add_gc_stat(vm, stats, "sweep_ns", FLOAT_VAL((double)vm->gc_sweep_ns));
	add_gc_stat(vm, stats, "last_total_ns", FLOAT_VAL((double)vm->gc_last_total_ns));
	add_gc_stat(vm, stats, "last_mark_roots_ns", FLOAT_VAL((double)vm->gc_last_mark_roots_ns));
	add_gc_stat(vm, stats, "last_trace_ns", FLOAT_VAL((double)vm->gc_last_trace_ns));
	add_gc_stat(vm, stats, "last_remove_white_ns", FLOAT_VAL((double)vm->gc_last_remove_white_ns));
	add_gc_stat(vm, stats, "last_sweep_ns", FLOAT_VAL((double)vm->gc_last_sweep_ns));
	add_gc_stat(vm, stats, "last_gray_peak", FLOAT_VAL((double)vm->gc_last_gray_peak));
	add_gc_stat(vm, stats, "max_gray_peak", FLOAT_VAL((double)vm->gc_max_gray_peak));
	add_gc_stat(vm, stats, "last_live_objects", FLOAT_VAL((double)vm->gc_last_live_objects));
	add_gc_stat(vm, stats, "last_pool_capacity", FLOAT_VAL((double)vm->gc_last_pool_capacity));
	add_gc_stat(vm, stats, "last_bytes_before", FLOAT_VAL((double)vm->gc_last_bytes_before));
	add_gc_stat(vm, stats, "last_bytes_after", FLOAT_VAL((double)vm->gc_last_bytes_after));
	add_gc_stat(vm, stats, "last_bytes_freed", FLOAT_VAL((double)vm->gc_last_bytes_freed));
	add_gc_stat(vm, stats, "last_next_gc", FLOAT_VAL((double)vm->gc_last_next_gc));
	add_gc_stat(vm, stats, "min_heap_size", FLOAT_VAL((double)vm->min_gc_heap_size));
	add_gc_stat(vm, stats, "min_growth_delta", FLOAT_VAL((double)vm->min_gc_growth_delta));
	add_gc_stat(vm, stats, "last_objects_before_sweep", FLOAT_VAL((double)vm->gc_last_objects_before_sweep));
	add_gc_stat(vm, stats, "last_objects_after_sweep", FLOAT_VAL((double)vm->gc_last_objects_after_sweep));
	add_gc_stat(vm, stats, "last_objects_freed", FLOAT_VAL((double)vm->gc_last_objects_freed));
	add_gc_stat(vm, stats, "last_strings_count", FLOAT_VAL((double)vm->gc_last_strings_count));
	add_gc_stat(vm, stats, "last_strings_capacity", FLOAT_VAL((double)vm->gc_last_strings_capacity));
	add_gc_stat(vm, stats, "last_strings_tombstones", FLOAT_VAL((double)vm->gc_last_strings_tombstones));
	add_gc_stat(vm, stats, "last_sweep_slots_scanned", FLOAT_VAL((double)vm->gc_last_sweep_slots_scanned));
	add_gc_stat(vm, stats, "sweep_slots_scanned", FLOAT_VAL((double)vm->gc_sweep_slots_scanned));

	pop(vm->current_module_record);
	return OBJECT_VAL(stats);
}
