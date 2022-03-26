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
static void initCompiler(Compiler* compiler, FunctionType type)
{
    compiler->enclosing = current;

    // Creating function at compile time for top level execution
    compiler->function = NULL;
    compiler->type = type;

    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    compiler->function = newFunction();

    current = compiler;

    if (type != TYPE_SCRIPT) {
        // Setting function name from previous token
        // We copy string because the source code string will get freed after compiling
        // But we need the string name in runtime to reference
        current->function->name = copyString(
            parser.previous.start,
            parser.previous.length
        );
    }

    // Defining stack slot zero for VM's own internal use
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
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
    // current chunk is always the chunk owned by the function 
    // we're in middle of compiling
    return &current->function->chunk;
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
    // Default return of a function
    emitByte(OP_NIL);
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

static ObjFunction* endCompiler()
{
    // Temporary emit to print the evaluated expression
    emitReturn();

    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        // Handling Implicit function since it does not have name
        disassembleChunk(currentChunk(), 
            function->name != NULL ? function->name->chars : "<script>"
        );
    }
#endif

    current = current->enclosing;
    return function;
}

// PARSING FUNCTIONS

// Forward declaration of functions for parser
static void expression();
static void statement();
static void declaration();
static void varDeclaration();
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

// Changing depth of local variable to mark it initialized
static void markInitialized()
{
    // Bound to be globalVariable
    if (current->scopeDepth == 0) {
        return;
    }

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

// Emitting Jump instaruction with placeholder to chunk
// Returns offset of emitted instruction
static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    // Emiting 2 offset placeholder bytes
    // Will be filled after 
    emitByte(0xff);
    emitByte(0xff);

    return currentChunk()->count - 2;
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    // Following Big Endian
    // Storing Higher byte of offset in lower address
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = (jump) & 0xff;
}

// Emitjump and patchJump combined to configure OP_LOOP instruction
static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    // Emiting offset bytes to Loop instruction
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
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

// Compiles and keyword
static void and_(bool canAssign)
{
    // Since logical operators support short circuting
    // Need to use jumps
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

// Compiles or keyword
static void or_(bool canAssign)
{
    // Also supports short circuiting
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);

    // OR can be much more efficient than this
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

// Compiling arguements list for function call
static uint8_t arguementList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();

            if (argCount == 255) {
                error("Can't have more than 255 arguements.");
            }

            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguements.");
    return argCount;
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


static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    // Compiling condition of if statement
    // Leaves the condition at top of stack
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    // Popping the condition evaluated by expression of If statement
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    // Patching if branch offset
    patchJump(thenJump);
    emitByte(OP_POP);

    // Handling Else Branch if found
    if (match(TOKEN_ELSE)) {
        statement();
    }

    // Patching Else branch offset
    patchJump(elseJump);
}

static void whileStatement()
{
    // How far to jump back after completing an iteration of loop
    int loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    // Evaluating While loop expression
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    // Compiling body of while loop
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);

}

static void forStatement()
{
    // Since forloop has local scope
    beginScope();

    // Consuming Mandatory punctuations for the loop
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // Initializer Clause
    if (match(TOKEN_SEMICOLON)) {
        // No initializer
    } else  if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    // Offset at top of the body
    int loopStart = currentChunk()->count;

    // Condition Clause
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of loop if condition is false
        exitJump = emitJump(OP_JUMP_IF_FALSE);

        // Popping out evaluated condition
        emitByte(OP_POP);
    }

    // Increment Clause
    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;

        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        // Since the compiler is single pass
        // We jump over the increment, run the body, jump back up to the increment
        // run it then go to next iteration
        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    // Patching Jump instructiono
    if (exitJump != -1) {
        // Only done if there is condition clause
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

static void returnStatement()
{
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        // Calculating return expression 
        // which will push the result on top of stack
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void statement()
{
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
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

// Compiling function 
static void function(FunctionType type)
{
    // Defining seperate compiler for each function being compiled
    Compiler compiler;
    initCompiler(&compiler, type);

    // Defining scope for current function body
    beginScope();

    // Compiling Parameter list
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }

            uint8_t paramConstant = parseVariable(
                "Expect parameter name."
            );

            // Only defining arguements and not initialising
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameter.");

    // Compiling Body
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    // Creating function object
    // endCompiler also sets the current compiler object to enclosing
    // Hence the bytes are emitted to the parent compiler's chunk
    ObjFunction* function = endCompiler();

    // Storing function object in constant table
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));

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

static void funDeclaration()
{
    // Compiling Variable name of function
    uint8_t global = parseVariable("Expect function name.");
    // Its safe for function to reder to its own name inside its body
    markInitialized();

    function(TYPE_FUNCTION);

    defineVariable(global);
}

static void declaration()
{
    if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) {
        synchronize();
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = arguementList();
    emitBytes(OP_CALL, argCount);
}

// Parse Rules for the compiler
ParseRule rules[] = {
    // Token Type               Prefix      Infix       Precedence       
    [TOKEN_LEFT_PAREN]      = { grouping,   call,      PREC_CALL    },
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
    [TOKEN_AND]             = { NULL,       and_,      PREC_AND     },
    [TOKEN_CLASS]           = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_ELSE]            = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FALSE]           = { literal,    NULL,      PREC_NONE    },
    [TOKEN_FOR]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_FUN]             = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_IF]              = { NULL,       NULL,      PREC_NONE    },
    [TOKEN_NIL]             = { literal,    NULL,      PREC_NONE    },
    [TOKEN_OR]              = { NULL,       or_,       PREC_OR    },
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

ObjFunction* compile(const char* source)
{
    initScanner(source);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    // According to LOX Grammar
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    // If there is error in compiler, we return NULL
    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}