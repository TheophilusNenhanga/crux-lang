#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "value.h"

typedef VM VM;

typedef struct {
  ObjectString *key;
  Value value;
} Entry;

typedef struct {
  Entry *entries;
  int count;
  int capacity;
} Table;

/**
 * Initializes a new, empty hash table.
 *
 * @param table Pointer to the Table structure to initialize.
 */
void initTable(Table *table);

/**
 * Frees all memory allocated for a hash table.
 *
 * @param vm Pointer to the virtual machine.
 * @param table Pointer to the Table structure to free.
 */
void freeTable(VM *vm, Table *table);

/**
 * Inserts or updates a key-value pair in the table.
 *
 * @param vm Pointer to the virtual machine.
 * @param table Pointer to the table to modify.
 * @param key String key to insert or update.
 * @param value Value to associate with the key.
 * @return true if a new key was added or an existing key was changed from nil,
 *         false otherwise.
 */
bool tableSet(VM *vm, Table *table, ObjectString *key, Value value);

/**
 * Retrieves a value associated with a key from the table.
 *
 * @param table Pointer to the table to search.
 * @param key String key to look up.
 * @param value Pointer to store the retrieved value.
 * @return true if the key was found, false otherwise.
 */
bool tableGet(const Table *table, const ObjectString *key, Value *value);

/**
 * Removes a key-value pair from the table.
 *
 * @param table Pointer to the table to modify.
 * @param key String key to remove.
 * @return true if the key was found and removed, false otherwise.
 */
bool tableDelete(const Table *table, const ObjectString *key);

/**
 * Copies all entries from one table to another.
 *
 * @param vm Pointer to the virtual machine.
 * @param from Source table to copy from.
 * @param to Destination table to copy to.
 */
void tableAddAll(VM *vm, const Table *from, Table *to);

/**
 * Finds a string in the table by its content and hash.
 * Used for string interning.
 *
 * @param table Pointer to the table to search.
 * @param chars Pointer to the character array to find.
 * @param length Length of the character array.
 * @param hash Hash value of the string.
 * @return Pointer to the found string object, or NULL if not found.
 */
ObjectString *tableFindString(const Table *table, const char *chars, uint64_t length,
                              uint32_t hash);

/**
 * Removes all entries with unmarked keys during garbage collection.
 *
 * @param table Pointer to the table to clean up.
 */
void tableRemoveWhite(const Table *table);

/**
 * Marks all objects in the table as reachable during garbage collection.
 *
 * @param vm Pointer to the virtual machine.
 * @param table Pointer to the table to mark.
 */
void markTable(VM *vm, const Table *table);

#endif
