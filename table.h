#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
	ObjectString *key;
	Value value;
} Entry;

typedef struct {
	int count;
	int capacity;
	Entry *entries;
} Table;

typedef enum {
	SET_SUCCESS,
	NEW_KEY_SUCCESS,
	IMMUTABLE_OVERWRITE,
	GET_SUCCESS,
	VAR_NOT_FOUND,
	TABLE_EMPTY
} TableResponse;

void initTable(Table *table);

void freeTable(Table *table);

TableResponse tableSet(Table *table, ObjectString *key, Value value);

TableResponse tableGet(Table *table, ObjectString *key, Value *value);

bool tableDelete(Table *table, ObjectString *key);

void tableAddAll(Table *from, Table *to);

ObjectString *tableFindString(Table *table, const char *chars, int length, uint32_t hash);

void tableRemoveWhite(Table *table);

void markTable(Table *table);

bool tableCheck(Table *table, ObjectString *key);

bool isTableValueMutable(Table *table, ObjectString *key);

#endif
