#include <stdio.h>
#include <stdlib.h>

#include "./../include/common.h"
#include "./../include/compiler.h"
#include "./../include/scanner.h"

Parser parser;
Chunk* compilingChunk;

// CODE GENERATION FUNCTIONS

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

static Chunk* currentChunk()
{
    return compilingChunk;
}

static void endCompiler()
{
    // Temporary emit to print the evaluated expression
    emitReturn();
}

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

// PARSING FUNCTIONS

// Parses any expression at the given precedence level or higher
static void parsePrecedence(Precedence precedence)
{

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


bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);

    // Setting passed Chunk to currently compiled chunk
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    // expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    endCompiler();
    return !parser.hadError;
}