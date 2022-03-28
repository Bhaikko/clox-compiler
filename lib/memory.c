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

    // Object already processed
    if (object->isMarked) {
        return;
    }

// Adding Debugging information to the object being marked
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    // storing all reachable objects as Gray to traverse them later
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);

        if (vm.grayStack == NULL) {
            exit(1);
        }
    }

    vm.grayStack[vm.grayCount++] = object;
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

// Marking array values of an object
static void markArray(ValueArray* array)
{
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

// Processing a Gray object
static void blackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_FUNCTION:  {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }

        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

// Traversing through the references of gray object then marking them Black
static void traceReferences()
{
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];

        // Black object is any object whose isMarked field is set 
        // and is no longer in gray stack
        blackenObject(object);
    }
}

// Garbage collecting memory based on Mark Sweep Garbage Collection
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("--gc begin\n");
#endif

    markRoots();
    traceReferences();

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