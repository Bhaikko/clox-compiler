#ifndef clox_scanner_h
#define clox_scanner_h

typedef struct {
    const char* start;  // Points to start of current lexeme
    const char* current;    // Points to current character being looked at
    int line;
} Scanner;

void initScanner(const char* source);

#endif