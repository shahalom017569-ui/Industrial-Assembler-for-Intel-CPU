#include "assembler.h"
#include "target.h"

const target_backend *target_find(cpu_target target);

static const target_backend backend = {
    TARGET_INTEL_8086,
    "intel_8086",
    16,
    16,
    1,
    NULL,
    NULL
};

const target_backend *target_find(cpu_target target) {
    if (target == TARGET_INTEL_8086) {
        return &backend;
    }
    return NULL;
}
