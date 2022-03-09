// Used as deassembler that converts 
// Chunks of byte into textual representation for debugging

#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"
#include "value.h"

// Disassemble all the instuctions in the entrie chunk
void disassembleChunk(Chunk* chunk, const char* name);

/**
 * @brief Disassemble a single instruction
 * 
 * @param chunk 
 * @param offset 
 * @return int offset of next instruction after deassembling current
 */
int disassembleInstruction(Chunk* chunk, int offset);

#endif