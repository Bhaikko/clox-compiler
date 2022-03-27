// File used to include header files commonly used in other files
// To import standard data types

#ifndef clox_common_h
#define clox_common_h 

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// When defined, the vm debugging will be enabled
#define DEBUG_PRINT_CODE        // Prints Byte Code of the Chunk
// #define DEBUG_TRACE_EXECUTION   // Prints VM stack State with Current Instruction
#define DEBUG_STRESS_GC         // GC runs as often as it possibly can if This flag is defined
#define DEBUG_LOG_GC            // Logging Garbage Collector information

#define UINT8_COUNT (UINT8_MAX + 1)

#endif