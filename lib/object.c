#include <stdio.h>
#include <string.h>

#include "./../include/object.h"
#include "./../include/memory.h"
#include "./../include/value.h"
#include "./../include/table.h"
#include "./../include/vm.h"

#define ALLOCATE_OBJ(type, objectType) \
        (type*)allocateObject(sizeof(type), objectType)

// Allocate Base struct state for derived types
static Obj* allocateObject(size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    // no need to maintain tail pointer this way
    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %ld for %d\n", (void*)object, size, type);
#endif

    return object;
}

// Calculates hash for a string based on FNV-1a hash function
static uint32_t hashString(const char* key, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash)
{
    // Calls Base struct function to initialize the Obj state
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    // Whenever we create a new unique string, 
    // we add it to VM string interning table
    tableSet(&vm.strings, string, NIL_VAL);
    

    return string;
}

ObjString* takeString(char* chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    // If string already exists
    // We free the duplicate string
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length)
{
    uint32_t hash = hashString(chars, length);

    // Only creating string if it does not exist in 
    // interning table of VM
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) {
        return interned;
    }

    char* heapChars = ALLOCATE(char, length + 1);
    // Not maintaing direct pointer to source code string but
    // Memory is copied to avoid bugs when this string will be freed
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction* function)
{
    if (function->name == NULL) {
        printf("<script>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;

        case OBJ_NATIVE:
            printf("<native fn>");
            break;

        default:
            break;
    }
}

ObjFunction* newFunction()
{
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(NativeFn function)
{
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}