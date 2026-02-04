#ifndef CRUX_LANG_TYPE_CHECKER_H
#define CRUX_LANG_TYPE_CHECKER_H

typedef enum
{
	NIL_TYPE,
	ANY_TYPE,
	INT_TYPE,
	FLOAT_TYPE,
	STRING_TYPE,
	ARRAY_TYPE,
	TABLE_TYPE,
	FUNCTION_TYPE,
	VECTOR_TYPE,
	COMPLEX_TYPE,
} BaseType;

typedef struct TypeRecord TypeRecord;

struct TypeRecord
{
	BaseType base_type;
	union
	{
		TypeRecord * array_of;
		struct
		{
			TypeRecord* key;
			TypeRecord* value;
		} table_of;
		struct
		{
			TypeRecord** args;
			TypeRecord* result;
		};
	} as;
};

#endif // CRUX_LANG_TYPE_CHECKER_H
