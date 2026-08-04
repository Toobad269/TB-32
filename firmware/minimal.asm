; ===========================================================================
;  MINIMAL-BIOS  --  die Vorlage fuer ein eigenes
;
;  Das ist ein vollstaendiges, funktionierendes BIOS fuer den TB-32. Es kann
;  nichts ausser dem Noetigsten: kein Startbild, kein Setup, kein Secure
;  Boot, keine Bildschirmhistorie, kein Speichertest. Es startet den
;  Rechner -- und das reicht, TOOBAD-OS laeuft damit.
;
;  Gedacht ist es zum Abgucken. Wer sein eigenes BIOS schreiben will, nimmt
;  diese Datei, baut sie um und flasht sie:
;
;      python3 build.py                 -> firmware/minimal.bin
;      im TB-32:  DEL  ->  Firmware  ->  Flash BIOS from File
;
;  Was ein BIOS liefern MUSS, steht in Doku/16 Eigenes BIOS schreiben.
;  Kurzfassung: die 16 Byte Kopf, die Interrupttabelle, vier Dienste
;  (0x10 Bildschirm, 0x13 Platte, 0x16 Tastatur, 0x1A Zeit), die beiden
;  Hardware-Interrupts fuer Timer und Tastatur -- und am Ende den
;  Bootsektor laden und hineinspringen.
;
;  Die Bildschirmroutinen kommen aus video.asm, damit hier nur das steht,
;  was ein BIOS ausmacht. Wer wirklich alles selbst schreiben will, ersetzt
;  auch die.
; ===========================================================================

.include "const.inc"
.org ROM_BASE

; --- Der Kopf: ohne ihn nimmt das Mainboard das Abbild nicht an -----------
reset:
    jmp start                         ; 0x00
    .db "TBBI"                        ; 0x04  Kennung
    .dw 0                             ; 0x08  Laenge     (build.py traegt ein)
    .dw 0                             ; 0x0C  Pruefsumme (build.py traegt ein)
    ; 0x10  Der Name, den das Mainboard beim Einschalten zeigt.
    ; Genau 32 Byte -- der Code faengt bei 0x30 an.
    .db "MINIMAL BIOS", 0
    .space 19

start:                                ; 0x30
    li sp, BIOS_STACK
    cli

    ; --- BIOS-Datenbereich leeren -------------------------------------
    ;     video.asm legt dort Cursor, Farbe und Tickzaehler ab.
    li r10, BDA_BASE
    li r11, 256
    movi r12, 0
.bda:
    stw [r10], r12
    addi r10, r10, 4
    subi r11, r11, 1
    cmpi r11, 0
    jnz .bda
    movi r10, ATTR_NORMAL
    stwa BDA_ATTR, r10

    ; --- Interrupttabelle: hier stehen die Adressen unserer Handler ----
    ;     Ohne diese acht Eintraege springt jeder Interrupt nach Adresse 0.
    li r10, IVT_BASE + IRQ_TIMER*4
    li r11, irq_timer
    stw [r10], r11
    li r10, IVT_BASE + IRQ_KBD*4
    li r11, irq_kbd
    stw [r10], r11
    li r10, IVT_BASE + INT_VIDEO*4
    li r11, int_video
    stw [r10], r11
    li r10, IVT_BASE + INT_DISK*4
    li r11, int_disk
    stw [r10], r11
    li r10, IVT_BASE + INT_KBD*4
    li r11, int_kbd
    stw [r10], r11
    li r10, IVT_BASE + INT_TIME*4
    li r11, int_time
    stw [r10], r11

    movi r10, 100                     ; Systemtakt: 100 Ticks je Sekunde
    out P_TIMER_HZ, r10
    sti

    movi r1, ATTR_NORMAL
    call vid_clear
    li r1, s_hallo
    movi r2, ATTR_NORMAL
    call vid_puts

    ; --- Bootsektor holen und hineinspringen ---------------------------
    movi r1, 0
    movi r2, 1
    li r3, BOOT_ADDR
    call disk_read
    cmpi r0, 0
    jnz .fehler

    li r10, BOOT_ADDR + 510           ; Bootsignatur 0x55 0xAA
    ldb r11, [r10]
    cmpi r11, 0x55
    jnz .fehler
    ldb r11, [r10+1]
    cmpi r11, 0xAA
    jnz .fehler

    li r10, BOOT_ADDR
    jmpr r10

.fehler:
    li r1, s_kein
    movi r2, ATTR_ERR
    call vid_puts
.stop:
    hlt
    jmp .stop

; ===========================================================================
;  Hardware-Interrupts
;
;  Der Timer zaehlt die Ticks mit, die Tastatur schaufelt Anschlaege in den
;  Ringpuffer. Beide muessen dem Interruptcontroller Bescheid geben
;  (P_PIC_ACK), sonst kommt nie wieder einer.
; ===========================================================================

irq_timer:
    push r1
    push r13
    in r1, P_TIMER_TICKS
    stwa BDA_TICKS, r1
    out P_PIC_ACK, r1
    pop r13
    pop r1
    iret

; ACHTUNG: alle wartenden Tasten holen, nicht nur eine. Der Controller kennt
; je Quelle nur ein Bit -- wer hier nach der ersten Taste aufhoert, hinkt bei
; schnellem Tippen dauerhaft einen Anschlag hinterher.
irq_kbd:
    push r1
    push r2
    push r3
    push r13
.hole:
    in r1, P_KBD_STATUS
    cmpi r1, 0
    jz .fertig
    in r1, P_KBD_DATA
    ldwa r2, BDA_KEYTAIL
    li r3, BDA_KEYBUF
    shli r10, r2, 2
    add r3, r3, r10
    stw [r3], r1
    addi r2, r2, 1
    andi r2, r2, 31
    stwa BDA_KEYTAIL, r2
    jmp .hole
.fertig:
    out P_PIC_ACK, r1
    pop r13
    pop r3
    pop r2
    pop r1
    iret

; ===========================================================================
;  INT 0x10 -- Bildschirm.  Funktionsnummer in r0, Argumente ab r1.
;  Die Reihenfolge der Tabelle ist Pflicht: system/start.asm ruft sie ueber
;  ihre Nummer auf.
; ===========================================================================

int_video:
    push r1
    push r2
    push r3
    push r4
    push r5
    push r10
    push r11
    push r12
    push r13
    cmpi r0, 17
    jae .fertig
    li r10, video_table
    shli r11, r0, 2
    add r10, r10, r11
    ldw r12, [r10]
    callr r12
.fertig:
    pop r13
    pop r12
    pop r11
    pop r10
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    iret

video_table:
    .dw vf_putc, vf_puts, vf_setcursor, vf_clear, vf_getcursor
    .dw vf_putat, vf_putn, vf_puthex, vf_setmode, vf_box
    .dw vf_fillrect, vf_hline, vf_scroll, vf_clearrow, vf_putsat
    .dw vf_sbcount, vf_sbline

vf_putc:      call vid_putc
              ret
vf_puts:      call vid_puts
              ret
vf_setcursor: call vid_setcursor
              ret
vf_clear:     call vid_clear
              ret
vf_getcursor: ldwa r0, BDA_CURY
              shli r0, r0, 16
              ldwa r10, BDA_CURX
              or r0, r0, r10
              ret
vf_putat:     call vid_putat
              ret
vf_putn:      call vid_putn
              ret
vf_puthex:    call vid_puthex
              ret
vf_setmode:   out P_VGA_MODE, r1
              ret
vf_box:       call vid_box
              ret
vf_fillrect:  call vid_fillrect
              ret
vf_hline:     call vid_hline
              ret
vf_scroll:    call vid_scroll
              ret
vf_clearrow:  call vid_clearrow
              ret
vf_putsat:    call vid_putsat
              ret
; Die Bildschirmhistorie kennt dieses BIOS nicht -- die beiden Dienste gibt
; es trotzdem, sonst faellt das System in ein Loch. Sie sagen einfach "nichts
; da". Genau so geht man mit einer Funktion um, die man weglaesst.
vf_sbcount:   movi r0, 0
              ret
vf_sbline:    movi r0, 0
              ret

; ===========================================================================
;  INT 0x13 -- Festplatte
; ===========================================================================

int_disk:
    push r10
    push r13
    cmpi r0, 0
    jz .lesen
    cmpi r0, 1
    jz .schreiben
    cmpi r0, 2
    jz .groesse
    movi r0, 0xFF
    jmp .fertig
.lesen:
    call disk_read
    jmp .fertig
.schreiben:
    call disk_write
    jmp .fertig
.groesse:
    in r0, P_DISK_SIZE
.fertig:
    pop r13
    pop r10
    iret

disk_read:
    out P_DISK_LBA, r1
    out P_DISK_COUNT, r2
    out P_DISK_ADDR, r3
    movi r10, 1
    out P_DISK_CMD, r10
    in r0, P_DISK_STATUS
    ret

disk_write:
    out P_DISK_LBA, r1
    out P_DISK_COUNT, r2
    out P_DISK_ADDR, r3
    movi r10, 2
    out P_DISK_CMD, r10
    in r0, P_DISK_STATUS
    ret

; ===========================================================================
;  INT 0x16 -- Tastatur
; ===========================================================================

int_kbd:
    push r10
    push r11
    push r12
    push r13
    cmpi r0, 0
    jz .holen
    cmpi r0, 1
    jz .gucken
    cmpi r0, 2
    jz .leeren
    movi r0, 0
    jmp .fertig
.holen:
    call kbd_getkey
    jmp .fertig
.gucken:
    call kbd_peek
    jmp .fertig
.leeren:
    ldwa r10, BDA_KEYTAIL
    stwa BDA_KEYHEAD, r10
    movi r0, 0
.fertig:
    pop r13
    pop r12
    pop r11
    pop r10
    iret

; Wartet auf eine Taste. Das `hlt` ist wichtig: ohne es dreht die CPU im
; Leerlauf mit voller Last und wird heiss.
kbd_getkey:
    sti
.warte:
    ldwa r10, BDA_KEYHEAD
    ldwa r11, BDA_KEYTAIL
    cmp r10, r11
    jnz .da
    hlt
    jmp .warte
.da:
    li r12, BDA_KEYBUF
    shli r11, r10, 2
    add r12, r12, r11
    ldw r0, [r12]
    addi r10, r10, 1
    andi r10, r10, 31
    stwa BDA_KEYHEAD, r10
    ret

kbd_peek:
    ldwa r10, BDA_KEYHEAD
    ldwa r11, BDA_KEYTAIL
    cmp r10, r11
    jnz .da
    movi r0, 0
    ret
.da:
    li r12, BDA_KEYBUF
    shli r11, r10, 2
    add r12, r12, r11
    ldw r0, [r12]
    ret

; ===========================================================================
;  INT 0x1A -- Zeit
;     r0=0 Ticks seit dem Start, r0=1 Uhrzeit, r0=2 Datum
; ===========================================================================

int_time:
    push r10
    push r11
    push r13
    cmpi r0, 0
    jz .ticks
    cmpi r0, 1
    jz .uhr
    cmpi r0, 2
    jz .datum
    movi r0, 0
    jmp .fertig
.ticks:
    ldwa r0, BDA_TICKS
    jmp .fertig
.uhr:
    movi r10, CM_HOUR
    call cmos_read
    shli r0, r0, 16
    mov r11, r0
    movi r10, CM_MIN
    call cmos_read
    shli r0, r0, 8
    or r11, r11, r0
    movi r10, CM_SEC
    call cmos_read
    or r0, r0, r11
    jmp .fertig
.datum:
    movi r10, CM_YEAR
    call cmos_read
    shli r0, r0, 16
    mov r11, r0
    movi r10, CM_MONTH
    call cmos_read
    shli r0, r0, 8
    or r11, r11, r0
    movi r10, CM_DAY
    call cmos_read
    or r0, r0, r11
.fertig:
    pop r13
    pop r11
    pop r10
    iret

; --- CMOS lesen: Platz in r10 -> r0 ---------------------------------------
cmos_read:
    out P_CMOS_IDX, r10
    in r0, P_CMOS_DATA
    ret

; ===========================================================================
;  Die Bildschirmroutinen. Sie brauchen nur BDA_CURX/CURY/ATTR und den
;  Textspeicher -- alles andere steht oben.
; ===========================================================================

.include "video.asm"

s_hallo:  .db "MINIMAL BIOS\n", 0
s_kein:   .db "No bootable disk\n", 0
