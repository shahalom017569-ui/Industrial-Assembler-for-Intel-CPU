#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

#define ASSEMBLER_MAX_PATH 4096
#define ASSEMBLER_MAX_ERROR_LEN 256

typedef enum {
    TARGET_UNKNOWN = 0,
    TARGET_INTEL_8008,
    TARGET_INTEL_8085,
    TARGET_INTEL_8086,
    TARGET_INTEL_8088,
    TARGET_INTEL_I386,
    TARGET_X86_64
} cpu_target;

typedef struct {
    char *name;
    cpu_target target;
    int word_size_bits;
    int address_size_bits;
    int little_endian;
} target_info;

typedef struct {
    char output_path[ASSEMBLER_MAX_PATH];
    char source_path[ASSEMBLER_MAX_PATH];
    cpu_target target;
    int emit_binary;
    int verbose;
} assembler_options;

typedef struct assembler assembler;

assembler *assembler_create(void);
void assembler_destroy(assembler *as);
int assembler_set_target(assembler *as, cpu_target target);
int assembler_assemble_file(assembler *as, const char *source_path, const char *output_path, const assembler_options *options);
int assembler_set_error(assembler *as, const char *message);
const char *assembler_error(assembler *as);
const target_info *assembler_target_info(const assembler *as);

const char *target_name(cpu_target target);
cpu_target target_from_name(const char *name);

#endif
