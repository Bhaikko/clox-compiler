#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)

// Expands a valid ObjString pointer on heap
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)

// Object Types for Obj
typedef enum {
    OBJ_STRING,
} ObjType;

// Base struct for all heap allocated types
struct Obj {
    ObjType type;
};

// Defining Obj as first field allows to safely cast it to Obj* 
// because of C expanding the Base struct inplace
// Representing String in clox with Obj as base struct
struct ObjString {
    Obj obj;
    int length;
    char* chars;
};

ObjString* takeString(char* chars, int length);

// Making copy of string literal from source code
ObjString* copyString(const char* chars, int length);

// For printing Obj value
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif