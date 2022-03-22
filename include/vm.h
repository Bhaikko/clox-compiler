// VM code which Executes the Byte code

// Architecture: Stack based VM

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    
    // instruction pointer, pointing to next instruction in bytecode to be executed
    uint8_t* ip;    

    // Stack of operands, acts like a shared workspace for instructions
    Value stack[STACK_MAX];
    // Will point to index next to top, to where the next pushed element will go
    Value* stackTop;    

    // Head of Linked List of Object Type Values
    Obj* objects;

    // Containing Reference to Global Variables
    Table globals;
    
    // for string interning
    Table strings;
} VM;

// For interpreter to set the exit code of the process
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// Exposing vm global instance externally
extern VM vm;

// For handling VM state
void initVM();
void freeVM();

InterpretResult interpret(const char* chunk);

// To push value at top pointer
void push(Value value);

// To get value stored just before top pointer
Value pop();

// To peek at stack value from top
// distance = 0 is top
Value peek(int distance);

// To free all heap allocated objects
void freeObjects();

#endif