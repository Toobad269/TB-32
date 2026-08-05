; ===========================================================================
;  Kernel start code + bridge from C to the BIOS services
;
;  The boot sector jumps here. We set up the stack and call the C function
;  main(). Everything else is written in C and translated by the
;  self-built compiler.
;
;  The sys_* functions use the same calling convention as the compiler
;  (arguments in r1..r5, return value in r0) -- that's why they are so
;  short: just put the function number after r0 and trigger the matching
;  interrupt.
; ===========================================================================

.include "../firmware/const.inc"
.org KERNEL_ADDR

.equ KSTACK, 0x0009FFF0

kernel_entry:
    li sp, KSTACK
    call main
.halt:
    hlt
    jmp .halt

; --- Screen (INT 0x10) -------------------------------------------------
sys_putc:       movi r0, 0
                int INT_VIDEO
                ret
sys_puts:       movi r0, 1
                int INT_VIDEO
                ret
sys_setcursor:  movi r0, 2
                int INT_VIDEO
                ret
sys_cls:        movi r0, 3
                int INT_VIDEO
                ret
sys_getcursor:  movi r0, 4
                int INT_VIDEO
                ret
sys_putat:      movi r0, 5
                int INT_VIDEO
                ret
sys_putn:       movi r0, 6
                int INT_VIDEO
                ret
sys_puthex:     movi r0, 7
                int INT_VIDEO
                ret
sys_setmode:    movi r0, 8
                int INT_VIDEO
                ret
sys_box:        movi r0, 9
                int INT_VIDEO
                ret
sys_fillrect:   movi r0, 10
                int INT_VIDEO
                ret
sys_hline:      movi r0, 11
                int INT_VIDEO
                ret
sys_scroll:     movi r0, 12
                int INT_VIDEO
                ret
sys_clearrow:   movi r0, 13
                int INT_VIDEO
                ret
sys_putsat:     movi r0, 14
                int INT_VIDEO
                ret
sys_sbcount:    movi r0, 15
                int INT_VIDEO
                ret
sys_sbline:     movi r0, 16
                int INT_VIDEO
                ret

; --- Disk (INT 0x13) -------------------------------------------------------
sys_diskread:   movi r0, 0
                int INT_DISK
                ret
sys_diskwrite:  movi r0, 1
                int INT_DISK
                ret
sys_disksize:   movi r0, 2
                int INT_DISK
                ret

; --- Keyboard (INT 0x16) -----------------------------------------------------
sys_getkey:     movi r0, 0
                int INT_KBD
                ret
sys_haskey:     movi r0, 1
                int INT_KBD
                ret
sys_flushkeys:  movi r0, 2
                int INT_KBD
                ret

; --- Time (INT 0x1A) ---------------------------------------------------------
sys_ticks:      movi r0, 0
                int INT_TIME
                ret
sys_clock:      movi r0, 1
                int INT_TIME
                ret
sys_date:       movi r0, 2
                int INT_TIME
                ret

; --- Direct hardware access -------------------------------------------------
sys_in:                                   ; sys_in(port)
    inr r0, r1
    ret
sys_out:                                  ; sys_out(port, value)
    outr r2, r1
    ret

; ---------------------------------------------------------------------------
;  One whole blitter command in ONE call
;
;  Previously the UI called sys_out six times for every filled rectangle --
;  each time pushing arguments on the stack, jumping, building a frame,
;  returning. A full redraw of the desktop cost over 400,000 instructions
;  because of that, and took eight visible frames to paint. Here the same
;  thing happens in a good dozen instructions.
;
;  sys_blit(r1 = x | y<<16, r2 = w | h<<16, r3 = colour, r4 = command)
; ---------------------------------------------------------------------------
sys_blit:
    andi r10, r1, 0xFFFF
    out P_BLT_X, r10
    shri r10, r1, 16
    out P_BLT_Y, r10
    andi r10, r2, 0xFFFF
    out P_BLT_W, r10
    shri r10, r2, 16
    out P_BLT_H, r10
    out P_BLT_COL, r3
    out P_BLT_CMD, r4
    ret

;  sys_blitchar(r1 = x | y<<16, r2 = colour, r3 = character, r4 = background)
sys_blitchar:
    andi r10, r1, 0xFFFF
    out P_BLT_X, r10
    shri r10, r1, 16
    out P_BLT_Y, r10
    out P_BLT_COL, r2
    out P_BLT_BG, r4
    out P_BLT_CHR, r3
    movi r10, 3
    out P_BLT_CMD, r10
    ret

sys_halt:                                 ; wait for the next interrupt
    hlt
    ret

; ===========================================================================
;  Process switcher
;
;  Hangs off the timer interrupt. On entry, the return address and flags of
;  the interrupted program are already on its stack (the CPU does that). We
;  push all the registers on top of that, pass the stack pointer to the
;  kernel, get back the stack pointer of the next process, and pop its
;  registers back out from there. The iret lets the other process continue
;  running from exactly where it was last interrupted.
; ===========================================================================

sched_irq_asm:
    push r0                               ; r0 first -- the compiler keeps
    push r1                               ; everything in it, it must not be lost
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14

    in r1, P_TIMER_TICKS                  ; advance the system clock
    stwa BDA_TICKS, r1
    out P_PIC_ACK, r1

    mov r1, sp                            ; pass the old stack pointer along
    call proc_schedule
    mov sp, r0                            ; switch to the new process

    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop r7
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    iret

; ===========================================================================
;  System call interface (INT 0x40)
;
;  Programs loaded from disk don't know the kernel -- they simply trigger
;  INT 0x40, with the function number in r0. That's exactly how real
;  programs talk to their operating system.
; ===========================================================================

syscall_asm:
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    mov r5, r4                            ; shift the arguments along by one slot,
    mov r4, r3                            ; so the function number ends up in r1
    mov r3, r2
    mov r2, r1
    mov r1, r0
    call syscall
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop r7
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    iret                                  ; r0 carries the result back here

; call_addr(address): jumps into loaded program code
call_addr:
    callr r1
    ret

; --- Copy memory byte by byte (faster than in C) ----------------------------
sys_memcpy:                               ; (dest, src, count)
    push r6
    mov r6, r1
.loop:
    cmpi r3, 0
    jz .done
    ldb r10, [r2]
    stb [r6], r10
    addi r6, r6, 1
    addi r2, r2, 1
    subi r3, r3, 1
    jmp .loop
.done:
    mov r0, r1
    pop r6
    ret

sys_memset:                               ; (dest, value, count)
    push r6
    mov r6, r1
.loop:
    cmpi r3, 0
    jz .done
    stb [r6], r2
    addi r6, r6, 1
    subi r3, r3, 1
    jmp .loop
.done:
    mov r0, r1
    pop r6
    ret
