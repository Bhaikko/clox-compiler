// Chunk refers to sequence of Bytecode
// Each instruction has one byte operation code(opcode)

#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// Defining opcodes for VM
typedef enum {
    // 2 Byte Instruction 
    OP_CONSTANT,        // Need constant index

    // 1 Byte Instuction
    OP_RETURN           // Return from current Function
} OpCode;

typedef struct {
    int count;          // Number of used elements
    int capacity;       // Number of allocated elements
    uint8_t* code;      // Dynamic array for ByteCode
    ValueArray constants;   // Pool of constants values
} Chunk;

// Used to initialize Chunk dynamic Array
void initChunk(Chunk* chunk);

// To append byte to the end of the chunk
void writeChunk(Chunk* chunk, uint8_t byte);

// Used to free the memory allocated to Chunk
void freeChunk(Chunk* chunk);

/**
 * @brief To add a constant to constant pool
 * 
 * @return int index of added constant
 */
int addConstant(Chunk* chunk, Value value);

#endif
