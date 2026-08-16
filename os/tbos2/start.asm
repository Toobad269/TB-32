; ===========================================================================
; TBOS 0.2 - kernel entry + BIOS bridge for the TOOBAD TB-32
;
; This is real TB-32 assembly. The TB-32 defender boot path loads
; \SYSTEM\TBOS2.BIN to 0x00010000 when selected in the BIOS boot menu.
; ===========================================================================

.org 0x00010000

.equ KSTACK, 0x0009FFF0

kernel_entry:
    li sp, KSTACK
    call main

.halt:
    hlt
    jmp .halt

; ---------------------------------------------------------------------------
; BIOS video services - INT 0x10
; ---------------------------------------------------------------------------

sys_putc:
    movi r0, 0
    int 0x10
    ret

sys_puts:
    movi r0, 1
    int 0x10
    ret

sys_cls:
    movi r0, 3
    int 0x10
    ret

sys_putn:
    movi r0, 6
    int 0x10
    ret

sys_puthex:
    movi r0, 7
    int 0x10
    ret

; ---------------------------------------------------------------------------
; BIOS keyboard services - INT 0x16
; ---------------------------------------------------------------------------

sys_getkey:
    movi r0, 0
    int 0x16
    ret

sys_haskey:
    movi r0, 1
    int 0x16
    ret

sys_flushkeys:
    movi r0, 2
    int 0x16
    ret

; ---------------------------------------------------------------------------
; BIOS clock services - INT 0x1A
; ---------------------------------------------------------------------------

sys_ticks:
    movi r0, 0
    int 0x1A
    ret

sys_clock:
    movi r0, 1
    int 0x1A
    ret

sys_date:
    movi r0, 2
    int 0x1A
    ret

; ---------------------------------------------------------------------------
; Direct port access
; sys_out(port, value)
; ---------------------------------------------------------------------------

sys_out:
    outr r2, r1
    ret

; Wait until an interrupt wakes the CPU.
sys_halt:
    hlt
    ret
