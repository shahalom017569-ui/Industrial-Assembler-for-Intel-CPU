#include <string.h>

#include "target.h"

static int encode_stub(void *opaque, const char *mnemonic, const char *operand, uint8_t *out_buffer, size_t out_capacity, size_t *written) {
    (void)opaque;
    (void)mnemonic;
    (void)operand;
    (void)out_buffer;
    (void)out_capacity;
    (void)written;
    return 0;
}

static int validate_stub(void *opaque, const char *mnemonic, const char *operand) {
    (void)opaque;
    (void)mnemonic;
    (void)operand;
    return 0;
}

static const target_backend backends[] = {
    { TARGET_INTEL_8008, "intel_8008", 8, 8, 1, encode_stub, validate_stub },
    { TARGET_INTEL_8085, "intel_8085", 8, 16, 1, encode_stub, validate_stub },
    { TARGET_INTEL_8086, "intel_8086", 16, 16, 1, encode_stub, validate_stub },
    { TARGET_INTEL_8088, "intel_8088", 16, 16, 1, encode_stub, validate_stub },
    { TARGET_INTEL_I386, "intel_i386", 32, 32, 1, encode_stub, validate_stub },
    { TARGET_X86_64, "x86_64", 64, 64, 1, encode_stub, validate_stub }
};

int target_register_backend(const target_backend *backend) {
    (void)backend;
    return 0;
}

const target_backend *target_find(cpu_target target) {
    size_t i;
    for (i = 0; i < sizeof(backends) / sizeof(backends[0]); ++i) {
        if (backends[i].target == target) {
            return &backends[i];
        }
    }
    return NULL;
}

const char *target_backend_name(cpu_target target) {
    const target_backend *backend = target_find(target);
    if (!backend) {
        return "unknown";
    }
    return backend->name;
}
