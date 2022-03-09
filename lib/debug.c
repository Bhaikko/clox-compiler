#include <stdio.h>

#include "./../include/debug.h"

static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
    // Fetching constant index in next byte of instruction
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);

    // Constant values are known at compile times
    printValue(chunk->constants.values[constant]);
    printf("'\n");

    // Since OP_CONSTANT is 2 Byte instruction
    return offset + 2;
}

void disassembleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    // Disassembling each instruction in the Chunk
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk* chunk, int offset)
{
    // Prints the byte offset of the given instruction
    // Useful in control flow
    printf("%04d ", offset);

    uint8_t instruction = chunk->code[offset];

    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);

        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);

        default:
            printf("UNknown opcode %d\n", instruction);
            return offset + 1;
    }
}

