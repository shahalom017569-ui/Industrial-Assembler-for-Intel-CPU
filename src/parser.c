#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static int is_label_like(const token *t) {
    if (t->type == TOKEN_IDENTIFIER || t->type == TOKEN_LABEL) {
        return 1;
    }
    return 0;
}

int parser_parse_tokens(const token_list *tokens, statement_list *out_statements, symbol_table *symbols) {
    size_t i = 0;
    statement stmt;
    char last_label[64] = {0};

    if (!tokens || !out_statements || !symbols) {
        return -1;
    }

    memset(out_statements, 0, sizeof(*out_statements));
    memset(&stmt, 0, sizeof(stmt));

    while (i < tokens->count) {
        const token *t = &tokens->items[i];

        if (t->type == TOKEN_EOF) {
            break;
        }

        if (t->type == TOKEN_IDENTIFIER && i + 1 < tokens->count && tokens->items[i + 1].type == TOKEN_COLON) {
            snprintf(stmt.label, sizeof(stmt.label), "%s", t->text);
            snprintf(last_label, sizeof(last_label), "%s", t->text);
            symbol_table_insert(symbols, t->text, (int64_t)out_statements->count, 1);
            i += 2;
            continue;
        }

        if (t->type == TOKEN_DIRECTIVE) {
            snprintf(stmt.mnemonic, sizeof(stmt.mnemonic), "%s", t->text);
            i++;
            if (i < tokens->count && tokens->items[i].type != TOKEN_EOF) {
                snprintf(stmt.operand, sizeof(stmt.operand), "%s", tokens->items[i].text);
            }
            stmt.line = t->line;
            if (out_statements->count == out_statements->capacity) {
                size_t new_cap = out_statements->capacity == 0 ? 8 : out_statements->capacity * 2;
                out_statements->items = (statement *)realloc(out_statements->items, new_cap * sizeof(statement));
                if (!out_statements->items) {
                    return -1;
                }
                out_statements->capacity = new_cap;
            }
            out_statements->items[out_statements->count++] = stmt;
            memset(&stmt, 0, sizeof(stmt));
            snprintf(stmt.label, sizeof(stmt.label), "%s", last_label);
            continue;
        }

        if (is_label_like(t)) {
            snprintf(stmt.mnemonic, sizeof(stmt.mnemonic), "%s", t->text);
            i++;
            if (i < tokens->count && tokens->items[i].type == TOKEN_IDENTIFIER) {
                snprintf(stmt.operand, sizeof(stmt.operand), "%s", tokens->items[i].text);
                i++;
            }
            stmt.line = t->line;
            if (out_statements->count == out_statements->capacity) {
                size_t new_cap = out_statements->capacity == 0 ? 8 : out_statements->capacity * 2;
                out_statements->items = (statement *)realloc(out_statements->items, new_cap * sizeof(statement));
                if (!out_statements->items) {
                    return -1;
                }
                out_statements->capacity = new_cap;
            }
            out_statements->items[out_statements->count++] = stmt;
            memset(&stmt, 0, sizeof(stmt));
            continue;
        }

        if (t->type == TOKEN_NUMBER) {
            if (stmt.operand[0] == '\0') {
                snprintf(stmt.operand, sizeof(stmt.operand), "%s", t->text);
            }
        }

        i++;
    }

    return 0;
}

void parser_free_statements(statement_list *statements) {
    if (!statements) {
        return;
    }
    free(statements->items);
    statements->items = NULL;
    statements->count = 0;
    statements->capacity = 0;
}
