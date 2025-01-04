#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "value.h"

typedef struct VM VM;

typedef struct {
	ObjectString *key;
	Value value;
	bool isPublic;
} Entry;

typedef struct {
	int count;
	int capacity;
	Entry *entries;
} Table;

void initTable(Table *table);

void freeTable(VM *vm, Table *table);

bool tableSet(VM *vm, Table *table, ObjectString *key, Value value, bool isPublic);

bool tableGet(Table *table, ObjectString *key, Value *value);

bool tablePublicGet(Table *table, ObjectString *key, Value *value);

bool tableDelete(Table *table, ObjectString *key);

void tableAddAll(VM *vm, Table *from, Table *to);

ObjectString *tableFindString(Table *table, const char *chars, uint64_t length, uint32_t hash);

void tableRemoveWhite(Table *table);

void markTable(VM *vm, Table *table);

bool tableDeepCopy(VM *vm, Table *from, Table *to, ObjectString* key);

#endif
