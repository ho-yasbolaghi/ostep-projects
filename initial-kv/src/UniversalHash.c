#include "UniversalHash.h"
#include <string.h>

void initializeHT(HashTable *hashTable)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        hashTable->table[i].status = EMPTY;
        hashTable->table[i].value = NULL;
    }

    hashTable->a = rand() % (PRIME - 1) + 1;
    hashTable->b = rand() % PRIME;
    // printf("Universal hash parameters: a = %d, b = %d, p = %lld, m = %d\n",
    //        hashTable->a, hashTable->b, PRIME, TABLE_SIZE);
}

int universalHashFunction(HashTable *hashTable, int key)
{
    return ((hashTable->a * (long long)key + hashTable->b) % PRIME) % TABLE_SIZE;
}

void insertIntoHT(HashTable *hashTable, int key, const char* value)
{
    int index = universalHashFunction(hashTable, key);

    while (hashTable->table[index].status == OCCUPIED)
    {
        index = (index + 1) % TABLE_SIZE;
    }
    
    hashTable->table[index].key = key;
    hashTable->table[index].value = strdup(value);
    hashTable->table[index].status = OCCUPIED;
}

char* searchHT(HashTable *hashTable, int key)
{
    int index = universalHashFunction(hashTable, key);

    while (hashTable->table[index].status != EMPTY)
    {
        if (hashTable->table[index].key == key && hashTable->table[index].status == OCCUPIED)
        {
            return hashTable->table[index].value;
        }
        index = (index + 1) % TABLE_SIZE;
    }

    return NULL;
}

void removeFromHT(HashTable *hashTable, int key)
{
    int index = universalHashFunction(hashTable, key);

    while (hashTable->table[index].status != EMPTY)
    {
        if (hashTable->table[index].key == key && hashTable->table[index].status == OCCUPIED)
        {
            free(hashTable->table[index].value);
            hashTable->table[index].value = NULL;
            hashTable->table[index].status = DELETED;
            return;
        }
        index = (index + 1) % TABLE_SIZE;
    }
}