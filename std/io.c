#include "io.h"

#include <stdio.h>

#include "../memory.h"
#include "../object.h"

void valuePrint(Value value);

void printNumber(Value value) {
	double number = AS_NUMBER(value);
	if (number == (int) number) {
		printf("%.0f", number);
	} else {
		printf("%lf", number);
	}
}


void printArray(ObjectArray *array) {
	printf("[");
	for (int i = 0; i < array->size; i++) {
		valuePrint(array->array[i]);
		if (i != array->size - 1) {
			printf(", ");
		}
	}
	printf("]");
}

void printTable(ObjectTable *table) {
	uint16_t printed = 0;
	printf("{");
	for (int i = 0; i < table->capacity; i++) {
		if (table->entries[i].isOccupied) {
			valuePrint(table->entries[i].key);
			printf(":");
			valuePrint(table->entries[i].value);
			if (printed != table->size - 1) {
				printf(", ");
			}
			printed++;
		}
	}
	printf("}");
}


void valuePrint(Value value) {
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_NUMBER(value)) {
		printNumber(value);
	} else if (IS_ARRAY(value)) {
		printArray(AS_ARRAY(value));
	} else if (IS_TABLE(value)) {
		printTable(AS_TABLE(value));
	} else if (IS_OBJECT(value)) {
		printObject(value);
	}
}

NativeReturn printNative(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}

NativeReturn printlnNative(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	printf("\n");
	NativeReturn nativeReturn = makeNativeReturn(vm, 1);
	nativeReturn.values[0] = NIL_VAL;
	return nativeReturn;
}
