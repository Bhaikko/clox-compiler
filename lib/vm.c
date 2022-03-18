#include <stdio.h>
#include <stdarg.h>

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

// Shows runtime errors thrown by VM
// variadic function
// format allows to pass format string like in printf()
static void runtimeError(const char* format, ...)
{
    // Full stack trace will be added later

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputs("\n", stderr);

    // Getting current instruction index from Instruction pointer
    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    resetStack();

}

// Lox follows Ruby such that nil and false are falsey
// Every other value behaves like true
static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// Responsible for running bytecode
// Most performance critical part of entire virtual machine
static InterpretResult run()
{
    // Increments the Byte pointer
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    // This do block ensures a local scope for macro 
    // Does type checking with performing binary oepration stack
    #define BINARY_OP(valueType, op) \
        do { \
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(valueType(a op b)); \
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

                case OP_NIL: {
                    push(NIL_VAL);
                    break;
                }

                case OP_FALSE: {
                    push(BOOL_VAL(false));
                    break;
                }

                case OP_TRUE: {
                    push(BOOL_VAL(true));
                    break;
                }

                case OP_NEGATE: {
                    // An optimisation can be done here which doesnt change stack pointer
                    // Since top pointer ends up at same place

                    if (!IS_NUMBER(peek(0))) {
                        runtimeError("Operand must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    push(NUMBER_VAL(-AS_NUMBER(pop())));
                    break;
                }

                case OP_ADD: {
                    BINARY_OP(NUMBER_VAL, +);
                    break;
                }

                case OP_SUBTRACT: {
                    BINARY_OP(NUMBER_VAL, -);
                    break;
                }

                case OP_MULTIPLY: {
                    BINARY_OP(NUMBER_VAL, *);
                    break;
                }

                case OP_DIVIDE: {
                    BINARY_OP(NUMBER_VAL, /);
                    break;
                }

                case OP_NOT: {
                    // Works like OP_NEGATION
                    push(BOOL_VAL(isFalsey(pop())));
                    break;
                }

                case OP_EQUAL: {
                    Value b = pop();
                    Value a = pop();

                    push(BOOL_VAL(valuesEqual(a, b)));
                    break;
                }

                case OP_GREATER: {
                    BINARY_OP(BOOL_VAL, >);
                    break;
                }

                case OP_LESS: {
                    BINARY_OP(BOOL_VAL, <);
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

Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}