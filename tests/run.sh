#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=${TMPDIR:-/tmp}/industrial-assembler-test-$$
trap 'rm -f "$TMP.asm" "$TMP.bin" "$TMP.expected"' EXIT HUP INT TERM

"$ROOT/bin/assembler" --target x86_64 "$ROOT/tests/simple.asm" -o "$TMP.bin"
printf '\110\270\001\000\000\000\000\000\000\000\110\203\300\002\110\203\350\001\351\001\000\000\000\220\303' > "$TMP.expected"
cmp "$TMP.expected" "$TMP.bin"

cat > "$TMP.asm" <<'ASM'
mov eax, 0x1234
ret
ASM
"$ROOT/bin/assembler" --target i386 "$TMP.asm" -o "$TMP.bin"
printf '\270\064\022\000\000\303' > "$TMP.expected"
cmp "$TMP.expected" "$TMP.bin"

if "$ROOT/bin/assembler" --target 8086 "$ROOT/tests/simple.asm" -o "$TMP.bin" 2>/dev/null; then
    echo "unimplemented 8086 backend unexpectedly succeeded" >&2
    exit 1
fi

echo "assembler smoke tests passed"
