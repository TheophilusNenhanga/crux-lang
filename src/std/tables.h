#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

ObjectResult* tableValuesMethod(VM *vm, int argCount, Value *args);
ObjectResult* tableKeysMethod(VM *vm, int argCount, Value *args);
ObjectResult* tablePairsMethod(VM *vm, int argCount, Value *args);

#endif // TABLES_H
