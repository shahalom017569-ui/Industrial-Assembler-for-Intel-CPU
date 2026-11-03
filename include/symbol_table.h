#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>

#define SYMBOL_MAX_NAME 128

typedef struct {
    char name[SYMBOL_MAX_NAME];
    int64_t value;
    int defined;
} symbol;

typedef struct {
    symbol *items;
    size_t count;
    size_t capacity;
} symbol_table;

int symbol_table_init(symbol_table *table);
void symbol_table_destroy(symbol_table *table);
int symbol_table_insert(symbol_table *table, const char *name, int64_t value, int defined);
int symbol_table_lookup(const symbol_table *table, const char *name, int64_t *value, int *defined);

#endif
