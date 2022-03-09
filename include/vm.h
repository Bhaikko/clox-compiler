// VM code which Executes the Byte code

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

typedef struct {
    Chunk* chunk;
} VM;

// For handling VM state
void initVM();
void freeVM();

#endif