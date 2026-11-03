#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"
#include "target.h"

struct assembler {
    target_info info;
    char error[ASSEMBLER_MAX_ERROR_LEN];
};

static void assembler_default_target(assembler *as) {
    as->info.name = "x86_64";
    as->info.target = TARGET_X86_64;
    as->info.word_size_bits = 64;
    as->info.address_size_bits = 64;
    as->info.little_endian = 1;
}

assembler *assembler_create(void) {
    assembler *as = (assembler *)calloc(1, sizeof(*as));
    if (!as) {
        return NULL;
    }
    assembler_default_target(as);
    as->error[0] = '\0';
    return as;
}

void assembler_destroy(assembler *as) {
    if (!as) {
        return;
    }
    free(as);
}

int assembler_set_target(assembler *as, cpu_target target) {
    const target_backend *backend = target_find(target);
    if (!as) {
        return -1;
    }
    if (!backend) {
        return assembler_set_error(as, "unsupported CPU target");
    }

    as->info.name = (char *)backend->name;
    as->info.target = backend->target;
    as->info.word_size_bits = backend->word_size_bits;
    as->info.address_size_bits = backend->address_size_bits;
    as->info.little_endian = backend->little_endian;
    return 0;
}

int assembler_set_error(assembler *as, const char *message) {
    if (!as || !message) {
        return -1;
    }
    snprintf(as->error, sizeof(as->error), "%s", message);
    return -1;
}

const char *assembler_error(assembler *as) {
    if (!as) {
        return "assembler is null";
    }
    return as->error;
}

const target_info *assembler_target_info(const assembler *as) {
    if (!as) {
        return NULL;
    }
    return &as->info;
}

int assembler_assemble_file(assembler *as, const char *source_path, const char *output_path, const assembler_options *options) {
    FILE *fp = NULL;
    char line[1024];
    size_t line_number = 0;

    if (!as || !source_path || !output_path) {
        return -1;
    }

    if (options) {
        snprintf(as->error, sizeof(as->error), "options are accepted but not yet used in this scaffold");
    }

    fp = fopen(source_path, "r");
    if (!fp) {
        snprintf(as->error, sizeof(as->error), "unable to open source file: %s", source_path);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        line_number++;
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            continue;
        }
        (void)line_number;
    }

    fclose(fp);

    fp = fopen(output_path, "wb");
    if (!fp) {
        snprintf(as->error, sizeof(as->error), "unable to open output file: %s", output_path);
        return -1;
    }
    fclose(fp);

    snprintf(as->error, sizeof(as->error), "assembler scaffold initialized for %s", as->info.name);
    return 0;
}

const char *target_name(cpu_target target) {
    switch (target) {
        case TARGET_INTEL_8008: return "intel_8008";
        case TARGET_INTEL_8085: return "intel_8085";
        case TARGET_INTEL_8086: return "intel_8086";
        case TARGET_INTEL_8088: return "intel_8088";
        case TARGET_INTEL_I386: return "intel_i386";
        case TARGET_X86_64: return "x86_64";
        default: return "unknown";
    }
}

cpu_target target_from_name(const char *name) {
    if (!name) {
        return TARGET_UNKNOWN;
    }
    if (strcmp(name, "80386") == 0 || strcmp(name, "i386") == 0 || strcmp(name, "intel_i386") == 0) {
        return TARGET_INTEL_I386;
    }
    if (strcmp(name, "8086") == 0 || strcmp(name, "intel_8086") == 0) {
        return TARGET_INTEL_8086;
    }
    if (strcmp(name, "8088") == 0 || strcmp(name, "intel_8088") == 0) {
        return TARGET_INTEL_8088;
    }
    if (strcmp(name, "8085") == 0 || strcmp(name, "intel_8085") == 0) {
        return TARGET_INTEL_8085;
    }
    if (strcmp(name, "8008") == 0 || strcmp(name, "intel_8008") == 0) {
        return TARGET_INTEL_8008;
    }
    if (strcmp(name, "x86_64") == 0 || strcmp(name, "amd64") == 0 || strcmp(name, "x64") == 0) {
        return TARGET_X86_64;
    }
    return TARGET_UNKNOWN;
}
