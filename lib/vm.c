#include <stdio.h>

#include "./../include/compiler.h"
#include "./../include/debug.h"
#include "./../include/vm.h"

// Since the VM object will be passed as arguement to all function
// We maintain a global VM object
VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
}

void initVM()
{
    resetStack();
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

    // This do block ensures a local scope for macro 
    #define BINARY_OP(op) \
        do { \
            double b = pop(); \
            double a = pop(); \
            push(a op b); \
        } while (false)

        for (;;) {
            #ifdef DEBUG_TRACE_EXECUTION
                // Printing Values of Stack before executing current instruction
                printf("             ");
                for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
                    printf("[ ");
                    printValue(*slot);
                    printf(" ]");
                }
                printf("\n");

                // Prints each instruction right before executing it
                // Disassembles instruction dynamically
                disassembleInstruction(
                    vm.chunk, 
                    // Converting IP to relative offset from beginning of bytecode
                    (int)(vm.ip - vm.chunk->code)   
                );
            #endif


            // Decoding / Dispatching the instruction
            uint8_t instruction;

            // Body of each opcode case handles the instruction
            switch (instruction = READ_BYTE()) {
                case OP_CONSTANT: {
                    Value constant = READ_CONSTANT();
                    push(constant);
                    break;
                }

                case OP_NEGATE: {
                    // An optimisation can be done here which doesnt change stack pointer
                    // Since top pointer ends up at same place
                    push(-pop());
                    break;
                }

                case OP_ADD: {
                    BINARY_OP(+);
                    break;
                }

                case OP_SUBTRACT: {
                    BINARY_OP(-);
                    break;
                }

                case OP_MULTIPLY: {
                    BINARY_OP(*);
                    break;
                }

                case OP_DIVIDE: {
                    BINARY_OP(/);
                    break;
                }

                case OP_RETURN: {
                    printValue(pop());
                    printf("\n");
                    return INTERPRET_OK;
                }
            }
        }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    Chunk chunk;
    initChunk(&chunk);

    // filling the chunk with bytecode
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}