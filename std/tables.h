#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

NativeReturn tableValuesMethod(VM* vm, int argCount, Value *args);
NativeReturn tableKeysMethod(VM* vm, int argCount, Value *args);
NativeReturn tablePairsMethod(VM* vm, int argCount, Value *args);

#endif //TABLES_H
