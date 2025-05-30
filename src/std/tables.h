#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

ObjectResult *tableValuesMethod(VM *vm, int argCount, const Value *args);
ObjectResult *tableKeysMethod(VM *vm, int argCount,const  Value *args);
ObjectResult *tablePairsMethod(VM *vm, int argCount, const Value *args);
ObjectResult* tableRemoveMethod(VM *vm, int argCount, const Value *args);
ObjectResult* tableGetMethod(VM *vm, int argCount, const Value *args);

Value tableHasKeyMethod(VM *vm, int argCount, const Value *args);
Value tableGetOrElseMethod(VM *vm, int argCount,const Value *args);

#endif // TABLES_H
