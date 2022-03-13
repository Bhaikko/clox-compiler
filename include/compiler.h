#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "scanner.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;     // For synchronizing in case of syntax errors
} Parser;

bool compile(const char* source, Chunk* chunk);

#endif