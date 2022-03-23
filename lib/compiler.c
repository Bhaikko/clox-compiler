#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "./../include/common.h"
#include "./../include/compiler.h"
// #include "./../include/scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "./../include/debug.h"
#endif

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

// COMPILER OPERTATIONS
static void initCompiler(Compiler* compiler)
{
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
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

static bool check(TokenType type)
{
    return parser.current.type == type;
}

// If current token has given type, we consume the token
// Otherwise, returns false without consuming
static bool match(TokenType type)
{
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
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


static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    // Removing Local variables from top of stack
    while (
        current->localCount > 0 &&
        current->locals[current->localCount - 1].depth > current->scopeDepth
    ) {
        emitByte(OP_POP);
        current->localCount--;
    }
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
static void expression();
static void statement();
static void declaration();
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

    // Allow assignement if expression has lower precedence than assignement
    bool canAssign = precedence <= PREC_ASSIGNMENT;

    // First, parsing the prefix expression
    prefixRule(canAssign);

    // Infix expressions evaluated based on Precedence
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();

        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    // Returning error if '=' was not consumed due to higher level precedence
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

// Defining Variable string name in chunk constant table
static uint8_t identifierConstant(Token* name)
{
    // Since whole string for variable name is too big
    // to put in 1 byte code
    // We use Constant Table
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// Comparing names of two identifiers
static bool identifiersEqual(Token* a, Token* b)
{
    if (a->length != b->length) {
        return false;
    }

    return memcmp(a->start, b->start, a->length) == 0;
}

// Creates a new local and appends it to compiler's array of variable
static void addLocal(Token name)
{
    // VM support 1 byte of indexing 
    // Hence 256 local variables are supported
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;

    // Marking the variable uniniatilized but declared
    // Declarting is when its added to scope
    // Defining is when it becomes available for use
    local->depth = -1;
}

static void declareVariable()
{
    if (current->scopeDepth == 0) {
        // Global variables are implicitly declared
        return;
    }

    Token* name = &parser.previous;

    // Lox allows shadowing of variables 
    // Detection of two variables having same name in same scope
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];

        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already variable with this name in this scope.");
        }
    }


    addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) {
        // Returning dummy index since at runtime
        // locals arent looked up by name
        return 0;
    }

    return identifierConstant(&parser.previous);
}

// Chaning depth of local variable to mark it initialized
static void markInitialized()
{
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
    // If not global scope
    // No need to create variable at runtime
    if (current->scopeDepth > 0) {
        // No code to create local variable at runtime
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

// Parsing number literal consisting of single token
static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// Parsing grouping expressions
static void grouping(bool canAssign)
{
    // '(' is assumed to be consumed earlier before calling grouping()

    // This takes care of generating bytecode for expression inside the parenthesis
    expression();   
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    // Since operand is evaluated first which pushes the value to stack
    // Then its negation is done
    // Compile the operand
    parsePrecedence(PREC_UNARY);

    // Emiting the operator instruction
    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG: emitByte(OP_NOT); break;

        default:
            return;
    }
}

static void binary(bool canAssign)
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

        // a != b <==> !(a == b)
        case TOKEN_BANG_EQUAL: {
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        }

        case TOKEN_EQUAL_EQUAL: {
            emitByte(OP_EQUAL);
            break;
        }

        case TOKEN_GREATER: {
            emitByte(OP_GREATER);
            break;
        }

        // a >= b <==> !(a < b)
        case TOKEN_GREATER_EQUAL: {
            emitBytes(OP_LESS, OP_NOT);
            break;
        }

        case TOKEN_LESS: {
            emitByte(OP_LESS);
            break;
        }

        // a <= b <==> !(a > b)
        case TOKEN_LESS_EQUAL: {
            emitBytes(OP_GREATER, OP_NOT);
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

static void literal(bool canAssign)
{
    switch (parser.previous.type) {
        case TOKEN_FALSE: {
            emitByte(OP_FALSE); 
            break;
        }

        case TOKEN_TRUE: {
            emitByte(OP_TRUE);
            break;
        }

        case TOKEN_NIL: {
            emitByte(OP_NIL);
            break;
        }

        default:
            break;
    }
}

static void string(bool canAssign)
{
    // +1 and -2 trims the leading and trailing quotation marks
    emitConstant(OBJ_VAL(copyString(
        parser.previous.start + 1,
        parser.previous.length - 2
    )));
}

// Returns local variable index otherwise -1 for global
static int resolveLocal(Compiler* compiler, Token* name)
{
    // Loops from backward because of Shadowing support for inner scope
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    // Getting index of variable name in constant table
    int arg = resolveLocal(current, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    // Checking if its variable assignment or lookup
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) 
{
    namedVariable(parser.previous, canAssign);
}

static void printStatement()
{
    // Print statement first evaluates the expression then prints it
    expression();

    // Expects a semicolon in the end of statement
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

    // An expression evalutates the expression and discard the result
    // Hence POP operation is used to remove top element of Stack
    emitByte(OP_POP);
}

static void block()
{
    // Keeps parsing declarations and statements until it hits the closing brace
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void statement()
{
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void varDeclaration()
{
    // Compiles the variable name
    // Adds variable name to constants table of chunk
    uint8_t global = parseVariable("Expect variable name.");

    // Checking if there is r-value for the variable declaration
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        // If no assignment then variable assigned to NIL
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    // Adding Byte code with OP code and Constants index
    defineVariable(global);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {

        // Using Statement boundaries to synchronize
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }

        // Looing subsequent token that begins a statement
        // usually one of flow control or declaration keywords
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                break;
        }

        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) {
        synchronize();
    }
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
    [TOKEN_BANG]            = { unary,      NULL,      PREC_NONE    },
    [TOKEN_BANG_EQUAL]      = { NULL,       binary,    PREC_EQUALITY},
    [TOKEN_EQUAL]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_EQUAL_EQUAL]     = { NULL,       binary,    PREC_EQUALITY},
    [TOKEN_GREATER]         = { NULL,       binary,    PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = { NULL,       binary,    PREC_COMPARISON},
    [TOKEN_LESS]            = { NULL,       binary,    PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = { NULL,       binary,    PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      = { variable,   NULL,      PREC_NONE    },
    [TOKEN_STRING]          = { string,     NULL,      PREC_NONE    },
    [TOKEN_NUMBER]          = { number,     NULL,      PREC_NONE    },
    [TOKEN_AND]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_CLASS]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_ELSE]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FALSE]           = { literal,    NULL,      PREC_NONE    },
    [TOKEN_FOR]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FUN]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_IF]              = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_NIL]             = { literal,    NULL,      PREC_NONE    },
    [TOKEN_OR]              = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_PRINT]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_RETURN]          = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_SUPER]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_THIS]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_TRUE]            = { literal,    NULL,      PREC_NONE    },
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

    Compiler compiler;
    initCompiler(&compiler);

    // Setting passed Chunk to currently compiled chunk
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    // According to LOX Grammar
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}