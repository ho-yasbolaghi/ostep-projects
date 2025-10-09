#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>

#define TABLE_SIZE 1000
#define PRIME 2147483659  // A prime > 2^31 - 1

typedef enum
{
    EMPTY,
    OCCUPIED,
    DELETED
} SlotStatus;

typedef struct
{
    int key;
    char* value;
    SlotStatus status;
} Entry;

typedef struct
{
    Entry table[TABLE_SIZE];
    int a, b;
} HashTable;

void initializeHT(HashTable *hashTable);
int universalHashFunction(HashTable *hashTable, int key);
void insertIntoHT(HashTable *hashTable, int key, const char* value);
char* searchHT(HashTable *hashTable, int key);
void removeFromHT(HashTable *hashTable, int key);

#endif