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
    if (!(isalpha(*p) || *p == '_' || *p == '.' || *p == '$')) return 0;
    for (p++; *p; p++) {
        if (!(isalnum(*p) || *p == '_' || *p == '.' || *p == '$')) return 0;
    }
    return 1;
}

static int parse_number(const char *text, int64_t *value) {
    char buffer[MAX_OPERAND_TEXT];
    char *end;
    long long parsed;
    size_t length = strlen(text);

    if (length == 0 || length >= sizeof(buffer)) return -1;
    memcpy(buffer, text, length + 1);

    if (length > 1 && (buffer[length - 1] == 'h' || buffer[length - 1] == 'H')) {
        buffer[length - 1] = '\0';
        errno = 0;
        parsed = strtoll(buffer, &end, 16);
    } else {
        errno = 0;
        parsed = strtoll(buffer, &end, 0);
    }

    if (errno == ERANGE || end == buffer || *end != '\0') return -1;
    *value = (int64_t)parsed;
    return 0;
}

static int add_label(label_entry **labels, size_t *count, size_t *capacity,
                     const char *name, size_t offset) {
    size_t i;
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
            *offset = labels[i].offset;
            return 0;
        }
    }
    return -1;
}

typedef struct {
    int number;
    int bits;
    int extended;
} register_info;

static int parse_register(const char *name, register_info *reg) {
    static const char *names64[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi"};
    static const char *names32[] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi"};
    size_t i;
    char lower[16];
    size_t n = strlen(name);

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

    if (lower[0] == 'r' && lower[1] >= '8' && lower[1] <= '9') {
        char *end;
        long number = strtol(lower + 1, &end, 10);
        if (number >= 8 && number <= 15 && *end == '\0') {
            reg->number = (int)number;
            reg->bits = 64;
            reg->extended = 1;
            return 0;
        }
    }

    if (lower[0] == 'r' && lower[1] >= '8' && lower[1] <= '9' && lower[2] == 'd' && lower[3] == '\0') {
        char *end;
        long number = strtol(lower + 1, &end, 10);
        if (number >= 8 && number <= 15 && end == lower + 2) {
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

static int parse_operands(char *text, source_statement *stmt) {
    char *comma = strchr(text, ',');
    char *first = trim(text);

    if (*first == '\0') return 0;
    if (!comma) {
        if (strlen(first) >= sizeof(stmt->operands[0])) return -1;
        strcpy(stmt->operands[0], first);
        stmt->operand_count = 1;
        return 0;
    }

    if (strchr(comma + 1, ',') != NULL) return -1;
    *comma = '\0';
    first = trim(first);
    comma = trim(comma + 1);
    if (*first == '\0' || *comma == '\0' || strlen(first) >= sizeof(stmt->operands[0]) || strlen(comma) >= sizeof(stmt->operands[1])) return -1;
    strcpy(stmt->operands[0], first);
    strcpy(stmt->operands[1], comma);
    stmt->operand_count = 2;
    return 0;
}

static int parse_line(char *line, size_t line_number, source_statement *stmt) {
    char *comment = strchr(line, ';');
    char *text;
    char *colon;
    char *end;

    memset(stmt, 0, sizeof(*stmt));
    stmt->line = line_number;
    if (comment) *comment = '\0';
    text = trim(line);
    if (*text == '\0') return 0;

    colon = strchr(text, ':');
    if (colon) {
        *colon = '\0';
        text = trim(text);
        if (!valid_name(text) || strlen(text) >= sizeof(stmt->label)) return -1;
        strcpy(stmt->label, text);
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

static int instruction_size(assembler *as, source_statement *stmt) {
    register_info r1, r2;
    int64_t immediate;
    char mnemonic[32];
    size_t i;

    for (i = 0; stmt->mnemonic[i]; i++) mnemonic[i] = (char)toupper((unsigned char)stmt->mnemonic[i]);
    mnemonic[i] = '\0';

    if (strcmp(mnemonic, "NOP") == 0 || strcmp(mnemonic, "RET") == 0) {
        if (stmt->operand_count != 0) return -1;
        return 1;
    }

    if (strcmp(mnemonic, "JMP") == 0) {
        if (stmt->operand_count != 1 || as->info.word_size_bits == 16) return -1;
        return 5;
    }

    if (strcmp(mnemonic, "MOV") == 0 || strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0) {
        if (stmt->operand_count != 2) return -1;

        if (operand_is_register(stmt->operands[1])) {
            if (parse_register(stmt->operands[0], &r1) != 0 || parse_register(stmt->operands[1], &r2) != 0) return -1;
            if (r1.bits != r2.bits) return -1;
            if (as->info.word_size_bits == 64) return 3;
            return 2;
        }

        if (parse_register(stmt->operands[0], &r1) != 0) return -1;
        if (r1.bits == 64 && as->info.word_size_bits != 64) return -1;
        if (parse_number(stmt->operands[1], &immediate) != 0) return -1;
        if (strcmp(mnemonic, "MOV") == 0) {
            if (r1.bits == 64) return 10;
            if (immediate < INT32_MIN || immediate > UINT32_MAX) return -1;
            return 5 + r1.extended;
        }
        if (r1.bits == 64 && (immediate < INT32_MIN || immediate > INT32_MAX)) return -1;
        if (immediate >= -128 && immediate <= 127) return 3 + (r1.bits == 64) + r1.extended;
        if (immediate < INT32_MIN || immediate > INT32_MAX) return -1;
        return 6 + (r1.bits == 64) + r1.extended;
    }

    return -1;
}

static void write_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static int encode_statement(assembler *as, const source_statement *stmt,
                            const label_entry *labels, size_t label_count,
                            uint8_t *out, size_t *written) {
    register_info r1, r2;
    int64_t immediate;
    char mnemonic[32];
    size_t i;

    for (i = 0; stmt->mnemonic[i]; i++) mnemonic[i] = (char)toupper((unsigned char)stmt->mnemonic[i]);
    mnemonic[i] = '\0';
    *written = 0;

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
        size_t destination;
        int64_t displacement;
        if (find_label(labels, label_count, stmt->operands[0], &destination) != 0) {
            if (parse_number(stmt->operands[0], &immediate) != 0 || immediate < 0 || (uint64_t)immediate > SIZE_MAX) return -1;
            destination = (size_t)immediate;
        }
        displacement = (int64_t)destination - (int64_t)(stmt->offset + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) return -1;
        out[0] = 0xe9;
        write_u32(out + 1, (uint32_t)(int32_t)displacement);
        *written = 5;
        return 0;
    }

    if (strcmp(mnemonic, "MOV") == 0 && operand_is_register(stmt->operands[1])) {
        if (parse_register(stmt->operands[0], &r1) != 0 || parse_register(stmt->operands[1], &r2) != 0) return -1;
        if (r1.bits != r2.bits) return -1;
        if (r1.bits == 64) {
            out[0] = 0x48;
            out[1] = 0x8b;
            out[2] = (uint8_t)(0xc0 | ((r2.number & 7) << 3) | (r1.number & 7));
            *written = 3;
            return 0;
        }
        out[0] = 0x8b;
        out[1] = (uint8_t)(0xc0 | ((r2.number & 7) << 3) | (r1.number & 7));
        *written = 2;
        return 0;
    }

    if (parse_register(stmt->operands[0], &r1) != 0) return -1;
    if (parse_number(stmt->operands[1], &immediate) != 0) return -1;

    if (strcmp(mnemonic, "MOV") == 0) {
        size_t pos = 0;
        if (r1.bits == 64) out[pos++] = (uint8_t)(0x48 | (r1.extended ? 1 : 0));
        else if (r1.extended) out[pos++] = 0x41;
        out[pos++] = (uint8_t)(0xb8 + (r1.number & 7));
        if (r1.bits == 64) {
            uint64_t value = (uint64_t)immediate;
            for (size_t j = 0; j < 8; j++) out[pos++] = (uint8_t)(value >> (j * 8));
        } else {
            write_u32(out + pos, (uint32_t)immediate);
            pos += 4;
        }
        *written = pos;
        return 0;
    }

    if (strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "SUB") == 0) {
        int is_sub = strcmp(mnemonic, "SUB") == 0;
        int use_short = immediate >= -128 && immediate <= 127;
        size_t pos = 0;

        if (r1.bits == 64 || r1.extended) out[pos++] = (uint8_t)(0x40 | (r1.bits == 64 ? 8 : 0) | (r1.extended ? 1 : 0));
        out[pos++] = (uint8_t)(use_short ? 0x83 : 0x81);
        out[pos++] = (uint8_t)(0xc0 | ((is_sub ? 5 : 0) << 3) | (r1.number & 7));
        if (use_short) out[pos++] = (uint8_t)immediate;
        else {
            write_u32(out + pos, (uint32_t)immediate);
            pos += 4;
        }
        *written = pos;
        return 0;
    }

    (void)as;
    return -1;
}

static void assembler_default_target(assembler *as) {
    const target_backend *backend = target_find(TARGET_X86_64);
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
            int size = instruction_size(as, &stmt);
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
