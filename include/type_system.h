#ifndef CRUX_LANG_TYPE_SYSTEM_H
#define CRUX_LANG_TYPE_SYSTEM_H

#include "value.h"
#include "object.h"

bool runtime_types_compatible(ValueType expected, Value actual);
const char *type_name(ValueType value_type);
const char *value_type_name(Value value);

#endif // CRUX_LANG_TYPE_SYSTEM_H
