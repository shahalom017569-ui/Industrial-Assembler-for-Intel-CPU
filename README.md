# Industrial Assembler for Intel CPU

A modular assembler framework for multiple Intel CPU targets, designed as a serious engineering project rather than a toy parser.

## Goals

- Multi-target x86 family support: 8086/8088, i386, x86-64
- Additional Intel targets: 8008 and 8085
- Shared frontend for tokens, labels, expressions, diagnostics, and memory layout
- Separate CPU backends for encoding and target-specific validation
- Flat binary output as the initial build target, with future ELF/COFF support

## Status

This repository currently contains the initial project scaffold and a working assembler core with stubbed CPU target implementations. The code is intended to be extended as the project grows.

## Quick start

```bash
make
./bin/assembler --target x86_64 tests/simple.asm -o output.bin
```

## Directory layout

```text
include/        Public headers
src/            Implementation files
src/targets/    CPU-specific backends
tests/          Sample inputs and smoke tests
Makefile        Build configuration
```

## Supported targets

- Intel 8008
- Intel 8085
- Intel 8086 / 8088
- Intel i386
- x86-64 (amd64)

## Design principles

- Shared frontend for all targets
- Target-specific instruction encoders separated by CPU
- Strict diagnostics instead of silent unsupported behavior
- Real expression evaluator and symbol table from the start
- Maintainable architecture for future object file output

## Roadmap

1. Core lexer and parser
2. Symbol table and expression handling
3. CPU target registration and validation
4. Instruction encoding for a subset of each target
5. Binary output generation
6. ELF/COFF output support
7. Advanced MASM-like compatibility layer (if explicitly desired later)

## License

MIT
