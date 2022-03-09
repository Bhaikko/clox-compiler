#include <stdio.h>

#include "./../include/debug.h"

static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);

    return offset + 1;
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
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);

        default:
            printf("UNknown opcode %d\n", instruction);
            return offset + 1;
    }
}
