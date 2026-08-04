; ===========================================================================
;  Bootsektor  --  512 Byte, mehr gibt es nicht.
;
;  Das BIOS laedt genau diesen einen Sektor an die Adresse 0x7C00 und springt
;  hinein. Unsere einzige Aufgabe: den richtigen Kernel von der Platte holen
;  und ihm das Kommando uebergeben. Genau so macht es jeder echte PC.
;
;  Frueher lag der Kernel auf festen Sektoren ab 1, und die Groesse stand im
;  Bootsektor. Das war bequem, aber gelogen: in \SYSTEM lag eine KERNEL.BIN,
;  die niemand brauchte -- man konnte sie loeschen und der Rechner startete
;  weiter. Jetzt sucht der Bootsektor die Datei wirklich im Dateisystem.
;  Loescht man sie, startet der Rechner nicht mehr. So ist es gemeint.
;
;  Der Preis: dieser Sektor muss TBFS lesen koennen -- Verzeichnis durchgehen,
;  \SYSTEM finden, darin KERNEL.BIN finden. Und das alles in 512 Byte.
;  Machbar ist es nur, weil TBFS Dateien am Stueck ablegt: Startsektor und
;  Groesse genuegen, es gibt keine Blockketten zu verfolgen.
; ===========================================================================

.include "../firmware/const.inc"

; --- TBFS, so weit der Bootsektor es kennen muss ---------------------------
;     Die Wahrheit steht in system/fs.c und tools/tbfs.py. Wer dort am
;     Aufbau etwas aendert, aendert es hier mit -- sonst bootet nichts mehr,
;     und zwar ohne jede Fehlermeldung, die auf das Dateisystem zeigt.
.equ FS_DIRSEC0,   513              ; Verzeichnis beginnt hier
.equ FS_DIRSECS,   8                ; Verzeichnis: 8 Sektoren ab 513
.equ FS_ENTSIZE,   32               ; ein Eintrag
.equ FS_MAXFILES,  128
.equ E_START,      16               ; Felder im Eintrag
.equ E_SIZE,       20
.equ E_INFO,       24               ; Art unten, Elternordner+1 in Bit 16..31
.equ FT_DIR,       2
.equ DIRBUF,       0x00008000       ; freier Platz zwischen uns und dem Kernel

.org BOOT_ADDR

boot_start:
    li sp, 0x00007B00                 ; eigener Stack, sicher unter uns

    li r1, s_loading                  ; "Lade Kernel ..." per BIOS-Dienst
    movi r2, 0x0F
    movi r0, 1
    int INT_VIDEO

    ; --- Das Verzeichnis holen (Sektor 513 .. 520) ------------------------
    ;     Den Superblock pruefen wir bewusst NICHT: die Magie kostet sieben
    ;     Befehle und eine eigene Meldung, und ohne Dateisystem findet die
    ;     Namenssuche gleich darauf sowieso nichts. In 512 Byte zaehlt jedes
    ;     Wort.
    li r1, FS_DIRSEC0
    movi r2, FS_DIRSECS
    li r3, DIRBUF
    movi r0, 0
    int INT_DISK
    cmpi r0, 0
    jnz .diskerr

    ; --- Erster Durchgang: den Ordner \SYSTEM finden ----------------------
    ;     Verglichen werden die ersten acht Namensbytes als zwei Woerter --
    ;     kuerzer geht ein Namensvergleich in so wenig Platz nicht.
    li r4, DIRBUF                     ; erster Verzeichniseintrag
    movi r5, 0                        ; laufende Nummer
    li r6, 0x54535953                 ; "SYST"
    li r7, 0x00004D45                 ; "EM" und zwei Nullbytes
.sys_loop:
    ldw r9, [r4]
    cmp r9, r6
    jnz .sys_next
    ldw r9, [r4+4]
    cmp r9, r7
    jnz .sys_next
    ldw r9, [r4+E_INFO]
    shri r8, r9, 16                   ; Elternordner+1; 0 = Hauptverzeichnis
    cmpi r8, 0
    jnz .sys_next
    andi r9, r9, 0xFF
    cmpi r9, FT_DIR                   ; eine Datei namens SYSTEM zaehlt nicht
    jz .sys_found
.sys_next:
    addi r4, r4, FS_ENTSIZE
    addi r5, r5, 1
    cmpi r5, FS_MAXFILES
    jl .sys_loop
    jmp .nokernel
.sys_found:
    addi r8, r5, 1                    ; so steht es in den Eintraegen darin

    ; --- Zweiter Durchgang: KERNEL.BIN in genau diesem Ordner -------------
    li r4, DIRBUF
    movi r5, 0
    li r6, 0x4E52454B                 ; "KERN"
    li r7, 0x422E4C45                 ; "EL.B"
    li r12, 0x00004E49                ; "IN" -- sonst passte auch KERNEL.BAK
.k_loop:
    ldw r9, [r4]
    cmp r9, r6
    jnz .k_next
    ldw r9, [r4+4]
    cmp r9, r7
    jnz .k_next
    ldw r9, [r4+8]
    cmp r9, r12
    jnz .k_next
    ldw r9, [r4+E_INFO]
    shri r9, r9, 16
    cmp r9, r8                        ; liegt sie wirklich in \SYSTEM?
    jz .k_found
.k_next:
    addi r4, r4, FS_ENTSIZE
    addi r5, r5, 1
    cmpi r5, FS_MAXFILES
    jl .k_loop
    jmp .nokernel

.k_found:
    ldw r1, [r4+E_START]              ; erster Sektor der Datei
    ldw r2, [r4+E_SIZE]               ; Groesse in Byte
    cmpi r2, 0
    jz .nokernel
    addi r2, r2, 511                  ; aufrunden auf ganze Sektoren
    shri r2, r2, 9
    li r3, KERNEL_ADDR
    movi r0, 0
    int INT_DISK
    cmpi r0, 0
    jnz .diskerr

    li r1, s_ok                       ; Erfolg melden
    movi r2, 0x0A
    movi r0, 1
    int INT_VIDEO

    li r10, KERNEL_ADDR               ; ... und dem Kernel das Ruder geben
    jmpr r10

.nokernel:
    li r1, s_nokernel
    jmp .fail
.diskerr:
    li r1, s_diskerr
.fail:
    movi r2, 0x0C
    movi r0, 1
    int INT_VIDEO
.stop:
    hlt
    jmp .stop

; Englisch wie BIOS und Betriebssystem -- der Bootsektor schreibt in
; dieselbe Zeilenfolge wie "Booting from Hard Disk 0 ... OK" darueber.
s_loading:   .db "Boot sector: loading kernel ... ", 0
s_ok:        .db "OK\n", 0
s_nokernel:  .db "\\SYSTEM\\KERNEL.BIN missing\n", 0
s_diskerr:   .db "Disk error reading kernel\n", 0
