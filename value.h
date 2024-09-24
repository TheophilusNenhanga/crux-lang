//
// Created by theon on 08/09/2024.
//

#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif //VALUE_H
