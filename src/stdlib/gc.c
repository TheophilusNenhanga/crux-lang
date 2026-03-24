#include "stdlib/gc.h"
#include "garbage_collector.h"
#include "panic.h"
#include "value.h"

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
