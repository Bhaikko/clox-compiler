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
    OP_DEFINE_GLOBAL,   // Defining Global Variable 
    OP_GET_GLOBAL,      // Reading Global from constant table
    OP_SET_GLOBAL,      // Setting Global Variable Value
    OP_GET_LOCAL,       // Reading Local variable from constant table 
    OP_SET_LOCAL,       // setting Local variable value
    OP_JUMP_IF_FALSE,   // specifies offset for IP to jump by
    OP_JUMP,            // Unconditional Jump to Offset
    OP_LOOP,

    // 1 Byte Instuction
    OP_NEGATE,          // Negates the operand
    OP_ADD,             // Adds the operands at top of stack
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NIL,             // Keyword NIL
    OP_TRUE,            // Keyword True
    OP_FALSE,           // Keyword False
    OP_NOT,
    OP_PRINT,
    OP_POP,

    // Below Instructions will be used to compile
    // <=, =>, != too, eg a != b <==> !(a == b)
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_RETURN           // Return from current Function
} OpCode;

typedef struct {
    int count;          // Number of used elements
    int capacity;       // Number of allocated elements
    uint8_t* code;      // Dynamic array for ByteCode
    int* lines;         // Stores line number for every byte in code
    ValueArray constants;   // Pool of constants values
} Chunk;

// Used to initialize Chunk dynamic Array
void initChunk(Chunk* chunk);

// To append byte to the end of the chunk
// Compiler will track of current line
void writeChunk(Chunk* chunk, uint8_t byte, int line);

// Used to free the memory allocated to Chunk
void freeChunk(Chunk* chunk);

/**
 * @brief To add a constant to constant pool
 * 
 * @return int index of added constant
 */
int addConstant(Chunk* chunk, Value value);

#endif
