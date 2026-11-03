#include "assembler.h"
#include "target.h"

const target_backend *target_find(cpu_target target);

static const target_backend backend = {
    TARGET_INTEL_8008,
    "intel_8008",
    8,
    8,
    1,
    NULL,
    NULL
};

const target_backend *target_find(cpu_target target) {
    if (target == TARGET_INTEL_8008) {
        return &backend;
    }
    return NULL;
}
