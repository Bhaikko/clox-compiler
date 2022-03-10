#include <stdio.h>

#include "./../include/vm.h"

// Since the VM object will be passed as arguement to all function
// We maintain a global VM object
VM vm;

void initVM()
{

}

void freeVM()
{

}

// Responsible for running bytecode
// Most performance critical part of entire virtual machine
static InterpretResult run()
{
    // Increments the Byte pointer
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

        for (;;) {
            // Decoding / Dispatching the instruction
            uint8_t instruction;

            // Body of each opcode case handles the instruction
            switch (instruction = READ_BYTE()) {
                case OP_CONSTANT: {
                    Value constant = READ_CONSTANT();
                    printValue(constant);
                    printf("\n");
                    break;
                }

                case OP_RETURN: {
                    return INTERPRET_OK;
                }
            }
        }


    #undef READ_BYTE
    #undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code; // Initialize IP to first byte of code
    return run();
}