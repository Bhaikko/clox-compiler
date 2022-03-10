// VM code which Executes the Byte code

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

typedef struct {
    Chunk* chunk;
    
    // instruction pointer, pointing to next instruction in bytecode to be executed
    uint8_t* ip;        
} VM;

// For interpreter to set the exit code of the process
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// For handling VM state
void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

#endif