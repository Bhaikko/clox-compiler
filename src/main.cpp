#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./../include/common.h"
#include "./../include/vm.h"

// Reading file content based on path provided
static char* readFile(const char* path)
{
    FILE* file = fopen(path, "rb");
    
    // Cannot find file
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    
    // Determing size of file
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // Allocating buffer for file content
    char* buffer = (char*)malloc(fileSize + 1);

    // Not able to allocate buffer for file content
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    // Reading file
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

    // Read Failed of file
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\". \n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void repl()
{
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static void runFile(const char* path)
{
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(65);
    }

    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}

int main(int argc, char** argv)
{
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}

