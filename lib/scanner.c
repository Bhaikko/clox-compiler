#include <stdio.h>
#include <string.h>

#include "./../include/common.h"
#include "./../include/scanner.h"

Scanner scanner;

static bool isAtEnd()
{
    return *scanner.current == '\0';
}

// Creates token based on start and current pointers in Scanner
static Token makeToken(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

// Creates error token with message as lexeme
static Token errorToken(const char* message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;

    return token;
}

// Returns current character with advancing the current pointer 
static char advance()
{
    scanner.current++;
    return scanner.current[-1];
}

// Returns current character without consuming
static char peek()
{
    return *scanner.current;
}

// Returns one look ahead character
static char peekNext()
{
    if (isAtEnd()) {
        return '\0';
    }

    return scanner.current[1];
}

// Matches current character with expected character
// Only advances if expected character matches
static bool match(char expected)
{
    if (isAtEnd()) {
        return false;
    }

    if (*scanner.current != expected) {
        return false;
    }

    scanner.current++;
    return true;
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Skips whitespaces and sets current pointer to a valid one
static void skipWhitespace()
{
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;

            case '\n':
                scanner.line++;
                advance();
                break;

            // Skipping comments 
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                    }
                } else {
                    return;
                }

                break;

            default:
                return;
        }
    }
}

static Token string()
{
    while (peek() != '"' && !isAtEnd()) {
        // Support for multi line strings
        if (peek() == '\n') {
            scanner.line++;
        }
        advance();
    }

    if (isAtEnd()) {
        return errorToken("Unterminated string.");
    }

    advance();
    return makeToken(TOKEN_STRING);
}

// Return lexeme representation of number
// Does not convert lexeme to double 
static Token number()
{
    while (isDigit(peek())) {
        advance();
    }

    // Looking for fractional part
    if (peek() == '.' && isDigit(peekNext())) {
        advance();

        while (isDigit(peek())) {
            advance();
        }
    }

    return makeToken(TOKEN_NUMBER);
}

void initScanner(const char* source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

Token scanToken() 
{
    // Skipping Whitespaces
    skipWhitespace();

    scanner.start = scanner.current;

    // Sent to compiler to stop asking for more tokens
    if (isAtEnd()) {
        return makeToken(TOKEN_EOF);
    }

    char c = advance();

    if (isDigit(c)) {
        return number();
    }

    switch (c) {
    // Handling Punctutations
        // Single character token
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);

        // Two character token
        case '!':
            return makeToken(
                match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG
            );

        
        case '=':
            return makeToken(
                match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL
            );

        
        case '<':
            return makeToken(
                match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS
            );

        
        case '>':
            return makeToken(
                match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER
            );

        case '"':
            return string();
        
    }

    return errorToken("Unexpected character.");
}