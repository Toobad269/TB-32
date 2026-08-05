; ===========================================================================
;  TB-HACK BIOS v2.5.2  --  Firmware fuer den TB-32, fuer Bastler
;
;  Das serienmaessige TOOBAD BIOS, erweitert um einen Reiter "Hack": einen
;  Hex-Monitor, eine Portkonsole, einen CMOS-Editor, einen Sektoreditor,
;  einen frei waehlbaren Startsektor und zwei Bytes, die kurz vor dem Sprung
;  in den Bootsektor in den Speicher geschrieben werden. Alles Neue steht in
;  hack.asm; hier sind es fuenf Stellen (Name im Kopf, ein .include, der
;  Startsektor aus dem CMOS, die uebergehbare Signaturpruefung und der Aufruf
;  von hk_patch_anwenden).
;
;  Das hier ist KEIN Python. Das ist echter Maschinencode, der im ROM des
;  virtuellen Rechners liegt und von der emulierten CPU ausgefuehrt wird.
;  Beim Einschalten springt die CPU an ROM_BASE -- also genau hierher.
;
;  Ablauf:
;     reset -> POST (Selbsttest) -> [Setup?] -> Bootsektor laden -> Sprung
; ===========================================================================

.include "const.inc"
.org ROM_BASE

; --- Der Reset-Vektor: die allererste Adresse nach dem Einschalten ---------
;
;  Die ersten 16 Byte sind der Kopf des Abbildes. Er ist der Grund, warum man
;  sich hier nicht so leicht ein totes Board flashen kann: das Mainboard
;  prueft Kennung und Pruefsumme, BEVOR es den Chip ueberhaupt startet, und
;  greift sonst zur Sicherung. Wer ein eigenes BIOS schreibt, muss diese 16
;  Byte genauso hinlegen -- siehe Doku 16.
reset:
    jmp bios_start                    ; 0x00  Sprung ueber den Kopf
    .db "TBBI"                        ; 0x04  Kennung
    .dw 0                             ; 0x08  Laenge  (build.py traegt ein)
    .dw 0                             ; 0x0C  Pruefsumme (build.py traegt ein)
    ; 0x10  Der Name, den das Mainboard beim Einschalten zeigt.
    ; Genau 32 Byte -- der Code faengt bei 0x30 an.
    .db "TB-HACK BIOS v2.5.2", 0     ; 20 Byte
    .space 12                         ; ... macht zusammen genau 32

bios_start:                           ; 0x30  ab hier der Code
    li sp, BIOS_STACK
    cli
    call bda_init
    call ivt_init
    movi r10, 100
    out P_TIMER_HZ, r10               ; Systemtakt: 100 Ticks pro Sekunde
    sti
    call flash_pruefen                ; liegt ein Flashwunsch an?
    call post
    call boot
    ; Kommen wir hier an, gab es kein bootfaehiges Medium.
    li r1, s_nosys
    call panic

; ===========================================================================
;  Ein angemeldeter Flashvorgang
;
;  Der Coder kann ein Abbild anmelden, aber nicht selbst brennen. Bestaetigt
;  wird HIER, vor dem Selbsttest, in Rot -- ein Programm darf nicht allein
;  entscheiden, dass der Chip ueberschrieben wird.
; ===========================================================================

flash_pruefen:
    push r6
    movi r10, 9                       ; liegt ein Wunsch an?
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    cmpi r0, 0
    jz .raus

    movi r1, 0x4F                     ; weiss auf rot
    call vid_clear
    movi r1, 24
    movi r2, 7
    li r3, s_fl_head
    movi r4, 0x4E
    call vid_putsat
    movi r1, 14
    movi r2, 10
    li r3, s_fl_w1
    movi r4, 0x4F
    call vid_putsat
    movi r1, 14
    movi r2, 11
    li r3, s_fl_w2
    movi r4, 0x4F
    call vid_putsat
    movi r1, 14
    movi r2, 13
    li r3, s_fl_w3
    movi r4, 0x4F
    call vid_putsat
    movi r1, 14
    movi r2, 15
    li r3, s_fl_w4
    movi r4, 0x4E
    call vid_putsat
.warte:
    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_ENTER
    jz .brennen
    cmpi r10, K_ESC
    jnz .warte
    movi r10, 7                       ; abgemeldet, nichts geschrieben
    out P_FLASH_CMD, r10
    jmp .fertig
.brennen:
    movi r1, 14
    movi r2, 17
    li r3, s_fl_busy
    movi r4, 0x4F
    call vid_putsat
    movi r10, 3                       ; jetzt wirklich
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    movi r1, 14
    movi r2, 19
    li r3, s_fl_done
    movi r4, 0x4A
    cmpi r0, 0
    jz .melden
    li r3, s_fl_fail
    movi r4, 0x4E
.melden:
    call vid_putsat
    movi r1, 250
    call delay
.fertig:
    movi r1, ATTR_NORMAL
    call vid_clear
.raus:
    pop r6
    ret

s_fl_head:  .db "FLASH BIOS", 0
s_fl_w1:    .db "A program has prepared a new BIOS image for this chip.", 0
s_fl_w2:    .db "Writing it is permanent.", 0
s_fl_w3:    .db "If the image does not work, this machine may not start", 0
s_fl_w4:    .db "again. ENTER = flash        ESC = cancel", 0
s_fl_busy:  .db "Writing the chip -- do not power off ...", 0
s_fl_done:  .db "Done. Restart to run the new BIOS.", 0
s_fl_fail:  .db "The chip could not be written. Nothing changed.", 0
; Text mitten im Code MUSS auf vier Byte aufgefuellt werden. Der TB-32 hat
; feste 4-Byte-Befehle; ohne das liegt jeder Befehl danach schief und der
; Rechner stuerzt schon vor dem Startbild ab. Bisher standen alle Texte am
; Dateiende, deshalb ist es nie aufgefallen.
.align 4

; ===========================================================================
;  Aufbau der Systemtabellen
; ===========================================================================

bda_init:
    li r10, BDA_BASE
    li r11, 256
    movi r12, 0
.loop:
    stw [r10], r12
    addi r10, r10, 4
    subi r11, r11, 1
    cmpi r11, 0
    jnz .loop
    movi r10, ATTR_NORMAL
    stwa BDA_ATTR, r10
    ret

; Traegt die Adressen aller Interrupt-Handler in die Vektortabelle ein.
ivt_init:
    li r10, IVT_BASE + INT_DIV0*4
    li r11, int_div0
    stw [r10], r11
    li r10, IVT_BASE + INT_BADOP*4
    li r11, int_badop
    stw [r10], r11
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
    ret

; ===========================================================================
;  Hardware-Interrupts
; ===========================================================================

irq_timer:
    push r1
    push r13
    in r1, P_TIMER_TICKS              ; direkt vom Zaehlerbaustein lesen, damit
    stwa BDA_TICKS, r1                ; kein Tick verlorengeht
    out P_PIC_ACK, r1
    pop r13
    pop r1
    iret

; Der Handler holt ALLE wartenden Tasten, nicht nur eine.
;
; Der Interruptcontroller kennt je Quelle nur ein Bit. Kommen zwei Tasten an,
; bevor der Handler laeuft, gibt es trotzdem nur einen Interrupt -- die zweite
; bliebe im Baustein liegen, bis irgendwann die naechste Taste einen neuen
; Interrupt ausloest. Genau das fuehlte sich an, als haenge die Tastatur einen
; Anschlag hinterher: im BIOS-Setup passierte beim ersten Pfeil nichts, und
; der naechste Druck fuehrte dann die vorige Bewegung aus.
;
; Derselbe Fehler steckte frueher im Timer -- siehe Doku/07 Fallstricke.
irq_kbd:
    push r1
    push r2
    push r3
    push r13
.hole:
    in r1, P_KBD_STATUS
    cmpi r1, 0
    jz .done
    in r1, P_KBD_DATA
    ldwa r2, BDA_KEYTAIL
    li r3, BDA_KEYBUF
    shli r10, r2, 2
    add r3, r3, r10
    stw [r3], r1
    addi r2, r2, 1
    andi r2, r2, 31
    stwa BDA_KEYTAIL, r2
    jmp .hole                         ; noch eine? dann gleich mit
.done:
    out P_PIC_ACK, r1
    pop r13
    pop r3
    pop r2
    pop r1
    iret

; --- Absturz-Interrupts ----------------------------------------------------
int_div0:
    li r1, s_div0
    call panic
int_badop:
    li r1, s_badop
    call panic

; ===========================================================================
;  BIOS-Dienst INT 0x10 -- Bildschirm
;  Funktionsnummer in r0, Argumente in r1..r5, Ergebnis in r0.
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
    jae .done
    li r10, video_table
    shli r11, r0, 2
    add r10, r10, r11
    ldw r12, [r10]
    callr r12
.done:
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
vf_sbcount:   ldwa r0, BDA_SBCOUNT
              ret
vf_sbline:    call sb_line
              ret

; ===========================================================================
;  BIOS-Dienst INT 0x13 -- Festplatte
;     r0=0 lesen (r1=Sektor, r2=Anzahl, r3=Zieladresse) -> r0 = Status
;     r0=1 schreiben (dito)
;     r0=2 Groesse in Sektoren -> r0
; ===========================================================================

int_disk:
    push r10
    push r13
    cmpi r0, 0
    jz .read
    cmpi r0, 1
    jz .write
    cmpi r0, 2
    jz .size
    movi r0, 0xFF
    jmp .done
.read:
    call disk_read
    jmp .done
.write:
    call disk_write
    jmp .done
.size:
    in r0, P_DISK_SIZE
.done:
    pop r13
    pop r10
    iret

; disk_read(r1 = LBA, r2 = Anzahl, r3 = Zieladresse) -> r0 = Status
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
;  BIOS-Dienst INT 0x16 -- Tastatur
;     r0=0  auf Taste warten          -> r0 = Scancode<<8 | ASCII
;     r0=1  nachsehen ob eine da ist  -> r0 = 0 oder Tastencode (bleibt drin)
;     r0=2  Puffer leeren
; ===========================================================================

int_kbd:
    push r10
    push r11
    push r12
    push r13
    cmpi r0, 0
    jz .get
    cmpi r0, 1
    jz .peek
    cmpi r0, 2
    jz .flush
    movi r0, 0
    jmp .done
.get:
    call kbd_getkey
    jmp .done
.peek:
    call kbd_peek
    jmp .done
.flush:
    ldwa r10, BDA_KEYTAIL
    stwa BDA_KEYHEAD, r10
    movi r0, 0
.done:
    pop r13
    pop r12
    pop r11
    pop r10
    iret

kbd_getkey:
    sti                               ; Tastatur-IRQ zulassen waehrend wir warten
.wait:
    ldwa r10, BDA_KEYHEAD
    ldwa r11, BDA_KEYTAIL
    cmp r10, r11
    jnz .have
    hlt                               ; schlafen bis irgendein Interrupt kommt
    jmp .wait
.have:
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
    jnz .have
    movi r0, 0
    ret
.have:
    li r12, BDA_KEYBUF
    shli r11, r10, 2
    add r12, r12, r11
    ldw r0, [r12]
    ret

; ===========================================================================
;  BIOS-Dienst INT 0x1A -- Zeit
;     r0=0 -> r0 = Ticks seit dem Einschalten (100 pro Sekunde)
;     r0=1 -> r0 = Stunde<<16 | Minute<<8 | Sekunde
;     r0=2 -> r0 = Jahr<<16 | Monat<<8 | Tag
; ===========================================================================

int_time:
    push r10
    push r11
    push r13
    cmpi r0, 0
    jz .ticks
    cmpi r0, 1
    jz .clock
    cmpi r0, 2
    jz .date
    movi r0, 0
    jmp .done
.ticks:
    ldwa r0, BDA_TICKS
    jmp .done
.clock:
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
    jmp .done
.date:
    movi r10, CM_YEAR
    call cmos_read
    addi r0, r0, 2000
    shli r0, r0, 16
    mov r11, r0
    movi r10, CM_MONTH
    call cmos_read
    shli r0, r0, 8
    or r11, r11, r0
    movi r10, CM_DAY
    call cmos_read
    or r0, r0, r11
.done:
    pop r13
    pop r11
    pop r10
    iret

; cmos_read(r10 = Register) -> r0
cmos_read:
    out P_CMOS_IDX, r10
    in r0, P_CMOS_DATA
    ret

; cmos_write(r10 = Register, r11 = Wert)
cmos_write:
    out P_CMOS_IDX, r10
    out P_CMOS_DATA, r11
    ret

; ===========================================================================
;  Hilfsfunktionen der Firmware
; ===========================================================================

; delay(r1 = Ticks): wartet r1 Hundertstelsekunden
delay:
    push r6
    push r7
    ldwa r6, BDA_TICKS
    add r6, r6, r1
.wait:
    ldwa r7, BDA_TICKS
    cmp r7, r6
    jae .done
    hlt
    jmp .wait
.done:
    pop r7
    pop r6
    ret

; print(r1 = Text, r2 = Attribut): Text ausgeben und Zeile umbrechen
; --- Kurze Pause zwischen zwei Pruefungen des Selbsttests ----------------
;
;  Ohne sie ist der ganze POST nach 16 Millisekunden fertig: alle sechs
;  Zeilen stehen im ersten Bild da, und vom Hochfahren sieht man nichts.
;  Ein echter Rechner braucht fuer jeden Schritt Zeit, weil er wirklich
;  Bausteine anspricht -- unserer ist nur zu schnell dafuer.
;
;  Bei eingeschaltetem Quick Boot entfaellt die Pause. Genau dafuer ist die
;  Einstellung da, und so macht es auch jedes echte BIOS.
post_pause:
    push r1
    movi r1, 18                       ; Ticks, also knapp eine Fuenftelsekunde
    call langsam_warten
    pop r1
    ret

; r1 Ticks warten -- aber nur, wenn Quick Boot ausgeschaltet ist.
langsam_warten:
    push r1
    push r2
    mov r2, r1
    movi r10, CM_QUICKBOOT
    call cmos_read
    cmpi r0, 0
    jnz .raus
    mov r1, r2
    call delay
.raus:
    pop r2
    pop r1
    ret

print:
    call vid_puts
    push r1
    push r2
    movi r1, 10
    call vid_putc
    pop r2
    pop r1
    ret

; ===========================================================================
;  POST -- Power On Self Test
; ===========================================================================

post:
    ; Der Eigentuemer-Eintrag gehoert einem Firmen-BIOS. Dieses hier ist
    ; keines -- also leeren wir das Feld, damit nicht der Eintrag eines
    ; frueher geflashten BIOS stehenbleibt und das System ihn weiter zeigt.
    movi r2, 0
    stwa BDA_FIRMA, r2
    stwa BDA_POLICY, r2

    push r6
    push r7
    push r8

    movi r1, ATTR_NORMAL
    call vid_clear

    ; --- Kopfzeile -----------------------------------------------------
    movi r1, 0
    movi r2, 0
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, ATTR_TITLE
    call vid_hline
    movi r1, 2
    movi r2, 0
    li r3, s_biosname
    movi r4, ATTR_TITLE
    call vid_putsat
    movi r1, 58
    movi r2, 0
    li r3, s_copyright
    movi r4, ATTR_TITLE
    call vid_putsat

    movi r1, 0
    movi r2, 2
    call vid_setcursor
    call post_pause

    ; --- Prozessor -----------------------------------------------------
    li r1, s_cpu
    movi r2, ATTR_NORMAL
    call vid_puts
    li r1, s_cpuname
    movi r2, ATTR_BRIGHT
    call vid_puts
    movi r10, CM_CPUSPEED
    call cmos_read
    li r10, cpu_speed_names
    shli r0, r0, 2
    add r10, r10, r0
    ldw r1, [r10]
    movi r2, ATTR_BRIGHT
    call print
    call post_pause

    ; --- Speichertest --------------------------------------------------
    li r1, s_memtest
    movi r2, ATTR_NORMAL
    call vid_puts
    call mem_test
    stwa BDA_MEMKB, r0

    li r1, s_ok
    movi r2, ATTR_OK
    call print
    call post_pause

    ; --- Massenspeicher ------------------------------------------------
    li r1, s_disk
    movi r2, ATTR_NORMAL
    call vid_puts
    in r0, P_DISK_SIZE
    stwa BDA_DISKSEC, r0
    shri r1, r0, 11                   ; Sektoren -> MB  (512 Byte * 2048)
    movi r2, ATTR_BRIGHT
    call vid_putn
    li r1, s_mb
    movi r2, ATTR_NORMAL
    call vid_puts
    ldwa r1, BDA_DISKSEC
    movi r2, ATTR_NORMAL
    call vid_putn
    li r1, s_sectors
    movi r2, ATTR_NORMAL
    call print
    call post_pause

    ; --- Tastatur, Maus, Grafik ----------------------------------------
    li r1, s_kbd
    movi r2, ATTR_NORMAL
    call vid_puts
    li r1, s_ok
    movi r2, ATTR_OK
    call print
    call post_pause

    li r1, s_vga
    movi r2, ATTR_NORMAL
    call vid_puts
    li r1, s_vgatype
    movi r2, ATTR_BRIGHT
    call print
    call post_pause

    ; --- Piep zum Zeichen, dass alles in Ordnung ist -------------------
    movi r10, CM_BEEP
    call cmos_read
    cmpi r0, 0
    jz .nobeep
    li r10, 880
    out P_SPK_FREQ, r10
    movi r10, 1
    out P_SPK_ON, r10
    movi r1, 12
    call delay
    movi r10, 0
    out P_SPK_ON, r10
.nobeep:

    ; --- Hinweiszeile unten --------------------------------------------
    movi r1, 0
    movi r2, SCR_H-1
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, ATTR_TITLE
    call vid_hline
    movi r1, 2
    movi r2, SCR_H-1
    li r3, s_setuphint
    movi r4, ATTR_TITLE
    call vid_putsat

    ; --- Auf DEL warten (oder Quick-Boot) ------------------------------
    movi r10, CM_QUICKBOOT
    call cmos_read
    cmpi r0, 0
    jnz .quick
    movi r6, 200                      ; 2 Sekunden Bedenkzeit
    jmp .wait
.quick:
    movi r6, 25
.wait:
    ldwa r7, BDA_TICKS
    add r6, r6, r7
.loop:
    movi r0, 1
    call kbd_peek
    cmpi r0, 0
    jz .nokey
    shri r10, r0, 8
    cmpi r10, K_DEL
    jz .setup
    cmpi r10, K_F2
    jz .setup
    call kbd_getkey                   ; andere Taste: wegwerfen
.nokey:
    ldwa r7, BDA_TICKS
    cmp r7, r6
    jae .fertig
    hlt
    jmp .loop
.setup:
    call kbd_getkey
    call setup_main
.fertig:
    call kuehlung_anwenden
    call secure_pruefen
    pop r8
    pop r7
    pop r6
    ret

; --- Kuehlung: die Einstellungen aus dem CMOS an den Chipsatz weitergeben --
;     Ohne das stuenden die Werte im Setup, wuerden aber nichts bewirken.
kuehlung_anwenden:
    push r10
    movi r10, CM_FANMODE
    call cmos_read
    out P_FANMODE, r0
    movi r10, CM_TEMPLIMIT
    call cmos_read
    cmpi r0, 40                       ; leeres CMOS: sinnvoller Standard
    jge .ok
    movi r0, 85
    movi r10, CM_TEMPLIMIT
    mov r11, r0
    call cmos_write
.ok:
    out P_TEMPLIMIT, r0
    pop r10
    ret

; --- Secure Boot: stimmt das Startabbild noch? ----------------------------
;     Ist die Pruefung eingeschaltet und die Summe eine andere als die
;     gemerkte, bootet der Rechner NICHT. Genau das ist der Sinn: lieber
;     stehenbleiben als etwas Fremdes starten.
secure_pruefen:
    push r6
    push r10
    movi r10, CM_SECURE
    call cmos_read
    cmpi r0, 0
    jz .raus                          ; ausgeschaltet
    call secure_gemerkt
    mov r6, r0
    cmpi r6, 0
    jz .raus                          ; noch nie etwas gemerkt
    call secure_summe
    cmp r0, r6
    jz .gut

    ; Abbild veraendert. Nicht einfach anhalten: wer den Kernel absichtlich
    ; neu gebaut hat, muss ins Setup kommen und die neue Summe merken lassen.
    ; Genau so machen es echte Rechner auch -- wer am Geraet steht, darf.
    movi r1, 0x4F                     ; weiss auf rot
    call vid_clear
    movi r1, 18
    movi r2, 8
    li r3, s_sec_head
    movi r4, 0x4E
    call vid_putsat
    movi r1, 18
    movi r2, 10
    li r3, s_sec_bad
    movi r4, 0x4F
    call vid_putsat
    movi r1, 18
    movi r2, 12
    li r3, s_sec_hint1
    movi r4, 0x4F
    call vid_putsat
    movi r1, 18
    movi r2, 13
    li r3, s_sec_hint2
    movi r4, 0x4F
    call vid_putsat
.sec_warte:
    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_DEL
    jz .sec_setup
    cmpi r10, K_F2
    jz .sec_setup
    li r1, s_sec_halt
    call panic
.sec_setup:
    call setup_main
    movi r1, ATTR_NORMAL
    call vid_clear
    jmp secure_pruefen                ; danach nochmal nachsehen
.gut:
    movi r1, 2
    movi r2, SCR_H-2
    li r3, s_sec_ok
    movi r4, ATTR_OK
    call vid_putsat
.raus:
    pop r10
    pop r6
    ret

; --- Speichertest: prueft den RAM ab 1 MB in 64-KB-Schritten --------------
;     Rueckgabe r0 = gefundene Kilobytes
mem_test:
    push r6
    push r7
    push r8
    push r9
    ldwa r6, BDA_CURX                 ; Position merken, damit die Zahl
    ldwa r7, BDA_CURY                 ; an derselben Stelle hochzaehlt
    movi r8, 0                        ; Blocknummer
    movi r9, 0                        ; gefundene KB
.loop:
    cmpi r8, 256
    jae .done
    cmpi r8, 16
    jl .assume                        ; erstes MB: dort arbeitet gerade das BIOS
    shli r10, r8, 16
    addi r10, r10, 0x100
    li r11, 0x55AA55AA
    stw [r10], r11
    ldw r12, [r10]
    cmp r12, r11
    jnz .done
    movi r11, 0
    stw [r10], r11
.assume:
    addi r9, r9, 64
    addi r8, r8, 1
    andi r10, r8, 7                   ; nur jeden 8. Schritt neu zeichnen
    cmpi r10, 0
    jnz .loop
    mov r1, r6
    mov r2, r7
    call vid_setcursor
    mov r1, r9
    movi r2, ATTR_BRIGHT
    call vid_putn
    li r1, s_kb
    movi r2, ATTR_NORMAL
    call vid_puts
    movi r1, 1                        ; einen Tick je Sprung: der Zaehler
    call langsam_warten               ; laeuft sichtbar hoch statt sofort da
    jmp .loop                         ; zu stehen
.done:
    mov r1, r6
    mov r2, r7
    call vid_setcursor
    mov r1, r9
    movi r2, ATTR_BRIGHT
    call vid_putn
    li r1, s_kb
    movi r2, ATTR_NORMAL
    call vid_puts
    mov r0, r9
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Bootvorgang
; ===========================================================================

boot:
    push r6
    movi r1, 0
    movi r2, SCR_H-1
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, ATTR_TITLE
    call vid_hline
    movi r1, 2
    movi r2, SCR_H-1
    li r3, s_booting
    movi r4, ATTR_TITLE
    call vid_putsat
    movi r1, 0
    movi r2, 12
    call vid_setcursor

    li r1, s_bootmsg
    movi r2, ATTR_NORMAL
    call vid_puts

    call hk_bootsek_lesen             ; TB-HACK: welcher Sektor, steht im CMOS
    mov r1, r0
    movi r2, 1
    li r3, BOOT_ADDR
    call disk_read
    cmpi r0, 0
    jnz .diskerr

    ; TB-HACK: die 55 AA am Ende sind eine Verabredung, keine Eigenschaft der
    ; Platte. Wer einen selbstgeschriebenen Sektor starten will, der sie nicht
    ; traegt, schaltet die Pruefung im Reiter Hack ab.
    movi r10, CM_HKNOSIG
    call cmos_read
    cmpi r0, 0
    jnz .signatur_egal

    li r10, BOOT_ADDR + 510           ; Bootsignatur pruefen
    ldb r11, [r10]
    cmpi r11, 0x55
    jnz .nosig
    ldb r11, [r10+1]
    cmpi r11, 0xAA
    jnz .nosig
.signatur_egal:

    li r1, s_ok
    movi r2, ATTR_OK
    call print

    ; TB-HACK: der Sektor liegt im Speicher, gesprungen ist noch nicht --
    ; das ist das Zeitfenster fuer die Startpatches, und ein anderes gibt es
    ; nicht.
    call hk_patch_anwenden

    movi r1, 20
    call delay

    li r10, BOOT_ADDR                 ; ... und den Bootsektor starten
    jmpr r10

.nosig:
    li r1, s_nosig
    movi r2, ATTR_ERR
    call print
    pop r6
    ret
.diskerr:
    li r1, s_diskerr
    movi r2, ATTR_ERR
    call print
    pop r6
    ret

; ===========================================================================
;  Panik-Bildschirm (r1 = Meldung)
; ===========================================================================

panic:
    push r1
    movi r1, 0x4F                     ; weiss auf rot
    call vid_clear
    movi r1, 20
    movi r2, 9
    movi r3, 40
    movi r4, 7
    movi r5, 0x4F
    call vid_box
    movi r1, 30
    movi r2, 10
    li r3, s_panic
    movi r4, 0x4E
    call vid_putsat
    pop r1
    mov r3, r1
    movi r1, 22
    movi r2, 12
    movi r4, 0x4F
    call vid_putsat
    movi r1, 22
    movi r2, 14
    li r3, s_halted
    movi r4, 0x4F
    call vid_putsat
.stop:
    hlt
    jmp .stop

; ===========================================================================
;  Eingebundene Programmteile
; ===========================================================================

.include "video.asm"
.include "setup.asm"
.include "hack.asm"

; ===========================================================================
;  Texte
; ===========================================================================

s_biosname:   .db "TB-HACK BIOS v2.5.2  --  TB-32 System", 0
s_copyright:  .db "(C) Toobad", 0
s_cpu:        .db "CPU Type ............ ", 0
s_cpuname:    .db "TOOBAD TB-32  ", 0
s_memtest:    .db "Memory Test ......... ", 0
s_disk:       .db "Primary Master ...... ", 0
s_kbd:        .db "Keyboard ............ ", 0
s_vga:        .db "Display Adapter ..... ", 0
s_vgatype:    .db "TB-VGA  80x25 Text / 640x400 Graphics", 0
s_ok:         .db "OK", 0
s_kb:         .db " KB  ", 0
s_mb:         .db " MB  (", 0
s_sectors:    .db " sectors)", 0
s_setuphint:  .db "Press DEL to enter SETUP     F2 = Setup", 0
s_sec_ok:     .db "Secure Boot: image verified", 0
s_sec_head:   .db "SECURE BOOT", 0
s_sec_bad:    .db "The boot image is not the one this machine trusts.", 0
s_sec_hint1:  .db "If you rebuilt the system yourself, this is expected.", 0
s_sec_hint2:  .db "DEL = Setup (Security > Trust Current Boot Image)", 0
s_sec_halt:   .db "Secure Boot: halted", 0
s_booting:    .db "Starting system ...", 0
s_bootmsg:    .db "Booting from Hard Disk 0 ... ", 0
s_nosig:      .db "no boot signature", 0
s_diskerr:    .db "read error", 0
s_nosys:      .db "No bootable device found", 0
s_panic:      .db "SYSTEM ERROR", 0
s_halted:     .db "The system has been halted.", 0
s_div0:       .db "Divide by zero", 0
s_badop:      .db "Invalid opcode", 0

cpu_speed_names:
    .dw s_spd0, s_spd1, s_spd2, s_spd3, s_spd4
s_spd0:       .db "@ 0.4 MHz", 0
s_spd1:       .db "@ 1 MHz", 0
s_spd2:       .db "@ 2 MHz", 0
s_spd3:       .db "@ 4 MHz (Turbo)", 0
s_spd4:       .db "@ 8 MHz (Overclocked)", 0
