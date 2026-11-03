; x86-64 smoke test: expected bytes are checked by tests/run.sh
start:
    mov rax, 1
    add rax, 2
    sub rax, 1
    jmp done
    nop
done:
    ret
