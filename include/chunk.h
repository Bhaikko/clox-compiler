// Chunk refers to sequence of Bytecode
// Each instruction has one byte operation code(opcode)

#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

// Defining opcodes for VM
typedef enum {
    OP_RETURN           // Return from current Function
} OpCode;

typedef struct {
    int count;          // Number of used elements
    int capacity;       // Number of allocated elements
    uint8_t* code;      // Dynamic array for ByteCode
} Chunk;

/**
 * @brief Used to initialize Chunk dynamic Array
 * 
 * @param chunk 
 */
void initChunk(Chunk* chunk);

/**
 * @brief To append byte to the end of the chunk
 * 
 * @param chunk 
 * @param byte 
 */
void writeChunk(Chunk* chunk, uint8_t byte);

#endif
