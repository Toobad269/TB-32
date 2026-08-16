; ===========================================================================
; HackOS 1.0 - native TB-32 kernel entry
; Target: Toobad269/TB-32 branch "toobad-defender"
; ===========================================================================

.org 0x00010000
.equ KSTACK, 0x0009FFF0

kernel_entry:
    li sp, KSTACK
    call main
.hang:
    hlt
    jmp .hang

; BIOS video
sys_setmode:
    movi r0, 8
    int 0x10
    ret

; keyboard
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

; clock
sys_ticks:
    movi r0, 0
    int 0x1A
    ret

sys_clock:
    movi r0, 1
    int 0x1A
    ret

; hardware ports
sys_in:
    inr r0, r1
    ret

sys_out:
    outr r2, r1
    ret

sys_halt:
    hlt
    ret
