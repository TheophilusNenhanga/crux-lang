#include "stdlib/option.h"

#include "object.h"

/**
 * args: [Option]
 * Returns true if the option is Some
 *  -> Bool
 */
Value option_is_some_method(VM *vm, const Value *args)
{
	(void)vm;
	return BOOL_VAL(AS_CRUX_OPTION(args[0])->is_some);
}

/**
 * args: [Option]
 * Returns true if the option is None
 *  -> Bool
 */
Value option_is_none_method(VM *vm, const Value *args)
{
	(void)vm;
	return BOOL_VAL(!AS_CRUX_OPTION(args[0])->is_some);
}

/**
 * args: [Option]
 * Unwraps an option
 * Returns the value if Some, otherwise returns NIL
 *  -> Any | Nil
 */
Value option_unwrap_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectOption *option = AS_CRUX_OPTION(args[0]);
	return option->is_some ? option->value : NIL_VAL;
}

/**
 * args: [Option, Any]
 * Unwraps an option
 * Returns the value if Some, otherwise returns the default value
 *  -> Any
 */
Value option_unwrap_or_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectOption *option = AS_CRUX_OPTION(args[0]);
	return option->is_some ? option->value : args[1];
}
