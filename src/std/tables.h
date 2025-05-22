#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

ObjectResult *tableValuesMethod(VM *vm, int argCount, Value *args);
ObjectResult *tableKeysMethod(VM *vm, int argCount, Value *args);
ObjectResult *tablePairsMethod(VM *vm, int argCount, Value *args);
ObjectResult* tableRemoveMethod(VM *vm, int argCount, Value *args);
ObjectResult* tableGetMethod(VM *vm, int argCount, Value *args);

Value tableHasKeyMethod(VM *vm, int argCount, Value *args);
Value tableGetOrElseMethod(VM *vm, int argCount, Value *args);

#endif // TABLES_H
