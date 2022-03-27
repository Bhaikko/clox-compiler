#include <stdlib.h>

#include "./../include/memory.h"
#include "./../include/vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

// Garbage collecting memory based on Mark Sweep Garbage Collection
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("--gc begin\n");
#endif

#ifdef DEBUG_LOG_GC
    printf("--gc end\n");
#endif
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    if (newSize > oldSize) {
    #ifdef DEBUG_STRESS_GC
        collectGarbage();
    #endif
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    // realloc handles all the other 3 cases on its own
    // Also handles the dynamic array behavior of copying the existing data
    void* result = realloc(pointer, newSize);

    if (result == NULL) {
        // Allocation of memory not possible
        exit(1);
    }

    return result;
}