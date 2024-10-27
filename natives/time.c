#include "time.h"

#include <time.h>

Value currentTimeMillis(int argCount, Value *args) { return NUMBER_VAL((double) clock()); }

Value currentTimeSeconds(int argCount, Value *args) { return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC); }
