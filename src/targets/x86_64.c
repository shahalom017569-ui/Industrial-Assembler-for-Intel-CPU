#include "assembler.h"
#include "target.h"

const target_backend *target_find(cpu_target target);

static const target_backend backend = {
    TARGET_INTEL_I386,
    "intel_i386",
    32,
    32,
    1,
    NULL,
    NULL
};

const target_backend *target_find(cpu_target target) {
    if (target == TARGET_INTEL_I386) {
        return &backend;
    }
    return NULL;
}
