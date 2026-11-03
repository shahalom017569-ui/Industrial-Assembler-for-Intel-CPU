#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "assembler.h"
#include "target.h"

#define MAX_SOURCE_LINE 4096
#define MAX_CODE_SIZE (64U * 1024U * 1024U)
#define MAX_OPERAND_TEXT 256

typedef struct {
    char label[128];
    char mnemonic[32];
    char operands[2][MAX_OPERAND_TEXT];
    size_t operand_count;
    size_t line;
    size_t offset;
    size_t size;
} source_statement;

typedef struct {
    char name[128];
    size_t offset;
} label_entry;

typedef struct {
    int number;
    int bits;
    int extended;
} register_info;

struct assembler {
    target_info info;
    char error[ASSEMBLER_MAX_ERROR_LEN];
};

static void set_error_at(assembler *as, size_t line, const char *message) {
    if (line != 0) {
        snprintf(as->error, sizeof(as->error), "line %zu: %s", line, message);
    } else {
        snprintf(as->error, sizeof(as->error), "%s", message);
    }
}

static int grow_array(void **items, size_t *capacity, size_t item_size) {
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    void *memory;
    if (next < *capacity || next > SIZE_MAX / item_size) return -1;
    memory = realloc(*items, next * item_size);
    if (!memory) return -1;
    *items = memory;
    *capacity = next;
    return 0;
}

static char *trim(char *text) {
    char *end;
    while (isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

static int valid_name(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (*p == '\0') return 0;
    if (!(isalpha(*p) || *p == '_' || *p == '.' || *p == '$')) return 0;
    for (p++; *p; p++) {
        if (!(isalnum(*p) || *p == '_' || *p == '.' || *p == '$')) return 0;
    }
    return 1;
}

static int parse_register(const char *name, register_info *reg) {
    static const char *names64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"};
    static const char *names32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    size_t i;
    char lower[32];
    size_t n = strlen(name);

    if (!reg || !name) return -1;
    if (n >= sizeof(lower)) return -1;
    for (i = 0; i <= n; i++) lower[i] = (char)tolower((unsigned char)name[i]);

    for (i = 0; i < 8; i++) {
        if (strcmp(lower, names64[i]) == 0) {
            reg->number = (int)i;
            reg->bits = 64;
            reg->extended = 0;
            return 0;
        }
        if (strcmp(lower, names32[i]) == 0) {
            reg->number = (int)i;
            reg->bits = 32;
            reg->extended = 0;
            return 0;
        }
    }

    if (lower[0] == 'r' && lower[1] >= '0' && lower[1] <= '9' && lower[2] == '\0') {
        long number = strtol(lower + 1, NULL, 10);
        if (number >= 8 && number <= 15) {
            reg->number = (int)number;
            reg->bits = 64;
            reg->extended = 1;
            return 0;
        }
    }

    if (lower[0] == 'r' && lower[1] >= '0' && lower[1] <= '9' && lower[2] == 'd' && lower[3] == '\0') {
        long number = strtol(lower + 1, NULL, 10);
        if (number >= 8 && number <= 15) {
            reg->number = (int)number;
            reg->bits = 32;
            reg->extended = 1;
            return 0;
        }
    }

    return -1;
}

static int operand_is_register(const char *text) {
    register_info reg;
    return parse_register(text, &reg) == 0;
}

static int add_label(label_entry **labels, size_t *count, size_t *capacity,
                     const char *name, size_t offset) {
    size_t i;
    if (!labels || !count || !capacity || !name) return -1;
    for (i = 0; i < *count; i++) {
        if (strcasecmp((*labels)[i].name, name) == 0) return -1;
    }
    if (*count == *capacity && grow_array((void **)labels, capacity, sizeof(**labels)) != 0) return -2;
    snprintf((*labels)[*count].name, sizeof((*labels)[*count].name), "%s", name);
    (*labels)[*count].offset = offset;
    (*count)++;
    return 0;
}

static int find_label(const label_entry *labels, size_t count, const char *name, size_t *offset) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcasecmp(labels[i].name, name) == 0) {
            if (offset) *offset = labels[i].offset;
            return 0;
        }
    }
    return -1;
}

static void split_operands_string(const char *text, char *left, size_t left_size,
                                  char *right, size_t right_size) {
    char buffer[MAX_OPERAND_TEXT];
    char *comma;

    if (!text || !left || !right) return;
    left[0] = '\0';
    right[0] = '\0';
    snprintf(buffer, sizeof(buffer), "%s", text);
    comma = strchr(buffer, ',');
    if (!comma) {
        snprintf(left, left_size, "%s", trim(buffer));
        return;
    }
    *comma = '\0';
    snprintf(left, left_size, "%s", trim(buffer));
    snprintf(right, right_size, "%s", trim(comma + 1));
}

static int parse_operands(char *text, source_statement *stmt) {
    char *comma = NULL;
    char *start;

    if (!text || !stmt) return -1;
    memset(stmt->operands, 0, sizeof(stmt->operands));
    stmt->operand_count = 0;

    start = trim(text);
    if (*start == '\0') return 0;

    comma = strchr(start, ',');
    if (comma) {
        char left[MAX_OPERAND_TEXT];
        char right[MAX_OPERAND_TEXT];
        *comma = '\0';
        snprintf(left, sizeof(left), "%s", trim(start));
        snprintf(right, sizeof(right), "%s", trim(comma + 1));
        if (left[0] == '\0' || right[0] == '\0') return -1;
        if (strlen(left) >= sizeof(stmt->operands[0]) || strlen(right) >= sizeof(stmt->operands[1])) return -1;
        snprintf(stmt->operands[0], sizeof(stmt->operands[0]), "%s", left);
        snprintf(stmt->operands[1], sizeof(stmt->operands[1]), "%s", right);
        stmt->operand_count = 2;
        return 0;
    }

    if (strlen(start) >= sizeof(stmt->operands[0])) return -1;
    snprintf(stmt->operands[0], sizeof(stmt->operands[0]), "%s", start);
    stmt->operand_count = 1;
    return 0;
}

static int parse_line(char *line, size_t line_number, source_statement *stmt) {
    char *comment = NULL;
    char *text;
    char *colon;
    char *end;

    if (!line || !stmt) return -1;
    memset(stmt, 0, sizeof(*stmt));
    stmt->line = line_number;

    comment = strchr(line, ';');
    if (comment) *comment = '\0';
    text = trim(line);
    if (*text == '\0') return 0;

    colon = strchr(text, ':');
    if (colon) {
        *colon = '\0';
        text = trim(text);
        if (!valid_name(text) || strlen(text) >= sizeof(stmt->label)) return -1;
        snprintf(stmt->label, sizeof(stmt->label), "%s", text);
        text = trim(colon + 1);
        if (*text == '\0') return 1;
    }

    end = text;
    while (*end && !isspace((unsigned char)*end)) end++;
    if ((size_t)(end - text) == 0 || (size_t)(end - text) >= sizeof(stmt->mnemonic)) return -1;
    memcpy(stmt->mnemonic, text, (size_t)(end - text));
    stmt->mnemonic[end - text] = '\0';

    text = trim(end);
    if (parse_operands(text, stmt) != 0) return -1;
    return 1;
}

static int expr_parse_primary(const char **p, const label_entry *labels, size_t label_count,
                              size_t current_offset, int64_t *result);

static int expr_parse_unary(const char **p, const label_entry *labels, size_t label_count,
                           size_t current_offset, int64_t *result) {
    int unary = 1;
    int64_t value = 0;

    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (**p == '+') {
        (*p)++;
    } else if (**p == '-') {
        unary = -1;
        (*p)++;
    }
    if (expr_parse_primary(p, labels, label_count, current_offset, &value) != 0) return -1;
    *result = unary * value;
    return 0;
}

static int expr_parse_term(const char **p, const label_entry *labels, size_t label_count,
                          size_t current_offset, int64_t *result) {
    int64_t left = 0;
    int64_t right = 0;

    if (expr_parse_unary(p, labels, label_count, current_offset, &left) != 0) return -1;
    while (**p) {
        while (**p && isspace((unsigned char)**p)) (*p)++;
        if (**p == '*') {
            (*p)++;
            if (expr_parse_unary(p, labels, label_count, current_offset, &right) != 0) return -1;
            left *= right;
        } else if (**p == '/') {
            (*p)++;
            if (expr_parse_unary(p, labels, label_count, current_offset, &right) != 0) return -1;
            if (right == 0) return -1;
            left /= right;
        } else {
            break;
        }
    }
    *result = left;
    return 0;
}

static int expr_parse_expr(const char **p, const label_entry *labels, size_t label_count,
                          size_t current_offset, int64_t *result) {
    int64_t left = 0;
    int64_t right = 0;

    if (expr_parse_term(p, labels, label_count, current_offset, &left) != 0) return -1;
    while (**p) {
        while (**p && isspace((unsigned char)**p)) (*p)++;
        if (**p == '+') {
            (*p)++;
            if (expr_parse_term(p, labels, label_count, current_offset, &right) != 0) return -1;
            left += right;
        } else if (**p == '-') {
            (*p)++;
            if (expr_parse_term(p, labels, label_count, current_offset, &right) != 0) return -1;
            left -= right;
        } else {
            break;
        }
    }
    *result = left;
    return 0;
}

static int expr_parse_primary(const char **p, const label_entry *labels, size_t label_count,
                              size_t current_offset, int64_t *result) {
    char ident[128];
    size_t n = 0;
    char *end = NULL;
    long long number = 0;

    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (**p == '(') {
        (*p)++;
        int64_t value = 0;
        if (expr_parse_expr(p, labels, label_count, current_offset, &value) != 0) return -1;
        while (**p && isspace((unsigned char)**p)) (*p)++;
        if (**p != ')') return -1;
        (*p)++;
        *result = value;
        return 0;
    }

    if (**p == '$') {
        (*p)++;
        *result = (int64_t)current_offset;
        return 0;
    }

    if (isdigit((unsigned char)**p) || (**p == '0' && ((*p)[1] == 'x' || (*p)[1] == 'X'))) {
        errno = 0;
        if (**p == '0' && ((*p)[1] == 'x' || (*p)[1] == 'X')) {
            number = strtoll(*p, &end, 16);
        } else {
            number = strtoll(*p, &end, 0);
        }
        if (errno == ERANGE || end == *p) return -1;
        *p = end;
        *result = (int64_t)number;
        return 0;
    }

    if (isalpha((unsigned char)**p) || **p == '_' || **p == '.') {
        while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '.')) {
            if (n + 1 < sizeof(ident)) ident[n++] = *(*p)++;
            else return -1;
        }
        ident[n] = '\0';
        for (size_t i = 0; i < label_count; i++) {
            if (strcasecmp(labels[i].name, ident) == 0) {
                *result = (int64_t)labels[i].offset;
                return 0;
            }
        }
        return -1;
    }

    return -1;
}

static int expression_value(const char *text, const label_entry *labels, size_t label_count,
                           size_t current_offset, int64_t *value) {
    const char *p = text;
    int64_t result = 0;

    if (!text || !value) return -1;
    if (expr_parse_expr(&p, labels, label_count, current_offset, &result) != 0) return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\0') return -1;
    *value = result;
    return 0;
}

static int rex_prefix_for_registers(int bits, int dst_extended, int src_extended) {
    int rex = 0x40;
    if (bits == 64) rex |= 0x08;
    if (dst_extended) rex |= 0x01;
    if (src_extended) rex |= 0x04;
    return rex;
}

static int instruction_size(assembler *as, source_statement *stmt,
                            const label_entry *labels, size_t label_count) {
    register_info r1, r2;
    char mnemonic[32];
    size_t i;
    int64_t imm = 0;

    for (i = 0; stmt->mnemonic[i]; i++) mnemonic[i] = (char)toupper((unsigned char)stmt->mnemonic[i]);
    mnemonic[i] = '\0';

    if (strcasecmp(mnemonic, "db") == 0 || strcasecmp(mnemonic, "dw") == 0 ||
        strcasecmp(mnemonic, "dd") == 0 || strcasecmp(mnemonic, "dq") == 0) {
        int width = strcasecmp(mnemonic, "db") == 0 ? 1 :
                    strcasecmp(mnemonic, "dw") == 0 ? 2 :
                    strcasecmp(mnemonic, "dd") == 0 ? 4 : 8;
        char opbuf[MAX_OPERAND_TEXT];
        char *comma;
        size_t total = 0;
        snprintf(opbuf, sizeof(opbuf), "%s", stmt->operands[0]);
        while (opbuf[0] != '\0') {
            comma = strchr(opbuf, ',');
            if (comma) *comma = '\0';
            if (expression_value(trim(opbuf), labels, label_count, stmt->offset, &imm) != 0) return -1;
            total += (size_t)width;
            if (!comma) break;
            memmove(opbuf, comma + 1, strlen(comma + 1) + 1);
        }
        return (int)total;
    }

    if (strcmp(mnemonic, "NOP") == 0 || strcmp(mnemonic, "RET") == 0) {
        if (stmt->operand_count != 0) return -1;
        return 1;
    }

    if (strcmp(mnemonic, "JMP") == 0) {
        if (stmt->operand_count != 1 || as->info.word_size_bits == 16) return -1;
        return 5;
    }

    if ((strcmp(mnemonic, "MOV") == 0 || strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0 ||
         strcmp(mnemonic, "AND") == 0 || strcmp(mnemonic, "OR") == 0 || strcmp(mnemonic, "XOR") == 0) &&
        stmt->operand_count == 2) {
        char left[MAX_OPERAND_TEXT];
        char right[MAX_OPERAND_TEXT];
        split_operands_string(stmt->operands[0], left, sizeof(left), right, sizeof(right));
        if (right[0] == '\0') return -1;
        if (operand_is_register(right)) {
            if (parse_register(stmt->operands[0], &r1) != 0 || parse_register(right, &r2) != 0) return -1;
            if (r1.bits != r2.bits) return -1;
            if (as->info.word_size_bits == 64) return 3;
            return 2;
        }
        if (parse_register(stmt->operands[0], &r1) != 0) return -1;
        if (expression_value(stmt->operands[1], labels, label_count, stmt->offset, &imm) != 0) return -1;
        if (r1.bits == 64 && as->info.word_size_bits != 64) return -1;
        if (strcmp(mnemonic, "MOV") == 0) {
            if (r1.bits == 64) return 10;
            if (imm < INT32_MIN || imm > UINT32_MAX) return -1;
            return 5 + (r1.extended ? 1 : 0);
        }
        if (r1.bits == 64 && (imm < INT32_MIN || imm > INT32_MAX)) return -1;
        if (imm >= -128 && imm <= 127) return 3 + ((r1.bits == 64) ? 1 : 0) + (r1.extended ? 1 : 0);
        if (imm < INT32_MIN || imm > INT32_MAX) return -1;
        return 6 + ((r1.bits == 64) ? 1 : 0) + (r1.extended ? 1 : 0);
    }

    if ((strcmp(mnemonic, "INC") == 0 || strcmp(mnemonic, "DEC") == 0 ||
         strcmp(mnemonic, "NEG") == 0 || strcmp(mnemonic, "NOT") == 0) &&
        stmt->operand_count == 1) {
        if (parse_register(stmt->operands[0], &r1) != 0) return -1;
        if (r1.bits == 64) return 3;
        return 2;
    }

    return -1;
}

static void write_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static void write_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void write_u64(uint8_t *out, uint64_t value) {
    for (size_t i = 0; i < 8; i++) out[i] = (uint8_t)(value >> (i * 8));
}

static int encode_register_move(const char *dst, const char *src, uint8_t *out, size_t *written) {
    register_info dst_reg, src_reg;
    size_t pos = 0;
    if (parse_register(dst, &dst_reg) != 0 || parse_register(src, &src_reg) != 0) return -1;
    if (dst_reg.bits != src_reg.bits) return -1;
    if (dst_reg.bits == 64) {
        out[pos++] = (uint8_t)rex_prefix_for_registers(64, dst_reg.extended, src_reg.extended);
        out[pos++] = 0x8b;
        out[pos++] = (uint8_t)(0xc0 | ((src_reg.number & 7) << 3) | (dst_reg.number & 7));
        *written = pos;
        return 0;
    }

    if (dst_reg.extended || src_reg.extended) {
        out[pos++] = (uint8_t)rex_prefix_for_registers(32, dst_reg.extended, src_reg.extended);
    }
    out[pos++] = 0x8b;
    out[pos++] = (uint8_t)(0xc0 | ((src_reg.number & 7) << 3) | (dst_reg.number & 7));
    *written = pos;
    return 0;
}

static int encode_register_imm(const char *reg_text, int64_t immediate, uint8_t *out, size_t *written) {
    register_info reg;
    size_t pos = 0;
    if (parse_register(reg_text, &reg) != 0) return -1;
    if (reg.bits == 64) {
        out[pos++] = (uint8_t)(0x48 | (reg.extended ? 0x01 : 0));
        out[pos++] = (uint8_t)(0xb8 + (reg.number & 7));
        write_u64(out + pos, (uint64_t)immediate);
        pos += 8;
        *written = pos;
        return 0;
    }
    if (reg.extended) out[pos++] = 0x41;
    out[pos++] = (uint8_t)(0xb8 + (reg.number & 7));
    write_u32(out + pos, (uint32_t)immediate);
    pos += 4;
    *written = pos;
    return 0;
}

static int encode_immediate_binop(const char *mnemonic, const char *reg_text, int64_t immediate,
                                 uint8_t *out, size_t *written) {
    register_info reg;
    int use_short = immediate >= -128 && immediate <= 127;
    int opcode_reg = 0;
    size_t pos = 0;

    if (parse_register(reg_text, &reg) != 0) return -1;
    if (reg.bits == 64 || reg.extended) out[pos++] = (uint8_t)(0x40 | (reg.bits == 64 ? 0x08 : 0) | (reg.extended ? 0x01 : 0));
    out[pos++] = (uint8_t)(use_short ? 0x83 : 0x81);
    if (strcasecmp(mnemonic, "ADD") == 0) opcode_reg = 0;
    else if (strcasecmp(mnemonic, "SUB") == 0) opcode_reg = 5;
    else if (strcasecmp(mnemonic, "AND") == 0) opcode_reg = 4;
    else if (strcasecmp(mnemonic, "OR") == 0) opcode_reg = 1;
    else if (strcasecmp(mnemonic, "XOR") == 0) opcode_reg = 6;
    else return -1;
    out[pos++] = (uint8_t)(0xc0 | ((opcode_reg & 7) << 3) | (reg.number & 7));
    if (use_short) out[pos++] = (uint8_t)immediate;
    else {
        write_u32(out + pos, (uint32_t)immediate);
        pos += 4;
    }
    *written = pos;
    return 0;
}

static int encode_inc_dec_not_neg(const char *mnemonic, const char *reg_text, uint8_t *out, size_t *written) {
    register_info reg;
    size_t pos = 0;
    int opcode = 0;

    if (parse_register(reg_text, &reg) != 0) return -1;
    if (reg.bits == 64 || reg.extended) out[pos++] = (uint8_t)(0x40 | (reg.bits == 64 ? 0x08 : 0) | (reg.extended ? 0x01 : 0));
    out[pos++] = 0xff;
    if (strcasecmp(mnemonic, "INC") == 0) opcode = 0;
    else if (strcasecmp(mnemonic, "DEC") == 0) opcode = 1;
    else if (strcasecmp(mnemonic, "NEG") == 0) opcode = 3;
    else if (strcasecmp(mnemonic, "NOT") == 0) opcode = 2;
    else return -1;
    out[pos++] = (uint8_t)(0xc0 | ((opcode & 7) << 3) | (reg.number & 7));
    *written = pos;
    return 0;
}

static int encode_statement(assembler *as, const source_statement *stmt,
                            const label_entry *labels, size_t label_count,
                            uint8_t *out, size_t *written) {
    char mnemonic[32];
    char left[MAX_OPERAND_TEXT];
    char right[MAX_OPERAND_TEXT];
    size_t i;
    int64_t imm = 0;

    for (i = 0; stmt->mnemonic[i]; i++) mnemonic[i] = (char)toupper((unsigned char)stmt->mnemonic[i]);
    mnemonic[i] = '\0';
    *written = 0;

    if (strcasecmp(mnemonic, "db") == 0 || strcasecmp(mnemonic, "dw") == 0 ||
        strcasecmp(mnemonic, "dd") == 0 || strcasecmp(mnemonic, "dq") == 0) {
        int width = strcasecmp(mnemonic, "db") == 0 ? 1 :
                    strcasecmp(mnemonic, "dw") == 0 ? 2 :
                    strcasecmp(mnemonic, "dd") == 0 ? 4 : 8;
        char opbuf[MAX_OPERAND_TEXT];
        char *comma;
        size_t pos = 0;
        snprintf(opbuf, sizeof(opbuf), "%s", stmt->operands[0]);
        while (opbuf[0] != '\0') {
            int64_t value = 0;
            comma = strchr(opbuf, ',');
            if (comma) *comma = '\0';
            if (expression_value(trim(opbuf), labels, label_count, stmt->offset, &value) != 0) return -1;
            if (width == 1) out[pos++] = (uint8_t)(value & 0xff);
            else if (width == 2) write_u16(out + pos, (uint16_t)(value & 0xffff)), pos += 2;
            else if (width == 4) write_u32(out + pos, (uint32_t)(value & 0xffffffffu)), pos += 4;
            else write_u64(out + pos, (uint64_t)value), pos += 8;
            if (!comma) break;
            memmove(opbuf, comma + 1, strlen(comma + 1) + 1);
        }
        *written = pos;
        return 0;
    }

    if (strcmp(mnemonic, "NOP") == 0) {
        out[0] = 0x90;
        *written = 1;
        return 0;
    }

    if (strcmp(mnemonic, "RET") == 0) {
        out[0] = 0xc3;
        *written = 1;
        return 0;
    }

    if (strcmp(mnemonic, "JMP") == 0) {
        size_t destination = 0;
        int64_t displacement;
        if (find_label(labels, label_count, stmt->operands[0], &destination) != 0) {
            if (expression_value(stmt->operands[0], labels, label_count, stmt->offset, &imm) != 0) return -1;
            destination = (size_t)imm;
        }
        displacement = (int64_t)destination - (int64_t)(stmt->offset + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) return -1;
        out[0] = 0xe9;
        write_u32(out + 1, (uint32_t)(int32_t)displacement);
        *written = 5;
        return 0;
    }

    if (stmt->operand_count == 2) {
        char combined[2][MAX_OPERAND_TEXT];
        memset(combined, 0, sizeof(combined));
        snprintf(combined[0], sizeof(combined[0]), "%s", stmt->operands[0]);
        snprintf(combined[1], sizeof(combined[1]), "%s", stmt->operands[1]);

        if (strcmp(mnemonic, "MOV") == 0 && operand_is_register(combined[1])) {
            return encode_register_move(combined[0], combined[1], out, written);
        }

        if (strcmp(mnemonic, "MOV") == 0) {
            if (expression_value(combined[1], labels, label_count, stmt->offset, &imm) != 0) return -1;
            return encode_register_imm(combined[0], imm, out, written);
        }

        if ((strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0 ||
             strcmp(mnemonic, "AND") == 0 || strcmp(mnemonic, "OR") == 0 ||
             strcmp(mnemonic, "XOR") == 0) && operand_is_register(combined[0])) {
            if (expression_value(combined[1], labels, label_count, stmt->offset, &imm) != 0) return -1;
            return encode_immediate_binop(mnemonic, combined[0], imm, out, written);
        }
    }

    if (stmt->operand_count == 1) {
        split_operands_string(stmt->operands[0], left, sizeof(left), right, sizeof(right));
        if ((strcmp(mnemonic, "INC") == 0 || strcmp(mnemonic, "DEC") == 0 ||
             strcmp(mnemonic, "NEG") == 0 || strcmp(mnemonic, "NOT") == 0) && right[0] == '\0') {
            return encode_inc_dec_not_neg(mnemonic, left, out, written);
        }
    }

    (void)as;
    return -1;
}

static void assembler_default_target(assembler *as) {
    const target_backend *backend = target_find(TARGET_X86_64);
    if (!backend) {
        as->info.name = "x86_64";
        as->info.target = TARGET_X86_64;
        as->info.word_size_bits = 64;
        as->info.address_size_bits = 64;
        as->info.little_endian = 1;
        return;
    }
    as->info.name = (char *)backend->name;
    as->info.target = backend->target;
    as->info.word_size_bits = backend->word_size_bits;
    as->info.address_size_bits = backend->address_size_bits;
    as->info.little_endian = backend->little_endian;
}

assembler *assembler_create(void) {
    assembler *as = (assembler *)calloc(1, sizeof(*as));
    if (as) assembler_default_target(as);
    return as;
}

void assembler_destroy(assembler *as) {
    free(as);
}

int assembler_set_target(assembler *as, cpu_target target) {
    const target_backend *backend;
    if (!as) return -1;
    backend = target_find(target);
    if (!backend) return assembler_set_error(as, "unknown CPU target");
    as->info.name = (char *)backend->name;
    as->info.target = backend->target;
    as->info.word_size_bits = backend->word_size_bits;
    as->info.address_size_bits = backend->address_size_bits;
    as->info.little_endian = backend->little_endian;
    return 0;
}

int assembler_set_error(assembler *as, const char *message) {
    if (!as || !message) return -1;
    snprintf(as->error, sizeof(as->error), "%s", message);
    return -1;
}

const char *assembler_error(assembler *as) {
    return as ? as->error : "assembler is null";
}

const target_info *assembler_target_info(const assembler *as) {
    return as ? &as->info : NULL;
}

int assembler_assemble_file(assembler *as, const char *source_path, const char *output_path,
                            const assembler_options *options) {
    FILE *source = NULL;
    FILE *output = NULL;
    char *line = NULL;
    size_t line_capacity = 0, line_number = 0, statement_count = 0, statement_capacity = 0;
    size_t label_count = 0, label_capacity = 0, code_size = 0, i;
    source_statement *statements = NULL;
    label_entry *labels = NULL;
    uint8_t *code = NULL;
    int result = -1;

    (void)options;
    if (!as || !source_path || !output_path) return -1;
    as->error[0] = '\0';

    if (!target_has_encoder(as->info.target)) {
        set_error_at(as, 0, "encoding for this CPU target is not implemented yet");
        return -1;
    }

    source = fopen(source_path, "r");
    if (!source) {
        snprintf(as->error, sizeof(as->error), "cannot open source file: %s", source_path);
        goto done;
    }

    while (getline(&line, &line_capacity, source) != -1) {
        source_statement stmt;
        int parsed;
        line_number++;

        if (strlen(line) > MAX_SOURCE_LINE) {
            set_error_at(as, line_number, "source line exceeds limit");
            goto done;
        }

        parsed = parse_line(line, line_number, &stmt);
        if (parsed < 0) {
            set_error_at(as, line_number, "invalid label, mnemonic, or operand syntax");
            goto done;
        }
        if (!parsed) continue;

        if (stmt.label[0]) {
            int added = add_label(&labels, &label_count, &label_capacity, stmt.label, code_size);
            if (added == -1) {
                set_error_at(as, line_number, "duplicate label");
                goto done;
            }
            if (added != 0) {
                set_error_at(as, line_number, "out of memory");
                goto done;
            }
        }

        if (!stmt.mnemonic[0]) continue;
        stmt.offset = code_size;
        {
            int size = instruction_size(as, &stmt, labels, label_count);
            if (size < 0) {
                set_error_at(as, line_number, "unsupported instruction or invalid operands for selected target");
                goto done;
            }
            stmt.size = (size_t)size;
        }

        if (code_size > MAX_CODE_SIZE - stmt.size) {
            set_error_at(as, line_number, "output exceeds size limit");
            goto done;
        }

        code_size += stmt.size;
        if (statement_count == statement_capacity && grow_array((void **)&statements, &statement_capacity, sizeof(*statements)) != 0) {
            set_error_at(as, line_number, "out of memory");
            goto done;
        }
        statements[statement_count++] = stmt;
    }

    if (ferror(source)) {
        set_error_at(as, 0, "error reading source file");
        goto done;
    }

    code = (uint8_t *)malloc(code_size ? code_size : 1);
    if (!code) {
        set_error_at(as, 0, "out of memory");
        goto done;
    }

    for (i = 0; i < statement_count; i++) {
        size_t written;
        if (encode_statement(as, &statements[i], labels, label_count, code + statements[i].offset, &written) != 0 || written != statements[i].size) {
            set_error_at(as, statements[i].line, "cannot encode instruction (unknown label or out-of-range value)");
            goto done;
        }
    }

    output = fopen(output_path, "wb");
    if (!output) {
        snprintf(as->error, sizeof(as->error), "cannot open output file: %s", output_path);
        goto done;
    }
    if (code_size && fwrite(code, 1, code_size, output) != code_size) {
        set_error_at(as, 0, "error writing output file");
        goto done;
    }
    if (fclose(output) != 0) {
        output = NULL;
        set_error_at(as, 0, "error closing output file");
        goto done;
    }
    output = NULL;
    result = 0;

done:
    if (source) fclose(source);
    if (output) fclose(output);
    free(line);
    free(statements);
    free(labels);
    free(code);
    return result;
}

const char *target_name(cpu_target target) {
    const target_backend *backend = target_find(target);
    return backend ? backend->name : "unknown";
}

cpu_target target_from_name(const char *name) {
    if (!name) return TARGET_UNKNOWN;
    if (strcasecmp(name, "80386") == 0 || strcasecmp(name, "i386") == 0 || strcasecmp(name, "intel_i386") == 0) return TARGET_INTEL_I386;
    if (strcasecmp(name, "8086") == 0 || strcasecmp(name, "intel_8086") == 0) return TARGET_INTEL_8086;
    if (strcasecmp(name, "8088") == 0 || strcasecmp(name, "intel_8088") == 0) return TARGET_INTEL_8088;
    if (strcasecmp(name, "8085") == 0 || strcasecmp(name, "intel_8085") == 0) return TARGET_INTEL_8085;
    if (strcasecmp(name, "8008") == 0 || strcasecmp(name, "intel_8008") == 0) return TARGET_INTEL_8008;
    if (strcasecmp(name, "x86_64") == 0 || strcasecmp(name, "amd64") == 0 || strcasecmp(name, "x64") == 0) return TARGET_X86_64;
    return TARGET_UNKNOWN;
}
