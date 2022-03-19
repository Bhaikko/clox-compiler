#include <stdio.h>
#include <string.h>

#include "./../include/object.h"
#include "./../include/memory.h"
#include "./../include/value.h"
#include "./../include/vm.h"

#define ALLOCATE_OBJ(type, objectType) \
        (type*)allocateObject(sizeof(type), objectType)

// Allocate Base struct state for derived types
static Obj* allocateObject(size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    // no need to maintain tail pointer this way
    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static ObjString* allocateString(char* chars, int length)
{
    // Calls Base struct function to initialize the Obj state
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}

ObjString* takeString(char* chars, int length)
{
    return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);
    // Not maintaing direct pointer to source code string but
    // Memory is copied to avoid bugs when this string will be freed
    memcpy(heapChars, chars, length);

    heapChars[length] = '\0';

    return allocateString(heapChars, length);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        default:
            break;
    }
}
