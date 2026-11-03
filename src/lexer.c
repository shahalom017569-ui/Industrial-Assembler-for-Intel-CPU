#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

static void trim_trailing_newline(char *text) {
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static void *xrealloc(void *ptr, size_t new_size) {
    void *out = realloc(ptr, new_size);
    if (!out) {
        fprintf(stderr, "allocation failure\n");
        exit(1);
    }
    return out;
}

int lexer_tokenize(const char *source, token_list *out_tokens) {
    lexer_state state;
    char buffer[TOKEN_MAX_LEN];
    size_t i = 0;

    if (!source || !out_tokens) {
        return -1;
    }

    memset(out_tokens, 0, sizeof(*out_tokens));
    state.source = source;
    state.length = strlen(source);
    state.index = 0;
    state.line = 1;
    state.column = 1;

    while (state.index < state.length) {
        char ch = source[state.index];
        token tok;

        memset(&tok, 0, sizeof(tok));
        tok.line = state.line;
        tok.column = state.column;

        if (isspace((unsigned char)ch)) {
            if (ch == '\n') {
                state.line++;
                state.column = 1;
            } else {
                state.column++;
            }
            state.index++;
            continue;
        }

        if (ch == ':') {
            tok.type = TOKEN_COLON;
            snprintf(tok.text, sizeof(tok.text), ":");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == ',') {
            tok.type = TOKEN_COMMA;
            snprintf(tok.text, sizeof(tok.text), ",");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == '+') {
            tok.type = TOKEN_PLUS;
            snprintf(tok.text, sizeof(tok.text), "+");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == '-') {
            tok.type = TOKEN_MINUS;
            snprintf(tok.text, sizeof(tok.text), "-");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == '*') {
            tok.type = TOKEN_STAR;
            snprintf(tok.text, sizeof(tok.text), "*");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == '/') {
            tok.type = TOKEN_SLASH;
            snprintf(tok.text, sizeof(tok.text), "/");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == '(') {
            tok.type = TOKEN_LPAREN;
            snprintf(tok.text, sizeof(tok.text), "(");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == ')') {
            tok.type = TOKEN_RPAREN;
            snprintf(tok.text, sizeof(tok.text), ")");
            state.index++;
            state.column++;
            goto push_token;
        }

        if (ch == ';') {
            while (state.index < state.length && source[state.index] != '\n') {
                state.index++;
                state.column++;
            }
            continue;
        }

        if (ch == '"') {
            size_t start = state.index + 1;
            size_t len = 0;
            state.index++;
            state.column++;
            while (state.index < state.length && source[state.index] != '"') {
                if (source[state.index] == '\\') {
                    state.index++;
                    state.column++;
                    if (state.index < state.length) {
                        state.index++;
                        state.column++;
                    }
                    continue;
                }
                state.index++;
                state.column++;
                len++;
            }
            if (state.index >= state.length) {
                fprintf(stderr, "unterminated string literal at line %zu\n", state.line);
                return -1;
            }
            if (len >= sizeof(buffer)) {
                len = sizeof(buffer) - 1;
            }
            memcpy(buffer, source + start, len);
            buffer[len] = '\0';
            tok.type = TOKEN_STRING;
            snprintf(tok.text, sizeof(tok.text), "%s", buffer);
            state.index++;
            state.column++;
            goto push_token;
        }

        if (isalnum((unsigned char)ch) || ch == '_' || ch == '.') {
            size_t start = state.index;
            i = 0;
            while (state.index < state.length && (isalnum((unsigned char)source[state.index]) || source[state.index] == '_' || source[state.index] == '.')) {
                if (i < sizeof(buffer) - 1) {
                    buffer[i++] = source[state.index];
                }
                state.index++;
                state.column++;
            }
            buffer[i] = '\0';
            if (buffer[0] == '.') {
                tok.type = TOKEN_DIRECTIVE;
            } else if (buffer[0] >= '0' && buffer[0] <= '9') {
                tok.type = TOKEN_NUMBER;
            } else {
                tok.type = TOKEN_IDENTIFIER;
            }
            snprintf(tok.text, sizeof(tok.text), "%s", buffer);
            goto push_token;
        }

        tok.type = TOKEN_UNKNOWN;
        snprintf(tok.text, sizeof(tok.text), "%c", ch);
        state.index++;
        state.column++;

    push_token:
        if (out_tokens->count == out_tokens->capacity) {
            size_t new_cap = out_tokens->capacity == 0 ? 16 : out_tokens->capacity * 2;
            out_tokens->items = (token *)xrealloc(out_tokens->items, new_cap * sizeof(token));
            out_tokens->capacity = new_cap;
        }
        out_tokens->items[out_tokens->count++] = tok;
    }

    {
        token eof_tok;
        memset(&eof_tok, 0, sizeof(eof_tok));
        eof_tok.type = TOKEN_EOF;
        eof_tok.line = state.line;
        eof_tok.column = state.column;
        snprintf(eof_tok.text, sizeof(eof_tok.text), "EOF");
        if (out_tokens->count == out_tokens->capacity) {
            size_t new_cap = out_tokens->capacity == 0 ? 16 : out_tokens->capacity * 2;
            out_tokens->items = (token *)xrealloc(out_tokens->items, new_cap * sizeof(token));
            out_tokens->capacity = new_cap;
        }
        out_tokens->items[out_tokens->count++] = eof_tok;
    }

    return 0;
}

void lexer_free_tokens(token_list *tokens) {
    if (!tokens) {
        return;
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}
