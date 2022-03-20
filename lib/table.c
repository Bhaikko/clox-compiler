#include <stdlib.h>
#include <string.h>

#include "./../include/memory.h"
#include "./../include/object.h"
#include "./../include/table.h"
#include "./../include/value.h"

void initTable(Table* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

// Finding entry based on Key in Hash table
// Used for both, to find exisiting entry or 
// to decide where to insert new ones
static Entry* findEntry(Entry* entries, int capacity, ObjString* key)
{
    uint32_t index = key->hash % capacity;

    // Searching based on Linear probing 
    for (;;) {
        Entry* entry = &entries[index];

        if (entry->key == key || entry->key == NULL) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool tableSet(Table* table, ObjString* key, Value value)
{
    // Allocating and growing entry array
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }


    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == NULL;
    if (isNewKey) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;

    return isNewKey;
}