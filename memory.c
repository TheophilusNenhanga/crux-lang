//
// Created by theon on 08/09/2024.
//

#include <stdlib.h>
#include "memory.h"


void *reallocate(void *pointer, size_t oldSize, const size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize); // When oldSize is zero this is equivalent to malloc
    if (result == NULL) exit(1);
    return result;
}
