#include "target.h"

static const target_backend backends[] = {
    { TARGET_INTEL_8008, "intel_8008", 8, 8, 1 },
    { TARGET_INTEL_8085, "intel_8085", 8, 16, 1 },
    { TARGET_INTEL_8086, "intel_8086", 16, 16, 1 },
    { TARGET_INTEL_8088, "intel_8088", 16, 16, 1 },
    { TARGET_INTEL_I386, "i386", 32, 32, 1 },
    { TARGET_X86_64, "x86_64", 64, 64, 1 }
};

const target_backend *target_find(cpu_target target) {
    size_t i;
    for (i = 0; i < sizeof(backends) / sizeof(backends[0]); i++) {
        if (backends[i].target == target) return &backends[i];
    }
    return NULL;
}

const char *target_backend_name(cpu_target target) {
    const target_backend *backend = target_find(target);
    return backend ? backend->name : "unknown";
}

int target_has_encoder(cpu_target target) {
    return target == TARGET_INTEL_I386 || target == TARGET_X86_64;
}
