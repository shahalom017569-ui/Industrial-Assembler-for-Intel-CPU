# Industrial Assembler for Intel CPU

A modular, portable C assembler project. The first implementation milestone intentionally provides a small, tested flat-binary assembler for i386 and x86-64; other registered CPU names are not yet encoded and fail with an explicit diagnostic.

## Build and run

Requirements: a C11 compiler and `make`.

```sh
make
./bin/assembler --target x86_64 tests/simple.asm -o output.bin
make test
```

Accepted target names are `x86_64`/`amd64` and `i386`/`intel_i386`. The 8008, 8085, 8086, and 8088 identifiers are reserved for later backends; selecting one currently reports that encoding is not implemented. The 8088 shares the 8086 instruction set, despite its narrower external data bus.

## Initial syntax and implemented instructions

The initial syntax is case-insensitive Intel-style syntax, one instruction per line. `;` starts a comment. Labels use `name:` and can precede an instruction on the same line. Numeric immediates are decimal or `0x`-prefixed hexadecimal.

Implemented for i386 and x86-64:

- `NOP`, `RET`
- `MOV r32, immediate`; additionally `MOV r64, immediate` in x86-64 mode
- `ADD` and `SUB` with a 32- or 64-bit general-purpose register and immediate
- `JMP label` using a near relative 32-bit displacement

Registers supported: legacy 32-bit registers (`eax` ... `edi`), x86-64 registers (`rax` ... `rdi`), and the corresponding `r8`-`r15` / `r8d`-`r15d` names in x86-64 mode. For 64-bit `ADD`/`SUB`, an immediate must fit a signed 32-bit value, as required by the instruction encoding. No directives, macros, relocatable object files, or MASM compatibility are claimed yet.

## Engineering principles

- Unsupported features produce errors; they must never silently assemble to empty or incorrect output.
- CPU-specific encoding stays behind target selection, with a shared source reader, label pass, diagnostics, and flat-binary writer.
- Tests compare exact emitted bytes.
- Compatibility claims will be limited to features covered by tests and documentation.

## Roadmap

1. Establish a reliable shared assembler core and tested i386/x86-64 instruction subset.
2. Add expression evaluation, data directives, and robust diagnostic locations.
3. Expand x86 instruction coverage and add object-file output/relocations.
4. Implement classic 8008, 8085, and 8086/8088 encoders as independent backends.
5. Add syntax compatibility layers only with explicit specifications and conformance tests.
