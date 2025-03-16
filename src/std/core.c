#include "core.h"

#include "../memory.h"
#include "../object.h"

static Value getLength(Value value) {
	if (IS_STL_ARRAY(value)) {
		return NUMBER_VAL(AS_STL_ARRAY(value)->size);
	}
	if (IS_STL_STRING(value)) {
		return NUMBER_VAL(AS_STL_STRING(value)->length);
	}
	if (IS_STL_TABLE(value)) {
		return NUMBER_VAL(AS_STL_TABLE(value)->size);
	}
	return NIL_VAL;
}

ObjectResult* lengthNative(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	Value length = getLength(value);
	if (IS_NIL(length)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Expected either a collection type ('string', 'array', 'table').", 46), TYPE, false));
	}
	return stellaOk(vm, length);
}

Value lengthNative_(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	return getLength(value);
}

/**
 * Infallible function that returns a string representing the value's type
 * 
 * @param vm The virtual machine
 * @param argCount Number of arguments
 * @param args Array of arguments
 * @return A string value representing the type
 */
Value typeNative(VM *vm, int argCount, Value *args) {
    Value value = args[0];
    
    if (IS_NUMBER(value)) {
        return OBJECT_VAL(copyString(vm, "number", 6));
    }
    
    if (IS_BOOL(value)) {
        return OBJECT_VAL(copyString(vm, "boolean", 7));
    }
    
    if (IS_NIL(value)) {
        return OBJECT_VAL(copyString(vm, "nil", 3));
    }
    
    if (IS_STL_STRING(value)) {
        return OBJECT_VAL(copyString(vm, "string", 6));
    }
    
    if (IS_STL_ARRAY(value)) {
        return OBJECT_VAL(copyString(vm, "array", 5));
    }
    
    if (IS_STL_TABLE(value)) {
        return OBJECT_VAL(copyString(vm, "table", 5));
    }
    
    if (IS_STL_FUNCTION(value) || IS_STL_CLOSURE(value) || 
        IS_STL_NATIVE_FUNCTION(value) || IS_STL_NATIVE_INFALLIBLE_FUNCTION(value)) {
        return OBJECT_VAL(copyString(vm, "function", 8));
    }
    
    if (IS_STL_NATIVE_METHOD(value) || IS_STL_NATIVE_INFALLIBLE_METHOD(value) || 
        IS_STL_BOUND_METHOD(value)) {
        return OBJECT_VAL(copyString(vm, "method", 6));
    }
    
    if (IS_STL_CLASS(value)) {
        return OBJECT_VAL(copyString(vm, "class", 5));
    }
    
    if (IS_STL_INSTANCE(value)) {
        return OBJECT_VAL(copyString(vm, "instance", 8));
    }
    
    if (IS_STL_ERROR(value)) {
        return OBJECT_VAL(copyString(vm, "error", 5));
    }
    
    if (IS_STL_RESULT(value)) {
        return OBJECT_VAL(copyString(vm, "result", 6));
    }
    
    return OBJECT_VAL(copyString(vm, "unknown", 7));
}
