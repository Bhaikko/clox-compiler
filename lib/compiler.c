#include <stdio.h>

#include "./../include/common.h"
#include "./../include/compiler.h"
#include "./../include/scanner.h"

Parser parser;
Chunk* compilingChunk;

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

static emitReturn()
{
    emitByte(OP_RETURN);
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