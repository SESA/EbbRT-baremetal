.macro pushq_cfi reg
        pushq \reg
        .cfi_adjust_cfa_offset 8
        .cfi_rel_offset \reg, 0
.endm

.macro popq_cfi reg
        popq \reg
        .cfi_adjust_cfa_offset -8
        .cfi_restore \reg
.endm

.macro interrupt_entry name, handler, has_error_code
        .align 8
        .global \name
        \name :
        .cfi_startproc simple
        .cfi_signal_frame
        .if \has_error_code == 0
        pushq $0
        .endif
        .cfi_def_cfa %rsp, 0
        .cfi_offset %rip, 8
        .cfi_offset %rsp, 32
        pushq_cfi %rax
        pushq_cfi %rbx
        pushq_cfi %rcx
        pushq_cfi %rdx
        pushq_cfi %rsi
        pushq_cfi %rdi
        pushq_cfi %rbp
        pushq_cfi %r8
        pushq_cfi %r9
        pushq_cfi %r10
        pushq_cfi %r11
        pushq_cfi %r12
        pushq_cfi %r13
        pushq_cfi %r14
        pushq_cfi %r15
        sub $520, %rsp
        .cfi_adjust_cfa_offset 520
        fwait
        fxsave (%rsp)
        mov %rsp, %rdi
        call \handler
        popq_cfi %r15
        popq_cfi %r14
        popq_cfi %r13
        popq_cfi %r12
        popq_cfi %r11
        popq_cfi %r10
        popq_cfi %r9
        popq_cfi %r8
        popq_cfi %rbp
        popq_cfi %rdi
        popq_cfi %rsi
        popq_cfi %rdx
        popq_cfi %rcx
        popq_cfi %rbx
        popq_cfi %rax
        add $8, %rsp
        iretq
        .cfi_endproc
.endm

.macro interrupt_error_entry name, handler
        interrupt_entry \name, \handler, 1
.endm

.macro interrupt_noerror_entry name, handler
        interrupt_entry \name, \handler, 0
.endm

.cfi_sections .eh_frame, .debug_frame

interrupt_noerror_entry int_de, divide_error_exception
interrupt_noerror_entry int_db, debug_exception
interrupt_noerror_entry int_nmi, nmi_interrupt
interrupt_noerror_entry int_bp, breakpoint_exception
interrupt_noerror_entry int_of, overflow_exception
interrupt_noerror_entry int_br, bound_range_exceeded_exception
interrupt_noerror_entry int_ud, invalid_opcode_exception
interrupt_noerror_entry int_nm, device_not_available_exception
interrupt_error_entry int_df, double_fault_exception
interrupt_error_entry int_ts, invalid_tss_exception
interrupt_error_entry int_np, segment_not_present
interrupt_error_entry int_ss, stack_fault_exception
interrupt_error_entry int_gp, general_protection_exception
interrupt_error_entry int_pf, page_fault_exception
interrupt_noerror_entry int_mf, x86_fpu_floating_point_error
interrupt_error_entry int_ac, alignment_check_exception
interrupt_noerror_entry int_mc, machine_check_exception
interrupt_noerror_entry int_xm, simd_floating_point_exception

.align 16
.globl event_entry
event_entry:
num = 32
.rept 256 - 32
        .align 16
        movq $num, %rdi
        jmp event_entry_common
        num = num + 1
.endr

event_entry_common:
        .cfi_startproc simple
        .cfi_signal_frame
        pushq %rsp
        pushq (%rsp)
        andq $-0x10, %rsp
        call event_interrupt
        movq 8(%rsp), %rsp
        iretq
        .cfi_endproc
