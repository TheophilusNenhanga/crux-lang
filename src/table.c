#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void init_table(Table *table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void free_table(VM *vm, Table *table)
{
	FREE_ARRAY(vm, Entry, table->entries, table->capacity);
	init_table(table);
}

static bool compare_strings(const ObjectString *a, const ObjectString *b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->chars, b->chars, a->length) == 0;
}

static Entry *find_entry(Entry *entries, const int capacity,
			const ObjectString *key)
{
	uint32_t index = key->hash & (capacity - 1);
	Entry *tombstone = NULL;
	for (;;) {
		Entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL)
				tombstone = entry;
		} else if (entry->key == key ||
			   compare_strings(entry->key, key)) {
			return entry;
		}

		// We have collided, start probing
		index = (index + 1) & (capacity - 1);
	}
}

static void adjust_capacity(VM *vm, Table *table, const int capacity)
{
	Entry *entries = ALLOCATE(vm, Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		const Entry *entry = &table->entries[i];
		if (entry->key == NULL)
			continue;

		Entry *dest = find_entry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	FREE_ARRAY(vm, Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool table_set(VM *vm, Table *table, ObjectString *key, const Value value)
{
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		const int capacity = GROW_CAPACITY(table->capacity);
		adjust_capacity(vm, table, capacity);
	}

	Entry *entry = find_entry(table->entries, table->capacity, key);
	const bool isNewKey = entry->key == NULL;

	if (isNewKey && IS_NIL(entry->value)) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool table_delete(const Table *table, const ObjectString *key)
{
	if (table->count == 0)
		return false;

	Entry *entry = find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;

	// place a tombstone in the entry
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

bool table_get(const Table *table, const ObjectString *key, Value *value)
{
	if (table->count == 0)
		return false;

	const Entry *entry = find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;
	*value = entry->value;
	return true;
}

void table_add_all(VM *vm, const Table *from, Table *to)
{
	for (int i = 0; i < from->capacity; i++) {
		const Entry *entry = &from->entries[i];
		if (entry->key != NULL) {
			table_set(vm, to, entry->key, entry->value);
		}
	}
}

ObjectString *table_find_string(const Table *table, const char *chars,
			      const uint64_t length, const uint32_t hash)
{
	if (table->count == 0)
		return NULL;

	uint32_t index = hash & (table->capacity - 1);
	for (;;) {
		const Entry *entry = &table->entries[index];
		if (entry->key == NULL) {
			// Stop if we find an empty non tombstone entry
			if (IS_NIL(entry->value))
				return NULL;
		} else if (entry->key->length == length &&
			   entry->key->hash == hash &&
			   memcmp(entry->key->chars, chars, length) == 0) {
			// we found it
			return entry->key;
		}
		index = (index + 1) & (table->capacity - 1);
	}
}

void mark_table(VM *vm, const Table *table)
{
	for (int i = 0; i < table->capacity; i++) {
		const Entry *entry = &table->entries[i];
		mark_object(vm, (Object *)entry->key);
		mark_value(vm, entry->value);
	}
}

void table_remove_white(const Table *table)
{
	for (int i = 0; i < table->capacity; i++) {
		const Entry *entry = &table->entries[i];
		if (entry->key != NULL && !entry->key->Object.is_marked) {
			table_delete(table, entry->key);
		}
	}
}
