#include "type_system.h"
#include "object.h"
#include "value.h"

TypeMask get_type_mask(Value value)
{
	if (IS_BOOL(value))
		return BOOL_TYPE;
	if (IS_INT(value))
		return INT_TYPE;
	if (IS_FLOAT(value))
		return FLOAT_TYPE;
	if (IS_NIL(value))
		return NIL_TYPE;

	if (IS_CRUX_OBJECT(value)) {
		switch (OBJECT_TYPE(value)) {
		case OBJECT_STRING:
			return STRING_TYPE;
		case OBJECT_ARRAY:
			return ARRAY_TYPE;
		case OBJECT_TABLE:
			return TABLE_TYPE;
		case OBJECT_ERROR:
			return ERROR_TYPE;
		case OBJECT_RESULT:
			return RESULT_TYPE;
		case OBJECT_RANDOM:
			return RANDOM_TYPE;
		case OBJECT_FILE:
			return FILE_TYPE;
		case OBJECT_STRUCT:
		case OBJECT_STRUCT_INSTANCE:
			return STRUCT_TYPE;
		case OBJECT_VECTOR:
			return VECTOR_TYPE;
		case OBJECT_COMPLEX:
			return COMPLEX_TYPE;
		case OBJECT_MATRIX:
			return MATRIX_TYPE;
		case OBJECT_MODULE_RECORD:
			return MODULE_TYPE;
		case OBJECT_UPVALUE: {
			ObjectUpvalue *upvalue = AS_CRUX_UPVALUE(value);
			return get_type_mask(upvalue->closed);
		}
		case OBJECT_CLOSURE:
		case OBJECT_NATIVE_CALLABLE:
		case OBJECT_FUNCTION:
			return FUNCTION_TYPE;
		case OBJECT_SET:
			return SET_TYPE;
		case OBJECT_TUPLE:
			return TUPLE_TYPE;
		case OBJECT_BUFFER:
			return BUFFER_TYPE;
		case OBJECT_RANGE:
			return RANGE_TYPE;
		default:
			return ANY_TYPE;
		}
	}
	return ANY_TYPE;
}

void type_mask_name(TypeMask mask, char *buf, int buf_size)
{
	if (mask == ANY_TYPE) {
		snprintf(buf, buf_size, "Any");
		return;
	}

	static const struct {
		TypeMask bit;
		const char *name;
	} entries[] = {{NIL_TYPE, "Nil"},	  {BOOL_TYPE, "Bool"},
		       {INT_TYPE, "Int"},	  {FLOAT_TYPE, "Float"},
		       {STRING_TYPE, "String"},	  {ARRAY_TYPE, "Array"},
		       {TABLE_TYPE, "Table"},	  {FUNCTION_TYPE, "Function"},
		       {ERROR_TYPE, "Error"},	  {RESULT_TYPE, "Result"},
		       {FILE_TYPE, "File"},	  {VECTOR_TYPE, "Vector"},
		       {COMPLEX_TYPE, "Complex"}, {MATRIX_TYPE, "Matrix"},
		       {STRUCT_TYPE, "Struct"},	  {MODULE_TYPE, "Module"},
		       {SET_TYPE, "Set"},	  {TUPLE_TYPE, "Tuple"},
		       {BUFFER_TYPE, "Buffer"},	  {RANGE_TYPE, "Range"}};

	int offset = 0;
	bool first = true;
	for (int i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++) {
		if (mask & entries[i].bit) {
			if (!first)
				offset += snprintf(buf + offset,
						   buf_size - offset, " | ");
			offset += snprintf(buf + offset, buf_size - offset,
					   "%s", entries[i].name);
			first = false;
		}
	}
}

bool runtime_types_compatible(TypeMask expected, Value actual)
{
	if (expected == ANY_TYPE)
		return true;
	TypeMask actual_mask = get_type_mask(actual);
	return (expected & actual_mask) != 0;
}
