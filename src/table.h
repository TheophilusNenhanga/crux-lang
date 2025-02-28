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

typedef struct {
	Object **objects;
	Object **copies;
	int count;
	int capacity;
	VM *fromVM;
	VM *toVM;
} ModuleCopyContext;

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
 * @param isPublic Flag indicating if this entry should be accessible publicly.
 * @return true if a new key was added or an existing key was changed from nil,
 *         false otherwise.
 */
bool tableSet(VM *vm, Table *table, ObjectString *key, Value value, bool isPublic);

/**
 * Retrieves a value associated with a key from the table.
 *
 * @param table Pointer to the table to search.
 * @param key String key to look up.
 * @param value Pointer to store the retrieved value.
 * @return true if the key was found, false otherwise.
 */
bool tableGet(Table *table, ObjectString *key, Value *value);

/**
 * Retrieves a public value associated with a key from the table.
 * Only returns the value if the entry is marked as public.
 *
 * @param table Pointer to the table to search.
 * @param key String key to look up.
 * @param value Pointer to store the retrieved value.
 * @return true if the key was found and the entry is public, false otherwise.
 */
bool tablePublicGet(Table *table, ObjectString *key, Value *value);

/**
 * Removes a key-value pair from the table.
 *
 * @param table Pointer to the table to modify.
 * @param key String key to remove.
 * @return true if the key was found and removed, false otherwise.
 */
bool tableDelete(Table *table, ObjectString *key);

/**
 * Copies all entries from one table to another.
 *
 * @param vm Pointer to the virtual machine.
 * @param from Source table to copy from.
 * @param to Destination table to copy to.
 */
void tableAddAll(VM *vm, Table *from, Table *to);

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
ObjectString *tableFindString(Table *table, const char *chars, uint64_t length, uint32_t hash);

/**
 * Removes all entries with unmarked keys during garbage collection.
 *
 * @param table Pointer to the table to clean up.
 */
void tableRemoveWhite(Table *table);

/**
 * Marks all objects in the table as reachable during garbage collection.
 *
 * @param vm Pointer to the virtual machine.
 * @param table Pointer to the table to mark.
 */
void markTable(VM *vm, Table *table);

/**
 * Creates a deep copy of a table entry from one VM to another.
 * Only copies entries marked as public.
 *
 * @param fromVM Source VM containing the original table.
 * @param toVM Destination VM where the copy will be stored.
 * @param fromTable Source table to copy from.
 * @param toTable Destination table to copy to.
 * @param key Key in the source table to copy.
 * @param newKey Key to use in the destination table.
 * @return true if the copy was successful, false otherwise.
 */
bool tableDeepCopy(VM *fromVM, VM* toVM, Table* fromTable, Table* toTable,  ObjectString *key, ObjectString* newKey);

#endif
