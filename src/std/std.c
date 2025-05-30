#include "std.h"

#include <string.h>

#include "../memory.h"
#include "../vm/vm_helpers.h"
#include "array.h"
#include "core.h"
#include "error.h"
#include "fs.h"
#include "io.h"
#include "math.h"
#include "random.h"
#include "string.h"
#include "sys.h"
#include "tables.h"
#include "time.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

static const Callable stringMethods[] = {
    {"first", stringFirstMethod, 1},
    {"last", stringLastMethod, 1},
    {"get", stringGetMethod, 2},
    {"upper", stringUpperMethod, 1},
    {"lower", stringLowerMethod, 1},
    {"strip", stringStripMethod, 1},
    {"starts_with", stringStartsWithMethod, 2},
    {"ends_with", stringEndsWithMethod, 2},
    {"contains", stringContainsMethod, 2},
    {"replace", stringReplaceMethod, 3},
    {"split", stringSplitMethod, 2},
    {"substring", stringSubstringMethod, 3}};

static const InfallibleCallable stringInfallibleMethods[] = {
    {"_is_empty", stringIsEmptyMethod, 1},
    {"_is_alpha", stringIsAlphaMethod, 1},
    {"_is_digit", stringIsDigitMethod, 1},
    {"_is_lower", stringIsLowerMethod, 1},
    {"_is_upper", stringIsUpperMethod, 1},
    {"_is_space", stringIsSpaceMethod, 1},
    {"_is_alnum", stringIsAlNumMethod, 1}};

static const Callable arrayMethods[] = {
    {"pop", arrayPopMethod, 1},         {"push", arrayPushMethod, 2},
    {"insert", arrayInsertMethod, 3},   {"remove", arrayRemoveAtMethod, 2},
    {"concat", arrayConcatMethod, 2},   {"slice", arraySliceMethod, 3},
    {"reverse", arrayReverseMethod, 1}, {"index", arrayIndexOfMethod, 2},
    {"map", arrayMapMethod, 2},         {"filter", arrayFilterMethod, 2},
    {"reduce", arrayReduceMethod, 3},   {"sort", arraySortMethod, 1},
    {"join", arrayJoinMethod, 2}};

static const InfallibleCallable arrayInfallibleMethods[] = {
    {"_contains", arrayContainsMethod, 2},
    {"_clear", arrayClearMethod, 1},
    {"_equals", arrayEqualsMethod, 2}};

static const Callable tableMethods[] = {{"values", tableValuesMethod, 1},
                                        {"keys", tableKeysMethod, 1},
                                        {"pairs", tablePairsMethod, 1},
                                        {"remove", tableRemoveMethod, 2},
                                        {"get", tableGetMethod, 2}};

static const InfallibleCallable tableInfallibleMethods[] = {
    {"_has_key", tableHasKeyMethod, 2},
    {"_get_or_else", tableGetOrElseMethod, 3}};

static const Callable errorMethods[] = {
    {"message", errorMessageMethod, 1},
    {"type", errorTypeMethod, 1},
};

static const Callable randomMethods[] = {{"seed", randomSeedMethod, 2},
                                         {"int", randomIntMethod, 3},
                                         {"double", randomDoubleMethod, 3},
                                         {"bool", randomBoolMethod, 2},
                                         {"choice", randomChoiceMethod, 2}};

static const InfallibleCallable randomInfallibleMethods[] = {
    {"_next", randomNextMethod, 1}};

static const Callable fileMethods[] = {{"readln", readlnFileMethod, 1},
                                       {"read_all", readAllFileMethod, 1},
                                       {"write", writeFileMethod, 2},
                                       {"writeln", writelnFileMethod, 2},
                                       {"close", closeFileMethod, 1}};

static const InfallibleCallable resultInfallibleMethods[] = {
    {"_unwrap", unwrapFunction, 1}};

static const Callable coreFunctions[] = {
    {"scanln", scanlnFunction, 0}, {"panic", panicFunction, 1},
    {"len", lengthFunction, 1},    {"error", errorFunction, 1},
    {"assert", assertFunction, 2}, {"err", errorFunction, 1},
    {"ok", okFunction, 1},         {"int", intFunction, 1},
    {"float", floatFunction, 1},   {"string", stringFunction, 1},
    {"table", tableFunction, 1},   {"array", arrayFunction, 1},
};

static const InfallibleCallable coreInfallibleFunctions[] = {
    {"_len", lengthFunction_, 1},    {"println", printlnFunction, 1},
    {"_print", printFunction, 1},    {"_type", typeFunction_, 1},
    {"_int", intFunction_, 1},       {"_float", floatFunction_, 1},
    {"_string", stringFunction_, 1}, {"_table", tableFunction_, 1},
    {"_array", arrayFunction_, 1}};

static const Callable mathFunctions[] = {
    {"pow", powFunction, 2},    {"sqrt", sqrtFunction, 1},
    {"ceil", ceilFunction, 1},  {"floor", floorFunction, 1},
    {"abs", absFunction, 1},    {"sin", sinFunction, 1},
    {"cos", cosFunction, 1},    {"tan", tanFunction, 1},
    {"atan", atanFunction, 1},  {"acos", acosFunction, 1},
    {"asin", asinFunction, 1},  {"exp", expFunction, 1},
    {"ln", lnFunction, 1},      {"log", log10Function, 1},
    {"round", roundFunction, 1}};

static const InfallibleCallable mathInfallibleFunctions[] = {
    {"_e", eFunction, 0}, {"_pi", piFunction, 0}};

static const Callable ioFunctions[] = {
    {"print_to", printToFunction, 2},       {"scan", scanFunction, 0},
    {"scanln", scanlnFunction, 0},          {"scan_from", scanFromFunction, 1},
    {"scanln_from", scanlnFromFunction, 1}, {"nscan", nscanFunction, 1},
    {"nscan_from", nscanFromFunction, 2},   {"open_file", openFileFunction, 2},
};

static const Callable timeFunctions[] = {
    {"sleep_s", sleepSecondsFunction, 1},
    {"sleep_ms", sleepMillisecondsFunction, 1},
};

static const InfallibleCallable timeInfallibleFunctions[] = {
    {"_time_s", timeSecondsFunction_, 0},
    {"_time_ms", timeMillisecondsFunction_, 0},
    {"_year", yearFunction_, 0},
    {"_month", monthFunction_, 0},
    {"_day", dayFunction_, 0},
    {"_hour", hourFunction_, 0},
    {"_minute", minuteFunction_, 0},
    {"_second", secondFunction_, 0},
    {"_weekday", weekdayFunction_, 0},
    {"_day_of_year", dayOfYearFunction_, 0},
};

static const InfallibleCallable randomInfallibleFunctions[] = {
    {"Random", randomInitFunction, 0},
};

static const Callable systemFunctions[] = {
    {"args", argsFunction, 0},
    {"get_env", getEnvFunction, 1},
    {"sleep", sleepFunction, 1},
};

static const InfallibleCallable systemInfallibleFunctions[] = {
    {"_platform", platformFunction, 0},
    {"_arch", archFunction, 0},
    {"_pid", pidFunction, 0},
    {"_exit", exitFunction, 1}};

static const Callable fileSystemFunctions[] = {
    {"lisr_dir", listDirFunction, 1},   {"is_file", isFileFunction, 1},
    {"is_dir", isDirFunction, 1},       {"make_dir", makeDirFunction, 1},
    {"delete_dir", makeDirFunction, 1}, {"path_exists", pathExistsFunction, 1},
    {"rename", renameFunction, 2},      {"copy_file", copyFileFunction, 2},
    {"is_file_in", isFileInFunction, 2}};

bool registerNativeMethod(VM *vm, Table *methodTable, const char *methodName,
                          const CruxCallable methodFunction, const int arity) {
  ObjectString *name = copyString(vm, methodName, (int)strlen(methodName));
  if (!tableSet(vm, methodTable, name,
                OBJECT_VAL(newNativeMethod(vm, methodFunction, arity, name)))) {
    return false;
  }
  return true;
}

bool registerNativeInfallibleMethod(VM *vm, Table *methodTable,
                                    const char *methodName,
                                    const CruxInfallibleCallable methodFunction,
                                    const int arity) {
  ObjectString *name = copyString(vm, methodName, (int)strlen(methodName));
  if (!tableSet(vm, methodTable, name,
                OBJECT_VAL(newNativeInfallibleMethod(vm, methodFunction, arity,
                                                     name)))) {
    return false;
  }
  return true;
}

static bool registerMethods(VM *vm, Table *methodTable, const Callable *methods,
                            const int count) {
  for (int i = 0; i < count; i++) {
    const Callable method = methods[i];
    if (!registerNativeMethod(vm, methodTable, method.name, method.function,
                              method.arity)) {
      return false;
    }
  }
  return true;
}

static bool registerInfallibleMethods(VM *vm, Table *methodTable,
                                      const InfallibleCallable *methods,
                                      const int count) {
  for (int i = 0; i < count; i++) {
    const InfallibleCallable method = methods[i];
    if (!registerNativeInfallibleMethod(vm, methodTable, method.name,
                                        method.function, method.arity)) {
      return false;
    }
  }
  return true;
}

static bool registerNativeFunction(VM *vm, Table *functionTable,
                                   const char *functionName,
                                   const CruxCallable function,
                                   const int arity) {

  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  ObjectString *name = copyString(vm, functionName, (int)strlen(functionName));
  push(currentModuleRecord, OBJECT_VAL(name));
  const Value func = OBJECT_VAL(newNativeFunction(vm, function, arity, name));
  push(currentModuleRecord, func);
  const bool success = tableSet(vm, functionTable, name, func);

  pop(currentModuleRecord);
  pop(currentModuleRecord);

  if (!success) {
    return false;
  }
  return true;
}

static bool registerNativeInfallibleFunction(
    VM *vm, Table *functionTable, const char *functionName,
    const CruxInfallibleCallable function, const int arity) {
  ObjectModuleRecord *currentModuleRecord = vm->currentModuleRecord;
  ObjectString *name = copyString(vm, functionName, (int)strlen(functionName));
  push(currentModuleRecord, OBJECT_VAL(name));
  const Value func =
      OBJECT_VAL(newNativeInfallibleFunction(vm, function, arity, name));
  push(currentModuleRecord, func);

  const bool success = tableSet(vm, functionTable, name, func);
  if (!success) {
    return false;
  }
  pop(currentModuleRecord);
  pop(currentModuleRecord);

  return true;
}

static bool registerNativeFunctions(VM *vm, Table *functionTable,
                                    const Callable *functions,
                                    const int count) {
  for (int i = 0; i < count; i++) {
    const Callable function = functions[i];
    if (!registerNativeFunction(vm, functionTable, function.name,
                                function.function, function.arity)) {
      return false;
    }
  }
  return true;
}

static bool
registerNativeInfallibleFunctions(VM *vm, Table *functionTable,
                                  const InfallibleCallable *functions,
                                  const int count) {
  for (int i = 0; i < count; i++) {
    const InfallibleCallable function = functions[i];
    if (!registerNativeInfallibleFunction(vm, functionTable, function.name,
                                          function.function, function.arity)) {
      return false;
    }
  }
  return true;
}

static bool initModule(VM *vm, const char *moduleName,
                       const Callable *functions, const int functionsCount,
                       const InfallibleCallable *infallibles,
                       const int infalliblesCount) {
  Table *moduleTable = ALLOCATE(vm, Table, 1);
  if (moduleTable == NULL)
    return false;

  initTable(moduleTable);

  if (functions != NULL) {
    if (!registerNativeFunctions(vm, moduleTable, functions, functionsCount)) {
      return false;
    }
  }

  if (infallibles != NULL) {
    if (!registerNativeInfallibleFunctions(vm, moduleTable, infallibles,
                                           infalliblesCount)) {
      return false;
    }
  }

  if (vm->nativeModules.count + 1 > vm->nativeModules.capacity) {
    const int newCapacity =
        vm->nativeModules.capacity == 0 ? 8 : vm->nativeModules.capacity * 2;
    NativeModule *newModules =
        GROW_ARRAY(vm, NativeModule, vm->nativeModules.modules,
                   vm->nativeModules.capacity, newCapacity);
    vm->nativeModules.modules = newModules;
    vm->nativeModules.capacity = newCapacity;
  }

  char *nameCopy = ALLOCATE(vm, char, strlen(moduleName) + 1);
  if (nameCopy == NULL)
    return false;
  strcpy(nameCopy, moduleName);

  vm->nativeModules.modules[vm->nativeModules.count] =
      (NativeModule){.name = nameCopy, .names = moduleTable};
  vm->nativeModules.count++;

  return true;
}

static bool initTypeMethodTable(VM *vm, Table *methodTable,
                                const Callable *methods, const int methodCount,
                                const InfallibleCallable *infallibleMethods,
                                const int infallibleCount) {
  if (methods != NULL) {
    if (!registerMethods(vm, methodTable, methods, methodCount)) {
      return false;
    }
  }

  if (infallibleMethods != NULL) {
    if (!registerInfallibleMethods(vm, methodTable, infallibleMethods,
                                   infallibleCount)) {
      return false;
    }
  }

  return true;
}

bool initializeStdLib(VM *vm) {
  if (!registerNativeFunctions(vm, &vm->currentModuleRecord->globals,
                               coreFunctions, ARRAY_COUNT(coreFunctions))) {
    return false;
  }

  if (!registerNativeInfallibleFunctions(
          vm, &vm->currentModuleRecord->globals, coreInfallibleFunctions,
          ARRAY_COUNT(coreInfallibleFunctions))) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->stringType, stringMethods,
                           ARRAY_COUNT(stringMethods), stringInfallibleMethods,
                           ARRAY_COUNT(stringInfallibleMethods))) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->arrayType, arrayMethods,
                           ARRAY_COUNT(arrayMethods), arrayInfallibleMethods,
                           ARRAY_COUNT(arrayInfallibleMethods))) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->tableType, tableMethods,
                           ARRAY_COUNT(tableMethods), tableInfallibleMethods,
                           ARRAY_COUNT(tableInfallibleMethods))) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->errorType, errorMethods,
                           ARRAY_COUNT(errorMethods), NULL, 0)) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->randomType, randomMethods,
                           ARRAY_COUNT(randomMethods), randomInfallibleMethods,
                           ARRAY_COUNT(randomInfallibleMethods))) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->fileType, fileMethods,
                           ARRAY_COUNT(fileMethods), NULL, 0)) {
    return false;
  }

  if (!initTypeMethodTable(vm, &vm->resultType, NULL, 0,
                           resultInfallibleMethods,
                           ARRAY_COUNT(resultInfallibleMethods))) {
    return false;
  }

  // Initialize standard library modules
  if (!initModule(vm, "math", mathFunctions, ARRAY_COUNT(mathFunctions),
                  mathInfallibleFunctions,
                  ARRAY_COUNT(mathInfallibleFunctions))) {
    return false;
  }

  if (!initModule(vm, "io", ioFunctions, ARRAY_COUNT(ioFunctions), NULL, 0)) {
    return false;
  }

  if (!initModule(vm, "time", timeFunctions, ARRAY_COUNT(timeFunctions),
                  timeInfallibleFunctions,
                  ARRAY_COUNT(timeInfallibleFunctions))) {
    return false;
  }

  if (!initModule(vm, "random", NULL, 0, randomInfallibleFunctions,
                  ARRAY_COUNT(randomInfallibleFunctions))) {
    return false;
  }

  if (!initModule(vm, "sys", systemFunctions, ARRAY_COUNT(systemFunctions),
                  systemInfallibleFunctions,
                  ARRAY_COUNT(systemInfallibleFunctions))) {
    return false;
  }

  if (!initModule(vm, "fs", fileSystemFunctions,
                  ARRAY_COUNT(fileSystemFunctions), NULL, 0)) {
    return false;
  }

  return true;
}
