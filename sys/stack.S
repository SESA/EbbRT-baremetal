.global switch_stack
switch_stack:
        .cfi_startproc simple
        .cfi_def_cfa %rsp, 0
        .cfi_undefined %rip
        mov %rsi, %rsp
        callq *%rdx
        .cfi_endproc
