#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "./../include/compiler.h"
#include "./../include/debug.h"
#include "./../include/vm.h"
#include "./../include/memory.h"

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
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);
}

void freeVM()
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);

    // Freeing memory when user program exits
    freeObjects();
}

// Freeing Type specifc object memory
static void freeObject(Obj* object)
{
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
    }
}

void freeObjects() 
{
    // Walking a linked list and freeing its nodes
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
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

// Concatenating two strings on top of stack
static void concatenate()
{
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);

    // Copying Two halves in chars
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    // Since ALLOCATE was called above to allocate memory to concatenated string
    // No need to copy string to new memory
    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));

}

// Responsible for running bytecode
// Most performance critical part of entire virtual machine
static InterpretResult run()
{
    // Increments the Byte pointer
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    // Converts and returns the constant value as Object String
    #define READ_STRING() AS_STRING(READ_CONSTANT())        

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
                    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                        concatenate();
                    } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                        double b = AS_NUMBER(pop());
                        double a = AS_NUMBER(pop());
                        push(NUMBER_VAL(a + b));
                    } else {
                        runtimeError("Operands must be two numbers or two strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

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

                case OP_PRINT: {
                    // Stack effect of Print is zero
                    // Since it evaluates the expression and prints it
                    // Statment produces no values
                    printValue(pop());
                    printf("\n");
                    break;
                }

                case OP_POP: {
                    pop();
                    break;
                }

                case OP_DEFINE_GLOBAL: {
                    // Redefinition of GLobal variables allowed
                    // Hence check for existence avoided
                    ObjString* name = READ_STRING();
                    tableSet(&vm.globals, name, peek(0));

                    pop();
                    break;
                }

                case OP_GET_GLOBAL: {
                    ObjString* name = READ_STRING();
                    Value value;

                    // value is passed as out parameter that contains
                    // value for the variable name passed
                    if (!tableGet(&vm.globals, name, &value)) {
                        runtimeError("Undefined variable '%s'.", name->chars);
                    }

                    push(value);
                    break;
                }

                case OP_SET_GLOBAL: {
                    ObjString* name = READ_STRING();

                    // Runtime error if key is newly inserted in Globals
                    // Implicit Variable declaration is not supported
                    if (tableSet(&vm.globals, name, peek(0))) {
                        tableDelete(&vm.globals, name);
                        runtimeError("Undefined variable '%s'.", name->chars);

                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // Not popping value off stack since 
                    // Assignment statement is an expression
                    // Which returns the assigned bale

                    break;
                }

                case OP_RETURN: {
                    // Exit Interpreter
                    return INTERPRET_OK;
                }
            }
        }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_STRING
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