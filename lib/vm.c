#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "./../include/compiler.h"
#include "./../include/debug.h"
#include "./../include/vm.h"
#include "./../include/memory.h"

// Since the VM object will be passed as arguement to all function
// We maintain a global VM object
VM vm;

// NATIVE FUNCTIONS
static Value clockNative(int argCount, Value* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// VM FUNCTIONS
static void defineNative(const char* name, NativeFn function);

static void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

void initVM()
{
    resetStack();
    vm.objects = NULL;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initTable(&vm.strings);
    initTable(&vm.globals);

    defineNative("clock", clockNative);
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
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }

        default:
            break;
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

    free(vm.grayStack);
}

// Shows runtime errors thrown by VM
// variadic function
// format allows to pass format string like in printf()
static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputs("\n", stderr);

    // Printing stack trace
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;

        size_t instruction = frame->ip - function->chunk.code - 1;

        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();

}

// Interface to define Native function in Lox
static void defineNative(const char* name, NativeFn function)
{
    // Push and pop because of garbage collector to not to free the string memory

    // Mapping Native function with a string keyword in globals table
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));

    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

    pop();
    pop();
}

// Lox follows Ruby such that nil and false are falsey
// Every other value behaves like true
static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// Setting up VM state according to function call
static bool call(ObjFunction* function, int argCount)
{
    // Runtime check for arguements passed
    if (argCount != function->arity) {
        runtimeError("Expected %d arguements but got %d.", function->arity, argCount);
        return false;
    }

    // Check for stack overflow
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;

    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION: 
                return call(AS_FUNCTION(callee), argCount);

            case OBJ_NATIVE: {
                // Call The C function and push the result onto stack
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }

            default:
                break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
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
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    // Increments the Byte pointer
    #define READ_BYTE() (*frame->ip++)
    #define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

    // Converts and returns the constant value as Object String
    #define READ_STRING() AS_STRING(READ_CONSTANT())        

    // Buidling 16-bit unsigned integer from two 8-bit unsigned integer
    // Done based on how we stored the 16 bit integer in compiler
    // Left shifting and Or to combine bits
    #define READ_SHORT() \
        (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

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
                    &frame->function->chunk, 
                    // Converting IP to relative offset from beginning of bytecode
                    (int)(frame->ip - frame->function->chunk.code)   
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

                case OP_GET_LOCAL: {
                    uint8_t slot = READ_BYTE();
                    push(frame->slots[slot]);
                    break;
                }

                case OP_SET_LOCAL: {
                    uint8_t slot = READ_BYTE();
                    frame->slots[slot] = peek(0);
                    break;
                }

                case OP_JUMP_IF_FALSE: {
                    // Reading 2 Bytes of Offset
                    uint16_t offset = READ_SHORT();

                    // Checking condition to manipulate instruction pointer
                    if (isFalsey(peek(0))) {
                        frame->ip += offset;
                    }
                    break;
                }

                case OP_JUMP: {
                    uint16_t offset = READ_SHORT();
                    frame->ip += offset;

                    break;
                }

                case OP_LOOP: {
                    // Unconditional Jump backwards in chunk
                    uint16_t offset = READ_SHORT();
                    frame->ip -= offset;

                    break;
                }

                case OP_CALL: {
                    int argCount = READ_BYTE();

                    // Calling function
                    if (!callValue(peek(argCount), argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // CallFrame for the called function
                    frame = &vm.frames[vm.frameCount - 1];

                    break;
                }

                case OP_RETURN: {
                    Value result = pop();

                    vm.frameCount--;

                    // Exiting from top level function in script
                    if (vm.frameCount == 0) {
                        pop();
                        return INTERPRET_OK;
                    }

                    // Discarding all the slots function was using
                    vm.stackTop = frame->slots;

                    // Pushing the returned result at top of stack
                    push(result);

                    frame = &vm.frames[vm.frameCount - 1];
                    break;
                }
            }
        }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    
    // Setting up first frame for executing top level code
    callValue(OBJ_VAL(function), 0);

    return run();
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