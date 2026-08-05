; ===========================================================================
;  TB-HACK  --  der Rueckuebersetzer
;
;  Ein Programm vom Wirtsrechner holen und ansehen, ohne seinen Quelltext zu
;  haben. Der Rest der Firmware ruft davon drei Dinge auf:
;
;      cd_laden        Reiter Code: Datei vom Wirt holen und den Kopf lesen
;      cd_viewer       Reiter Code: das Listing auf dem ganzen Bildschirm
;      cd_adresse      Reiter Code: irgendeine Adresse zurueckuebersetzen
;
;  Warum das ueberhaupt geht: der Dateidialog des Wirtsrechners hat KEINEN
;  Typfilter (pc.py, bios_datei_waehlen -- die Begruendung steht dort im
;  Kommentar). Er wurde fuer BIOS-Abbilder gebaut, nimmt aber jede Datei.
;  Also nimmt er auch eine .TBX. Der Flash-Baustein ist damit nicht laenger
;  nur der Weg fuer neue Firmware, sondern die Tuer nach draussen.
;
;  Und warum es sich lohnt: auf dem TB-32 ist jeder Befehl genau vier Byte
;  breit und liegt auf einer durch vier teilbaren Adresse. Es gibt keine
;  Praefixe, keine variablen Laengen, kein Raten, wo der naechste Befehl
;  anfaengt. Ein Rueckuebersetzer, der auf einem echten PC ein Projekt waere,
;  ist hier eine Tabelle und ein Dutzend Zweige.
;
;  Der Kopf einer .TBX ist acht Byte:
;      0x00  Kennung -- als Wort gelesen 0x54425850, im Datenstrom "PXBT"
;      0x04  Ladeadresse, also wohin das Programm gehoert
;      0x08  hier faengt der Code an
;  Fehlt die Kennung, ist die Datei roher Code; das System laedt sie dann
;  nach PROG_ADDR (siehe prog_start in system/syscall.c).
; ===========================================================================

.equ CD_PUFFER,    0x00500000        ; hierhin kommt die geholte Datei
.equ CD_MAX,       0x00080000        ; 512 KB, so gross wie ein Programm werden darf
.equ CD_MAGIC,     0x54425850        ; "PXBT" im Datenstrom
.equ CD_ZEILEN,    16                ; Befehle je Seite
.equ CD_MY,        5                 ; erste Bildzeile des Listings

; Spalten: Adresse ab 1, das rohe Wort ab 11, der Befehl ab 21
.equ CD_XWORT,     11
.equ CD_XBEF,      21
.equ CD_XARG,      29                ; Argumente, damit sie untereinander stehen

; Formatschluessel der Tabelle unten
.equ F_NIX,        1                 ; ret
.equ F_RR,         2                 ; mov  rd, ra
.equ F_RI,         3                 ; movi rd, 0x1234
.equ F_LADEN,      4                 ; ldw  rd, [ra+8]
.equ F_RRR,        5                 ; add  rd, ra, rb
.equ F_RRI,        6                 ; addi rd, ra, 0x1234
.equ F_R,          7                 ; push rd
.equ F_CALL,       8                 ; call 0x0F000123
.equ F_SPRUNG,     9                 ; jz   0x0F000123
.equ F_IR,        10                 ; out  0x0070, rd
.equ F_INT,       11                 ; int  0x40
.equ F_SPEICHERN, 12                 ; stw  [ra+8], rd

; Aus demselben Grund wie in hack.asm: code.asm wird nach dessen
; Zeichenkettentabelle eingebunden, und die endet auf einer krummen Adresse.
.align 4

; ===========================================================================
;  Eine Datei vom Wirtsrechner holen
; ===========================================================================
cd_laden:
    push r6
    movi r10, 1                       ; Wirtsrechner nach einer Datei fragen
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    cmpi r0, 0
    jnz .keine
    in r6, P_FLASH_SIZE
    cmpi r6, 0
    jz .keine
    li r10, CD_MAX
    cmp r6, r10
    ja .zugross

    li r10, CD_PUFFER                 ; ... und sie in den Arbeitsspeicher holen
    out P_FLASH_ADDR, r10
    movi r10, 2
    out P_FLASH_CMD, r10
    stwa CD_LEN, r6

    call cd_kopf_lesen                ; r0 = erste Codeadresse
    stwa CD_ADDR, r0
    li r1, s_cd_geladen
    jmp .melden
.keine:
    movi r10, 0
    stwa CD_LEN, r10
    li r1, s_cd_keine
    jmp .melden
.zugross:
    movi r10, 0
    stwa CD_LEN, r10
    li r1, s_cd_zugross
.melden:
    call setup_message
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Den Kopf der geholten Datei ansehen
;      -> r0 = Adresse, ab der Code steht
;  Nebenbei: CD_HDR = 1 wenn eine Kennung da war, CD_ORG = die Ladeadresse
; ---------------------------------------------------------------------------
cd_kopf_lesen:
    push r6
    li r6, CD_PUFFER
    ldw r10, [r6]
    li r11, CD_MAGIC
    cmp r10, r11
    jnz .roh
    movi r10, 1
    stwa CD_HDR, r10
    ldw r10, [r6+4]                   ; die gewuenschte Ladeadresse
    stwa CD_ORG, r10
    addi r0, r6, 8                    ; hinter dem Kopf faengt der Code an
    jmp .fertig
.roh:
    movi r10, 0
    stwa CD_HDR, r10
    stwa CD_ORG, r10
    mov r0, r6
.fertig:
    pop r6
    ret

; ===========================================================================
;  Register, Zahlen, Sprungziele -- die Bausteine einer Befehlszeile
; ===========================================================================

; cd_reg(r1 = Registernummer 0..15, r2 = Attribut)
;
;  Die drei letzten heissen im Quelltext nicht r13..r15, sondern at, fp und
;  sp. Wer ein Listing mit dem Assemblerhandbuch danebenlegt, soll dieselben
;  Namen lesen -- sonst uebersetzt er im Kopf zurueck.
cd_reg:
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    cmpi r6, 13
    jl .zahl
    subi r10, r6, 13
    shli r10, r10, 2
    li r11, cd_sondernamen
    add r10, r10, r11
    ldw r1, [r10]
    mov r2, r7
    call vid_puts
    jmp .fertig
.zahl:
    movi r1, 0x72                     ; 'r'
    mov r2, r7
    call vid_putc
    mov r1, r6
    mov r2, r7
    call vid_putn
.fertig:
    pop r7
    pop r6
    ret

; cd_hex(r1 = Wert, r2 = Attribut, r3 = Stellen): "0x" davor
cd_hex:
    push r6
    push r7
    push r8
    mov r6, r1
    mov r7, r2
    mov r8, r3
    li r1, s_cd_0x
    mov r2, r7
    call vid_puts
    mov r1, r6
    mov r2, r7
    mov r3, r8
    call vid_puthex
    pop r8
    pop r7
    pop r6
    ret

; cd_versatz(r1 = 16-Bit-Wert, r2 = Attribut): als vorzeichenbehaftete Zahl
;
;  Speicherzugriffe zaehlen rueckwaerts, sobald es um Stackrahmen geht:
;  [fp-4] ist eine oertliche Variable. Als 0xFFFC geschrieben waere das
;  richtig und trotzdem unlesbar.
cd_versatz:
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    andi r6, r6, 0xFFFF
    ; Das Vorzeichenbit direkt abfragen und NICHT mit "cmpi r6, 0x8000"
    ; vergleichen: cmpi erweitert seinen 16-Bit-Wert vorzeichenbehaftet
    ; (hardware/cpu.py, Opcode 0x3D), aus 0x8000 wird darin -32768, und dann
    ; ist jeder positive Versatz groesser statt kleiner. andi nimmt seinen
    ; Wert dagegen roh (Opcode 0x35) -- die beiden sind sich nicht einig, und
    ; genau daran ist der erste Bau gescheitert: aus [sp+8] wurde [sp-65528].
    shri r10, r6, 15
    andi r10, r10, 1
    cmpi r10, 0
    jz .positiv
    movi r1, 0x2D                     ; '-'
    mov r2, r7
    call vid_putc
    li r10, 0x10000
    sub r6, r10, r6
    jmp .zahl
.positiv:
    cmpi r6, 0
    jz .fertig                        ; [r7] statt [r7+0]
    movi r1, 0x2B                     ; '+'
    mov r2, r7
    call vid_putc
.zahl:
    mov r1, r6
    mov r2, r7
    call vid_putn
.fertig:
    pop r7
    pop r6
    ret

; cd_ziel(r1 = Versatz in Woertern (schon vorzeichenrichtig), r2 = Adresse
;         des Befehls, r3 = Attribut)
;
;  Die CPU rechnet npc = pc + Versatz*4, wobei pc die Adresse DIESES Befehls
;  ist und nicht die des naechsten (hardware/cpu.py, Opcode 0x50 und 0x42).
;  Also genau dasselbe hier -- ein Listing mit falschen Sprungzielen waere
;  schlimmer als gar keins.
cd_ziel:
    push r6
    shli r6, r1, 2
    add r6, r6, r2
    mov r1, r6
    mov r2, r3
    movi r3, 8
    call cd_hex
    pop r6
    ret

; ===========================================================================
;  cd_decode(r1 = Befehlswort, r2 = Adresse des Befehls, r4 = Attribut)
;
;  Schreibt den Befehl ab der aktuellen Cursorstelle. Der Aufrufer hat den
;  Cursor auf CD_XBEF gesetzt.
; ===========================================================================
cd_decode:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1                        ; r6 = Wort
    mov r7, r2                        ; r7 = Adresse
    mov r9, r4                        ; r9 = Attribut

    shri r10, r6, 24                  ; Opcode
    cmpi r10, 0x65
    jae .unbekannt
    shli r10, r10, 3
    li r11, cd_tab
    add r8, r10, r11                  ; r8 = Tabelleneintrag
    ldw r10, [r8]
    cmpi r10, 0
    jz .unbekannt

    ; --- Der Name. Bei einem Sprung steht er nicht am Opcode, sondern an
    ;     der Bedingung im rd-Feld -- alle neunzehn Sprungbefehle teilen
    ;     sich den Opcode 0x50.
    ;
    ;     Der Formatschluessel wird hier nach r11 geholt und NICHT nach r10:
    ;     dort liegt der Name, und er wird gleich gebraucht. Genau das war
    ;     beim ersten Bau vertauscht -- die Zeilen zeigten dann brav ihre
    ;     Argumente, aber ohne den Befehl davor.
    ldw r11, [r8+4]
    cmpi r11, F_SPRUNG
    jnz .name
    shri r11, r6, 20
    andi r11, r11, 15
    cmpi r11, 15
    jae .unbekannt
    shli r11, r11, 2
    li r10, cd_bedingungen
    add r11, r11, r10
    ldw r10, [r11]
.name:
    mov r1, r10
    mov r2, r9
    call vid_puts

    movi r1, CD_XARG                  ; Argumente immer an derselben Spalte
    ldwa r2, CD_ZEILE
    call vid_setcursor

    ldw r10, [r8+4]                   ; Formatschluessel
    cmpi r10, F_NIX
    jz .fertig
    cmpi r10, F_RR
    jz .rr
    cmpi r10, F_RI
    jz .ri
    cmpi r10, F_LADEN
    jz .laden
    cmpi r10, F_RRR
    jz .rrr
    cmpi r10, F_RRI
    jz .rri
    cmpi r10, F_R
    jz .r
    cmpi r10, F_CALL
    jz .call
    cmpi r10, F_SPRUNG
    jz .sprung
    cmpi r10, F_IR
    jz .ir
    cmpi r10, F_INT
    jz .int
    cmpi r10, F_SPEICHERN
    jz .speichern
    jmp .fertig

; --- mov rd, ra ------------------------------------------------------------
.rr:
    call cd_rd
    call cd_komma
    call cd_ra
    jmp .fertig

; --- movi rd, 0x1234 -------------------------------------------------------
.ri:
    call cd_rd
    call cd_komma
    andi r1, r6, 0xFFFF
    mov r2, r9
    movi r3, 4
    call cd_hex
    jmp .fertig

; --- add rd, ra, rb --------------------------------------------------------
.rrr:
    call cd_rd
    call cd_komma
    call cd_ra
    call cd_komma
    shri r1, r6, 12
    andi r1, r1, 15
    mov r2, r9
    call cd_reg
    jmp .fertig

; --- addi rd, ra, 0x1234 ---------------------------------------------------
.rri:
    call cd_rd
    call cd_komma
    call cd_ra
    call cd_komma
    andi r1, r6, 0xFFFF
    mov r2, r9
    movi r3, 4
    call cd_hex
    jmp .fertig

; --- ldw rd, [ra+8] --------------------------------------------------------
.laden:
    call cd_rd
    call cd_komma
    call cd_klammer
    jmp .fertig

; --- stw [ra+8], rd --------------------------------------------------------
.speichern:
    call cd_klammer
    call cd_komma
    call cd_rd
    jmp .fertig

; --- push rd ---------------------------------------------------------------
.r:
    call cd_rd
    jmp .fertig

; --- call 0x0F000123 -- 24 Bit Versatz, vorzeichenbehaftet -----------------
.call:
    li r10, 0xFFFFFF
    and r1, r6, r10
    li r10, 0x800000
    cmp r1, r10
    jb .call_ziel
    li r10, 0x1000000
    sub r1, r1, r10
.call_ziel:
    mov r2, r7
    mov r3, r9
    call cd_ziel
    jmp .fertig

; --- jz 0x0F000123 -- 20 Bit Versatz, vorzeichenbehaftet -------------------
.sprung:
    li r10, 0xFFFFF
    and r1, r6, r10
    li r10, 0x80000
    cmp r1, r10
    jb .spr_ziel
    li r10, 0x100000
    sub r1, r1, r10
.spr_ziel:
    mov r2, r7
    mov r3, r9
    call cd_ziel
    jmp .fertig

; --- out 0x0070, rd --------------------------------------------------------
.ir:
    andi r1, r6, 0xFFFF
    mov r2, r9
    movi r3, 4
    call cd_hex
    call cd_komma
    call cd_rd
    jmp .fertig

; --- int 0x40 --------------------------------------------------------------
.int:
    andi r1, r6, 0xFFFF
    mov r2, r9
    movi r3, 2
    call cd_hex
    jmp .fertig

; --- Kein Befehl: dann eben als Wort, so wie es dasteht --------------------
.unbekannt:
    li r1, s_cd_wort
    mov r2, r9
    call vid_puts
    movi r1, CD_XARG
    ldwa r2, CD_ZEILE
    call vid_setcursor
    mov r1, r6
    mov r2, r9
    movi r3, 8
    call cd_hex
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Die vier Handgriffe, die oben immer wieder vorkommen -----------------
;     Sie lesen das Wort aus r6 und das Attribut aus r9 -- beides gehoert
;     cd_decode und ueberlebt jeden Aufruf, weil es gerettete Register sind.
cd_rd:
    shri r1, r6, 20
    andi r1, r1, 15
    mov r2, r9
    call cd_reg
    ret

cd_ra:
    shri r1, r6, 16
    andi r1, r1, 15
    mov r2, r9
    call cd_reg
    ret

cd_komma:
    movi r1, 0x2C                     ; ','
    mov r2, r9
    call vid_putc
    movi r1, 0x20
    mov r2, r9
    call vid_putc
    ret

cd_klammer:
    movi r1, 0x5B                     ; '['
    mov r2, r9
    call vid_putc
    call cd_ra
    andi r1, r6, 0xFFFF
    mov r2, r9
    call cd_versatz
    movi r1, 0x5D                     ; ']'
    mov r2, r9
    call vid_putc
    ret

; ===========================================================================
;  Das Listing
; ===========================================================================
cd_viewer:
    push r6
    push r7
.redraw:
    movi r1, A_BG
    call vid_clear
    li r1, s_cd_titel
    call hk_kopf
    call cd_infozeile
    movi r1, 1
    movi r2, 3
    li r3, s_cd_kopf
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 4
    call hk_linie
    movi r1, 1
    movi r2, 21
    li r3, s_cd_keys
    movi r4, A_HELP
    call vid_putsat

.zeichnen:
    movi r6, 0                        ; Zeilenzaehler
.zeile:
    cmpi r6, CD_ZEILEN
    jae .warten
    addi r7, r6, CD_MY
    stwa CD_ZEILE, r7                 ; cd_decode braucht die Bildzeile

    ldwa r10, CD_ADDR                 ; Adresse dieses Befehls
    shli r11, r6, 2
    add r10, r10, r11
    stwa CD_BEFADR, r10

    movi r1, 1                        ; Spalte 1: die Adresse
    mov r2, r7
    call vid_setcursor
    ldwa r1, CD_BEFADR
    movi r2, A_TITLE
    movi r3, 8
    call vid_puthex

    movi r1, CD_XWORT                 ; Spalte 11: das rohe Wort
    mov r2, r7
    call vid_setcursor
    ldwa r10, CD_BEFADR
    ldw r1, [r10]
    stwa CD_WORT, r1
    movi r2, A_BG
    movi r3, 8
    call vid_puthex

    movi r1, CD_XBEF                  ; Spalte 21: der Befehl
    mov r2, r7
    call vid_setcursor
    ldwa r1, CD_WORT
    ldwa r2, CD_BEFADR
    movi r4, A_TITLE
    call cd_decode

    addi r6, r6, 1
    jmp .zeile

.warten:
    call kbd_getkey
    shri r6, r0, 8
    andi r7, r0, 0xFF
    cmpi r6, K_ESC
    jz .raus
    cmpi r6, K_UP
    jz .hoch
    cmpi r6, K_DOWN
    jz .runter
    cmpi r6, K_PGUP
    jz .pgup
    cmpi r6, K_PGDN
    jz .pgdn
    cmpi r7, 0x67                     ; 'g'
    jz .goto
    cmpi r7, 0x47                     ; 'G'
    jz .goto
    jmp .zeichnen

.hoch:
    ldwa r10, CD_ADDR
    subi r10, r10, 4
    stwa CD_ADDR, r10
    jmp .zeichnen
.runter:
    ldwa r10, CD_ADDR
    addi r10, r10, 4
    stwa CD_ADDR, r10
    jmp .zeichnen
.pgup:
    ldwa r10, CD_ADDR
    subi r10, r10, CD_ZEILEN*4
    stwa CD_ADDR, r10
    jmp .zeichnen
.pgdn:
    ldwa r10, CD_ADDR
    addi r10, r10, CD_ZEILEN*4
    stwa CD_ADDR, r10
    jmp .zeichnen

.goto:
    li r1, s_cd_titel2
    li r2, s_cd_frage
    movi r3, 8
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    shri r10, r0, 2                   ; auf vier abrunden -- ein Befehl liegt
    shli r10, r10, 2                  ; immer auf einer solchen Adresse
    stwa CD_ADDR, r10
    jmp .redraw

.raus:
    pop r7
    pop r6
    ret

; --- Die Zeile ueber dem Listing: was da eigentlich liegt -----------------
cd_infozeile:
    movi r1, 1
    movi r2, 2
    call vid_setcursor
    ldwa r10, CD_LEN
    cmpi r10, 0
    jz .nichts
    ldwa r10, CD_HDR
    cmpi r10, 0
    jz .ohnekopf
    li r1, s_cd_mitkopf
    movi r2, A_BG
    call vid_puts
    ldwa r1, CD_ORG
    movi r2, ATTR_BRIGHT
    movi r3, 8
    call cd_hex
    jmp .groesse
.ohnekopf:
    li r1, s_cd_ohnekopf
    movi r2, A_BG
    call vid_puts
.groesse:
    li r1, s_cd_komma
    movi r2, A_BG
    call vid_puts
    ldwa r1, CD_LEN
    movi r2, ATTR_BRIGHT
    call vid_putn
    li r1, s_cd_bytes
    movi r2, A_BG
    call vid_puts
    ret
.nichts:
    li r1, s_cd_nichts
    movi r2, A_BG
    call vid_puts
    ret

; ===========================================================================
;  Der Knopf "Disassemble Any Address"
;
;  Der Rueckuebersetzer haengt an keiner Datei. Er liest Speicher, und im
;  Speicher liegt beim Start unter anderem das ROM -- dieses BIOS kann sich
;  also selbst zeigen.
; ===========================================================================
cd_adresse:
    li r1, s_cd_titel2
    li r2, s_cd_frage
    movi r3, 8
    call hk_hexfrage
    cmpi r1, 0
    jz .abbruch
    shri r10, r0, 2
    shli r10, r10, 2
    stwa CD_ADDR, r10
    call cd_viewer
    call setup_frame
    ret
.abbruch:
    call setup_frame
    ret

; Der Knopf "Show Code" -- nur sinnvoll, wenn ueberhaupt etwas dasteht
cd_zeigen:
    ldwa r10, CD_ADDR
    cmpi r10, 0
    jz .leer
    call cd_viewer
    call setup_frame
    ret
.leer:
    li r1, s_cd_leer
    call setup_message
    ret

; ===========================================================================
;  Die Befehlstabelle
;
;  Sie spiegelt INSTRUCTIONS in hardware/isa.py -- und sie steht hier
;  bewusst ein zweites Mal, genau wie die TBFS-Konstanten in setup.asm ein
;  zweites Mal dastehen: die Firmware kann keine Python-Datei lesen. Wer den
;  Befehlssatz erweitert, muss beide Stellen nachziehen. Ein unbekannter
;  Opcode ist kein Absturz, sondern eine Zeile ".word 0x...".
;
;  Je Eintrag: Zeiger auf den Namen, Formatschluessel. Der Index ist der
;  Opcode, Luecken sind mit Nullen gefuellt.
; ===========================================================================
.align 4
cd_tab:
    .dw s_i_nop,   F_NIX              ; 0x00
    .dw s_i_hlt,   F_NIX
    .dw s_i_cli,   F_NIX
    .dw s_i_sti,   F_NIX
    .dw s_i_iret,  F_NIX
    .dw s_i_ret,   F_NIX
    .dw s_i_brk,   F_NIX              ; 0x06
    .space 9*8                        ; 0x07..0x0F
    .dw s_i_mov,   F_RR               ; 0x10
    .dw s_i_movi,  F_RI               ; 0x11
    .space 8                          ; 0x12
    .dw s_i_movh,  F_RI               ; 0x13
    .space 4*8                        ; 0x14..0x17
    .dw s_i_ldb,   F_LADEN            ; 0x18
    .dw s_i_ldsb,  F_LADEN
    .dw s_i_ldh,   F_LADEN
    .dw s_i_ldw,   F_LADEN            ; 0x1B
    .dw s_i_stb,   F_SPEICHERN        ; 0x1C
    .dw s_i_sth,   F_SPEICHERN
    .dw s_i_stw,   F_SPEICHERN        ; 0x1E
    .space 8                          ; 0x1F
    .dw s_i_add,   F_RRR              ; 0x20
    .dw s_i_sub,   F_RRR
    .dw s_i_mul,   F_RRR
    .dw s_i_div,   F_RRR
    .dw s_i_mod,   F_RRR
    .dw s_i_and,   F_RRR
    .dw s_i_or,    F_RRR
    .dw s_i_xor,   F_RRR
    .dw s_i_shl,   F_RRR
    .dw s_i_shr,   F_RRR
    .dw s_i_sar,   F_RRR              ; 0x2A
    .dw s_i_not,   F_RR               ; 0x2B
    .dw s_i_neg,   F_RR
    .dw s_i_cmp,   F_RR
    .dw s_i_tst,   F_RR               ; 0x2E
    .dw s_i_udiv,  F_RRR              ; 0x2F
    .dw s_i_addi,  F_RRI              ; 0x30
    .dw s_i_subi,  F_RRI
    .dw s_i_muli,  F_RRI
    .dw s_i_divi,  F_RRI
    .dw s_i_modi,  F_RRI
    .dw s_i_andi,  F_RRI
    .dw s_i_ori,   F_RRI
    .dw s_i_xori,  F_RRI
    .dw s_i_shli,  F_RRI
    .dw s_i_shri,  F_RRI
    .dw s_i_sari,  F_RRI              ; 0x3A
    .space 2*8                        ; 0x3B..0x3C
    .dw s_i_cmpi,  F_RI               ; 0x3D
    .dw s_i_tsti,  F_RI               ; 0x3E
    .dw s_i_umod,  F_RRR              ; 0x3F
    .dw s_i_push,  F_R                ; 0x40
    .dw s_i_pop,   F_R
    .dw s_i_call,  F_CALL             ; 0x42
    .dw s_i_callr, F_R
    .dw s_i_pushf, F_NIX
    .dw s_i_popf,  F_NIX              ; 0x45
    .space 10*8                       ; 0x46..0x4F
    .dw s_i_jmp,   F_SPRUNG           ; 0x50 -- Name kommt aus cd_bedingungen
    .dw s_i_jmpr,  F_R                ; 0x51
    .space 14*8                       ; 0x52..0x5F
    .dw s_i_in,    F_RI               ; 0x60
    .dw s_i_inr,   F_RR
    .dw s_i_out,   F_IR               ; 0x62
    .dw s_i_outr,  F_RR
    .dw s_i_int,   F_INT              ; 0x64

; Die fuenfzehn Sprungbedingungen, Index = rd-Feld (isa.py, COND)
cd_bedingungen:
    .dw s_i_jmp, s_i_jz,  s_i_jnz, s_i_jc,  s_i_jnc
    .dw s_i_jn,  s_i_jnn, s_i_jv,  s_i_jnv, s_i_jbe
    .dw s_i_ja,  s_i_jl,  s_i_jge, s_i_jle, s_i_jg

; r13, r14, r15 heissen im Quelltext anders
cd_sondernamen:
    .dw s_i_at, s_i_fp, s_i_sp

; --- Namen -----------------------------------------------------------------
s_i_nop:   .db "nop", 0
s_i_hlt:   .db "hlt", 0
s_i_cli:   .db "cli", 0
s_i_sti:   .db "sti", 0
s_i_iret:  .db "iret", 0
s_i_ret:   .db "ret", 0
s_i_brk:   .db "brk", 0
s_i_mov:   .db "mov", 0
s_i_movi:  .db "movi", 0
s_i_movh:  .db "movh", 0
s_i_ldb:   .db "ldb", 0
s_i_ldsb:  .db "ldsb", 0
s_i_ldh:   .db "ldh", 0
s_i_ldw:   .db "ldw", 0
s_i_stb:   .db "stb", 0
s_i_sth:   .db "sth", 0
s_i_stw:   .db "stw", 0
s_i_add:   .db "add", 0
s_i_sub:   .db "sub", 0
s_i_mul:   .db "mul", 0
s_i_div:   .db "div", 0
s_i_mod:   .db "mod", 0
s_i_and:   .db "and", 0
s_i_or:    .db "or", 0
s_i_xor:   .db "xor", 0
s_i_shl:   .db "shl", 0
s_i_shr:   .db "shr", 0
s_i_sar:   .db "sar", 0
s_i_not:   .db "not", 0
s_i_neg:   .db "neg", 0
s_i_cmp:   .db "cmp", 0
s_i_tst:   .db "tst", 0
s_i_udiv:  .db "udiv", 0
s_i_umod:  .db "umod", 0
s_i_addi:  .db "addi", 0
s_i_subi:  .db "subi", 0
s_i_muli:  .db "muli", 0
s_i_divi:  .db "divi", 0
s_i_modi:  .db "modi", 0
s_i_andi:  .db "andi", 0
s_i_ori:   .db "ori", 0
s_i_xori:  .db "xori", 0
s_i_shli:  .db "shli", 0
s_i_shri:  .db "shri", 0
s_i_sari:  .db "sari", 0
s_i_cmpi:  .db "cmpi", 0
s_i_tsti:  .db "tsti", 0
s_i_push:  .db "push", 0
s_i_pop:   .db "pop", 0
s_i_call:  .db "call", 0
s_i_callr: .db "callr", 0
s_i_pushf: .db "pushf", 0
s_i_popf:  .db "popf", 0
s_i_jmpr:  .db "jmpr", 0
s_i_in:    .db "in", 0
s_i_inr:   .db "inr", 0
s_i_out:   .db "out", 0
s_i_outr:  .db "outr", 0
s_i_int:   .db "int", 0
s_i_jmp:   .db "jmp", 0
s_i_jz:    .db "jz", 0
s_i_jnz:   .db "jnz", 0
s_i_jc:    .db "jc", 0
s_i_jnc:   .db "jnc", 0
s_i_jn:    .db "jn", 0
s_i_jnn:   .db "jnn", 0
s_i_jv:    .db "jv", 0
s_i_jnv:   .db "jnv", 0
s_i_jbe:   .db "jbe", 0
s_i_ja:    .db "ja", 0
s_i_jl:    .db "jl", 0
s_i_jge:   .db "jge", 0
s_i_jle:   .db "jle", 0
s_i_jg:    .db "jg", 0
s_i_at:    .db "at", 0
s_i_fp:    .db "fp", 0
s_i_sp:    .db "sp", 0

; --- Texte -----------------------------------------------------------------
s_cd_0x:        .db "0x", 0
s_cd_wort:      .db ".word", 0
s_cd_titel:     .db "TB-HACK CODE VIEWER", 0
s_cd_titel2:    .db " Code Viewer ", 0
s_cd_kopf:      .db "ADDRESS   WORD      INSTRUCTION", 0
s_cd_keys:      .db "Up/Down  one instruction    PgUp/PgDn  page    G  go to    ESC  back", 0
s_cd_frage:     .db "Address (hex):", 0
s_cd_mitkopf:   .db "TBX program, wants to load at ", 0
s_cd_ohnekopf:  .db "No TBX header -- reading it as plain code", 0
s_cd_komma:     .db ", ", 0
s_cd_bytes:     .db " bytes", 0
s_cd_nichts:    .db "Nothing loaded -- showing whatever is at this address", 0
s_cd_geladen:   .db "File loaded. Show Code opens it.", 0
s_cd_keine:     .db "No file selected.", 0
s_cd_zugross:   .db "That file is larger than 512 KB -- more than a program may be.", 0
s_cd_leer:      .db "Nothing to show yet. Load a file, or use Disassemble Any Address.", 0
