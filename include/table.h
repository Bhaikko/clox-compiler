#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// Threshold Load Factor for Table 
#define TABLE_MAX_LOAD 0.75

// Indiviual Entry of Hash Table
typedef struct {
    ObjString* key;     // Key realted to Entry 
    Value value;        // Entry Value
} Entry;

typedef struct {
    int count;      // Number of entries + tombstones
    int capacity;   // Load Factor = count / capacity
    Entry* entries;
} Table;

// Initiliazing and Memory Freeing Functions
void initTable(Table* table);
void freeTable(Table* table);

// Inserting entry in Table
// Returns true if new entry is added
bool tableSet(Table* table, ObjString* key, Value value);

// Retrieving values from table
bool tableGet(Table* table, ObjString* key, Value* value);

// Deleting value from table
bool tableDelete(Table* table, ObjString* key);

// Copying Hash table values
void tableAddAll(Table* from, Table* to);

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

// Removing Dangling pointers from table of ObjString freed by Garbage Collectors
void tableRemoveWhite(Table* table);

#endif