#include <stdint.h>

#include "assembler.h"
#include "target.h"

const target_backend *target_find(cpu_target target);

static const target_backend intel_8008 = {
    TARGET_INTEL_8008,
    "intel_8008",
    8,
    8,
    1,
    NULL,
    NULL
};

static const target_backend intel_8085 = {
    TARGET_INTEL_8085,
    "intel_8085",
    8,
    16,
    1,
    NULL,
    NULL
};

static const target_backend intel_8086 = {
    TARGET_INTEL_8086,
    "intel_8086",
    16,
    16,
    1,
    NULL,
    NULL
};

static const target_backend intel_8088 = {
    TARGET_INTEL_8088,
    "intel_8088",
    16,
    16,
    1,
    NULL,
    NULL
};

static const target_backend intel_i386 = {
    TARGET_INTEL_I386,
    "intel_i386",
    32,
    32,
    1,
    NULL,
    NULL
};

static const target_backend x86_64 = {
    TARGET_X86_64,
    "x86_64",
    64,
    64,
    1,
    NULL,
    NULL
};

const target_backend *target_find(cpu_target target) {
    const target_backend *targets[] = {
        &intel_8008,
        &intel_8085,
        &intel_8086,
        &intel_8088,
        &intel_i386,
        &x86_64
    };
    size_t i;

    for (i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        if (targets[i]->target == target) {
            return targets[i];
        }
    }

    return NULL;
}
