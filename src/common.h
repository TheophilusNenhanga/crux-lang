#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_PRINT_CODE
// #define DEBUG_LOG_GC
// #define DEBUG_STRESS_GC
// #define DUMP_BYTECODE

#define UINT8_COUNT (UINT8_MAX + 1)
#define MAX_ARRAY_SIZE UINT16_MAX - 1
#define FRAMES_MAX 128
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT * 8) // Approx 2MB stack size
#define IMPORT_MAX FRAMES_MAX / 2
#define RUNTIME_EXIT_CODE 70
#define COMPILER_EXIT_CODE 65

#endif
