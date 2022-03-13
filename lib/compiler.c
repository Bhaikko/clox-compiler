#include <stdio.h>

#include "./../include/common.h"
#include "./../include/compiler.h"
#include "./../include/scanner.h"

Parser parser;

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
static consume(TokenType type, const char* message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
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

bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    return !parser.hadError;
}