#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(VM *vm, Table *table) {
  FREE_ARRAY(vm, Entry, table->entries, table->capacity);
  initTable(table);
}

static bool compareStrings(const ObjectString *a, const ObjectString *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->chars, b->chars, a->length) == 0;
}

static Entry *findEntry(Entry *entries, const int capacity,
                        const ObjectString *key) {
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
    } else if (entry->key == key || compareStrings(entry->key, key)) {
      return entry;
    }

    // We have collided, start probing
    index = (index + 1) & (capacity - 1);
  }
}

static void adjustCapacity(VM *vm, Table *table, const int capacity) {
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

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }
  FREE_ARRAY(vm, Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(VM *vm, Table *table, ObjectString *key, const Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    const int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(vm, table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  const bool isNewKey = entry->key == NULL;

  if (isNewKey && IS_NIL(entry->value)) {
    table->count++;
  }

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(const Table *table, const ObjectString *key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  // place a tombstone in the entry
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

bool tableGet(const Table *table, const ObjectString *key, Value *value) {
  if (table->count == 0)
    return false;

  const Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;
  *value = entry->value;
  return true;
}

void tableAddAll(VM *vm, const Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    const Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(vm, to, entry->key, entry->value);
    }
  }
}

ObjectString *tableFindString(const Table *table, const char *chars,
                              const uint64_t length, const uint32_t hash) {
  if (table->count == 0)
    return NULL;

  uint32_t index = hash & (table->capacity - 1);
  for (;;) {
    const Entry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non tombstone entry
      if (IS_NIL(entry->value))
        return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash &&
               memcmp(entry->key->chars, chars, length) == 0) {
      // we found it
      return entry->key;
    }
    index = (index + 1) & (table->capacity - 1);
  }
}

void markTable(VM *vm, const Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    const Entry *entry = &table->entries[i];
    markObject(vm, (Object *)entry->key);
    markValue(vm, entry->value);
  }
}

void tableRemoveWhite(const Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    const Entry *entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->Object.isMarked) {
      tableDelete(table, entry->key);
    }
  }
}
