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

bool compile(const char* source, Chunk* chunk);

#endif