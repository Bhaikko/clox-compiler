#include <stdlib.h>

#include "./../include/memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
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