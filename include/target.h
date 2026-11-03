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
} target_backend;

const target_backend *target_find(cpu_target target);
const char *target_backend_name(cpu_target target);
int target_has_encoder(cpu_target target);

#endif
