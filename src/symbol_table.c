#include <string.h>
#include <stdlib.h>

#include "symbol_table.h"

int symbol_table_init(symbol_table *table) {
    if (!table) {
        return -1;
    }
    memset(table, 0, sizeof(*table));
    return 0;
}

void symbol_table_destroy(symbol_table *table) {
    if (!table) {
        return;
    }
    free(table->items);
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

int symbol_table_insert(symbol_table *table, const char *name, int64_t value, int defined) {
    size_t i;
    symbol *new_items;

    if (!table || !name) {
        return -1;
    }

    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].name, name) == 0) {
            table->items[i].value = value;
            table->items[i].defined = defined;
            return 0;
        }
    }

    if (table->count == table->capacity) {
        size_t new_capacity = table->capacity == 0 ? 8 : table->capacity * 2;
        new_items = (symbol *)realloc(table->items, new_capacity * sizeof(*new_items));
        if (!new_items) {
            return -1;
        }
        table->items = new_items;
        table->capacity = new_capacity;
    }

    snprintf(table->items[table->count].name, sizeof(table->items[table->count].name), "%s", name);
    table->items[table->count].value = value;
    table->items[table->count].defined = defined;
    table->count++;
    return 0;
}

int symbol_table_lookup(const symbol_table *table, const char *name, int64_t *value, int *defined) {
    size_t i;

    if (!table || !name) {
        return -1;
    }

    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].name, name) == 0) {
            if (value) {
                *value = table->items[i].value;
            }
            if (defined) {
                *defined = table->items[i].defined;
            }
            return 0;
        }
    }

    return -1;
}
