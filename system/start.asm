; ===========================================================================
;  Startcode des Kernels + Brücke von C zu den BIOS-Diensten
;
;  Der Bootsektor springt hierher. Wir richten den Stack ein und rufen die
;  C-Funktion main() auf. Alles Weitere ist in C geschrieben und wird vom
;  selbst gebauten Compiler uebersetzt.
;
;  Die sys_*-Funktionen benutzen dieselbe Aufrufkonvention wie der Compiler
;  (Argumente in r1..r5, Rueckgabe in r0) -- deshalb sind sie so kurz: nur
;  die Funktionsnummer nach r0 und den passenden Interrupt ausloesen.
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

; --- Bildschirm (INT 0x10) -------------------------------------------------
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

; --- Festplatte (INT 0x13) -------------------------------------------------
sys_diskread:   movi r0, 0
                int INT_DISK
                ret
sys_diskwrite:  movi r0, 1
                int INT_DISK
                ret
sys_disksize:   movi r0, 2
                int INT_DISK
                ret

; --- Tastatur (INT 0x16) ---------------------------------------------------
sys_getkey:     movi r0, 0
                int INT_KBD
                ret
sys_haskey:     movi r0, 1
                int INT_KBD
                ret
sys_flushkeys:  movi r0, 2
                int INT_KBD
                ret

; --- Zeit (INT 0x1A) -------------------------------------------------------
sys_ticks:      movi r0, 0
                int INT_TIME
                ret
sys_clock:      movi r0, 1
                int INT_TIME
                ret
sys_date:       movi r0, 2
                int INT_TIME
                ret

; --- Direkter Hardwarezugriff ---------------------------------------------
sys_in:                                   ; sys_in(port)
    inr r0, r1
    ret
sys_out:                                  ; sys_out(port, wert)
    outr r2, r1
    ret

; ---------------------------------------------------------------------------
;  Ein ganzer Blitter-Befehl in EINEM Aufruf
;
;  Vorher rief die Oberflaeche fuer jede gefuellte Flaeche sechsmal sys_out --
;  jedes Mal Argumente auf den Stack, Sprung, Rahmen aufbauen, zurueck. Ein
;  volles Neuzeichnen des Schreibtischs kostete dadurch ueber 400.000 Befehle
;  und war acht Bilder lang beim Malen zuzusehen. Hier passiert dasselbe in
;  gut einem Dutzend Befehlen.
;
;  sys_blit(r1 = x | y<<16, r2 = w | h<<16, r3 = Farbe, r4 = Kommando)
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

;  sys_blitchar(r1 = x | y<<16, r2 = Farbe, r3 = Zeichen, r4 = Hintergrund)
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

sys_halt:                                 ; auf den naechsten Interrupt warten
    hlt
    ret

; ===========================================================================
;  Prozessumschalter
;
;  Haengt am Timer-Interrupt. Beim Eintritt liegen Ruecksprungadresse und
;  Flags des unterbrochenen Programms bereits auf dessen Stack (das macht die
;  CPU). Wir legen alle Register obendrauf, uebergeben den Stackpointer an den
;  Kernel, bekommen den Stackpointer des naechsten Prozesses zurueck und holen
;  von dort dessen Register wieder hervor. Das iret laesst den anderen Prozess
;  genau dort weiterlaufen, wo er zuletzt unterbrochen wurde.
; ===========================================================================

sched_irq_asm:
    push r0                               ; r0 zuerst -- der Compiler rechnet
    push r1                               ; alles darin, es darf nicht verloren gehen
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

    in r1, P_TIMER_TICKS                  ; Systemuhr weiterfuehren
    stwa BDA_TICKS, r1
    out P_PIC_ACK, r1

    mov r1, sp                            ; alten Stackpointer uebergeben
    call proc_schedule
    mov sp, r0                            ; auf den neuen Prozess umschalten

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
;  Systemaufruf-Schnittstelle (INT 0x40)
;
;  Programme, die von der Platte geladen werden, kennen den Kernel nicht --
;  sie loesen einfach INT 0x40 aus, mit der Funktionsnummer in r0. Genau so
;  reden echte Programme mit ihrem Betriebssystem.
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
    mov r5, r4                            ; Argumente eine Stelle weiterruecken,
    mov r4, r3                            ; damit die Funktionsnummer in r1 passt
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
    iret                                  ; r0 traegt hier das Ergebnis zurueck

; call_addr(adresse): springt in geladenen Programmcode
call_addr:
    callr r1
    ret

; --- Speicher zeichenweise kopieren (schneller als in C) -------------------
sys_memcpy:                               ; (ziel, quelle, anzahl)
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

sys_memset:                               ; (ziel, wert, anzahl)
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
