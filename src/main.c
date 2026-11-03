#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"

static int print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --target <target> <source.asm> -o <output.bin>\n", program_name);
    fprintf(stderr, "Targets: intel_8008, intel_8085, intel_8086, intel_8088, intel_i386, x86_64\n");
    return 1;
}

int main(int argc, char **argv) {
    assembler *as = NULL;
    assembler_options options;
    cpu_target target = TARGET_X86_64;
    const char *source_path = NULL;
    const char *output_path = NULL;
    int i;

    memset(&options, 0, sizeof(options));

    if (argc < 3) {
        return print_usage(argv[0]);
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target = target_from_name(argv[++i]);
            if (target == TARGET_UNKNOWN) {
                fprintf(stderr, "unknown target: %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-v") == 0) {
            options.verbose = 1;
            continue;
        }
        if (!source_path) {
            source_path = argv[i];
        }
    }

    if (!source_path || !output_path) {
        return print_usage(argv[0]);
    }

    as = assembler_create();
    if (!as) {
        fprintf(stderr, "failed to create assembler instance\n");
        return 1;
    }

    if (assembler_set_target(as, target) != 0) {
        fprintf(stderr, "error: %s\n", assembler_error(as));
        assembler_destroy(as);
        return 1;
    }

    if (assembler_assemble_file(as, source_path, output_path, &options) != 0) {
        fprintf(stderr, "error: %s\n", assembler_error(as));
        assembler_destroy(as);
        return 1;
    }

    printf("assembled %s -> %s (target=%s)\n", source_path, output_path, target_name(target));

    assembler_destroy(as);
    return 0;
}
