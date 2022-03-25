#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "scanner.h"
#include "object.h"
#include "vm.h"

// Function type that takes no arguement and returns nothing
typedef void (*ParseFn)(bool canAssign);  

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;     // For synchronizing in case of syntax errors
} Parser;

/*
 This exists because Praser does not cascade to parse expressions
 of higher precedence. Hence a difference solution is implemented where
 parsePrecedence(precedence) will parse any expression at the given
 precedence level or higher.

 Lower enums have higher precedence
*/
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // * /
    PREC_CALL,          // . ()
    PREC_PRIMARY        
} Precedence;

typedef struct {
    ParseFn prefix;     // function to compile a prefix expression starting with token of that type
    ParseFn infix;      // function to compile an infix expression whose left operand is followed by a token of that type
    Precedence precedence;  // precedence of an infix expression that uses that token as an operator
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

// Distinction for which type of function is being executed
typedef enum {
    TYPE_FUNCTION,  // Function body
    TYPE_SCRIPT     // Top level code
} FunctionType;

// Mainting state for Local Variables
typedef struct {
    // Wrapping the top level program into a function
    ObjFunction* function;
    FunctionType type;

    // Flat array of all locals that are in scope
    // during each point in the compilation process
    // Since 1 byte only allows 255 values
    // Locals array will be restricted to 255 values
    Local locals[UINT8_COUNT];  

    // Number of local variables
    int localCount;

    // 0 - Global scope, 1 - first top level block and so on
    int scopeDepth;
} Compiler;

ObjFunction* compile(const char* source, Chunk* chunk);

#endif