#include <stdlib.h>

#include "./../include/compiler.h"
#include "./../include/memory.h"
#include "./../include/vm.h"

#ifdef DEBUG_LOG_GC
    #include <stdio.h>
    #include "./../include/debug.h"
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
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);

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
    printf("%p blacken ", (void*)object);
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

// Freeing all white marked objects
static void sweep()
{
    Obj* previous = NULL;
    Obj* object = vm.objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            // If object is unmarked
            // Unliknk from Linked list and Free it
            Obj* unreached = object;

            object = object->next;

            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
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

    // Every object in the heap is now either black or white
}

// Garbage collecting memory based on Mark Sweep Garbage Collection
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("--gc begin\n");
#endif

    markRoots();
    traceReferences();

    // Strings are special case since interning of strings was implemented
    // Removing Dangling pointers from table of ObjString freed by Garbage Collectors
    tableRemoveWhite(&vm.strings);

    sweep();

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