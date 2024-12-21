#ifndef TABLES_H
#define TABLES_H

#include "../object.h"

NativeReturn tableValuesMethod(int argCount, Value *args);
NativeReturn tableKeysMethod(int argCount, Value *args);
NativeReturn tablePairsMethod(int argCount, Value *args);

#endif //TABLES_H
