; ===========================================================================
;  BIOS-Setup  --  das Menue, das man mit DEL beim Start erreicht
;
;  Die Einstellungen landen im CMOS (batteriegepufferter Speicher), damit sie
;  einen Neustart ueberleben. Beim Verlassen mit ESC werden die alten Werte
;  wiederhergestellt -- genau wie bei einem echten Mainboard.
; ===========================================================================

.equ SET_X,        4
.equ SET_Y,        6
.equ SET_ENTSIZE,  16
.equ SET_TABS,     9                 ; + Company, + Event Log, + Exit
.equ A_BG,         0x17              ; grau auf blau
.equ A_TITLE,      0x1E              ; gelb auf blau
.equ A_SEL,        0x70              ; schwarz auf grau
.equ A_HELP,       0x1B              ; hellcyan auf blau

; Sonderregister: alles ab 0xC0 ist kein normaler CMOS-Platz, sondern eine
; Zeile mit eigenem Verhalten -- Uhr, Anzeige eines Messwerts oder ein Knopf.
; (Die Grenze lag frueher bei 0xF0; dort war nach vierzehn Sonderzeilen kein
; Platz mehr. Dann war auch ab 0xE0 kein Platz mehr -- Ereignisspeicher und
; Inventar brauchen zusammen zwoelf Zeilen. CMOS-Plaetze gehen nur bis 0x3F,
; also ist selbst 0xC0 noch reichlich weit weg.)
.equ REG_EV0,      0xC0            ; acht Zeilen Ereignisspeicher, 0xC0..0xC7
.equ REG_EV7,      0xC7
.equ REG_EVCLR,    0xC8            ; Knopf: Ereignisspeicher leeren
.equ REG_INVSER,   0xC9            ; nur Anzeige: Seriennummer
.equ REG_INVBOOT,  0xCA            ; nur Anzeige: Anzahl Starts
.equ REG_INVMIN,   0xCB            ; nur Anzeige: Betriebsminuten
.equ REG_DELAY,    0xCC            ; Startverzoegerung in Sekunden
.equ REG_EXITSAVE, 0xCD            ; Knopf: sichern und verlassen
.equ REG_EXITDROP, 0xCE            ; Knopf: verwerfen und verlassen
.equ REG_BIOSLEN,  0xE0            ; nur Anzeige: Groesse des BIOS-Chips
.equ REG_BIOSSUM,  0xE1            ; nur Anzeige: Pruefsumme des Chips
.equ REG_FLASH,    0xE2            ; Knopf: BIOS aus einer Datei neu brennen
.equ REG_RESTORE,  0xE3            ; Knopf: Sicherung zurueckspielen
.equ REG_PWSTATE,  0xE4            ; nur Anzeige: Installed / Not Installed
.equ REG_PWSET,    0xE5            ; Knopf: Passwort setzen oder aendern
.equ REG_PWCLR,    0xE6            ; Knopf: Passwort loeschen
; COMPANY-OS: die fuenf Schalter des Schalterworts. Sie liegen absichtlich
; luekenlos hintereinander -- setup_change rechnet die Bitnummer aus der
; Registernummer aus, statt fuenfmal dasselbe zu schreiben.
.equ REG_POL0,     0xE7            ; Owner Tag                 (Bit 0)
.equ REG_POL1,     0xE8            ; Block Compiler            (Bit 1)
.equ REG_POL2,     0xE9            ; Block Network             (Bit 2)
.equ REG_POL3,     0xEA            ; Require Login Password    (Bit 3)
.equ REG_POL4,     0xEB            ; Boot From Internal Disk   (Bit 4)
.equ REG_COTEXT,   0xEC            ; Knopf: Firmentext tippen
.equ REG_COBLK,    0xED            ; Knopf: Programme abhaken
.equ REG_COCLR,    0xEE            ; nur Anzeige: war das CMOS geleert?
.equ REG_PWUSTATE, 0xEF            ; nur Anzeige: Power-On-Passwort da?
.equ REG_PWUSET,   0xFE            ; Knopf: Power-On-Passwort setzen/aendern
.equ REG_PWUCLR,   0xFF            ; Knopf: Power-On-Passwort loeschen
.equ REG_TIME,     0xF0            ; Uhrzeit, mit ENTER editierbar
.equ REG_DATE,     0xF1            ; Datum, mit ENTER editierbar
.equ REG_DEFAULTS, 0xF2            ; Knopf: Standardwerte laden
.equ REG_TEMP,     0xF3            ; nur Anzeige: Temperatur
.equ REG_FAN,      0xF4            ; nur Anzeige: Luefter
.equ REG_THROT,    0xF5            ; nur Anzeige: Drosselung
.equ REG_MEM,      0xF6            ; nur Anzeige: Arbeitsspeicher
.equ REG_DISK,     0xF7            ; nur Anzeige: Plattengroesse
.equ REG_SUM,      0xF8            ; nur Anzeige: Pruefsumme des Startabbildes
.equ REG_TRUST,    0xF9            ; Knopf: aktuelles Abbild als gut merken
.equ REG_TEMPLIM,  0xFA            ; Drosselgrenze, in Zehnergrad gespeichert
.equ REG_TMAX,     0xFB            ; nur Anzeige: hoechste je gemessene Temperatur
.equ REG_VGA,      0xFC            ; nur Anzeige: Grafikkarte
.equ REG_INFO,     0xFD            ; reine Erklaerzeile, kein Wert

; r9 waehrend des ganzen Setups: der aktive Reiter (0..SET_TABS-1)
setup_main:
    push r6
    push r7
    push r8
    push r9

    call setup_backup
    movi r6, 0
    stwa SETUP_ENDE, r6               ; Merkzeichen des Reiters Exit
    movi r6, 0                        ; r6 = markierte Zeile
    movi r9, 0                        ; r9 = aktiver Reiter
    stwa SETUP_TAB, r9

.redraw:
    call setup_frame
.loop:
    stwa SETUP_ROW, r6
    mov r1, r6
    call setup_draw
    call kbd_getkey
    shri r7, r0, 8                    ; r7 = Scancode
    andi r8, r0, 0xFF                 ; r8 = ASCII

    cmpi r7, K_UP
    jz .up
    cmpi r7, K_DOWN
    jz .down
    cmpi r7, K_LEFT
    jz .tab_links
    cmpi r7, K_RIGHT
    jz .tab_rechts
    cmpi r7, K_ENTER
    jz .inc
    cmpi r8, 0x2B                     ; '+'
    jz .inc
    cmpi r8, 0x2D                     ; '-'
    jz .dec
    cmpi r7, K_F10
    jz .save
    cmpi r7, K_F5
    jz .defaults
    cmpi r7, K_ESC
    jz .cancel
    jmp .loop

.tab_links:
    ldwa r9, SETUP_TAB
    subi r9, r9, 1
    cmpi r9, 0
    jge .tab_setzen
    movi r9, SET_TABS-1
    jmp .tab_setzen
.tab_rechts:
    ldwa r9, SETUP_TAB
    addi r9, r9, 1
    cmpi r9, SET_TABS
    jl .tab_setzen
    movi r9, 0
.tab_setzen:
    stwa SETUP_TAB, r9
    movi r6, 0                        ; oben anfangen
    jmp .redraw

.up:
    subi r6, r6, 1
    cmpi r6, 0
    jge .loop
    call setup_zeilen                 ; r0 = Zeilen im aktiven Reiter
    subi r6, r0, 1
    jmp .loop
.down:
    addi r6, r6, 1
    call setup_zeilen
    cmp r6, r0
    jl .loop
    movi r6, 0
    jmp .loop
.inc:
    mov r1, r6
    movi r2, 1
    call setup_change
    ; Der Reiter Exit setzt beim Druck auf seine Knoepfe ein Merkzeichen.
    ; setup_change kann nicht selbst aus setup_main herausspringen -- also
    ; wird hier nachgesehen, direkt nach dem Knopfdruck. In der Tastenkette
    ; weiter unten kaeme man nie an: .inc springt gleich zurueck in .loop.
    ldwa r8, SETUP_ENDE
    cmpi r8, 0
    jnz .exit
    jmp .loop
.dec:
    mov r1, r6
    li r2, 0xFFFFFFFF
    call setup_change
    jmp .loop
.defaults:
    call setup_load_defaults
    jmp .redraw
.save:
    movi r10, CM_SAVE                 ; Schreiben auf dieses Register
    movi r11, 1                        ; loest das Sichern der Knopfzelle aus
    call cmos_write
    li r1, s_set_saved
    call setup_message
    jmp .exit
.cancel:
    call setup_restore
    li r1, s_set_cancel
    call setup_message
.exit:
    movi r1, ATTR_NORMAL
    call vid_clear
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Tabelle und Zeilenzahl des aktiven Reiters --------------------------
;  setup_tabelle: r0 = Zeiger auf die Eintragstabelle
;  setup_zeilen : r0 = Anzahl Zeilen
;  Beide lesen den Reiter aus SETUP_TAB -- ein Register dafuer durch alle
;  Unterprogramme zu schleifen waere im Assembler mehr Buchhaltung als Nutzen.
setup_tabelle:
    push r10
    push r11
    ldwa r10, SETUP_TAB
    shli r10, r10, 3                  ; je Reiter 8 Byte: Tabelle, Anzahl
    li r11, setup_tabs
    add r10, r10, r11
    ldw r0, [r10]
    pop r11
    pop r10
    ret

setup_zeilen:
    push r10
    push r11
    ldwa r10, SETUP_TAB
    shli r10, r10, 3
    li r11, setup_tabs
    add r10, r10, r11
    ldw r0, [r10+4]
    pop r11
    pop r10
    ret

setup_tabname:
    push r10
    push r11
    ldwa r10, SETUP_TAB
    shli r10, r10, 2
    li r11, setup_tabnamen
    add r10, r10, r11
    ldw r0, [r10]
    pop r11
    pop r10
    ret

; --- Alte CMOS-Werte sichern / zurueckholen (fuer ESC) --------------------
setup_backup:
    push r6
    push r7
    movi r6, 0x10
.loop:
    cmpi r6, 0x20
    jae .done
    mov r10, r6
    call cmos_read
    li r7, SETUP_SAVE
    addi r11, r6, -0x10
    add r7, r7, r11
    stb [r7], r0
    addi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

setup_restore:
    push r6
    push r7
    movi r6, 0x10
.loop:
    cmpi r6, 0x20
    jae .done
    li r7, SETUP_SAVE
    addi r11, r6, -0x10
    add r7, r7, r11
    ldb r11, [r7]
    mov r10, r6
    call cmos_write
    addi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

setup_load_defaults:
    movi r10, CM_BOOTDEV
    movi r11, 0
    call cmos_write
    movi r10, CM_QUICKBOOT
    movi r11, 0
    call cmos_write
    movi r10, CM_BEEP
    movi r11, 1
    call cmos_write
    movi r10, CM_CPUSPEED
    movi r11, 2
    call cmos_write
    movi r10, CM_VERBOSE
    movi r11, 1
    call cmos_write
    movi r10, CM_FANMODE
    movi r11, 0                       ; automatisch regeln
    call cmos_write
    movi r10, CM_TEMPLIMIT
    movi r11, 85                      ; Grad, wie beim echten Chipsatz
    call cmos_write
    out P_TEMPLIMIT, r11
    movi r10, CM_SECURE
    movi r11, 0
    call cmos_write
    ret

; --- Wert der markierten Zeile aendern (r1 = Zeile, r2 = +1/-1) -----------
setup_change:
    push r6
    push r7
    push r8
    muli r6, r1, SET_ENTSIZE
    push r1
    push r2
    call setup_tabelle
    pop r2
    pop r1
    add r6, r6, r0                    ; r6 = Zeiger auf den Eintrag
    ldw r7, [r6+4]                    ; r7 = CMOS-Register
    ; A6: die Startquelle ist festgenagelt, solange das Bit steht.
    cmpi r7, CM_BOOTDEV
    jnz .kein_bootdev
    push r2
    movi r1, POL_INTDISK
    call pol_frage
    pop r2
    cmpi r0, 0
    jnz .bootgesperrt
.kein_bootdev:
    cmpi r7, REG_DEFAULTS
    jz .defaults
    cmpi r7, REG_TRUST
    jz .trust
    cmpi r7, REG_TIME
    jz .zeit
    cmpi r7, REG_DATE
    jz .datum
    cmpi r7, REG_TEMPLIM
    jz .limit
    cmpi r7, REG_FLASH
    jz .flash
    cmpi r7, REG_RESTORE
    jz .restore
    cmpi r7, REG_PWSET
    jz .pwset
    cmpi r7, REG_PWCLR
    jz .pwclr
    cmpi r7, REG_COTEXT
    jz .cotext
    cmpi r7, REG_COBLK
    jz .coblk
    cmpi r7, REG_PWUSET
    jz .pwuset
    cmpi r7, REG_PWUCLR
    jz .pwuclr
    cmpi r7, REG_EVCLR
    jz .evclr
    cmpi r7, REG_DELAY
    jz .delay
    cmpi r7, REG_EXITSAVE
    jz .xsave
    cmpi r7, REG_EXITDROP
    jz .xdrop
    cmpi r7, REG_POL0
    jl .kein_pol
    cmpi r7, REG_POL4
    jle .polbit
.kein_pol:
    cmpi r7, 0xC0
    jae .done                         ; reine Anzeigezeilen
    ldw r8, [r6+8]                    ; r8 = Anzahl moeglicher Werte
    mov r10, r7
    call cmos_read
    add r0, r0, r2
    cmpi r0, 0
    jge .nowrap
    subi r0, r8, 1
    jmp .store
.nowrap:
    cmp r0, r8
    jl .store
    movi r0, 0
.store:
    mov r10, r7
    mov r11, r0
    call cmos_write
    jmp .done
.defaults:
    call setup_load_defaults
    jmp .done
.flash:
    call flash_bios
    jmp .done
.restore:
    call flash_restore
    jmp .done
.pwset:
    call pw_setzen
    jmp .done
.pwclr:
    call pw_loeschen
    jmp .done
.cotext:
    call firma_text_setzen
    jmp .done
.coblk:
    call firma_sperrliste
    jmp .done
.pwuset:
    call pwu_setzen
    jmp .done
.pwuclr:
    call pwu_loeschen
    jmp .done
.bootgesperrt:
    li r1, s_a6_fest
    call setup_message
    jmp .done
.delay:
    movi r10, CM_BOOTDELAY
    push r2
    call cmos_read
    pop r2
    add r0, r0, r2
    cmpi r0, 0
    jge .delay_max
    movi r0, 9
    jmp .delay_setzen
.delay_max:
    cmpi r0, 9
    jle .delay_setzen
    movi r0, 0
.delay_setzen:
    movi r10, CM_BOOTDELAY
    mov r11, r0
    call cmos_write
    jmp .done
.xsave:
    movi r10, CM_SAVE
    movi r11, 1
    call cmos_write
    li r1, s_set_saved
    call setup_message
    movi r0, 1
    stwa SETUP_ENDE, r0
    jmp .done
.xdrop:
    call setup_restore
    li r1, s_set_cancel
    call setup_message
    movi r0, 1
    stwa SETUP_ENDE, r0
    jmp .done
.evclr:
    call ev_leeren
    li r1, s_ev_geleert
    call setup_message
    jmp .done
.polbit:
    subi r7, r7, REG_POL0             ; Registernummer -> Bitnummer
    movi r1, 1
    shl r1, r1, r7
    call pol_umschalten
    call pw_sichern                   ; Richtlinien gelten sofort
    call firma_veroeffentlichen
    jmp .done
.trust:
    call secure_summe                 ; aktuelles Abbild durchrechnen
    mov r1, r0
    call secure_merken
    jmp .done
.zeit:
    li r1, felder_zeit
    movi r2, 3
    call setup_edit_felder
    jmp .done
.datum:
    li r1, felder_datum
    movi r2, 3
    call setup_edit_felder
    jmp .done
.limit:
    movi r10, CM_TEMPLIMIT
    call cmos_read
    muli r11, r2, 5                   ; in Fuenferschritten
    add r0, r0, r11
    cmpi r0, 60
    jge .lim_ok
    movi r0, 100
    jmp .lim_store
.lim_ok:
    cmpi r0, 100
    jle .lim_store
    movi r0, 60
.lim_store:
    movi r10, CM_TEMPLIMIT
    mov r11, r0
    call cmos_write
    out P_TEMPLIMIT, r11              ; sofort wirksam
.done:
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Feldeditor fuer Uhrzeit und Datum
;
;  r1 = Tabelle mit je drei Woertern (CMOS-Register, kleinster, groesster
;       erlaubter Wert), r2 = Anzahl der Felder.
;
;  Hoch/Runter aendert das gewaehlte Feld, Links/Rechts wechselt es, ENTER
;  oder ESC beendet. Welches Feld gerade dran ist, steht unten im Hilfekasten
;  -- so braucht der Zeilenzeichner nichts davon zu wissen.
; ===========================================================================
setup_edit_felder:
    push r6
    push r7
    push r8
    push r9
    mov r8, r1                        ; Tabelle
    mov r9, r2                        ; Anzahl Felder
    movi r6, 0                        ; gewaehltes Feld

.loop:
    movi r1, 5                        ; Hinweiszeile im Hilfekasten
    movi r2, 21
    movi r3, 70
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 5
    movi r2, 21
    li r3, s_edit_hilfe
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 5
    movi r2, 22
    movi r3, 70
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline
    movi r1, 5
    movi r2, 22
    muli r7, r6, 16
    add r7, r7, r8
    ldw r3, [r7+12]                   ; Name des Feldes (viertes Wort)
    movi r4, A_HELP
    call vid_putsat

    call kbd_getkey
    shri r7, r0, 8
    cmpi r7, K_ENTER
    jz .fertig
    cmpi r7, K_ESC
    jz .fertig
    cmpi r7, K_LEFT
    jz .links
    cmpi r7, K_RIGHT
    jz .rechts
    cmpi r7, K_UP
    jz .mehr
    cmpi r7, K_DOWN
    jz .weniger
    jmp .loop

.links:
    subi r6, r6, 1
    cmpi r6, 0
    jge .loop
    subi r6, r9, 1
    jmp .loop
.rechts:
    addi r6, r6, 1
    cmp r6, r9
    jl .loop
    movi r6, 0
    jmp .loop

.mehr:
    movi r5, 1
    jmp .aendern
.weniger:
    movi r5, 0
    subi r5, r5, 1
.aendern:
    muli r7, r6, 16
    add r7, r7, r8
    ldw r10, [r7]                     ; CMOS-Register
    push r10
    call cmos_read
    pop r10
    add r0, r0, r5
    ldw r11, [r7+4]                   ; kleinster Wert
    cmp r0, r11
    jge .obergrenze
    ldw r0, [r7+8]                    ; unterlaufen -> groesster Wert
    jmp .schreiben
.obergrenze:
    ldw r11, [r7+8]
    cmp r0, r11
    jle .schreiben
    ldw r0, [r7+4]                    ; ueberlaufen -> kleinster Wert
.schreiben:
    mov r11, r0
    call cmos_write
    call setup_zeile_neu
    jmp .loop

.fertig:
    call setup_frame                  ; Hilfekasten wieder herstellen
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; Die markierte Zeile neu zeichnen -- welche das ist, weiss der Aufrufer
; nicht mehr, also einfach alle. Kostet nichts, das Setup ist kein Spiel.
setup_zeile_neu:
    push r1
    ldwa r1, SETUP_ROW
    call setup_draw
    pop r1
    ret

; --- Grundgeruest des Setup-Bildschirms zeichnen --------------------------
setup_frame:
    push r6
    movi r1, A_BG
    call vid_clear

    movi r1, 0                        ; Titelbalken
    movi r2, 0
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, A_SEL
    call vid_hline
    movi r1, 24
    movi r2, 0
    li r3, s_set_title
    movi r4, A_SEL
    call vid_putsat

    call setup_reiter                 ; Reiterleiste unter dem Titel

    movi r1, 2                        ; Rahmen um die Liste
    movi r2, 3
    movi r3, 76
    movi r4, 14
    movi r5, A_BG
    call vid_box
    movi r1, 5
    movi r2, 3
    call setup_tabname
    mov r3, r0
    movi r4, A_TITLE
    call vid_putsat

    movi r1, 2                        ; Hilfe unten
    movi r2, 19
    movi r3, 76
    movi r4, 5
    movi r5, A_BG
    call vid_box
    movi r1, 5
    movi r2, 19
    li r3, s_set_help
    movi r4, A_TITLE
    call vid_putsat
    movi r1, 5
    movi r2, 20
    li r3, s_set_keys1
    movi r4, A_HELP
    call vid_putsat
    movi r1, 5
    movi r2, 21
    li r3, s_set_keys2
    movi r4, A_HELP
    call vid_putsat
    movi r1, 5
    movi r2, 22
    li r3, s_set_keys3
    movi r4, A_HELP
    call vid_putsat
    pop r6
    ret

; --- Reiterleiste: der aktive steht hell, die anderen dunkel -------------
; Laenge einer Zeichenkette (r1 = Zeiger) -> r0
setup_len:
    push r6
    mov r6, r1
    movi r0, 0
.loop:
    ldb r10, [r6]
    cmpi r10, 0
    jz .done
    addi r0, r0, 1
    addi r6, r6, 1
    jmp .loop
.done:
    pop r6
    ret

setup_reiter:
    push r6
    push r7
    push r8
    push r9
    movi r1, 0                        ; Streifen freiraeumen
    movi r2, 1
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, A_BG
    call vid_hline

    ldwa r9, SETUP_TAB              ; aktiver Reiter
    movi r6, 0                        ; Zaehler
    movi r8, 3                        ; x-Position
.loop:
    cmpi r6, SET_TABS
    jae .done
    shli r7, r6, 2
    li r10, setup_tabnamen
    add r7, r7, r10
    ldw r3, [r7]                      ; Name

    movi r4, A_BG                     ; Farbe
    cmp r6, r9
    jnz .normal
    movi r4, A_SEL
.normal:
    mov r1, r8
    movi r2, 1
    push r3
    call vid_putsat
    pop r3

    mov r1, r3                        ; Breite des Namens dazurechnen
    call setup_len
    add r8, r8, r0
    addi r8, r8, 1
    addi r6, r6, 1
    jmp .loop
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Alle Zeilen mit aktuellen Werten zeichnen (r1 = markierte Zeile) -----
setup_draw:
    push r6
    push r7
    push r8
    push r9
    mov r9, r1                        ; markierte Zeile
    movi r6, 0                        ; Zaehler
.loop:
    ; Zeilenzahl direkt vergleichen. Sie in r10-r12 zwischenzulagern waere
    ; falsch: das sind Kratzregister, die jeder Aufruf zerstoeren darf --
    ; siehe Doku/05 Konventionen.
    call setup_zeilen
    cmp r6, r0
    jae .done

    movi r8, A_BG                     ; Farbe bestimmen
    cmp r6, r9
    jnz .notsel
    movi r8, A_SEL
.notsel:
    add r2, r6, r6                    ; y = SET_Y + i (ohne Multiplikation)
    sub r2, r2, r6
    addi r2, r2, SET_Y

    movi r1, SET_X                    ; Balken ueber die ganze Breite
    movi r3, 70
    movi r4, 0x20
    mov r5, r8
    push r2
    call vid_hline
    pop r2

    muli r7, r6, SET_ENTSIZE
    push r2
    call setup_tabelle
    pop r2
    add r7, r7, r0

    movi r1, SET_X+2                  ; Beschriftung
    ldw r3, [r7]
    mov r4, r8
    push r2
    call vid_putsat
    pop r2

    movi r1, SET_X+38                 ; Wert
    mov r4, r8
    push r2
    push r7
    call setup_value
    pop r7
    pop r2

    addi r6, r6, 1
    jmp .loop
.done:
    movi r1, 79                       ; Cursor aus dem Weg
    movi r2, 24
    call vid_setcursor
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ===========================================================================
;  Secure Boot
;
;  Der Gedanke ist derselbe wie beim echten PC: Beim Start wird nachgerechnet,
;  ob Bootsektor und Kernel noch die sind, die man kennt. Nur ist die Rechnung
;  hier eine schlichte Pruefsumme statt einer Unterschrift mit Schluessel --
;  das Prinzip "erst pruefen, dann starten" bleibt dasselbe.
;
;  Die gemerkte Summe liegt in vier CMOS-Plaetzen, also in der Knopfzelle.
; ===========================================================================

; Pruefsumme ueber Bootsektor und Kernel -> r0
;
;  Frueher stand die Kernelgroesse im Bootsektor und der Kernel lag auf festen
;  Sektoren ab 1. Seit der Kernel als \SYSTEM\KERNEL.BIN im Dateisystem liegt
;  und der Bootsektor ihn dort sucht, muss Secure Boot **dieselbe** Datei
;  messen. Sonst prueft die Firmware Bytes, die gar niemand mehr startet --
;  und das waere schlimmer als keine Pruefung, weil es nach Sicherheit
;  aussieht.
;
;  Das ist keine Unterschrift mit Schluessel, sondern eine Pruefsumme -- sie
;  erkennt jede Aenderung, aber niemand koennte sie faelschungssicher nennen.
.equ SEC_PUFFER,   0x00200000

; --- TBFS, so weit die Firmware es kennen muss ----------------------------
;     Steht bewusst ein zweites Mal da (das erste Mal in system/boot.asm):
;     der Bootsektor kann keine BIOS-Innereien aufrufen. Wer den Aufbau in
;     system/fs.c aendert, muss beide Stellen nachziehen.
.equ SS_DIRSEC0,   513
.equ SS_DIRSECS,   8
.equ SS_ENTSIZE,   32
.equ SS_MAXFILES,  128
.equ SS_E_START,   16
.equ SS_E_SIZE,    20
.equ SS_E_INFO,    24
.equ SS_FT_DIR,    2

secure_summe:
    push r6
    push r7
    push r8
    push r9

    movi r9, 0x1234                   ; Startwert

    movi r1, 0                        ; Bootsektor lesen und verrechnen
    movi r2, 1
    li r3, SEC_PUFFER
    call disk_read
    li r6, SEC_PUFFER
    movi r8, 128                      ; 512 Byte = 128 Woerter
    call secure_block

    call kernel_finden                ; r0 = Startsektor, r1 = Sektoren
    cmpi r0, 0
    jle .ohne_kernel                  ; keine Datei da -- dann eben nur ROM
    mov r7, r1
    mov r2, r1
    mov r1, r0
    li r3, SEC_PUFFER
    call disk_read
    cmpi r0, 0
    jnz .ohne_kernel
    li r6, SEC_PUFFER
    shli r8, r7, 7                    ; Sektoren * 128 = Anzahl Woerter
    call secure_block
.ohne_kernel:
    ; Das ROM gehoert dazu -- sonst waere ein veraendertes BIOS unsichtbar,
    ; und gerade das will man ja bemerken.
    li r6, ROM_BASE
    li r8, 4096                       ; 16 KB reichen als Fingerabdruck
    call secure_block

    mov r0, r9
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; r6 = Adresse, r8 = Anzahl Woerter, r9 = laufende Summe -> r9
secure_block:
.loop:
    cmpi r8, 0
    jz .fertig
    ldw r0, [r6]
    muli r9, r9, 31
    add r9, r9, r0
    addi r6, r6, 4
    subi r8, r8, 1
    jmp .loop
.fertig:
    ret

; Sucht \SYSTEM\KERNEL.BIN -- genau die Suche, die auch der Bootsektor macht.
;   -> r0 = erster Sektor (oder -1), r1 = Anzahl Sektoren
;  Verglichen werden die ersten zwoelf Namensbytes als drei Woerter; die
;  Eintraege sind mit Nullbytes aufgefuellt, deshalb genuegt das.
;  ACHTUNG: benutzt SEC_PUFFER als Kratzpapier.
kernel_finden:
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9

    li r1, SS_DIRSEC0                 ; Verzeichnis holen
    movi r2, SS_DIRSECS
    li r3, SEC_PUFFER
    call disk_read
    cmpi r0, 0
    jnz .weg

    li r4, SEC_PUFFER                 ; erst den Ordner \SYSTEM
    movi r5, 0
    li r6, 0x54535953                 ; "SYST"
    li r7, 0x00004D45                 ; "EM"
.sys_loop:
    ldw r9, [r4]
    cmp r9, r6
    jnz .sys_next
    ldw r9, [r4+4]
    cmp r9, r7
    jnz .sys_next
    ldw r9, [r4+SS_E_INFO]
    shri r8, r9, 16                   ; Elternordner+1; 0 = Hauptverzeichnis
    cmpi r8, 0
    jnz .sys_next
    andi r9, r9, 0xFF
    cmpi r9, SS_FT_DIR
    jz .sys_found
.sys_next:
    addi r4, r4, SS_ENTSIZE
    addi r5, r5, 1
    cmpi r5, SS_MAXFILES
    jl .sys_loop
    jmp .weg
.sys_found:
    addi r8, r5, 1

    li r4, SEC_PUFFER                 ; und darin KERNEL.BIN
    movi r5, 0
    li r6, 0x4E52454B                 ; "KERN"
    li r7, 0x422E4C45                 ; "EL.B"
.k_loop:
    ldw r9, [r4]
    cmp r9, r6
    jnz .k_next
    ldw r9, [r4+4]
    cmp r9, r7
    jnz .k_next
    ldw r9, [r4+8]
    li r0, 0x00004E49                 ; "IN" -- sonst passte auch KERNEL.BAK
    cmp r9, r0
    jnz .k_next
    ldw r9, [r4+SS_E_INFO]
    shri r9, r9, 16
    cmp r9, r8
    jz .k_found
.k_next:
    addi r4, r4, SS_ENTSIZE
    addi r5, r5, 1
    cmpi r5, SS_MAXFILES
    jl .k_loop
    jmp .weg
.k_found:
    ldw r1, [r4+SS_E_SIZE]
    addi r1, r1, 511
    shri r1, r1, 9                    ; Groesse in Byte -> Sektoren
    ldw r0, [r4+SS_E_START]
    jmp .fertig
.weg:
    movi r0, 0-1
    movi r1, 0
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    pop r5
    pop r4
    ret

; r0 = die im CMOS gemerkte Pruefsumme
secure_gemerkt:
    push r6
    push r10
    movi r10, CM_SUM0
    call cmos_read
    mov r6, r0
    movi r10, CM_SUM1
    call cmos_read
    shli r0, r0, 8
    or r6, r6, r0
    movi r10, CM_SUM2
    call cmos_read
    shli r0, r0, 16
    or r6, r6, r0
    movi r10, CM_SUM3
    call cmos_read
    shli r0, r0, 24
    or r6, r6, r0
    mov r0, r6
    pop r10
    pop r6
    ret

; Pruefsumme in r1 ins CMOS schreiben
secure_merken:
    push r6
    push r10
    push r11
    mov r6, r1
    movi r10, CM_SUM0
    andi r11, r6, 0xFF
    call cmos_write
    movi r10, CM_SUM1
    shri r11, r6, 8
    andi r11, r11, 0xFF
    call cmos_write
    movi r10, CM_SUM2
    shri r11, r6, 16
    andi r11, r11, 0xFF
    call cmos_write
    movi r10, CM_SUM3
    shri r11, r6, 24
    andi r11, r11, 0xFF
    call cmos_write
    pop r11
    pop r10
    pop r6
    ret

; --- Zehntelgrad als "72.5 C" ausgeben (r0 = Wert, r4 = Attribut) --------
setup_zehntel:
    push r6
    push r7
    mov r6, r0
    divi r1, r6, 10
    mov r2, r4
    call vid_putn
    movi r1, 0x2E                     ; Punkt
    mov r2, r4
    call vid_putc
    modi r1, r6, 10
    mov r2, r4
    call vid_putn
    li r1, s_grad
    mov r2, r4
    call vid_puts
    pop r7
    pop r6
    ret

; --- Wert eines Eintrags ausgeben (r1 = x, r2 = y, r4 = attr, r7 = Eintrag)
setup_value:
    push r6
    push r8
    push r9
    mov r8, r1
    mov r9, r2
    call vid_setcursor
    ldw r6, [r7+4]                    ; CMOS-Register
    cmpi r6, REG_TIME
    jz .time
    cmpi r6, REG_DATE
    jz .date
    cmpi r6, REG_DEFAULTS
    jz .action
    cmpi r6, REG_TRUST
    jz .action
    cmpi r6, REG_FLASH
    jz .action
    cmpi r6, REG_RESTORE
    jz .action
    cmpi r6, REG_PWSET
    jz .action
    cmpi r6, REG_PWCLR
    jz .action
    cmpi r6, REG_PWSTATE
    jz .pwstate
    cmpi r6, REG_COTEXT
    jz .action
    cmpi r6, REG_COBLK
    jz .action
    cmpi r6, REG_COCLR
    jz .coclr
    cmpi r6, REG_PWUSET
    jz .action
    cmpi r6, REG_PWUCLR
    jz .action
    cmpi r6, REG_PWUSTATE
    jz .pwustate
    cmpi r6, REG_EVCLR
    jz .action
    cmpi r6, REG_EXITSAVE
    jz .action
    cmpi r6, REG_EXITDROP
    jz .action
    cmpi r6, REG_DELAY
    jz .delayv
    cmpi r6, REG_INVSER
    jz .invser
    cmpi r6, REG_INVBOOT
    jz .invboot
    cmpi r6, REG_INVMIN
    jz .invmin
    cmpi r6, REG_EV0
    jl .kein_ev
    cmpi r6, REG_EV7
    jle .evzeile
.kein_ev:
    cmpi r6, REG_POL0
    jl .kein_polv
    cmpi r6, REG_POL4
    jle .polv
.kein_polv:
    cmpi r6, REG_BIOSLEN
    jz .bioslen
    cmpi r6, REG_BIOSSUM
    jz .biossum
    cmpi r6, REG_TEMP
    jz .temp
    cmpi r6, REG_TMAX
    jz .tmax
    cmpi r6, REG_FAN
    jz .fan
    cmpi r6, REG_THROT
    jz .throt
    cmpi r6, REG_MEM
    jz .mem
    cmpi r6, REG_DISK
    jz .disk
    cmpi r6, REG_VGA
    jz .vga
    cmpi r6, REG_SUM
    jz .sum
    cmpi r6, REG_TEMPLIM
    jz .templim
    cmpi r6, REG_INFO
    jz .done

    mov r10, r6                       ; normaler CMOS-Wert
    call cmos_read
    ldw r6, [r7+12]                   ; Tabelle mit Klartexten?
    cmpi r6, 0
    jz .number
    shli r10, r0, 2
    add r6, r6, r10
    ldw r1, [r6]
    mov r2, r4
    call vid_puts
    jmp .done
.number:
    mov r1, r0
    mov r2, r4
    call vid_putn
    jmp .done
.action:
    li r1, s_set_press
    mov r2, r4
    call vid_puts
    jmp .done

; --- Steht ein Passwort? -------------------------------------------------
.pwstate:
    call pw_gesetzt
    cmpi r0, 0
    jz .pw_nein
    li r1, s_pw_inst
    mov r2, r4
    call vid_puts
    jmp .done
.pw_nein:
    li r1, s_pw_notinst
    mov r2, r4
    call vid_puts
    jmp .done

.pwustate:
    push r4
    call pwu_gesetzt
    pop r4
    cmpi r0, 0
    jz .pwu_nein
    li r1, s_pw_inst
    mov r2, r4
    call vid_puts
    jmp .done
.pwu_nein:
    li r1, s_pw_notinst
    mov r2, r4
    call vid_puts
    jmp .done

; --- B1: eine Zeile des Ereignisspeichers -------------------------------
.evzeile:
    subi r6, r6, REG_EV0
    mov r1, r6
    mov r2, r4
    call ev_zeile_zeigen
    jmp .done

; --- B3: Inventar -------------------------------------------------------
.invser:
    li r1, TXT_BUF
    push r4
    li r2, NV_SERIAL
    push r1
    mov r1, r2
    li r2, TXT_BUF
    movi r3, 15
    call nv_str_lesen
    pop r1
    pop r4
    li r1, TXT_BUF
    mov r2, r4
    call vid_puts
    jmp .done
.invboot:
    push r4
    movi r1, NV_BOOTS
    call nv_read32
    pop r4
    mov r1, r0
    mov r2, r4
    call vid_putn
    jmp .done
.delayv:
    movi r10, CM_BOOTDELAY
    push r4
    call cmos_read
    pop r4
    mov r1, r0
    mov r2, r4
    call vid_putn
    li r1, s_sekunden
    mov r2, r4
    call vid_puts
    jmp .done
.invmin:
    push r4
    movi r1, NV_MINUTES
    call nv_read32
    pop r4
    mov r1, r0
    mov r2, r4
    call vid_putn
    li r1, s_minuten
    mov r2, r4
    call vid_puts
    jmp .done

; --- Ein Bit des Schalterworts als Enabled/Disabled ----------------------
.polv:
    subi r6, r6, REG_POL0
    movi r1, 1
    shl r1, r1, r6
    push r4
    call pol_frage
    pop r4
    cmpi r0, 0
    jz .polv_aus
    li r1, s_on
    mov r2, r4
    call vid_puts
    jmp .done
.polv_aus:
    li r1, s_off
    mov r2, r4
    call vid_puts
    jmp .done

; --- Wurde die Knopfzelle gezogen? --------------------------------------
.coclr:
    push r4
    call intrusion_frage
    pop r4
    cmpi r0, 0
    jz .coclr_nein
    li r1, s_co_yes
    mov r2, r4
    call vid_puts
    jmp .done
.coclr_nein:
    li r1, s_co_no
    mov r2, r4
    call vid_puts
    jmp .done

; --- Der BIOS-Chip beschreibt sich selbst --------------------------------
;     Beides steht im Kopf des Abbildes, das gerade laeuft. Wer ein eigenes
;     BIOS geflasht hat, sieht hier sofort, ob es wirklich seines ist.
.bioslen:
    li r10, ROM_BASE + BIOSHDR_LEN
    ldw r1, [r10]
    mov r2, r4
    call vid_putn
    li r1, s_setbyte
    mov r2, r4
    call vid_puts
    jmp .done
.biossum:
    li r10, ROM_BASE + BIOSHDR_SUM
    ldw r1, [r10]
    mov r2, r4
    movi r3, 8                        ; acht Stellen -- vid_puthex braucht das
    call vid_puthex
    jmp .done

; --- Messwerte, direkt vom Baustein gelesen ------------------------------
.temp:
    in r0, P_TEMP
    call setup_zehntel
    jmp .done
.tmax:
    in r0, P_TEMPMAX
    call setup_zehntel
    jmp .done
.fan:
    in r0, P_FAN
    mov r1, r0
    mov r2, r4
    call vid_putn
    li r1, s_proz
    mov r2, r4
    call vid_puts
    jmp .done
.throt:
    in r0, P_THROTTLE
    mov r1, r0
    mov r2, r4
    call vid_putn
    li r1, s_proz
    mov r2, r4
    call vid_puts
    jmp .done
.mem:
    ldwa r1, BDA_MEMKB
    mov r2, r4
    call vid_putn
    li r1, s_setkb
    mov r2, r4
    call vid_puts
    jmp .done
.disk:
    ldwa r1, BDA_DISKSEC
    mov r2, r4
    call vid_putn
    li r1, s_setsec
    mov r2, r4
    call vid_puts
    jmp .done
.vga:
    li r1, s_vgatyp
    mov r2, r4
    call vid_puts
    jmp .done
.sum:
    call secure_gemerkt                ; r0 = gemerkte Pruefsumme
    mov r1, r0
    mov r2, r4
    movi r3, 8                         ; vid_puthex will die Stellenzahl in r3
    call vid_puthex
    jmp .done
.templim:
    movi r10, CM_TEMPLIMIT
    call cmos_read
    mov r1, r0                         ; steht direkt in Grad im CMOS
    mov r2, r4
    call vid_putn
    li r1, s_grad
    mov r2, r4
    call vid_puts
    jmp .done
.time:
    movi r10, CM_HOUR
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
    movi r1, 0x3A
    mov r2, r4
    call vid_putc
    movi r10, CM_MIN
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
    movi r1, 0x3A
    mov r2, r4
    call vid_putc
    movi r10, CM_SEC
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
    jmp .done
.date:
    movi r10, CM_DAY
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
    movi r1, 0x2E
    mov r2, r4
    call vid_putc
    movi r10, CM_MONTH
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
    movi r1, 0x2E
    mov r2, r4
    call vid_putc
    li r1, s_set_20
    mov r2, r4
    call vid_puts
    movi r10, CM_YEAR
    call cmos_read
    mov r1, r0
    mov r2, r4
    call setup_put2
.done:
    pop r9
    pop r8
    pop r6
    ret

; --- Zahl zweistellig mit fuehrender Null (r1 = Zahl, r2 = Attribut) ------
setup_put2:
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    cmpi r6, 10
    jge .two
    movi r1, 0x30
    mov r2, r7
    call vid_putc
.two:
    mov r1, r6
    mov r2, r7
    call vid_putn
    pop r7
    pop r6
    ret

; --- Kurze Meldung unten einblenden (r1 = Text) ---------------------------
; ===========================================================================
;  Firmware: den BIOS-Chip neu beschreiben
;
;  Das Gegenstueck zu "BIOS Flashback" auf einem echten Board: eine Datei
;  vom Wirtsrechner aussuchen, pruefen, brennen.
;
;  Wichtig ist die Arbeitsteilung. Der Chip selbst prueft NICHTS -- er nimmt
;  jedes Byte, das man ihm gibt, genau wie in echt. Ob ein Abbild taugt,
;  entscheidet diese Firmware hier. Und weil eine kaputte Firmware sich nicht
;  selbst pruefen kann, sieht das Mainboard beim Einschalten noch einmal nach
;  und greift notfalls zur Sicherung (hardware/machine.py, rom_pruefen).
;  Drei Stellen, drei verschiedene Zeitpunkte -- so faengt man auch den Fall,
;  in dem das Flashen selbst schiefgeht.
; ===========================================================================

.equ FLASH_PUFFER, 0x00300000       ; hierhin kommt das Abbild zum Pruefen

; Prueft ein BIOS-Abbild ab r1, r2 Byte lang.
;   -> r0 = 0 gut, 1 keine Kennung, 2 Laenge passt nicht, 3 Pruefsumme falsch
bios_pruefen:
    push r6
    push r7
    push r8
    push r9
    cmpi r2, BIOSHDR_ENDE
    jl .laenge
    ldw r0, [r1+BIOSHDR_MAGIC]
    li r7, BIOS_MAGIC
    cmp r0, r7
    jnz .kennung
    ldw r0, [r1+BIOSHDR_LEN]
    cmp r0, r2                        ; Laenge im Kopf gegen die Dateigroesse
    jnz .laenge

    ; Die Pruefsumme rechnet ueber das ganze Abbild -- ihr eigenes Feld dabei
    ; als Null. Also kurz herausnehmen und danach zurueckschreiben.
    addi r6, r1, BIOSHDR_SUM
    ldw r7, [r6]                      ; r7 = gemerkte Summe
    movi r0, 0
    stw [r6], r0
    movi r9, 0x1234
    mov r6, r1
    shri r8, r2, 2                    ; Byte -> Woerter
    call secure_block                 ; r9 = gerechnete Summe
    addi r6, r1, BIOSHDR_SUM
    stw [r6], r7
    cmp r9, r7
    jnz .summe
    movi r0, 0
    jmp .raus
.kennung:
    movi r0, 1
    jmp .raus
.laenge:
    movi r0, 2
    jmp .raus
.summe:
    movi r0, 3
.raus:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; Fragt in der Fusszeile nach.  r1 = Text  ->  r0 = 1 bei ENTER, sonst 0
setup_frage:
    push r6
    mov r6, r1
    movi r1, 0
    movi r2, SCR_H-1
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, A_SEL
    call vid_hline
    movi r1, 2
    movi r2, SCR_H-1
    mov r3, r6
    movi r4, A_SEL
    call vid_putsat
.warte:
    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_ENTER
    jz .ja
    cmpi r10, K_ESC
    jz .nein
    jmp .warte
.ja:
    movi r0, 1
    pop r6
    ret
.nein:
    movi r0, 0
    pop r6
    ret

; Der Knopf "Flash BIOS from File"
flash_bios:
    push r6
    push r7

    movi r10, 1                       ; Wirtsrechner nach einer Datei fragen
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    cmpi r0, 0
    jnz .keine
    in r7, P_FLASH_SIZE
    cmpi r7, 0
    jz .keine

    li r10, FLASH_PUFFER              ; ... und sie in den Arbeitsspeicher holen
    out P_FLASH_ADDR, r10
    movi r10, 2
    out P_FLASH_CMD, r10

    li r1, FLASH_PUFFER               ; erst pruefen, dann brennen
    mov r2, r7
    call bios_pruefen
    cmpi r0, 3
    jz .summefalsch
    cmpi r0, 0
    jnz .keinbios

    li r1, s_fl_ask
    call setup_frage
    cmpi r0, 0
    jz .abbruch

    movi r10, 3                       ; brennen
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    cmpi r0, 0
    jnz .fehler
    li r1, s_fl_ok
    jmp .melden
.keine:
    li r1, s_fl_none
    jmp .melden
.keinbios:
    li r1, s_fl_bad
    jmp .melden
.summefalsch:
    li r1, s_fl_sum
    jmp .melden
.fehler:
    li r1, s_fl_err
    jmp .melden
.abbruch:
    li r1, s_set_cancel
.melden:
    call setup_message
    pop r7
    pop r6
    ret

; Der Knopf "Restore Backup BIOS"
flash_restore:
    movi r10, 4
    out P_FLASH_CMD, r10
    in r0, P_FLASH_CMD
    cmpi r0, 0
    jnz .nichts
    li r1, s_fl_back
    jmp .melden
.nichts:
    li r1, s_fl_nobak
.melden:
    call setup_message
    ret

setup_message:
    push r6
    mov r6, r1
    movi r1, 0
    movi r2, SCR_H-1
    movi r3, SCR_W
    movi r4, 0x20
    movi r5, A_SEL
    call vid_hline
    movi r1, 2
    movi r2, SCR_H-1
    mov r3, r6
    movi r4, A_SEL
    call vid_putsat
    movi r1, 60
    call delay
    pop r6
    ret

; ===========================================================================
;  Tabelle der Einstellungen
;  Aufbau je Eintrag: Beschriftung, CMOS-Register, Anzahl Werte, Klartexte
; ===========================================================================

; --- Die vier Reiter -----------------------------------------------------
;  je Reiter: Zeiger auf die Eintragstabelle, Anzahl der Zeilen
setup_tabs:
    .dw tab_main,     8
    .dw tab_hardware, 5
    .dw tab_cooling,  6
    .dw tab_security, 4
    .dw tab_password, 8
    .dw tab_company,  12
    .dw tab_events,   10
    .dw tab_firmware, 5
    .dw tab_exit,     4

setup_tabnamen:
    .dw s_tab_main, s_tab_hw, s_tab_cool, s_tab_sec, s_tab_pw, s_tab_comp, s_tab_ev, s_tab_fw, s_tab_exit

;  je Eintrag: Beschriftung, CMOS-Register, Anzahl Werte, Klartexttabelle
tab_main:
    .dw s_e_time,  REG_TIME,     0, 0
    .dw s_e_date,  REG_DATE,     0, 0
    .dw s_e_quick, CM_QUICKBOOT, 2, opts_onoff
    .dw s_e_beep,  CM_BEEP,      2, opts_onoff
    .dw s_e_verb,  CM_VERBOSE,   2, opts_verb
    .dw s_e_boot2, CM_BOOTMODE,  2, opts_boot2
    .dw s_e_delay, REG_DELAY,    0, 0
    .dw s_e_def,   REG_DEFAULTS, 0, 0

tab_hardware:
    .dw s_e_speed, CM_CPUSPEED,  5, opts_speed
    .dw s_e_boot,  CM_BOOTDEV,   3, opts_boot
    .dw s_e_mem,   REG_MEM,      0, 0
    .dw s_e_disk,  REG_DISK,     0, 0
    .dw s_e_vga,   REG_VGA,      0, 0

tab_cooling:
    .dw s_e_fanm,  CM_FANMODE,   3, opts_fan
    .dw s_e_lim,   REG_TEMPLIM,  0, 0
    .dw s_e_temp,  REG_TEMP,     0, 0
    .dw s_e_fan,   REG_FAN,      0, 0
    .dw s_e_thr,   REG_THROT,    0, 0
    .dw s_e_tmax,  REG_TMAX,     0, 0

tab_security:
    .dw s_e_sec,   CM_SECURE,    3, opts_secure
    .dw s_e_sum,   REG_SUM,      0, 0
    .dw s_e_trust, REG_TRUST,    0, 0
    .dw s_e_secinfo, REG_INFO,   0, 0

tab_password:
    .dw s_e_pwstate, REG_PWSTATE, 0, 0
    .dw s_e_pwset,   REG_PWSET,   0, 0
    .dw s_e_pwclr,   REG_PWCLR,   0, 0
    .dw s_e_pwustate, REG_PWUSTATE, 0, 0
    .dw s_e_pwuset,  REG_PWUSET,  0, 0
    .dw s_e_pwuclr,  REG_PWUCLR,  0, 0
    .dw s_e_pwinfo,  REG_INFO,    0, 0
    .dw s_e_pwuinfo, REG_INFO,    0, 0

tab_company:
    .dw s_e_cotag,  REG_POL0,   0, 0
    .dw s_e_cotext, REG_COTEXT, 0, 0
    .dw s_e_cocc,   REG_POL1,   0, 0
    .dw s_e_conet,  REG_POL2,   0, 0
    .dw s_e_copw,   REG_POL3,   0, 0
    .dw s_e_codisk, REG_POL4,   0, 0
    .dw s_e_coblk,  REG_COBLK,  0, 0
    .dw s_e_coclr,  REG_COCLR,  0, 0
    .dw s_e_invser, REG_INVSER, 0, 0
    .dw s_e_invbt,  REG_INVBOOT,0, 0
    .dw s_e_invmin, REG_INVMIN, 0, 0
    .dw s_e_coinfo, REG_INFO,   0, 0

tab_events:
    .dw s_e_ev1, REG_EV0,   0, 0
    .dw s_e_ev2, REG_EV0+1, 0, 0
    .dw s_e_ev3, REG_EV0+2, 0, 0
    .dw s_e_ev4, REG_EV0+3, 0, 0
    .dw s_e_ev5, REG_EV0+4, 0, 0
    .dw s_e_ev6, REG_EV0+5, 0, 0
    .dw s_e_ev7, REG_EV0+6, 0, 0
    .dw s_e_ev8, REG_EV0+7, 0, 0
    .dw s_e_evclr, REG_EVCLR, 0, 0
    .dw s_e_evinfo, REG_INFO, 0, 0

tab_firmware:
    .dw s_e_blen,  REG_BIOSLEN,  0, 0
    .dw s_e_bsum,  REG_BIOSSUM,  0, 0
    .dw s_e_flash, REG_FLASH,    0, 0
    .dw s_e_frest, REG_RESTORE,  0, 0
    .dw s_e_finfo, REG_INFO,     0, 0

tab_exit:
    .dw s_e_xsave, REG_EXITSAVE, 0, 0
    .dw s_e_xdrop, REG_EXITDROP, 0, 0
    .dw s_e_def,   REG_DEFAULTS, 0, 0
    .dw s_e_xinfo, REG_INFO,     0, 0

; Feldtabellen: CMOS-Register, kleinster Wert, groesster Wert, Name
felder_zeit:
    .dw CM_HOUR,  0, 23, s_f_std
    .dw CM_MIN,   0, 59, s_f_min
    .dw CM_SEC,   0, 59, s_f_sek
felder_datum:
    .dw CM_DAY,   1, 31, s_f_tag
    .dw CM_MONTH, 1, 12, s_f_mon
    .dw CM_YEAR,  0, 99, s_f_jahr

opts_onoff:  .dw s_off, s_on
opts_secure: .dw s_off, s_audit, s_enforce
opts_fan:    .dw s_fan0, s_fan1, s_fan2
opts_boot:   .dw s_boot0, s_boot1, s_boot2
opts_speed:  .dw s_spd0, s_spd1, s_spd2, s_spd3, s_spd4
opts_verb:   .dw s_short, s_full
opts_boot2:  .dw s_bm0, s_bm1

s_set_title: .db "TOOBAD BIOS SETUP UTILITY", 0
s_set_main:  .db " Main ", 0
s_set_help:  .db " Item Help ", 0
s_set_keys1: .db "Up/Down    Select Item            Left/Right   Select Tab", 0
s_set_keys2: .db "ENTER +/-  Change Value           F5           Load Setup Defaults", 0
s_set_keys3: .db "F10        Save & Exit            ESC          Exit Without Saving", 0
s_set_saved: .db "Configuration saved to CMOS.", 0
s_set_cancel:.db "Changes discarded.", 0
s_set_press: .db "<Press ENTER>", 0
s_set_20:    .db "20", 0

s_e_time:    .db "System Time", 0
s_e_date:    .db "System Date", 0
s_e_boot:    .db "Boot Device Priority", 0
s_e_quick:   .db "Quick Boot (skip full memory test)", 0
s_e_beep:    .db "POST Beep", 0
s_e_speed:   .db "CPU Clock Speed", 0
s_e_verb:    .db "POST Messages", 0
s_e_boot2:   .db "Boot To", 0
s_bm0:       .db "Desktop", 0
s_bm1:       .db "Console", 0
s_e_def:     .db "Load Setup Defaults", 0
s_e_delay:   .db "Boot Delay", 0
s_e_xsave:   .db "Save Changes and Exit", 0
s_e_xdrop:   .db "Discard Changes and Exit", 0
s_e_xinfo:   .db "The same as F10 and ESC -- here so you can see them", 0
s_e_mem:     .db "Installed Memory", 0
s_e_disk:    .db "Primary Master", 0
s_e_vga:     .db "Display Adapter", 0
s_e_fanm:    .db "Fan Control", 0
s_e_lim:     .db "Throttle Threshold", 0
s_e_temp:    .db "CPU Temperature", 0
s_e_fan:     .db "Fan Speed", 0
s_e_thr:     .db "Throttling", 0
s_e_tmax:    .db "Highest Seen", 0
s_e_sec:     .db "Secure Boot", 0
s_e_sum:     .db "Boot Image Checksum", 0
s_e_trust:   .db "Trust Current Boot Image", 0
s_e_secinfo: .db "Halts at boot if BIOS or kernel were changed", 0
s_e_cotag:   .db "Owner Tag", 0
s_e_cotext:  .db "Owner Text", 0
s_e_cocc:    .db "Block Compiler", 0
s_e_conet:   .db "Block Network", 0
s_e_copw:    .db "Require Login Password", 0
s_e_codisk:  .db "Boot From Internal Disk Only", 0
s_e_coblk:   .db "Blocked Programs", 0
s_e_coclr:   .db "Configuration Cleared", 0
s_e_invser:  .db "Serial Number", 0
s_e_invbt:   .db "Power-On Count", 0
s_e_invmin:  .db "Operating Time", 0
s_e_coinfo:  .db "The system reads these at every start", 0
s_co_yes:    .db "Yes -- settings were reset", 0
s_co_no:     .db "No", 0
s_a6_fest:   .db "Boot source is locked by system policy (Company > Boot From Internal Disk Only).", 0
s_ev_geleert:.db "Event log cleared.", 0
s_minuten:   .db " min", 0
s_sekunden:  .db " s", 0
s_e_ev1:     .db "1", 0
s_e_ev2:     .db "2", 0
s_e_ev3:     .db "3", 0
s_e_ev4:     .db "4", 0
s_e_ev5:     .db "5", 0
s_e_ev6:     .db "6", 0
s_e_ev7:     .db "7", 0
s_e_ev8:     .db "8", 0
s_e_evclr:   .db "Clear Event Log", 0
s_e_evinfo:  .db "Newest first. Survives a restart, not a dead battery", 0
s_e_pwstate: .db "Supervisor Password", 0
s_e_pwset:   .db "Set / Change Password", 0
s_e_pwclr:   .db "Clear Password", 0
s_e_pwinfo:  .db "Guards this setup. Removing the CMOS battery clears it", 0
s_e_pwustate:.db "Power-On Password", 0
s_e_pwuset:  .db "Set / Change Power-On Password", 0
s_e_pwuclr:  .db "Clear Power-On Password", 0
s_e_pwuinfo: .db "Power-On is asked before booting. Three tries, counted in CMOS", 0
s_e_blen:    .db "BIOS Image Size", 0
s_e_bsum:    .db "BIOS Image Checksum", 0
s_e_flash:   .db "Flash BIOS from File", 0
s_e_frest:   .db "Restore Backup BIOS", 0
s_e_finfo:   .db "A bad image is refused; the board keeps a backup copy", 0
s_fl_ask:    .db "Flash this image? It runs after the next restart.  ENTER = yes, ESC = no", 0
s_fl_ok:     .db "Flash complete. Power cycle the machine to run the new BIOS.", 0
s_fl_none:   .db "No file selected.", 0
s_fl_bad:    .db "Not a TOOBAD BIOS image (missing TBBI header or wrong size).", 0
s_fl_sum:    .db "Checksum does not match -- the file is damaged. Nothing was written.", 0
s_fl_err:    .db "Could not write the BIOS chip.", 0
s_fl_nobak:  .db "There is no backup copy yet.", 0
s_fl_back:   .db "Backup BIOS restored. Power cycle the machine.", 0

s_edit_hilfe: .db "Up/Down  change      Left/Right  next field      ENTER  done", 0
s_f_std:     .db "Editing: hours", 0
s_f_min:     .db "Editing: minutes", 0
s_f_sek:     .db "Editing: seconds", 0
s_f_tag:     .db "Editing: day", 0
s_f_mon:     .db "Editing: month", 0
s_f_jahr:    .db "Editing: year", 0

s_tab_main:  .db " Main ", 0
s_tab_hw:    .db " Hardware ", 0
s_tab_cool:  .db " Cooling ", 0
s_tab_sec:   .db " Security ", 0
s_tab_pw:    .db " Password ", 0
s_tab_comp:  .db " Company ", 0
s_tab_ev:    .db " Event Log ", 0
s_tab_exit:  .db " Exit ", 0
s_tab_fw:    .db " Firmware ", 0

s_fan0:      .db "Automatic", 0
s_fan1:      .db "Quiet", 0
s_fan2:      .db "Full Speed", 0
s_stored:    .db "stored", 0
s_setkb:     .db " KB", 0
s_setbyte:   .db " bytes", 0
s_setsec:    .db " sectors", 0
s_grad:      .db " C", 0
s_proz:      .db " %", 0
s_vgatyp:    .db "TB-VGA 640x400, 256 colours", 0

s_on:        .db "Enabled", 0
s_off:       .db "Disabled", 0
s_audit:     .db "Audit (warn only)", 0
s_enforce:   .db "Enforce (halt)", 0
s_boot0:     .db "Hard Disk 0", 0
s_boot1:     .db "Floppy (not installed)", 0
s_boot2:     .db "Network (not installed)", 0
s_short:     .db "Minimal", 0
s_full:      .db "Verbose", 0
