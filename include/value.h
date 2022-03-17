#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// Representing the type of Values
// Each kind of value type the VM supports
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER
} ValueType;

// Only double precision floating point numbers will be supported
// typedef allows to abstract Lox datatype
// Any further changes to this type can be made here only
// typedef double Value;   // double Size - 8 Byte
typedef struct {
    ValueType type; // Storing type of Value

    // Storing how the value is stored
    union {
        bool boolean;       
        double number;      // Double size: 8 bytes
    } as;
} Value;

// These Macros directly access the union fields
// Hoists statically-typed values into clox's dynamically types values
// Macros to produce Value type that has correct type rag and contains underlying values
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})

// Unwrapping the Clox value type to raw C value
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)

// Value check Macros
// Will be required to call before converting clox values
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)

// Constant pool is array of values 
// Instruction to load constant looks up the value by index 
// This index will be used as address in instruction
typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

void printValue(Value value);

#endif