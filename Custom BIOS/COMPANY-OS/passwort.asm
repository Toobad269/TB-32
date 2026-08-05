; ===========================================================================
;  TB-LOCK  --  Supervisor-Passwort fuer das BIOS-Setup
;
;  Alles, was das Passwort angeht, steht in dieser einen Datei. Der Rest der
;  Firmware ruft genau drei Dinge auf:
;
;      pw_tor          vor dem Setup: darf der Mensch hier ueberhaupt rein?
;      pw_setzen       Knopf "Set / Change Password" im Reiter Password
;      pw_loeschen     Knopf "Clear Password" ebendort
;
;  Wo das Passwort liegt: CM_PWFLAG sagt, ob ueberhaupt eines gesetzt ist,
;  CM_PWSUM0..3 halten die Pruefsumme. Die Plaetze liegen bei 0x20..0x24 --
;  ABSICHTLICH oberhalb von 0x1F, denn setup_backup/setup_restore sichert nur
;  0x10..0x1F. Laege das Passwort dort, wuerde ESC ("Exit Without Saving") ein
;  frisch gesetztes Passwort stillschweigend wieder abraeumen. Es liegt aber
;  noch unterhalb von 0x2E, also innerhalb des Bereichs, ueber den die
;  Knopfzelle ihre eigene Pruefsumme rechnet.
;
;  Die Pruefsumme ist dieselbe Rechnung, die TOOBAD-OS fuer die Anmeldung
;  benutzt (pw_summe in kernel.c) und die build.py fuer den BIOS-Kopf nimmt:
;  h = h * 31 + Zeichen, angefangen bei 0x1234. Das ist eine Pruefsumme und
;  keine kryptografische Hash-Funktion -- wer die vier Byte aus cmos.bin
;  liest, findet in Sekunden ein anderes Passwort mit derselben Summe. Das
;  steht so auch in der README, und es ist dieselbe Ehrlichkeit, mit der
;  Doku 13 ueber Secure Boot spricht.
; ===========================================================================

.equ PW_MAX,       20                ; Zeichen, ohne die abschliessende Null
.equ PW_VERSUCHE,  3                 ; Fehlversuche am Tor, dann ist Schluss

; Das Fenster, in dem gefragt wird
.equ PWD_X,        14
.equ PWD_Y,        9
.equ PWD_W,        52
.equ PWD_H,        7

; Auf eine 4-Byte-Grenze, BEVOR hier der erste Befehl steht.
;
; Das ist keine Kosmetik: passwort.asm wird nach setup.asm eingebunden, und
; setup.asm hoert mit seiner Zeichenkettentabelle auf -- die endet auf einer
; krummen Adresse. Befehle sind auf dem TB-32 aber fest vier Byte breit und
; werden ab einer durch vier teilbaren Adresse geholt. Ohne diese Zeile faengt
; der Code hier um zwei Byte versetzt an: der Rechner startet noch, der POST
; laeuft, und beim ersten DEL zerlegt es ihn mit "Invalid opcode". Genau so
; ist es beim ersten Bau auch passiert.
.align 4

; ---------------------------------------------------------------------------
;  pw_hash(r1 = Zeiger auf 0-terminierten Text) -> r0 = Pruefsumme
; ---------------------------------------------------------------------------
pw_hash:
    push r6
    push r7
    li r0, 0x1234
    mov r6, r1
.loop:
    ldb r7, [r6]
    cmpi r7, 0
    jz .done
    muli r0, r0, 31
    add r0, r0, r7
    addi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  pw_gleich(r1, r2 = zwei Zeiger) -> r0 = 1 wenn Zeichen fuer Zeichen gleich
;
;  Verglichen wird der Text, nicht die Summe: zwei verschiedene Eingaben mit
;  zufaellig gleicher Summe sollen beim Wiederholen NICHT durchgehen.
; ---------------------------------------------------------------------------
pw_gleich:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1
    mov r7, r2
.loop:
    ldb r8, [r6]
    ldb r9, [r7]
    cmp r8, r9
    jnz .nein
    cmpi r8, 0
    jz .ja
    addi r6, r6, 1
    addi r7, r7, 1
    jmp .loop
.ja:
    movi r0, 1
    jmp .done
.nein:
    movi r0, 0
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Die gemerkte Pruefsumme aus der Knopfzelle holen / hineinschreiben
; ---------------------------------------------------------------------------
pw_sum_lesen:
    push r6
    movi r10, CM_PWSUM3
    call cmos_read
    mov r6, r0
    shli r6, r6, 8
    movi r10, CM_PWSUM2
    call cmos_read
    or r6, r6, r0
    shli r6, r6, 8
    movi r10, CM_PWSUM1
    call cmos_read
    or r6, r6, r0
    shli r6, r6, 8
    movi r10, CM_PWSUM0
    call cmos_read
    or r6, r6, r0
    mov r0, r6
    pop r6
    ret

pw_sum_schreiben:                     ; r1 = Summe
    push r6
    mov r6, r1
    movi r10, CM_PWSUM0
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWSUM1
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWSUM2
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWSUM3
    andi r11, r6, 0xFF
    call cmos_write
    pop r6
    ret

; ---------------------------------------------------------------------------
;  pw_gesetzt -> r0 = 1, wenn ueberhaupt ein Passwort eingerichtet ist
; ---------------------------------------------------------------------------
pw_gesetzt:
    movi r10, CM_PWFLAG
    call cmos_read
    cmpi r0, 1
    jz .ja
    movi r0, 0
    ret
.ja:
    movi r0, 1
    ret

; ---------------------------------------------------------------------------
;  Die Knopfzelle sofort auf die Platte schreiben
;
;  Ein Passwort ist keine Einstellung, die man mit F10 noch bestaetigt -- wer
;  es gerade zweimal eingetippt hat, erwartet, dass es gilt. Also wird hier
;  dasselbe getan, was F10 tut: auf CM_SAVE schreiben.
; ---------------------------------------------------------------------------
pw_sichern:
    movi r10, CM_SAVE
    movi r11, 1
    call cmos_write
    ret

; ---------------------------------------------------------------------------
;  pw_fenster(r1 = Ueberschrift): das Fragefenster aufziehen
; ---------------------------------------------------------------------------
pw_fenster:
    push r6
    mov r6, r1
    movi r1, PWD_X
    movi r2, PWD_Y
    movi r3, PWD_W
    movi r4, PWD_H
    movi r5, A_SEL
    call vid_fillrect
    movi r1, PWD_X
    movi r2, PWD_Y
    movi r3, PWD_W
    movi r4, PWD_H
    movi r5, A_SEL
    call vid_box
    movi r1, PWD_X+3
    movi r2, PWD_Y
    mov r3, r6
    movi r4, A_SEL
    call vid_putsat
    movi r1, PWD_X+3
    movi r2, PWD_Y+PWD_H-2
    li r3, s_pw_keys
    movi r4, A_SEL
    call vid_putsat
    pop r6
    ret

; ---------------------------------------------------------------------------
;  te_zeichnen(r1 = Anzahl Zeichen): den Inhalt des Eingabefelds malen
;
;  Sterne oder Klartext, je nach TE_SICHTBAR. Der Puffer steht in TE_BUF und
;  nicht in einem Register: vid_putat darf sonst nichts ueberschreiben, und
;  mit zwei Registern kommt die Schleife sicher aus.
; ---------------------------------------------------------------------------
te_zeichnen:
    push r6
    push r7
    mov r7, r1
    movi r6, 0
.loop:
    cmp r6, r7
    jge .done
    ldwa r10, TE_SICHTBAR
    cmpi r10, 0
    jz .stern
    ldwa r10, TE_BUF
    add r10, r10, r6
    ldb r3, [r10]
    jmp .malen
.stern:
    movi r3, 0x2A                     ; '*'
.malen:
    addi r1, r6, PWD_X+3
    movi r2, PWD_Y+3
    movi r4, A_SEL
    call vid_putat
    addi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  text_eingabe(r1 = Puffer, r2 = Frage) -> r0 = Laenge, oder -1 bei ESC
;
;  Die Tippschleife gibt es nur EINMAL -- fuer Passwoerter (Sterne) und fuer
;  den Firmentext (sichtbar). Vorher TE_MAX und TE_SICHTBAR setzen; dafuer
;  gibt es die beiden Einstiege pw_eingabe und txt_eingabe darunter.
;  Das Fragefenster muss schon stehen (pw_fenster).
; ---------------------------------------------------------------------------
text_eingabe:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1                        ; r6 = Puffer
    stwa TE_BUF, r6
    mov r8, r2                        ; r8 = Frage, nur bis zum Zeichnen
    movi r7, 0                        ; r7 = Anzahl Zeichen

    movi r1, PWD_X+3
    movi r2, PWD_Y+2
    mov r3, r8
    movi r4, A_SEL
    call vid_putsat

.loop:
    movi r1, PWD_X+3                  ; Feldboden, dann der Inhalt darauf
    movi r2, PWD_Y+3
    ldwa r3, TE_MAX
    addi r3, r3, 2
    movi r4, 0x5F                     ; '_'
    movi r5, A_SEL
    call vid_hline
    mov r1, r7
    call te_zeichnen

    call kbd_getkey
    shri r8, r0, 8                    ; Scancode
    andi r9, r0, 0xFF                 ; ASCII

    cmpi r8, K_ENTER
    jz .fertig
    cmpi r8, K_ESC
    jz .abbruch
    cmpi r8, K_BACKSPACE
    jz .zurueck

    cmpi r9, 32                       ; nur druckbare Zeichen
    jl .loop
    cmpi r9, 127
    jge .loop
    ldwa r10, TE_MAX
    cmp r7, r10
    jge .loop
    add r10, r6, r7
    stb [r10], r9
    addi r7, r7, 1
    jmp .loop

.zurueck:
    cmpi r7, 0
    jz .loop
    subi r7, r7, 1
    jmp .loop

.fertig:
    add r10, r6, r7
    movi r11, 0
    stb [r10], r11
    mov r0, r7
    jmp .done
.abbruch:
    movi r11, 0
    stb [r6], r11
    li r0, 0xFFFFFFFF
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Die beiden Einstiege -------------------------------------------------
;  Beide springen weiter statt aufzurufen: text_eingabe endet mit ret, und
;  der bringt uns gleich zum urspruenglichen Aufrufer zurueck. r1 und r2
;  stehen dabei schon richtig.
pw_eingabe:                           ; r1 = Puffer, r2 = Frage -- mit Sternen
    movi r10, PW_MAX
    stwa TE_MAX, r10
    movi r10, 0
    stwa TE_SICHTBAR, r10
    jmp text_eingabe

txt_eingabe:                          ; r1 = Puffer, r2 = Frage, r3 = max
    stwa TE_MAX, r3
    movi r10, 1
    stwa TE_SICHTBAR, r10
    jmp text_eingabe

; ---------------------------------------------------------------------------
;  pw_fragen(r1 = Puffer, r2 = Frage, r3 = Ueberschrift) -> r0 wie pw_eingabe
; ---------------------------------------------------------------------------
pw_fragen:
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    mov r1, r3
    call pw_fenster
    mov r1, r6
    mov r2, r7
    call pw_eingabe
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  pw_pruefen(r1 = Ueberschrift) -> r0 = 1, wenn das Eingetippte passt
; ---------------------------------------------------------------------------
pw_pruefen:
    push r6
    push r7
    mov r7, r1                        ; Ueberschrift retten
    li r1, PW_BUF1
    li r2, s_pw_current
    mov r3, r7
    call pw_fragen
    cmpi r0, 0
    jl .abbruch                       ; mit ESC abgebrochen: -1, kein Fehlversuch
    li r1, PW_BUF1
    call pw_hash
    mov r6, r0
    call pw_sum_lesen
    cmp r6, r0
    jnz .nein
    call pw_puffer_loeschen
    movi r0, 1
    jmp .done
.abbruch:
    call pw_puffer_loeschen
    li r0, 0xFFFFFFFF
    jmp .done
.nein:
    call pw_puffer_loeschen
    movi r0, 0
.done:
    pop r7
    pop r6
    ret

; ===========================================================================
;  Das Tor: wird vor jedem Einstieg ins Setup gerufen
;
;  r0 = 1 heisst "darf rein". Ohne eingerichtetes Passwort ist das immer so --
;  ein BIOS, das ab Werk niemanden hereinlaesst, waere ein totes Board.
; ===========================================================================
pw_tor:
    push r6
    call pw_gesetzt
    cmpi r0, 0
    jz .frei

    ; A2 gilt auch hier: der Zaehler liegt in der Knopfzelle, nicht in einem
    ; Register. Sonst waere dieses Tor die weiche Stelle -- dreimal raten,
    ; Reset, dreimal raten, und so fort. Und jeder Fehlversuch gehoert ins
    ; Protokoll, egal an welchem der beiden Tore er passiert.
    call pw_fehler_lesen
    cmpi r0, PW_MAXTRIES
    jge .abgelehnt

.versuch:
    movi r1, ATTR_NORMAL
    call vid_clear
    li r1, s_pw_locked
    call pw_pruefen
    cmpi r0, 1
    jz .richtig
    cmpi r0, 0
    jl .weg                           ; mit ESC abgebrochen: kein Fehlversuch

    movi r1, EV_BADPW
    call ev_log
    call pw_fehler_plus
    cmpi r0, PW_MAXTRIES
    jge .abgelehnt
    li r1, s_pw_wrong
    call pw_melden
    jmp .versuch

.richtig:
    call pw_fehler_null
    jmp .frei

.weg:
    movi r1, ATTR_NORMAL
    call vid_clear
    movi r0, 0
    pop r6
    ret

.abgelehnt:
    movi r1, ATTR_NORMAL
    call vid_clear
    li r1, s_pw_denied
    call pw_melden
    movi r1, 150
    call delay
    movi r1, ATTR_NORMAL
    call vid_clear
    movi r0, 0
    pop r6
    ret
.frei:
    movi r1, ATTR_NORMAL
    call vid_clear
    movi r0, 1
    pop r6
    ret

; ---------------------------------------------------------------------------
;  setup_tor  --  steht in bios.asm an beiden Stellen, wo frueher direkt
;  "call setup_main" stand. Beide muessen es sein: der zweite Weg ins Setup
;  fuehrt ueber den roten Secure-Boot-Bildschirm, und genau dort liegt der
;  Knopf "Trust Current Boot Image". Waere nur der normale Weg bewacht,
;  koennte man das Passwort umgehen, indem man das Startabbild kaputtmacht.
; ---------------------------------------------------------------------------
setup_tor:
    call pw_tor
    cmpi r0, 0
    jz .zu
    call setup_main
.zu:
    ret

; --- Eine Zeile unten, wie setup_message, aber auch ausserhalb des Setups --
pw_melden:
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
    movi r1, 90
    call delay
    pop r6
    ret

; ===========================================================================
;  Der Knopf "Set / Change Password"
;
;  Steht schon eines, muss zuerst das alte kommen. Danach zweimal das neue --
;  wer sich vertippt, sperrt sich sonst mit einem Passwort aus, das er nie
;  gewollt hat.
; ===========================================================================
pw_setzen:
    push r6

    call pw_gesetzt
    cmpi r0, 0
    jz .neu                           ; noch keins: direkt zum Einrichten

    li r1, s_pw_change                ; erst das alte
    call pw_pruefen
    cmpi r0, 1
    jz .neu
    li r1, s_pw_wrong
    jmp .melden

.neu:
    li r1, PW_BUF1
    li r2, s_pw_new
    li r3, s_pw_change
    call pw_fragen
    cmpi r0, 0
    jl .abbruch
    cmpi r0, 0
    jz .leer                          ; leere Eingabe waere ein offenes Board

    li r1, PW_BUF2
    li r2, s_pw_again
    li r3, s_pw_change
    call pw_fragen
    cmpi r0, 0
    jl .abbruch

    li r1, PW_BUF1
    li r2, PW_BUF2
    call pw_gleich
    cmpi r0, 1
    jnz .ungleich

    li r1, PW_BUF1
    call pw_hash
    mov r1, r0
    call pw_sum_schreiben
    movi r10, CM_PWFLAG
    movi r11, 1
    call cmos_write
    call pw_sichern
    call pw_puffer_loeschen
    li r1, s_pw_set
    jmp .melden

.ungleich:
    call pw_puffer_loeschen
    li r1, s_pw_nomatch
    jmp .melden
.leer:
    call pw_puffer_loeschen
    li r1, s_pw_empty
    jmp .melden
.abbruch:
    call pw_puffer_loeschen
    li r1, s_pw_abort
.melden:
    push r1
    call setup_frame                  ; das Fragefenster wieder wegraeumen
    pop r1
    call setup_message
    pop r6
    ret

; ===========================================================================
;  Der Knopf "Clear Password"  --  geht nur mit dem alten Passwort
; ===========================================================================
pw_loeschen:
    push r6
    call pw_gesetzt
    cmpi r0, 0
    jz .keins

    li r1, s_pw_clear
    call pw_pruefen
    cmpi r0, 1
    jnz .falsch

    movi r10, CM_PWFLAG
    movi r11, 0
    call cmos_write
    movi r1, 0
    call pw_sum_schreiben
    call pw_sichern
    li r1, s_pw_cleared
    jmp .melden
.falsch:
    li r1, s_pw_wrong
    jmp .melden
.keins:
    li r1, s_pw_none
.melden:
    push r1
    call setup_frame
    pop r1
    call setup_message
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Die Eingabepuffer ueberschreiben
;
;  Das Passwort im Klartext bliebe sonst im RAM stehen, und gleich darauf
;  wird der Kernel geladen -- auf einem Rechner ohne Speicherschutz koennte
;  es jedes Programm auslesen. Viel gewonnen ist damit nicht (die Pruefsumme
;  steht ja in der Knopfzelle), aber es kostet auch nichts.
; ---------------------------------------------------------------------------
pw_puffer_loeschen:
    push r6
    push r7
    li r6, PW_BUF1
    movi r7, 0
.loop:
    cmpi r7, 64                       ; beide Puffer am Stueck
    jae .done
    add r10, r6, r7
    movi r11, 0
    stb [r10], r11
    addi r7, r7, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

; --- Texte ----------------------------------------------------------------
s_pw_locked:  .db " BIOS Setup is locked ", 0
s_pw_change:  .db " Supervisor Password ", 0
s_pw_clear:   .db " Clear Supervisor Password ", 0
s_pw_current: .db "Enter Current Password:", 0
s_pw_new:     .db "Enter New Password:", 0
s_pw_again:   .db "Confirm New Password:", 0
s_pw_keys:    .db "ENTER  confirm      ESC  cancel", 0
s_pw_wrong:   .db "Wrong password.", 0
s_pw_denied:  .db "Access denied. Setup remains locked.", 0
s_pw_set:     .db "Password installed. Setup asks for it from now on.", 0
s_pw_cleared: .db "Password cleared. Setup is open again.", 0
s_pw_nomatch: .db "The two entries do not match. Nothing was changed.", 0
s_pw_empty:   .db "An empty password would leave the board open. Nothing changed.", 0
s_pw_abort:   .db "Cancelled. Nothing was changed.", 0
s_pw_none:    .db "No password is set.", 0
s_pw_inst:    .db "Installed", 0
s_pw_notinst: .db "Not Installed", 0

; ===========================================================================
;  A1 / A2  --  das Power-On-Passwort und der Zaehler in der Knopfzelle
;
;  Das Supervisor-Passwort oben bewacht nur das Setup. Wer den Rechner
;  einfach einschaltet und benutzt, kommt daran vorbei -- fuer einen
;  Firmenrechner ist das zu wenig. Das Power-On-Passwort wird VOR dem
;  Bootversuch verlangt; ohne es startet gar nichts.
;
;  Zwei Dinge daran sind wichtiger, als sie aussehen:
;
;  Erstens kommt das Supervisor-Passwort hier ebenfalls durch. Sonst sperrt
;  sich der Administrator selbst aus, sobald er das Benutzerpasswort
;  vergisst -- und der einzige Weg zurueck waere die Knopfzelle.
;
;  Zweitens zaehlt der Fehlversuchszaehler in der KNOPFZELLE und nicht in
;  einem Register. Vorher fing ein Druck auf Reset wieder bei drei an: man
;  konnte in Ruhe raten, beliebig oft. Jetzt ueberlebt der Zaehler den
;  Neustart, und erst die richtige Eingabe setzt ihn zurueck.
; ===========================================================================

.align 4

pwu_gesetzt:                          ; -> r0 = 1, wenn eines eingerichtet ist
    movi r10, CM_PWUFLAG
    call cmos_read
    cmpi r0, 1
    jz .ja
    movi r0, 0
    ret
.ja:
    movi r0, 1
    ret

pwu_sum_lesen:                        ; -> r0
    push r6
    movi r10, CM_PWUSUM3
    call cmos_read
    mov r6, r0
    shli r6, r6, 8
    movi r10, CM_PWUSUM2
    call cmos_read
    or r6, r6, r0
    shli r6, r6, 8
    movi r10, CM_PWUSUM1
    call cmos_read
    or r6, r6, r0
    shli r6, r6, 8
    movi r10, CM_PWUSUM0
    call cmos_read
    or r6, r6, r0
    mov r0, r6
    pop r6
    ret

pwu_sum_schreiben:                    ; r1 = Summe
    push r6
    mov r6, r1
    movi r10, CM_PWUSUM0
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWUSUM1
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWUSUM2
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_PWUSUM3
    andi r11, r6, 0xFF
    call cmos_write
    pop r6
    ret

; --- Der Fehlversuchszaehler ----------------------------------------------
pw_fehler_lesen:                      ; -> r0
    movi r10, CM_PWTRIES
    call cmos_read
    ret

pw_fehler_plus:                       ; -> r0 = neuer Stand
    push r6
    movi r10, CM_PWTRIES
    call cmos_read
    addi r6, r0, 1
    movi r10, CM_PWTRIES
    mov r11, r6
    call cmos_write
    call pw_sichern                   ; sofort in die Knopfzelle, nicht erst bei F10
    mov r0, r6
    pop r6
    ret

pw_fehler_null:
    movi r10, CM_PWTRIES
    movi r11, 0
    call cmos_write
    call pw_sichern
    ret

; ---------------------------------------------------------------------------
;  pw_passt(r1 = Puffer) -> r0 = 1, wenn es eines der beiden Passwoerter ist
; ---------------------------------------------------------------------------
pw_passt:
    push r6
    push r7
    call pw_hash
    mov r6, r0

    call pwu_gesetzt                  ; erst das Power-On-Passwort
    cmpi r0, 0
    jz .kein_user
    call pwu_sum_lesen
    cmp r6, r0
    jz .ja
.kein_user:
    call pw_gesetzt                   ; dann das des Administrators
    cmpi r0, 0
    jz .nein
    call pw_sum_lesen
    cmp r6, r0
    jz .ja
.nein:
    movi r0, 0
    pop r7
    pop r6
    ret
.ja:
    movi r0, 1
    pop r7
    pop r6
    ret

; ===========================================================================
;  Das Tor vor dem Bootvorgang
;
;  Es gibt hier kein ESC. Wer den Rechner einschaltet, kommt entweder mit dem
;  Passwort weiter oder gar nicht -- genau das unterscheidet es vom Tor vor
;  dem Setup.
; ===========================================================================
pwu_tor:
    push r6
    call pwu_gesetzt
    cmpi r0, 0
    jz .frei

    call pw_fehler_lesen              ; schon verbraucht? Dann gar nicht fragen
    cmpi r0, PW_MAXTRIES
    jge .gesperrt

.versuch:
    movi r1, ATTR_NORMAL
    call vid_clear
    li r1, s_pwu_locked
    call pw_fenster
    li r1, PW_BUF1
    li r2, s_pw_current
    call pw_eingabe
    cmpi r0, 0
    jl .versuch                       ; ESC hilft hier nicht weiter

    li r1, PW_BUF1
    call pw_passt
    mov r6, r0
    call pw_puffer_loeschen
    cmpi r6, 1
    jz .richtig

    movi r1, EV_BADPW
    call ev_log
    call pw_fehler_plus
    cmpi r0, PW_MAXTRIES
    jge .gesperrt
    li r1, s_pw_wrong
    call pw_melden
    jmp .versuch

.richtig:
    call pw_fehler_null               ; erst die richtige Eingabe raeumt auf
    movi r1, ATTR_NORMAL
    call vid_clear
    pop r6
    ret
.frei:
    pop r6
    ret

.gesperrt:
    movi r1, EV_LOCKOUT
    call ev_log
    li r1, s_pwu_halt
    call panic                        ; kommt nicht zurueck

; ===========================================================================
;  Die Knoepfe im Reiter Password
; ===========================================================================
pwu_setzen:
    push r6
    call pwu_gesetzt
    cmpi r0, 0
    jz .neu
    li r1, s_pwu_change               ; erst das alte -- oder das des Chefs
    call pwu_alt_pruefen
    cmpi r0, 1
    jz .neu
    li r1, s_pw_wrong
    jmp .melden
.neu:
    li r1, PW_BUF1
    li r2, s_pw_new
    li r3, s_pwu_change
    call pw_fragen
    cmpi r0, 0
    jl .abbruch
    cmpi r0, 0
    jz .leer
    li r1, PW_BUF2
    li r2, s_pw_again
    li r3, s_pwu_change
    call pw_fragen
    cmpi r0, 0
    jl .abbruch
    li r1, PW_BUF1
    li r2, PW_BUF2
    call pw_gleich
    cmpi r0, 1
    jnz .ungleich
    li r1, PW_BUF1
    call pw_hash
    mov r1, r0
    call pwu_sum_schreiben
    movi r10, CM_PWUFLAG
    movi r11, 1
    call cmos_write
    call pw_fehler_null
    call pw_sichern
    call pw_puffer_loeschen
    li r1, s_pwu_set
    jmp .melden
.ungleich:
    call pw_puffer_loeschen
    li r1, s_pw_nomatch
    jmp .melden
.leer:
    call pw_puffer_loeschen
    li r1, s_pw_empty
    jmp .melden
.abbruch:
    call pw_puffer_loeschen
    li r1, s_pw_abort
.melden:
    push r1
    call setup_frame
    pop r1
    call setup_message
    pop r6
    ret

; --- Wie pw_pruefen, aber es gelten BEIDE Passwoerter ---------------------
pwu_alt_pruefen:                      ; r1 = Ueberschrift -> r0 = 0/1
    push r6
    push r7
    mov r7, r1
    li r1, PW_BUF1
    li r2, s_pw_current
    mov r3, r7
    call pw_fragen
    cmpi r0, 0
    jl .nein
    li r1, PW_BUF1
    call pw_passt
    mov r6, r0
    call pw_puffer_loeschen
    mov r0, r6
    pop r7
    pop r6
    ret
.nein:
    call pw_puffer_loeschen
    movi r0, 0
    pop r7
    pop r6
    ret

pwu_loeschen:
    push r6
    call pwu_gesetzt
    cmpi r0, 0
    jz .keins
    li r1, s_pwu_clear
    call pwu_alt_pruefen
    cmpi r0, 1
    jnz .falsch
    movi r10, CM_PWUFLAG
    movi r11, 0
    call cmos_write
    movi r1, 0
    call pwu_sum_schreiben
    call pw_fehler_null
    call pw_sichern
    li r1, s_pwu_cleared
    jmp .melden
.falsch:
    li r1, s_pw_wrong
    jmp .melden
.keins:
    li r1, s_pw_none
.melden:
    push r1
    call setup_frame
    pop r1
    call setup_message
    pop r6
    ret

s_pwu_locked:  .db " This computer is locked ", 0
s_pwu_change:  .db " Power-On Password ", 0
s_pwu_clear:   .db " Clear Power-On Password ", 0
s_pwu_set:     .db "Power-On password installed. It is asked at every start.", 0
s_pwu_cleared: .db "Power-On password cleared.", 0
s_pwu_halt:    .db "Too many failed attempts", 0
.align 4
