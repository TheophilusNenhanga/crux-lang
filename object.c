#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJECT(type, objectType)                                      \
  (type *)allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjectType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object;

  return object;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash) {
  // creates a copy of the characters on the heap
  // that the ObjectString can own
  ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  tableSet(&vm.strings, string, NIL_VAL);
  return string;
}

uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i< length; i++){
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjectString *copyString(const char *chars, int length) {
	uint32_t hash = hashString(chars, length);

	ObjectString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] =
      '\0'; // terminating the string because it is not terminated in the source
  return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}

ObjectString *takeString(char *chars, int length) {
  // claims ownership of the string that you give it
  uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		// free the string that was passed to us.
		FREE_ARRAY(char, chars, length+1);
		return interned;
	}

  return allocateString(chars, length, hash);
}