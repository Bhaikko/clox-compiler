#include <stdio.h>
#include <stdlib.h>

#include "./../include/common.h"
#include "./../include/compiler.h"
#include "./../include/scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "./../include/debug.h"
#endif

Parser parser;
Chunk* compilingChunk;

// PARSER UTILITIES
static void errorAt(Token* token, const char* message)
{
    if (parser.panicMode) {
        return;
    }

    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {

    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

// Prints current based on current token
static void error(const char* message)
{
    errorAt(&parser.previous, message);
}

// Prints current based on previous token
static void errorAtCurrent(const char* message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    // Storing previous token in parser before advancing
    parser.previous = parser.current;

    // Keeps looping until non error token is encountered
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        // Parser responsible to report Lexical errors
        errorAtCurrent(parser.current.start);
    }
}

// Checks current token type to passed
// Prints error if type doesnt match
static void consume(TokenType type, const char* message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

// CODE GENERATION FUNCTIONS
static Chunk* currentChunk()
{
    return compilingChunk;
}

// Writes the given byte to chunk
static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

// Helper function to emit opcodes that have operands
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn()
{
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);

    // Restricting max number of constants in chunk
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler()
{
    // Temporary emit to print the evaluated expression
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

// PARSING FUNCTIONS

// Forward declaration of functions for parser
static ParseRule* getRule(TokenType type);

// Parses any expression at the given precedence level or higher
static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    // Syntax Error
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    // First, parsing the prefix expression
    prefixRule();

    // Infix expressions evaluated based on Precedence
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();

        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

// Parsing number literal consisting of single token
static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

// Parsing grouping expressions
static void grouping()
{
    // '(' is assumed to be consumed earlier before calling grouping()

    // This takes care of generating bytecode for expression inside the parenthesis
    expression();   
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary()
{
    TokenType operatorType = parser.previous.type;

    // Since operand is evaluated first which pushes the value to stack
    // Then its negation is done
    // Compile the operand
    parsePrecedence(PREC_UNARY);

    // Emiting the operator instruction
    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;

        default:
            return;
    }
}

static void binary()
{
    // The value of left operand will end up on stack
    // Get operator
    TokenType operatorType = parser.previous.type;

    // Compiling right operand which have higher precedence than current operator
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emiting operator instruction that performs the binary operation
    switch (operatorType) {
        case TOKEN_PLUS: {
            emitByte(OP_ADD);
            break;
        }

        case TOKEN_MINUS: {
            emitByte(OP_SUBTRACT);
            break;
        }

        case TOKEN_STAR: {
            emitByte(OP_MULTIPLY);
            break;
        }

        case TOKEN_SLASH: {
            emitByte(OP_DIVIDE);
            break;
        }

        default:
            return;
    }

    /*
     The VM will execute the left and right operand code 
     and leave their values on the stack.
    */
}

// Parse Rules for the compiler
ParseRule rules[] = {
    // Token Type               Prefix      Infix       Precedence       
    [TOKEN_LEFT_PAREN]      = { grouping,   NULL,      PREC_NONE    },
    [TOKEN_RIGHT_PAREN]     = { NULL,       NULL,      PREC_NONE    }, 
    [TOKEN_LEFT_BRACE]      = { NULL,       NULL,      PREC_NONE    }, 
    [TOKEN_RIGHT_BRACE]     = { NULL,       NULL,      PREC_NONE    }, 
    [TOKEN_COMMA]           = { NULL,       NULL,      PREC_NONE    }, 
    [TOKEN_DOT]             = { NULL,       NULL,      PREC_NONE    }, 
    [TOKEN_MINUS]           = { unary,      binary,    PREC_TERM    },   // Both unary and binary as '-' can be used for negation too
    [TOKEN_PLUS]            = { NULL,       binary,    PREC_TERM    },
    [TOKEN_SEMICOLON]       = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_SLASH]           = { NULL,       binary,    PREC_FACTOR  },
    [TOKEN_STAR]            = { NULL,       binary,    PREC_FACTOR  },
    [TOKEN_BANG]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_BANG_EQUAL]      = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_EQUAL]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_EQUAL_EQUAL]     = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_GREATER]         = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_GREATER_EQUAL]   = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_LESS]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_LESS_EQUAL]      = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_IDENTIFIER]      = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_STRING]          = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_NUMBER]          = { number,     NULL,      PREC_NONE    },
    [TOKEN_AND]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_CLASS]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_ELSE]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FALSE]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FOR]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FUN]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_IF]              = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_NIL]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_OR]              = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_PRINT]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_RETURN]          = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_SUPER]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_THIS]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_TRUE]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_VAR]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_WHILE]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_ERROR]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_EOF]             = { NULL,       NULL,      PREC_NONE    }
};

// Returns the rule at given index
static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);

    // Setting passed Chunk to currently compiled chunk
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    endCompiler();
    return !parser.hadError;
}