; ===========================================================================
;  Startup code for standalone TOOBAD-OS programs (.TBX)
;
;  A file like this sits on disk, gets loaded by the kernel at PROG_ADDR
;  and started from the first byte. It doesn't know the kernel -- it only
;  talks to it via INT 0x40.
; ===========================================================================

; Where this program belongs is decided by build.py: it prepends a
; .equ PROG_BASE, <address>. It used to be fixed at 0x00200000 --
; which meant EVERY program loaded at the same spot. As long as only one
; ran at a time, that was fine. Since several run at once in windows, the
; second one would overwrite the first one's code out from under it:
; Paint got garbled strokes while the memory test ran, and everything
; froze as soon as a third one joined in.
.org PROG_BASE

; A small header so the loader knows where the program belongs.
; Programs without this header (for example ones compiled on the device
; itself) still land at the first slot as before -- that stays valid.
    .dw 0x54425850                     ; "TBXP"
    .dw PROG_BASE                      ; this is where it belongs

prog_entry:
    call main
    movi r0, 4                         ; System call 4 = exit.
    int 0x40                           ; In the background this never returns --
    ret                                ; in the foreground it goes back to the shell.

; sc(number, a1, a2, a3, a4) -- the way to the operating system
sc:
    mov r10, r1
    mov r1, r2
    mov r2, r3
    mov r3, r4
    mov r4, r5
    mov r0, r10
    int 0x40
    ret

; ---------------------------------------------------------------------------
;  Hardware without a detour
;
;  Ports on the TB-32 are not protected -- a program is allowed to operate
;  them directly. The route via int 0x40, by contrast, costs saving 15
;  registers and a long case dispatch in the kernel. At forty drawing
;  calls per frame, that's the difference between choppy and smooth.
;
;  CC on the device emits the same two instructions for portout/portin
;  directly at the call site -- so both compilers end up doing the same
;  thing.
; ---------------------------------------------------------------------------

; portout(port, value) -- outr writes rd to the port in ra, i.e. r2 to r1
portout:
    outr r2, r1
    ret

; portin(port)
portin:
    inr r0, r1
    ret
