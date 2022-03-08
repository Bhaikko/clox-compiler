#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

// MACROS defined in .h file

// Calculates new capacity based on given current capacity
// Handles both Initial Empty array and full capacity case
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Takes care of getting the size of array's element type
// and casting the resulting void* back to pointer of right type.
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*) reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

// Frees the momeory by passing in zero for the new size
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief used for all dynamic memory management in clox
 * - Allocating memory
 * - Freeing it
 * - Chaning the size of an existing allocation
 * 
 * Everything in one function for garbage collector to keep track of memory
 * @param pointer 
 * @param oldSize 
 * @param newSize 
 * @return void* 
 * 
 * oldSize      newSize     operation
 * 0            Non Zero    Allocate new Block
 * Non Zero     0           Free Allocation
 * Non Zero     < old size  shrink existing allocation
 * Non Zero     > old size  Grow existing allocation
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif