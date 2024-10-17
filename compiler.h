#ifndef COMPILER_H
#define COMPILER_H
#include "object.h"

ObjectFunction *compile(const char *source);
void markCompilerRoots();

#endif // COMPILER_H
