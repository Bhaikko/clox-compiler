#include <stdlib.h>

#include "./../include/compiler.h"
#include "./../include/memory.h"
#include "./../include/vm.h"

#ifdef DEBUG_LOG_GC
    #include <stdio.h>
    #include "debug.h"
#endif

void markTable(Table* table)
{
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];

        // Marking key as they are heap allocated too
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}

void markObject(Obj* object)
{
    if (object == NULL) {
        return;
    }

// Adding Debugging information to the object being marked
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;
}

void markValue(Value value)
{
    // Numbers, boolean and nil require no heap allocation
    if (!IS_OBJ(value)) {
        return;
    }

    markObject(AS_OBJ(value));
}

// Marking objects which root refers to
static void markRoots()
{
    // Marking local variables and temporaries sitting in the VM'stack 
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // Marking Global variables
    markTable(&vm.globals);

    // Marking any values that compiler directly accessess
    markCompilerRoots();
}

// Garbage collecting memory based on Mark Sweep Garbage Collection
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("--gc begin\n");
#endif

    markRoots();

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