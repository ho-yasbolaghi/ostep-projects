#include "UniversalHash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int load_database(HashTable* table, const char* database_name) {
    FILE* fp = NULL;
    if ((fp = fopen(database_name, "r")) == NULL) {
        printf("File could not be opened");
        return 0;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t nread;

    while((nread = getline(&line, &len, fp)) != -1) {
        char* token = strtok(line, " ");
        int key = atoi(token);
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = 0;
        insertIntoHT(table, key, token);
    }

    free(line);
    fclose(fp);
    return 1;
}

int store_database(HashTable* table, const char* database_name) {
    FILE* fp = NULL;
    if ((fp = fopen(database_name, "w")) == NULL) {
        printf("File could not be opened");
        return 0;
    }

    for (int i = 0; i < TABLE_SIZE; i++) {
        if (table->table[i].status == OCCUPIED) {
            fprintf(fp, "%d %s\n", table->table[i].key, table->table[i].value);
        }
    }

    fclose(fp);
    return 1;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    HashTable table;
    initializeHT(&table);

    load_database(&table, "database.txt");

    for (int i = 1; i < argc; i++) {
        char* string = strdup(argv[i]);
        char* to_free = string;

        char* temp;
        char* tokens[3];
        int number_of_tokens = 0;

        temp = strtok(string, ",");
        while (temp && number_of_tokens < 3) {
            tokens[number_of_tokens] = strdup(temp);
            number_of_tokens++;
            temp = strtok(NULL, ",");
        }

        free(to_free);

        if (strcmp(tokens[0], "p") == 0) {
            insertIntoHT(&table, atoi(tokens[1]), tokens[2]);
        } else if (strcmp(tokens[0], "g") == 0)
        {
            char* value = searchHT(&table, atoi(tokens[1]));
            if (value == NULL) {
                printf("%d %s\n", atoi(tokens[1]), "not found");
            } else
            {
                printf("%d,%s\n", atoi(tokens[1]), value);
            }
        } else if (strcmp(tokens[0], "d") == 0)
        {
            removeFromHT(&table, atoi(tokens[1]));
        } else if (strcmp(tokens[0], "c") == 0)
        {
            initializeHT(&table);
        } else if (strcmp(tokens[0], "a") == 0)
        {
            for (int i = 0; i < TABLE_SIZE; i++) {
                if (table.table[i].status == OCCUPIED) {
                    printf("%d %s", table.table[i].key, table.table[i].value);
                }
            }
        } else
        {
            printf("Wrong command\n");
        }
    }

    store_database(&table, "database.txt");
    
    return 0;
}