#ifndef TARGET_H
#define TARGET_H

#include "assembler.h"
#include <stddef.h>
#include <stdint.h>

struct target_backend;

typedef struct target_backend {
    cpu_target target;
    const char *name;
    int word_size_bits;
    int address_size_bits;
    int little_endian;
    int (*encode_instruction)(void *opaque, const char *mnemonic, const char *operand, uint8_t *out_buffer, size_t out_capacity, size_t *written);
    int (*validate)(void *opaque, const char *mnemonic, const char *operand);
} target_backend;

int target_register_backend(const target_backend *backend);
const target_backend *target_find(cpu_target target);
const char *target_backend_name(cpu_target target);

#endif
