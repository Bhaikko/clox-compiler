#include "./../include/common.h"
#include "./../include/chunk.h"
#include "./../include/debug.h"

int main(int argc, char** argv)
{
    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT);
    writeChunk(&chunk, constant);

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}

