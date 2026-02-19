#include "type_system.h"
#include "object.h"
#include "value.h"

const char *type_name(ValueType value_type)
{
	switch (value_type) {
	case ANY_TYPE: {
		return "Any";
	}
	case INT_TYPE: {
		return "Int";
	}
	case FLOAT_TYPE: {
		return "Float";
	}
	case NIL_TYPE: {
		return "Nil";
	}
	case BOOL_TYPE: {
		return "Bool";
	}
	case STRING_TYPE: {
		return "String";
	}
	case FUNCTION_TYPE: {
		return "Function";
	}
	case ARRAY_TYPE: {
		return "Array";
	}
	case TABLE_TYPE: {
		return "Table";
	}
	case ERROR_TYPE: {
		return "Error";
	}
	case RESULT_TYPE: {
		return "Result";
	}
	case RANDOM_TYPE: {
		return "Random";
	}
	case FILE_TYPE: {
		return "File";
	}
	case STRUCT_TYPE: {
		return "Struct";
	}
	case VECTOR_TYPE: {
		return "Vector";
	}
	case COMPLEX_TYPE: {
		return "Complex";
	}
	case MATRIX_TYPE: {
		return "Matrix";
	}
	default:
		return "unknown";
	}
}

static ValueType get_value_type(Value value)
{
	if (IS_BOOL(value)) {
		return BOOL_TYPE;
	}
	if (IS_INT(value)) {
		return INT_TYPE;
	}
	if (IS_FLOAT(value)) {
		return FLOAT_TYPE;
	}
	if (IS_CRUX_STRING(value)) {
		return STRING_TYPE;
	}
	if (IS_CRUX_FUNCTION(value) || IS_CRUX_NATIVE_FUNCTION(value) ||
	    IS_CRUX_NATIVE_METHOD(value)) {
		return FUNCTION_TYPE;
	}
	if (IS_CRUX_ARRAY(value)) {
		return ARRAY_TYPE;
	}
	if (IS_CRUX_TABLE(value)) {
		return TABLE_TYPE;
	}
	if (IS_CRUX_ERROR(value)) {
		return ERROR_TYPE;
	}
	if (IS_CRUX_RESULT(value)) {
		return RESULT_TYPE;
	}
	if (IS_CRUX_RANDOM(value)) {
		return RANDOM_TYPE;
	}
	if (IS_CRUX_FILE(value)) {
		return FILE_TYPE;
	}
	if (IS_CRUX_STRUCT(value)) {
		return STRUCT_TYPE;
	}
	if (IS_CRUX_VECTOR(value)) {
		return VECTOR_TYPE;
	}
	if (IS_CRUX_COMPLEX(value)) {
		return COMPLEX_TYPE;
	}
	if (IS_CRUX_MATRIX(value)) {
		return MATRIX_TYPE;
	}
	return ANY_TYPE;
}

const char *value_type_name(Value value)
{
	ValueType type = get_value_type(value);
	return type_name(type);
}

bool runtime_types_compatible(ValueType expected, Value actual)
{
	ValueType actual_type = get_value_type(actual);
	if (expected == ANY_TYPE) {
		return true;
	}
	if (expected == FLOAT_TYPE && actual_type == INT_TYPE) {
		return true;
	}
	return expected == actual_type;
}
