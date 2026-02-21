#ifndef CRUX_LANG_TYPE_SYSTEM_H
#define CRUX_LANG_TYPE_SYSTEM_H

#include "value.h"
#include "object.h"

bool runtime_types_compatible(TypeMask expected, Value actual);
void type_mask_name(TypeMask mask, char *buf, int buf_size);
TypeMask get_type_mask(Value value);

#endif // CRUX_LANG_TYPE_SYSTEM_H
