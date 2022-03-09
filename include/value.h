#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// Only double precision floating point numbers will be supported

// typedef allows to abstract Lox datatype
// Any further changes to this type can be made here only
typedef double Value;   // double Size - 8 Byte

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

#endif