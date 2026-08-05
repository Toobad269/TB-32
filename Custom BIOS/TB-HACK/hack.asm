; ===========================================================================
;  TB-HACK  --  die Bastlerwerkzeuge im BIOS
;
;  Alles Neue steht in dieser einen Datei. Der Rest der Firmware ruft davon
;  genau sieben Dinge auf:
;
;      hk_monitor          Reiter Hack: der Hex-Monitor auf dem ganzen Bild
;      hk_ports            Reiter Hack: Ports lesen und schreiben
;      hk_cmos             Reiter Hack: die Knopfzelle roh bearbeiten
;      hk_sektor           Reiter Hack: einen Plattensektor in den Monitor holen
;      hk_bootsektor       Reiter Hack: von welchem Sektor gestartet wird
;      hk_patches          Reiter Hack: die zwei Startpatches einstellen
;      hk_patch_anwenden   bios.asm, kurz vor dem Sprung in den Bootsektor
;
;  Wo die Einstellungen liegen: CM_HKBSEC0/1 bis CM_HKP2V, also 0x20..0x2D.
;  Die Grenzen sind dieselben wie bei TB-LOCK und aus demselben Grund: ueber
;  0x1F, weil setup_backup/setup_restore nur 0x10..0x1F fuer ESC sichert --
;  ein hier eingestellter Startsektor waere sonst beim Verlassen ohne
;  Speichern stillschweigend wieder weg. Und unter 0x2E, weil die Knopfzelle
;  genau ueber 0x10..0x2D ihre eigene Pruefsumme rechnet (siehe CMOS.save in
;  hardware/devices.py). Darueber liegt ab 0x30 der Uhrenversatz -- vier Byte,
;  die niemandem sonst gehoeren.
;
;  Was dieses BIOS bewusst NICHT tut: irgendetwas verbieten. Es gibt hier
;  keinen Port, den man nicht schreiben, und keine Adresse, die man nicht
;  ueberschreiben darf. Das ist der ganze Zweck -- und es heisst auch, dass
;  man sich damit den laufenden Rechner zerlegen kann. Siehe README.
; ===========================================================================

; Das Fragefenster fuer Hexeingaben
.equ HKD_X,        12
.equ HKD_Y,         9
.equ HKD_W,        56
.equ HKD_H,         7

; Der Hexmonitor: 16 Zeilen a 16 Byte, also 256 Byte je Seite
.equ HK_ZEILEN,    16
.equ HK_SPALTEN,   16
.equ HK_SEITE,     256
.equ HK_MY,         4                 ; Bildzeile, in der der Dump anfaengt

; Spalten im Dump: Adresse ab 1, Hexbytes ab 10 (je drei breit), Text ab 59
.equ HK_XHEX,      10
.equ HK_XTXT,      59

; Auf eine 4-Byte-Grenze, BEVOR hier der erste Befehl steht.
;
; Keine Kosmetik: hack.asm wird nach setup.asm eingebunden, und setup.asm
; hoert mit seiner Zeichenkettentabelle auf -- die endet auf einer krummen
; Adresse. Befehle sind auf dem TB-32 aber fest vier Byte breit und werden ab
; einer durch vier teilbaren Adresse geholt. Ohne diese Zeile faengt der Code
; hier versetzt an: der Rechner startet noch, der POST laeuft sauber, und beim
; ersten DEL zerlegt es ihn mit "Invalid opcode". Genau dieselbe Falle steht
; in TB-LOCK und in Doku 07.
.align 4

; ===========================================================================
;  Zwei Handgriffe, die jedes Werkzeug braucht
; ===========================================================================

; hk_kopf(r1 = Name des Werkzeugs): der Balken ganz oben
;
;  Links der Name, rechts die Marke des Chips. Das ist keine Zierde: die vier
;  Werkzeuge nehmen den ganzen Bildschirm ein, und dann ist der Kopfbalken die
;  einzige Stelle, an der noch steht, wessen Firmware hier gerade laeuft.
hk_kopf:
    push r6
    mov r6, r1
    movi r1, 0
    movi r2, 0
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, A_SEL
    call vid_hline
    movi r1, 2
    movi r2, 0
    mov r3, r6
    movi r4, A_SEL
    call vid_putsat
    movi r1, 58
    movi r2, 0
    li r3, s_hk_marke
    movi r4, A_SEL
    call vid_putsat
    pop r6
    ret

; hk_linie(r1 = Bildzeile): eine duenne Trennlinie ueber die Arbeitsflaeche
hk_linie:
    mov r2, r1
    movi r1, 1
    movi r3, SCR_W-2
    movi r4, 0xC4                     ; einfache waagerechte Linie, CP437
    movi r5, A_BG
    call vid_hline
    ret

; ===========================================================================
;  Eingabe einer Hexzahl
; ===========================================================================

; hk_hexziffer(r1 = Zeichen) -> r0 = 0..15, oder -1 wenn es keine Ziffer ist
hk_hexziffer:
    cmpi r1, 0x30                     ; '0'
    jl .nein
    cmpi r1, 0x39                     ; '9'
    jg .gross
    subi r0, r1, 0x30
    ret
.gross:
    cmpi r1, 0x41                     ; 'A'
    jl .nein
    cmpi r1, 0x46                     ; 'F'
    jg .klein
    subi r0, r1, 55                   ; 'A' - 10
    ret
.klein:
    cmpi r1, 0x61                     ; 'a'
    jl .nein
    cmpi r1, 0x66                     ; 'f'
    jg .nein
    subi r0, r1, 87                   ; 'a' - 10
    ret
.nein:
    li r0, 0xFFFFFFFF
    ret

; ---------------------------------------------------------------------------
;  hk_hexfrage(r1 = Ueberschrift, r2 = Frage, r3 = Stellen)
;      -> r0 = eingetippter Wert, r1 = 1 wenn bestaetigt, 0 bei ESC
;
;  Der Erfolg steht bewusst in einem EIGENEN Register und nicht als -1 im
;  Wert: 0xFFFFFFFF ist hier eine voellig normale Eingabe. Wer den Abbruch am
;  Vorzeichen erkennen wollte, koennte genau diesen Wert nie schreiben.
; ---------------------------------------------------------------------------
hk_hexfrage:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1                        ; Ueberschrift
    mov r7, r2                        ; Frage
    mov r8, r3                        ; Stellen
    movi r9, 0                        ; bisher eingetippter Wert

    movi r1, HKD_X                    ; Fenster aufziehen
    movi r2, HKD_Y
    movi r3, HKD_W
    movi r4, HKD_H
    movi r5, A_SEL
    call vid_fillrect
    movi r1, HKD_X
    movi r2, HKD_Y
    movi r3, HKD_W
    movi r4, HKD_H
    movi r5, A_SEL
    call vid_box
    movi r1, HKD_X+3
    movi r2, HKD_Y
    mov r3, r6
    movi r4, A_SEL
    call vid_putsat
    movi r1, HKD_X+3
    movi r2, HKD_Y+2
    mov r3, r7
    movi r4, A_SEL
    call vid_putsat
    movi r1, HKD_X+3
    movi r2, HKD_Y+HKD_H-2
    li r3, s_hk_hexkeys
    movi r4, A_SEL
    call vid_putsat

    movi r6, 0                        ; ab hier: Anzahl eingetippter Ziffern
.loop:
    movi r1, HKD_X+3                  ; Feldboden, dann die Ziffern darauf
    movi r2, HKD_Y+3
    mov r3, r8
    movi r4, 0x5F                     ; '_'
    movi r5, A_SEL
    call vid_hline
    cmpi r6, 0
    jz .warten
    movi r1, HKD_X+3
    movi r2, HKD_Y+3
    call vid_setcursor
    mov r1, r9
    movi r2, A_SEL
    mov r3, r6
    call vid_puthex
.warten:
    call kbd_getkey
    shri r10, r0, 8
    andi r11, r0, 0xFF
    cmpi r10, K_ENTER
    jz .fertig
    cmpi r10, K_ESC
    jz .abbruch
    cmpi r10, K_BACKSPACE
    jz .zurueck
    mov r1, r11
    call hk_hexziffer
    cmpi r0, 0
    jl .loop                          ; keine Hexziffer -- ignorieren
    cmp r6, r8
    jge .loop                         ; Feld ist voll
    shli r9, r9, 4
    add r9, r9, r0
    addi r6, r6, 1
    jmp .loop

.zurueck:
    cmpi r6, 0
    jz .loop
    shri r9, r9, 4
    subi r6, r6, 1
    jmp .loop

.fertig:
    cmpi r6, 0
    jz .abbruch                       ; ENTER auf ein leeres Feld = Abbruch
    mov r0, r9
    movi r1, 1
    jmp .done
.abbruch:
    movi r0, 0
    movi r1, 0
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Der Hex-Monitor
;
;  Zeigt 256 Byte ab HK_ADDR und laesst jedes davon aendern. Es gibt keine
;  Bereichspruefung: der Monitor zeigt genauso bereitwillig das ROM (wo ein
;  Schreibversuch schlicht wirkungslos bleibt) wie den Bildspeicher (wo man
;  beim Tippen sofort sieht, was passiert).
; ===========================================================================

; hk_attr(r1 = Index auf der Seite) -> r0 = Attribut, hell wenn der Cursor
; darauf steht
hk_attr:
    ldwa r0, HK_CUR
    cmp r0, r1
    jz .sel
    movi r0, A_BG
    ret
.sel:
    movi r0, A_SEL
    ret

; --- Das feste Beiwerk: Titel, Spaltenkopf, Tastenhilfe -------------------
hk_mon_rahmen:
    movi r1, A_BG
    call vid_clear
    li r1, s_hk_montitle
    call hk_kopf
    movi r1, 1
    movi r2, 2
    li r3, s_hk_monkopf
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 3                        ; Spaltenkopf vom Dump trennen
    call hk_linie
    movi r1, 1
    movi r2, 21
    li r3, s_hk_monkeys1
    movi r4, A_HELP
    call vid_putsat
    movi r1, 1
    movi r2, 22
    li r3, s_hk_monkeys2
    movi r4, A_HELP
    call vid_putsat
    ret

; --- Eine Seite zeichnen: 16 Zeilen Adresse, Hexbytes und Klartext --------
hk_mon_seite:
    push r6
    push r7
    push r8
    push r9
    movi r7, 0                        ; Zeile
.zeile:
    cmpi r7, HK_ZEILEN
    jae .fertig

    movi r1, 1                        ; die Adresse links
    addi r2, r7, HK_MY
    call vid_setcursor
    ldwa r1, HK_ADDR
    muli r10, r7, HK_SPALTEN
    add r1, r1, r10
    movi r2, A_TITLE
    movi r3, 8
    call vid_puthex

    movi r1, HK_XTXT-1                ; die zwei senkrechten Striche
    addi r2, r7, HK_MY
    movi r3, 0xB3
    movi r4, A_BG
    call vid_putat
    movi r1, HK_XTXT+HK_SPALTEN
    addi r2, r7, HK_MY
    movi r3, 0xB3
    movi r4, A_BG
    call vid_putat

    movi r6, 0                        ; Spalte
.spalte:
    cmpi r6, HK_SPALTEN
    jae .naechste
    muli r9, r7, HK_SPALTEN           ; r9 = Index auf der Seite
    add r9, r9, r6
    ldwa r8, HK_ADDR
    add r8, r8, r9
    ldb r8, [r8]                      ; r8 = das Byte selbst

    muli r1, r6, 3                    ; ... als zwei Hexziffern
    addi r1, r1, HK_XHEX
    addi r2, r7, HK_MY
    call vid_setcursor
    mov r1, r9
    call hk_attr
    mov r2, r0
    mov r1, r8
    movi r3, 2
    call vid_puthex

    mov r1, r9                        ; ... und als Zeichen rechts
    call hk_attr
    mov r4, r0
    mov r3, r8
    cmpi r3, 32
    jl .punkt
    cmpi r3, 127
    jl .zeichen
.punkt:
    movi r3, 0x2E                     ; '.' fuer alles Undruckbare
.zeichen:
    addi r1, r6, HK_XTXT
    addi r2, r7, HK_MY
    call vid_putat

    addi r6, r6, 1
    jmp .spalte
.naechste:
    addi r7, r7, 1
    jmp .zeile
.fertig:
    call hk_mon_fuss
    movi r1, 79                       ; Cursor aus dem Weg
    movi r2, 24
    call vid_setcursor
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Die Zeile unter dem Dump: wo der Cursor steht, was dort drin steht ---
hk_mon_fuss:
    push r6
    movi r1, 1
    movi r2, HK_MY+HK_ZEILEN
    movi r3, 78
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 1
    movi r2, HK_MY+HK_ZEILEN
    call vid_setcursor
    li r1, s_hk_at
    movi r2, A_TITLE
    call vid_puts
    ldwa r1, HK_ADDR
    ldwa r10, HK_CUR
    add r1, r1, r10
    movi r2, ATTR_BRIGHT
    movi r3, 8
    call vid_puthex
    li r1, s_hk_is
    movi r2, A_TITLE
    call vid_puts
    ldwa r6, HK_ADDR
    ldwa r10, HK_CUR
    add r6, r6, r10
    ldb r1, [r6]
    movi r2, ATTR_BRIGHT
    movi r3, 2
    call vid_puthex
    li r1, s_hk_sector
    movi r2, A_TITLE
    call vid_puts
    ldwa r1, HK_SEK
    movi r2, ATTR_BRIGHT
    movi r3, 4
    call vid_puthex
    pop r6
    ret

; ---------------------------------------------------------------------------
;  hk_monitor  --  die Schleife
; ---------------------------------------------------------------------------
hk_monitor:
    push r6
    push r7
.redraw:
    call hk_mon_rahmen
.loop:
    call hk_mon_seite
    call kbd_getkey
    shri r6, r0, 8                    ; Scancode
    andi r7, r0, 0xFF                 ; Zeichen

    cmpi r6, K_ESC
    jz .raus
    cmpi r6, K_LEFT
    jz .links
    cmpi r6, K_RIGHT
    jz .rechts
    cmpi r6, K_UP
    jz .hoch
    cmpi r6, K_DOWN
    jz .runter
    cmpi r6, K_PGUP
    jz .pgup
    cmpi r6, K_PGDN
    jz .pgdn
    cmpi r6, K_ENTER
    jz .edit
    cmpi r7, 0x67                     ; 'g'
    jz .goto
    cmpi r7, 0x47                     ; 'G'
    jz .goto
    cmpi r7, 0x73                     ; 's'
    jz .laden
    cmpi r7, 0x53                     ; 'S'
    jz .laden
    cmpi r7, 0x77                     ; 'w'
    jz .schreiben
    cmpi r7, 0x57                     ; 'W'
    jz .schreiben
    jmp .loop

; --- Bewegen. Am Rand rutscht die ganze Seite weiter, damit man mit den
;     Pfeiltasten allein durch den gesamten Speicher laufen kann.
.links:
    ldwa r10, HK_CUR
    cmpi r10, 0
    jnz .links_1
    call hk_seite_zurueck
    movi r10, HK_SPALTEN-1
    stwa HK_CUR, r10
    jmp .loop
.links_1:
    subi r10, r10, 1
    stwa HK_CUR, r10
    jmp .loop

.rechts:
    ldwa r10, HK_CUR
    cmpi r10, HK_SEITE-1
    jnz .rechts_1
    call hk_seite_vor
    movi r10, HK_SEITE-HK_SPALTEN
    stwa HK_CUR, r10
    jmp .loop
.rechts_1:
    addi r10, r10, 1
    stwa HK_CUR, r10
    jmp .loop

.hoch:
    ldwa r10, HK_CUR
    cmpi r10, HK_SPALTEN
    jae .hoch_1
    call hk_seite_zurueck             ; oben angekommen: Seite nachziehen
    jmp .loop
.hoch_1:
    subi r10, r10, HK_SPALTEN
    stwa HK_CUR, r10
    jmp .loop

.runter:
    ldwa r10, HK_CUR
    cmpi r10, HK_SEITE-HK_SPALTEN
    jae .runter_1
    addi r10, r10, HK_SPALTEN
    stwa HK_CUR, r10
    jmp .loop
.runter_1:
    call hk_seite_vor
    jmp .loop

.pgup:
    ldwa r10, HK_ADDR
    subi r10, r10, HK_SEITE
    stwa HK_ADDR, r10
    jmp .loop
.pgdn:
    ldwa r10, HK_ADDR
    addi r10, r10, HK_SEITE
    stwa HK_ADDR, r10
    jmp .loop

; --- Zu einer Adresse springen -------------------------------------------
.goto:
    li r1, s_hk_montitle2
    li r2, s_hk_askaddr
    movi r3, 8
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    andi r10, r0, HK_SPALTEN-1        ; Rest innerhalb der Zeile
    stwa HK_CUR, r10
    shri r10, r0, 4                   ; ... und der Rest ist der Zeilenanfang
    shli r10, r10, 4
    stwa HK_ADDR, r10
    jmp .redraw

; --- Das Byte unter dem Cursor aendern -----------------------------------
.edit:
    ldwa r10, HK_ADDR
    ldwa r11, HK_CUR
    add r10, r10, r11
    stwa HK_EDIT, r10                 ; merken: hk_hexfrage braucht r10 selbst
    li r1, s_hk_montitle2
    li r2, s_hk_askbyte
    movi r3, 2
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    ldwa r10, HK_EDIT
    stb [r10], r0
    jmp .redraw

; --- Einen Sektor von der Platte in den Puffer holen ---------------------
.laden:
    call hk_sektor
    jmp .redraw

; --- Den Puffer zurueck auf die Platte schreiben -------------------------
.schreiben:
    li r1, s_hk_wask
    call setup_frage
    cmpi r0, 0
    jz .redraw
    ldwa r1, HK_SEK
    movi r2, 1
    li r3, HK_SEKPUF
    call disk_write
    cmpi r0, 0
    jnz .schreibfehler
    li r1, s_hk_written
    jmp .schreibmelden
.schreibfehler:
    li r1, s_hk_wfailed
.schreibmelden:
    call setup_message
    jmp .redraw

.raus:
    pop r7
    pop r6
    ret

; Die Seite um eine Zeile verschieben -- der Cursor bleibt, wo er ist
hk_seite_zurueck:
    ldwa r10, HK_ADDR
    subi r10, r10, HK_SPALTEN
    stwa HK_ADDR, r10
    ret

hk_seite_vor:
    ldwa r10, HK_ADDR
    addi r10, r10, HK_SPALTEN
    stwa HK_ADDR, r10
    ret

; ===========================================================================
;  Einen Plattensektor holen und im Monitor anzeigen
;
;  Zusammen mit 'w' im Monitor ist das ein vollstaendiger Sektoreditor:
;  lesen, Byte fuer Byte aendern, zurueckschreiben. Der Puffer liegt bei
;  4 MB -- weit weg von SEC_PUFFER (2 MB, wo Secure Boot den Kernel
;  durchrechnet) und FLASH_PUFFER (3 MB).
; ===========================================================================
hk_sektor:
    push r6
    li r1, s_hk_sektitle
    li r2, s_hk_asksek
    movi r3, 4
    call hk_hexfrage
    cmpi r1, 0
    jz .raus
    mov r6, r0
    stwa HK_SEK, r6
    mov r1, r6
    movi r2, 1
    li r3, HK_SEKPUF
    call disk_read
    cmpi r0, 0
    jnz .fehler
    li r10, HK_SEKPUF
    stwa HK_ADDR, r10
    movi r10, 0
    stwa HK_CUR, r10
    jmp .raus
.fehler:
    li r1, s_hk_rfailed
    call setup_message
.raus:
    pop r6
    ret

; ===========================================================================
;  Die Portkonsole
;
;  Ein Fenster auf die Bausteine, ohne ein einziges Programm dazwischen.
;  Lesen ist nicht immer folgenlos -- P_KBD_DATA etwa gibt eine Taste heraus
;  und ist sie danach los. Das steht so in der README; hier wird nichts
;  davon abgefangen, weil genau das der Zweck dieses BIOS ist.
; ===========================================================================
hk_ports:
.redraw:
    movi r1, A_BG
    call vid_clear
    li r1, s_hk_ptitle
    call hk_kopf
    movi r1, 12                       ; die drei Werte vom Hinweis trennen
    call hk_linie
    movi r1, 6
    movi r2, 6
    li r3, s_hk_pport
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 6
    movi r2, 8
    li r3, s_hk_pread
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 6
    movi r2, 10
    li r3, s_hk_pwrote
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 2                        ; die drei langen Zeilen fangen weiter
    movi r2, 14                       ; links an -- ab Spalte 6 waeren sie
    li r3, s_hk_phint                 ; laenger als der Bildschirm breit ist,
    movi r4, A_HELP                   ; und vid_putsat bricht dann um
    call vid_putsat
    movi r1, 2
    movi r2, 21
    li r3, s_hk_pkeys
    movi r4, A_HELP
    call vid_putsat
    movi r1, 2
    movi r2, 22
    li r3, s_hk_pwarn
    movi r4, A_HELP
    call vid_putsat

.loop:
    movi r1, 26                       ; die drei Werte frisch hinschreiben
    movi r2, 6
    movi r3, 12
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 26
    movi r2, 6
    call vid_setcursor
    ldwa r1, HK_PORT
    movi r2, ATTR_BRIGHT
    movi r3, 4
    call vid_puthex

    movi r1, 26
    movi r2, 8
    movi r3, 12
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 26
    movi r2, 8
    call vid_setcursor
    ldwa r10, HK_PORT
    inr r1, r10                       ; hier passiert es wirklich
    movi r2, ATTR_BRIGHT
    movi r3, 8
    call vid_puthex

    movi r1, 26
    movi r2, 10
    movi r3, 12
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 26
    movi r2, 10
    call vid_setcursor
    ldwa r1, HK_WROTE
    movi r2, ATTR_BRIGHT
    movi r3, 8
    call vid_puthex

    call kbd_getkey
    shri r10, r0, 8
    andi r11, r0, 0xFF
    cmpi r10, K_ESC
    jz .raus
    cmpi r11, 0x70                    ; 'p'
    jz .port
    cmpi r11, 0x50                    ; 'P'
    jz .port
    cmpi r11, 0x77                    ; 'w'
    jz .write
    cmpi r11, 0x57                    ; 'W'
    jz .write
    jmp .loop                         ; jede andere Taste liest einfach neu

.port:
    li r1, s_hk_ptitle2
    li r2, s_hk_askport
    movi r3, 4
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    stwa HK_PORT, r0
    jmp .redraw

.write:
    li r1, s_hk_ptitle2
    li r2, s_hk_askval
    movi r3, 8
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    stwa HK_WROTE, r0
    ldwa r10, HK_PORT
    outr r0, r10                      ; port[r10] = r0
    jmp .redraw

.raus:
    ret

; ===========================================================================
;  Der CMOS-Editor
;
;  Alle 64 Plaetze der Knopfzelle roh, ohne Namen und ohne Ruecksicht. Was
;  ein anderes BIOS hier versteckt -- eine Passwortpruefsumme etwa -- liegt
;  hier einfach da.
;
;  F10 schreibt die Knopfzelle in ihre Datei. Ohne das bleibt jede Aenderung
;  nur im Baustein und ist beim naechsten Einschalten wieder weg.
; ===========================================================================
hk_cmos:
    push r6
    push r7
    push r8
    push r9
    movi r6, 0                        ; Cursor: Registernummer 0..63
.redraw:
    movi r1, A_BG
    call vid_clear
    li r1, s_hk_ctitle
    call hk_kopf
    movi r1, 5
    movi r2, 4
    li r3, s_hk_ckopf
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 5                        ; Spaltenkopf von den Zeilen trennen
    call hk_linie
    movi r1, 5
    movi r2, 13
    li r3, s_hk_chint1
    movi r4, A_HELP
    call vid_putsat
    movi r1, 5
    movi r2, 14
    li r3, s_hk_chint2
    movi r4, A_HELP
    call vid_putsat
    movi r1, 2
    movi r2, 21
    li r3, s_hk_ckeys
    movi r4, A_HELP
    call vid_putsat

.zeichnen:
    movi r7, 0                        ; Zeile 0..3
.zeile:
    cmpi r7, 4
    jae .warten
    movi r1, 5
    addi r2, r7, 6
    call vid_setcursor
    shli r1, r7, 4
    movi r2, A_TITLE
    movi r3, 2
    call vid_puthex

    movi r8, 0                        ; Spalte 0..15
.spalte:
    cmpi r8, 16
    jae .naechste
    shli r9, r7, 4
    add r9, r9, r8                    ; r9 = Registernummer
    muli r1, r8, 3
    addi r1, r1, 10
    addi r2, r7, 6
    call vid_setcursor
    mov r10, r9
    call cmos_read
    mov r1, r0
    movi r2, A_BG
    cmp r9, r6
    jnz .normal
    movi r2, A_SEL
.normal:
    movi r3, 2
    call vid_puthex
    addi r8, r8, 1
    jmp .spalte
.naechste:
    addi r7, r7, 1
    jmp .zeile

.warten:
    movi r1, 5                        ; Fusszeile: welches Register gerade
    movi r2, 11
    movi r3, 60
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 5
    movi r2, 11
    call vid_setcursor
    li r1, s_hk_creg
    movi r2, A_TITLE
    call vid_puts
    mov r1, r6
    movi r2, ATTR_BRIGHT
    movi r3, 2
    call vid_puthex
    li r1, s_hk_is
    movi r2, A_TITLE
    call vid_puts
    mov r10, r6
    call cmos_read
    mov r1, r0
    movi r2, ATTR_BRIGHT
    movi r3, 2
    call vid_puthex

    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_ESC
    jz .raus
    cmpi r10, K_LEFT
    jz .links
    cmpi r10, K_RIGHT
    jz .rechts
    cmpi r10, K_UP
    jz .hoch
    cmpi r10, K_DOWN
    jz .runter
    cmpi r10, K_ENTER
    jz .edit
    cmpi r10, K_F10
    jz .sichern
    jmp .zeichnen

.links:
    subi r6, r6, 1
    cmpi r6, 0
    jge .zeichnen
    movi r6, 63
    jmp .zeichnen
.rechts:
    addi r6, r6, 1
    cmpi r6, 64
    jl .zeichnen
    movi r6, 0
    jmp .zeichnen
.hoch:
    subi r6, r6, 16
    cmpi r6, 0
    jge .zeichnen
    addi r6, r6, 64
    jmp .zeichnen
.runter:
    addi r6, r6, 16
    cmpi r6, 64
    jl .zeichnen
    subi r6, r6, 64
    jmp .zeichnen

.edit:
    li r1, s_hk_ctitle2
    li r2, s_hk_askbyte
    movi r3, 2
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    mov r10, r6
    mov r11, r0
    call cmos_write
    jmp .redraw

.sichern:
    movi r10, CM_SAVE
    movi r11, 1
    call cmos_write
    li r1, s_hk_csaved
    call setup_message
    jmp .redraw

.raus:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Der Startsektor
;
;  Das serienmaessige BIOS liest immer Sektor 0. Hier steht die Nummer im
;  CMOS, in zwei Byte -- ein Byte haette bei Sektor 255 aufgehoert, und die
;  Platte hat 16384 davon.
; ===========================================================================
hk_bootsek_lesen:                     ; -> r0
    push r6
    movi r10, CM_HKBSEC1
    call cmos_read
    shli r6, r0, 8
    movi r10, CM_HKBSEC0
    call cmos_read
    or r0, r0, r6
    pop r6
    ret

hk_bootsek_setzen:                    ; r1 = Sektor
    push r6
    mov r6, r1
    movi r10, CM_HKBSEC0
    andi r11, r6, 0xFF
    call cmos_write
    movi r10, CM_HKBSEC1
    shri r11, r6, 8
    andi r11, r11, 0xFF
    call cmos_write
    pop r6
    ret

; Der Knopf im Reiter Hack
hk_bootsektor:
    li r1, s_hk_btitle
    li r2, s_hk_askboot
    movi r3, 4
    call hk_hexfrage
    cmpi r1, 0
    jz .abbruch
    mov r1, r0
    call hk_bootsek_setzen
    call setup_frame                  ; das Fragefenster wieder wegraeumen
    li r1, s_hk_bset
    call setup_message
    ret
.abbruch:
    call setup_frame
    ret

; ===========================================================================
;  Beim Einschalten: gehoert die Knopfzelle ueberhaupt uns?
;
;  Die Knopfzelle gehoert dem RECHNER, nicht dem BIOS. Auf demselben TB-32
;  koennen nacheinander TB-LOCK, COMPANY-OS und dieses hier im Sockel sitzen,
;  und alle drei benutzen die Plaetze ab 0x20 -- nur mit voellig anderer
;  Bedeutung. Wo TB-HACK seinen Startsektor liest, legt TB-LOCK die
;  Pruefsumme seines Passworts ab, und eine Pruefsumme ist praktisch Zufall.
;
;  Ungeprueft heisst das: ein Rechner, der von einem zufaelligen Sektor
;  starten will und vorher zwei Bytes an zufaellige Adressen schreibt. Das
;  ist schlimmer als eine kaputte Anzeige, und es faellt niemandem auf, weil
;  alles davon "eingestellt" aussieht.
;
;  Also wird beim Start nachgesehen, ob dort ueberhaupt etwas stehen kann,
;  das von uns stammt. Wenn nicht, ist der ganze Block fremd und wird
;  geleert -- nicht einzelne Felder zurechtgebogen, denn dann bliebe der
;  Rest der fremden Werte stehen. Dasselbe Vorgehen wie bei CM_TEMPLIMIT in
;  kuehlung_anwenden, nur konsequenter.
; ===========================================================================
hk_cmos_pruefen:
    push r6
    call hk_cmos_plausibel
    cmpi r0, 1
    jz .fertig
    movi r6, CM_HKBSEC0
.loeschen:
    cmpi r6, CM_HKP2V+1
    jae .gesichert
    mov r10, r6
    movi r11, 0
    call cmos_write
    addi r6, r6, 1
    jmp .loeschen
.gesichert:
    movi r10, CM_SAVE                 ; sofort festschreiben, sonst steht beim
    movi r11, 1                       ; naechsten Start wieder das Fremde da
    call cmos_write
    li r1, s_hk_fremd
    movi r2, ATTR_ERR
    call print
.fertig:
    pop r6
    ret

; -> r0 = 1, wenn die eigenen Plaetze aussehen, als haetten wir sie
;    beschrieben. Ein zufaelliger Block faellt an einer der vier Huerden:
;    die zwei Schalter sind 0 oder 1, der Startsektor liegt auf der Platte,
;    und eine Patchadresse passt in den Arbeitsspeicher.
hk_cmos_plausibel:
    push r6
    movi r10, CM_HKNOSIG
    call cmos_read
    cmpi r0, 1
    ja .nein
    movi r10, CM_HKPATCHON
    call cmos_read
    cmpi r0, 1
    ja .nein
    call hk_bootsek_lesen
    in r10, P_DISK_SIZE
    cmp r0, r10
    jae .nein
    movi r6, 0
.patch:
    cmpi r6, 2
    jae .ja
    mov r1, r6
    call hk_padr
    li r10, 0x01000000                ; 16 MB, mehr Speicher hat der TB-32 nicht
    cmp r0, r10
    jae .nein
    addi r6, r6, 1
    jmp .patch
.ja:
    movi r0, 1
    pop r6
    ret
.nein:
    movi r0, 0
    pop r6
    ret

; ===========================================================================
;  Die zwei Startpatches
;
;  Ein Patch ist eine Adresse und ein Byte. Kurz bevor das BIOS in den
;  Bootsektor springt, wird das Byte an die Adresse geschrieben -- also
;  nachdem der Sektor im Speicher liegt und bevor irgendein fremder Befehl
;  gelaufen ist. Genau das Zeitfenster, in dem man auf einem echten Rechner
;  nie steht.
;
;  Adresse 0 heisst "Platz unbenutzt". Damit braucht es kein eigenes
;  Gueltigkeitsbit je Patch, und Adresse 0 ist ohnehin der Interruptvektor
;  fuer Division durch null -- den will niemand mit einem einzelnen Byte
;  antasten.
; ===========================================================================

; r1 = Patchnummer (0 oder 1) -> r0 = CMOS-Basisregister des Patches
hk_pbase:
    muli r0, r1, 5
    addi r0, r0, CM_HKP1A0
    ret

; r1 = Patchnummer -> r0 = die gespeicherte Adresse
hk_padr:
    push r6
    push r7
    call hk_pbase
    mov r6, r0
    addi r10, r6, 3
    call cmos_read
    mov r7, r0
    shli r7, r7, 8
    addi r10, r6, 2
    call cmos_read
    or r7, r7, r0
    shli r7, r7, 8
    addi r10, r6, 1
    call cmos_read
    or r7, r7, r0
    shli r7, r7, 8
    mov r10, r6
    call cmos_read
    or r7, r7, r0
    mov r0, r7
    pop r7
    pop r6
    ret

; r1 = Patchnummer, r2 = Adresse
hk_padr_setzen:
    push r6
    push r7
    mov r7, r2
    call hk_pbase
    mov r6, r0
    mov r10, r6
    andi r11, r7, 0xFF
    call cmos_write
    shri r7, r7, 8
    addi r10, r6, 1
    andi r11, r7, 0xFF
    call cmos_write
    shri r7, r7, 8
    addi r10, r6, 2
    andi r11, r7, 0xFF
    call cmos_write
    shri r7, r7, 8
    addi r10, r6, 3
    andi r11, r7, 0xFF
    call cmos_write
    pop r7
    pop r6
    ret

; r1 = Patchnummer -> r0 = das gespeicherte Byte
hk_pwert:
    call hk_pbase
    addi r10, r0, 4
    call cmos_read
    ret

; r1 = Patchnummer, r2 = Byte
hk_pwert_setzen:
    push r6
    mov r6, r2
    call hk_pbase
    addi r10, r0, 4
    mov r11, r6
    call cmos_write
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Der Editor: vier Zeilen, je Patch eine Adresse und ein Byte
; ---------------------------------------------------------------------------
hk_patches:
    push r6
    push r7
    movi r6, 0                        ; markierte Zeile 0..3
.redraw:
    movi r1, A_BG
    call vid_clear
    li r1, s_hk_pattitle
    call hk_kopf
    movi r1, 10                       ; die vier Zeilen vom Hinweis trennen
    call hk_linie
    movi r1, 5
    movi r2, 12
    li r3, s_hk_pathint1
    movi r4, A_HELP
    call vid_putsat
    movi r1, 5
    movi r2, 13
    li r3, s_hk_pathint2
    movi r4, A_HELP
    call vid_putsat
    movi r1, 5
    movi r2, 21
    li r3, s_hk_patkeys
    movi r4, A_HELP
    call vid_putsat

.zeichnen:
    movi r7, 0
.zeile:
    cmpi r7, 4
    jae .warten
    movi r1, 5                        ; Balken ueber die Zeile
    addi r2, r7, 6
    movi r3, 60
    movi r4, 0x20
    movi r5, A_BG
    cmp r7, r6
    jnz .balken
    movi r5, A_SEL
.balken:
    call vid_hline

    movi r1, 7                        ; Beschriftung
    addi r2, r7, 6
    shli r10, r7, 2
    li r11, s_hk_patnamen
    add r10, r10, r11
    ldw r3, [r10]
    movi r4, A_BG
    cmp r7, r6
    jnz .name
    movi r4, A_SEL
.name:
    call vid_putsat

    movi r1, 34                       ; Wert
    addi r2, r7, 6
    call vid_setcursor
    shri r1, r7, 1                    ; Zeile 0,1 -> Patch 0; 2,3 -> Patch 1
    andi r10, r7, 1
    cmpi r10, 0
    jnz .istwert
    call hk_padr
    movi r3, 8
    jmp .ausgeben
.istwert:
    call hk_pwert
    movi r3, 2
.ausgeben:
    mov r1, r0
    movi r2, A_BG
    cmp r7, r6
    jnz .attr
    movi r2, A_SEL
.attr:
    call vid_puthex

    addi r7, r7, 1
    jmp .zeile

.warten:
    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_ESC
    jz .raus
    cmpi r10, K_UP
    jz .hoch
    cmpi r10, K_DOWN
    jz .runter
    cmpi r10, K_ENTER
    jz .edit
    jmp .zeichnen
.hoch:
    subi r6, r6, 1
    cmpi r6, 0
    jge .zeichnen
    movi r6, 3
    jmp .zeichnen
.runter:
    addi r6, r6, 1
    cmpi r6, 4
    jl .zeichnen
    movi r6, 0
    jmp .zeichnen

.edit:
    andi r10, r6, 1
    cmpi r10, 0
    jnz .edit_wert
    li r1, s_hk_pattitle2
    li r2, s_hk_askaddr
    movi r3, 8
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    mov r2, r0
    shri r1, r6, 1
    call hk_padr_setzen
    jmp .redraw
.edit_wert:
    li r1, s_hk_pattitle2
    li r2, s_hk_askbyte
    movi r3, 2
    call hk_hexfrage
    cmpi r1, 0
    jz .redraw
    mov r2, r0
    shri r1, r6, 1
    call hk_pwert_setzen
    jmp .redraw

.raus:
    pop r7
    pop r6
    ret

; ===========================================================================
;  hk_patch_anwenden  --  steht in bios.asm zwischen "Bootsektor geladen"
;  und dem Sprung hinein.
;
;  Spaeter geht nicht: sobald gesprungen ist, laeuft fremder Code. Frueher
;  auch nicht: dann waere der Sektor noch gar nicht im Speicher, und ein
;  Patch auf 0x7C00 traefe ins Leere.
; ===========================================================================
hk_patch_anwenden:
    push r6
    push r7
    push r8
    movi r10, CM_HKPATCHON
    call cmos_read
    cmpi r0, 0
    jz .raus
    movi r6, 0                        ; Patchnummer
    movi r7, 0                        ; wie viele wirklich gesetzt wurden
.loop:
    cmpi r6, 2
    jae .melden
    mov r1, r6
    call hk_padr
    cmpi r0, 0
    jz .weiter                        ; Adresse 0 = Platz unbenutzt
    mov r8, r0
    mov r1, r6
    call hk_pwert
    stb [r8], r0
    addi r7, r7, 1
.weiter:
    addi r6, r6, 1
    jmp .loop
.melden:
    cmpi r7, 0
    jz .raus
    li r1, s_hk_patched
    movi r2, ATTR_OK
    call vid_puts
    mov r1, r7
    movi r2, ATTR_OK
    call vid_putn
    movi r1, 10                       ; Zeilenumbruch, damit der Bootsektor
    call vid_putc                     ; nicht in dieselbe Zeile schreibt
.raus:
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Texte
; ===========================================================================
s_hk_marke:     .db "TB-HACK BIOS v2.5.2", 0
s_hk_hexkeys:   .db "0-9 A-F  type      BACKSPACE  erase      ENTER  ok      ESC  cancel", 0

s_hk_montitle:  .db "TB-HACK MEMORY MONITOR", 0
s_hk_montitle2: .db " Memory Monitor ", 0
s_hk_monkopf:   .db "ADDRESS   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F", 0
s_hk_monkeys1:  .db "Arrows  move      PgUp/PgDn  page      G  go to address      ENTER  edit byte", 0
s_hk_monkeys2:  .db "S  load disk sector into the buffer      W  write buffer back      ESC  back", 0
s_hk_at:        .db "Cursor at ", 0
s_hk_is:        .db "  =  ", 0
s_hk_sector:    .db "     Buffer holds sector ", 0
s_hk_askaddr:   .db "Address (hex):", 0
s_hk_askbyte:   .db "New value (hex, 2 digits):", 0

s_hk_sektitle:  .db " Load Disk Sector ", 0
s_hk_asksek:    .db "Sector number (hex):", 0
s_hk_wask:      .db "Write the buffer back to that sector?  ENTER = yes, ESC = no", 0
s_hk_written:   .db "Sector written.", 0
s_hk_wfailed:   .db "The disk refused the write. Nothing was changed.", 0
s_hk_rfailed:   .db "That sector could not be read. The buffer is unchanged.", 0

s_hk_ptitle:    .db "TB-HACK PORT CONSOLE", 0
s_hk_ptitle2:   .db " Port Console ", 0
s_hk_pport:     .db "Port", 0
s_hk_pread:     .db "Reads", 0
s_hk_pwrote:    .db "Last value written", 0
s_hk_phint:     .db "The port list is in Doku/02 -- 0x00A0 is the thermometer, 0x0050 the speaker.", 0
s_hk_pkeys:     .db "P  choose port    W  write a value    any key  read again    ESC  back", 0
s_hk_pwarn:     .db "Reading is not always free: port 0x0020 hands out a key and then loses it.", 0
s_hk_askport:   .db "Port number (hex):", 0
s_hk_askval:    .db "Value to write (hex):", 0

s_hk_ctitle:    .db "TB-HACK CMOS EDITOR", 0
s_hk_ctitle2:   .db " CMOS Editor ", 0
; Fuenf Fuehrungsleerzeichen: der Kopf steht ab x=5, die Bytespalten ab x=10.
s_hk_ckopf:     .db "     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F", 0
s_hk_creg:      .db "Register ", 0
s_hk_chint1:    .db "0x2E is the checksum, 0x2F the magic byte, 0x30-0x33 the clock offset.", 0
s_hk_chint2:    .db "Changing those is how you make the board forget its own settings.", 0
s_hk_ckeys:     .db "Arrows  move      ENTER  change byte      F10  save coin cell      ESC  back", 0
s_hk_csaved:    .db "Coin cell written to file.", 0

s_hk_btitle:    .db " Boot Sector ", 0
s_hk_askboot:   .db "Boot from which sector (hex)?", 0
s_hk_bset:      .db "Boot sector changed. F10 saves it, ESC does not undo it.", 0

s_hk_pattitle:  .db "TB-HACK BOOT PATCHES", 0
s_hk_pattitle2: .db " Boot Patch ", 0
s_hk_pathint1:  .db "Written straight into memory after the boot sector is loaded and", 0
s_hk_pathint2:  .db "before the jump into it. Address 0 means the slot is unused.", 0
s_hk_patkeys:   .db "Up/Down  select      ENTER  change      ESC  back", 0
s_hk_pat1a:     .db "Patch 1  Address", 0
s_hk_pat1v:     .db "Patch 1  Value", 0
s_hk_pat2a:     .db "Patch 2  Address", 0
s_hk_pat2v:     .db "Patch 2  Value", 0
s_hk_patched:   .db "Boot patches applied: ", 0
s_hk_fremd:     .db "CMOS held another BIOS's settings -- Hack tab reset to defaults.", 0

; Auf vier Byte, bevor die Zeigertabelle kommt -- .dw will ausgerichtet sein.
.align 4
s_hk_patnamen:  .dw s_hk_pat1a, s_hk_pat1v, s_hk_pat2a, s_hk_pat2v
