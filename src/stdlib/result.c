#include "stdlib/result.h"

#include "object.h"

/**
 * args: [Result]
 * Unwraps a result
 * Returns Value if Ok, otherwise returns an error value
 *  -> Any | Error
 */
Value result_unwrap_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectResult *result = AS_CRUX_RESULT(args[0]);
	if (result->is_ok) {
		return result->as.value;
	}
	return OBJECT_VAL(result->as.error);
}

/**
 * args: [Result]
 * Returns true if the result is Ok
 *  -> Bool
 */
Value result_is_ok_method(VM *vm, const Value *args)
{
	(void)vm;
	return BOOL_VAL(AS_CRUX_RESULT(args[0])->is_ok);
}

/**
 * args: [Result]
 * Returns true if the result is Err
 *  -> Bool
 */
Value result_is_err_method(VM *vm, const Value *args)
{
	(void)vm;
	return BOOL_VAL(!AS_CRUX_RESULT(args[0])->is_ok);
}

/**
 * args: [Result, Any]
 * Returns the value if Ok, otherwise returns the default value
 *  -> Any
 */
Value result_unwrap_or_method(VM *vm, const Value *args)
{
	(void)vm;
	const ObjectResult *result = AS_CRUX_RESULT(args[0]);
	return result->is_ok ? result->as.value : args[1];
}
