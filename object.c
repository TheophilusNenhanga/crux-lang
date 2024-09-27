#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, objectType)                                      \
  (type *)allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjectType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object;

  return object;
}

static ObjectString *
allocateString(char *chars,
               int length) { // creates a copy of the characters on the heap
                             // that the ObjectString can own
  ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjectString *copyString(const char *chars, int length) {
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] =
      '\0'; // terminating the string because it is not terminated in the source
  return allocateString(heapChars, length);
}

void printObject(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}

ObjectString *
takeString(char *chars,
           int length) { // claims ownership of the string that you give it
  return allocateString(chars, length);
}