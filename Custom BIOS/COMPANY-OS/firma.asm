; ===========================================================================
;  COMPANY-OS  --  Firmenrichtlinien: NVRAM, Eigentuemer, Sperrliste
;
;  Was hier drin steckt:
;
;      nv_*        der zweite batteriegepufferte Speicher (256 Byte)
;      firma_*     Eigentuemer-Eintrag: lesen, tippen, veroeffentlichen
;      pol_*       das Schalterwort (Compiler, Netz, Anmeldung, Startquelle)
;      blk_*       die Sperrliste: welche Programme das System nicht startet
;      inv_*       Inventar: Seriennummer, Starts, Betriebsminuten
;      ev_log      der Ereignisspeicher
;
;  Der Grundsatz dahinter: Das BIOS kennt keine Dateien und kein
;  Dateisystem. Es kennt eine FESTE Liste von Programmen -- so wie ein echtes
;  BIOS eine feste Liste von Anschluessen kennt ("USB Ports: Enabled"). Ein
;  Bit je Programm, und weil das BIOS die Namen kennt, legt es sie beim Start
;  im Klartext in den Speicher. Das System muss dann keine Bits deuten und
;  keine eigene Tabelle mitfuehren; kommt ein Programm dazu, aendert sich nur
;  die Firmware.
; ===========================================================================

.align 4

; ---------------------------------------------------------------------------
;  Der NVRAM-Baustein
; ---------------------------------------------------------------------------
nv_read:                              ; r1 = Adresse -> r0
    out P_NVRAM_IDX, r1
    in r0, P_NVRAM_DATA
    ret

nv_write:                             ; r1 = Adresse, r2 = Wert
    out P_NVRAM_IDX, r1
    out P_NVRAM_DATA, r2
    ret

nv_read32:                            ; r1 = Adresse -> r0
    push r6
    push r7
    mov r6, r1
    addi r1, r6, 3
    call nv_read
    mov r7, r0
    shli r7, r7, 8
    addi r1, r6, 2
    call nv_read
    or r7, r7, r0
    shli r7, r7, 8
    addi r1, r6, 1
    call nv_read
    or r7, r7, r0
    shli r7, r7, 8
    mov r1, r6
    call nv_read
    or r7, r7, r0
    mov r0, r7
    pop r7
    pop r6
    ret

nv_write32:                           ; r1 = Adresse, r2 = Wert
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    mov r1, r6
    andi r2, r7, 0xFF
    call nv_write
    shri r7, r7, 8
    addi r1, r6, 1
    andi r2, r7, 0xFF
    call nv_write
    shri r7, r7, 8
    addi r1, r6, 2
    andi r2, r7, 0xFF
    call nv_write
    shri r7, r7, 8
    addi r1, r6, 3
    andi r2, r7, 0xFF
    call nv_write
    pop r7
    pop r6
    ret

; --- Zeichenkette aus dem NVRAM holen / hineinlegen ------------------------
nv_str_lesen:                         ; r1 = NVRAM-Adresse, r2 = Ziel, r3 = max
    push r6
    push r7
    push r8
    push r9
    mov r6, r1
    mov r7, r2
    mov r8, r3
    movi r9, 0
.loop:
    cmp r9, r8
    jge .ende
    add r1, r6, r9
    call nv_read
    add r10, r7, r9
    stb [r10], r0
    cmpi r0, 0
    jz .fertig
    addi r9, r9, 1
    jmp .loop
.ende:
    add r10, r7, r9
    movi r11, 0
    stb [r10], r11
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

nv_str_schreiben:                     ; r1 = NVRAM-Adresse, r2 = Quelle, r3 = max
    push r6
    push r7
    push r8
    push r9
    mov r6, r1
    mov r7, r2
    mov r8, r3
    movi r9, 0
.loop:
    cmp r9, r8
    jge .fertig
    add r10, r7, r9
    ldb r2, [r10]
    add r1, r6, r9
    call nv_write
    cmpi r2, 0
    jz .fertig
    addi r9, r9, 1
    jmp .loop
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  nv_init -- beim Start: ist das NVRAM ueberhaupt schon eingerichtet?
;
;  Beim allerersten Mal bekommt es den Firmentext aus dem Abbild, eine
;  Seriennummer aus Datum und Uhrzeit (zwei Rechner haben so nicht dieselbe)
;  und Zaehler auf null.
; ---------------------------------------------------------------------------
nv_init:
    push r6
    movi r6, 0
    stwa NV_WAR_DA, r6                ; erst mal: der Rechner ist neu
    movi r1, NV_MAGIC
    call nv_read
    cmpi r0, NV_NAME_MAGIC
    jz .schon_da

    li r1, NV_FIRMA                   ; Werkstext hinein
    li r2, s_firma
    movi r3, 31
    call nv_str_schreiben

    call nv_serial_bauen

    movi r1, NV_BOOTS                 ; Zaehler auf null
    movi r2, 0
    call nv_write32
    movi r1, NV_MINUTES
    movi r2, 0
    call nv_write32
    movi r1, NV_LOGHEAD
    movi r2, 0
    call nv_write

    movi r1, NV_MAGIC
    movi r2, NV_NAME_MAGIC
    call nv_write
    pop r6
    ret
.schon_da:
    movi r6, 1                        ; der Rechner war schon eingerichtet
    stwa NV_WAR_DA, r6
    pop r6
    ret

; --- "TB32-JJMMTThhmm", aus der Uhr gebaut --------------------------------
nv_serial_bauen:
    push r6
    li r1, NV_SERIAL                  ; die fuenf festen Zeichen
    movi r2, 0x54                     ; 'T'
    call nv_write
    li r1, NV_SERIAL+1
    movi r2, 0x42                     ; 'B'
    call nv_write
    li r1, NV_SERIAL+2
    movi r2, 0x33                     ; '3'
    call nv_write
    li r1, NV_SERIAL+3
    movi r2, 0x32                     ; '2'
    call nv_write
    li r1, NV_SERIAL+4
    movi r2, 0x2D                     ; '-'
    call nv_write

    movi r10, CM_YEAR
    call cmos_read
    mov r6, r0
    li r1, NV_SERIAL+5
    mov r2, r6
    call nv_zweistellig
    movi r10, CM_MONTH
    call cmos_read
    li r1, NV_SERIAL+7
    mov r2, r0
    call nv_zweistellig
    movi r10, CM_DAY
    call cmos_read
    li r1, NV_SERIAL+9
    mov r2, r0
    call nv_zweistellig
    movi r10, CM_HOUR
    call cmos_read
    li r1, NV_SERIAL+11
    mov r2, r0
    call nv_zweistellig
    movi r10, CM_MIN
    call cmos_read
    li r1, NV_SERIAL+13
    mov r2, r0
    call nv_zweistellig
    li r1, NV_SERIAL+15
    movi r2, 0
    call nv_write
    pop r6
    ret

nv_zweistellig:                       ; r1 = NVRAM-Adresse, r2 = Wert 0..99
    push r6
    push r7
    mov r6, r1
    mov r7, r2
    divi r2, r7, 10
    modi r2, r2, 10
    addi r2, r2, 0x30
    mov r1, r6
    call nv_write
    modi r2, r7, 10
    addi r2, r2, 0x30
    addi r1, r6, 1
    call nv_write
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Der Ereignisspeicher -- ein Ring aus acht Eintraegen a acht Byte
;
;  Aufbau je Eintrag: Art, Stunde, Minute, Tag, Monat, Jahr, 0, 0
;
;  Warum ein Protokoll neben all den Sperren: Sperren sagen, was nicht
;  passieren darf. Das Protokoll sagt, was passiert IST -- und fuer einen
;  Firmenrechner ist das oft das Nuetzlichere. Echte BIOSe fuehren es als
;  SMBIOS Type 15 Event Log.
; ---------------------------------------------------------------------------
ev_log:                               ; r1 = Ereignisart
    push r6
    push r7
    push r8
    mov r8, r1                        ; Art merken

    movi r1, NV_LOGHEAD
    call nv_read
    andi r6, r0, 7                    ; r6 = Platz im Ring
    muli r7, r6, NV_LOGLEN
    addi r7, r7, NV_LOG               ; r7 = Adresse des Eintrags

    mov r1, r7
    mov r2, r8
    call nv_write
    movi r10, CM_HOUR
    call cmos_read
    addi r1, r7, 1
    mov r2, r0
    call nv_write
    movi r10, CM_MIN
    call cmos_read
    addi r1, r7, 2
    mov r2, r0
    call nv_write
    movi r10, CM_DAY
    call cmos_read
    addi r1, r7, 3
    mov r2, r0
    call nv_write
    movi r10, CM_MONTH
    call cmos_read
    addi r1, r7, 4
    mov r2, r0
    call nv_write
    movi r10, CM_YEAR
    call cmos_read
    addi r1, r7, 5
    mov r2, r0
    call nv_write

    addi r6, r6, 1                    ; Zeiger weiterdrehen
    andi r6, r6, 7
    movi r1, NV_LOGHEAD
    mov r2, r6
    call nv_write

    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Das Schalterwort
; ---------------------------------------------------------------------------
pol_lesen:                            ; -> r0
    movi r10, CM_POLICY
    call cmos_read
    ret

pol_schreiben:                        ; r1 = Wert
    movi r10, CM_POLICY
    mov r11, r1
    call cmos_write
    ret

pol_frage:                            ; r1 = Bitmaske -> r0 = 0/1
    push r6
    mov r6, r1
    call pol_lesen
    and r0, r0, r6
    cmpi r0, 0
    jz .nein
    movi r0, 1
    pop r6
    ret
.nein:
    movi r0, 0
    pop r6
    ret

pol_umschalten:                       ; r1 = Bitmaske
    push r6
    mov r6, r1
    call pol_lesen
    xor r0, r0, r6
    mov r1, r0
    call pol_schreiben
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Die Sperrliste: ein Bit je Programm, sechzehn Stueck in zwei CMOS-Plaetzen
; ---------------------------------------------------------------------------
blk_lesen:                            ; -> r0 = 16 Bit
    push r6
    movi r10, CM_BLOCK1
    call cmos_read
    shli r6, r0, 8
    movi r10, CM_BLOCK0
    call cmos_read
    or r0, r0, r6
    pop r6
    ret

blk_schreiben:                        ; r1 = 16 Bit
    push r6
    mov r6, r1
    movi r10, CM_BLOCK0
    andi r11, r6, 0xFF
    call cmos_write
    shri r6, r6, 8
    movi r10, CM_BLOCK1
    andi r11, r6, 0xFF
    call cmos_write
    pop r6
    ret

blk_gesetzt:                          ; r1 = Nummer -> r0 = 0/1
    push r6
    push r7
    mov r6, r1
    call blk_lesen
    mov r7, r0
    movi r0, 1
    shl r0, r0, r6
    and r0, r0, r7
    cmpi r0, 0
    jz .nein
    movi r0, 1
    pop r7
    pop r6
    ret
.nein:
    movi r0, 0
    pop r7
    pop r6
    ret

blk_umschalten:                       ; r1 = Nummer
    push r6
    push r7
    mov r6, r1
    call blk_lesen
    mov r7, r0
    movi r0, 1
    shl r0, r0, r6
    xor r7, r7, r0
    mov r1, r7
    call blk_schreiben
    pop r7
    pop r6
    ret

; --- Name eines Programms holen (r1 = Nummer) -> r0 = Zeiger --------------
blk_name:
    push r6
    shli r6, r1, 2
    li r10, prog_namen
    add r6, r6, r10
    ldw r0, [r6]
    pop r6
    ret

; ===========================================================================
;  Alles in den Speicher legen, wo das Betriebssystem es findet
;
;  Das ist unser SMBIOS. Beim echten PC legt die Firmware eine Tabelle hin
;  und das System liest sie -- hier genauso, nur einfacher.
; ===========================================================================
firma_veroeffentlichen:
    push r6
    push r7
    push r8
    push r9

    ; --- 1. Alles leeren. Auch die neuen Bereiche.
    ; Sonst bliebe nach dem Zurueckflashen auf ein anderes BIOS die
    ; Sperrliste stehen, und das System sperrte Programme, fuer die es
    ; laengst keine Firmware mehr gibt.
    li r6, BDA_FIRMA
    li r7, BDA_INVENT+BDA_INVLEN
.leeren:
    cmp r6, r7
    jge .geleert
    movi r11, 0
    stb [r6], r11
    addi r6, r6, 1
    jmp .leeren
.geleert:

    ; --- 2. Der Eigentuemer-Eintrag, aber nur wenn er gewuenscht ist
    movi r1, POL_OWNER
    call pol_frage
    cmpi r0, 0
    jz .ohne_text
    li r1, NV_FIRMA
    li r2, BDA_FIRMA
    movi r3, 31
    call nv_str_lesen
.ohne_text:

    ; --- 3. Das Schalterwort, unveraendert aus dem CMOS
    call pol_lesen
    stwa BDA_POLICY, r0

    ; --- 4. Die gesperrten Programme im Klartext
    li r8, BDA_BLOCK                  ; r8 = naechster Platz
    movi r9, 0                        ; r9 = Programmnummer
.sperrliste:
    cmpi r9, BDA_BLOCKN
    jge .liste_fertig
    mov r1, r9
    call blk_gesetzt
    cmpi r0, 0
    jz .naechstes
    mov r1, r9
    call blk_name
    mov r1, r0
    mov r2, r8
    call firma_strcpy
    addi r8, r8, 16
.naechstes:
    addi r9, r9, 1
    jmp .sperrliste
.liste_fertig:

    ; --- 5. Inventar: Seriennummer, Starts, Betriebsminuten
    li r1, NV_SERIAL
    li r2, BDA_INVENT
    movi r3, 15
    call nv_str_lesen
    movi r1, NV_BOOTS
    call nv_read32
    stwa BDA_INVENT+16, r0
    movi r1, NV_MINUTES
    call nv_read32
    stwa BDA_INVENT+20, r0

    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- Zeichenkette kopieren, hoechstens 15 Zeichen plus Null ---------------
firma_strcpy:                         ; r1 = Quelle, r2 = Ziel
    push r6
    push r7
    push r8
    mov r6, r1
    mov r7, r2
    movi r8, 0
.loop:
    cmpi r8, 15
    jge .ende
    add r10, r6, r8
    ldb r11, [r10]
    add r10, r7, r8
    stb [r10], r11
    cmpi r11, 0
    jz .fertig
    addi r8, r8, 1
    jmp .loop
.ende:
    add r10, r7, r8
    movi r11, 0
    stb [r10], r11
.fertig:
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Inventar: bei jedem Start einmal weiterzaehlen
; ---------------------------------------------------------------------------
inv_start_zaehlen:
    push r6
    movi r1, NV_BOOTS
    call nv_read32
    addi r6, r0, 1
    movi r1, NV_BOOTS
    mov r2, r6
    call nv_write32
    pop r6
    ret

; ===========================================================================
;  Der Knopf "Owner Text" im Reiter Company
; ===========================================================================
firma_text_setzen:
    push r6
    li r1, TXT_BUF                    ; den aktuellen Text vorlegen
    li r2, NV_FIRMA
    push r1
    mov r1, r2
    li r2, TXT_BUF
    movi r3, 31
    call nv_str_lesen
    pop r1

    li r1, s_co_owner
    call pw_fenster
    li r1, TXT_BUF
    li r2, s_co_frage
    movi r3, 31
    call txt_eingabe
    cmpi r0, 0
    jl .abbruch

    li r1, NV_FIRMA
    li r2, TXT_BUF
    movi r3, 31
    call nv_str_schreiben
    call firma_veroeffentlichen
    li r1, s_co_gesetzt
    jmp .melden
.abbruch:
    li r1, s_co_abbruch
.melden:
    push r1
    call setup_frame
    pop r1
    call setup_message
    pop r6
    ret

; ===========================================================================
;  Der Knopf "Blocked Programs" -- eine Liste zum Abhaken
; ===========================================================================
.equ BLK_X,        18
.equ BLK_Y,        3
.equ BLK_W,        44
.equ BLK_H,        21

firma_sperrliste:
    push r6
    push r7
    movi r6, 0                        ; markierte Zeile
.zeichnen:
    movi r1, BLK_X
    movi r2, BLK_Y
    movi r3, BLK_W
    movi r4, BLK_H
    movi r5, A_SEL
    call vid_fillrect
    movi r1, BLK_X
    movi r2, BLK_Y
    movi r3, BLK_W
    movi r4, BLK_H
    movi r5, A_SEL
    call vid_box
    movi r1, BLK_X+3
    movi r2, BLK_Y
    li r3, s_co_blkhead
    movi r4, A_SEL
    call vid_putsat
    movi r1, BLK_X+3
    movi r2, BLK_Y+BLK_H-2
    li r3, s_co_blkkeys
    movi r4, A_SEL
    call vid_putsat

    movi r7, 0
.zeile:
    cmpi r7, BDA_BLOCKN
    jge .zeilen_fertig
    mov r1, r7
    call blk_gesetzt
    push r0
    addi r1, r7, BLK_Y+2              ; y-Position
    mov r2, r1
    movi r1, BLK_X+3
    movi r3, 0x5B                     ; '['
    movi r4, A_SEL
    cmp r7, r6
    jnz .normal1
    movi r4, A_BG
.normal1:
    push r4
    call vid_putat
    pop r4
    pop r0
    push r4
    push r0
    movi r3, 0x20
    cmpi r0, 0
    jz .leer
    movi r3, 0x58                     ; 'X'
.leer:
    addi r2, r7, BLK_Y+2
    movi r1, BLK_X+4
    call vid_putat
    pop r0
    pop r4
    addi r2, r7, BLK_Y+2
    movi r1, BLK_X+5
    movi r3, 0x5D                     ; ']'
    call vid_putat

    mov r1, r7
    call blk_name
    mov r3, r0
    movi r1, BLK_X+7
    addi r2, r7, BLK_Y+2
    movi r4, A_SEL
    cmp r7, r6
    jnz .normal2
    movi r4, A_BG
.normal2:
    call vid_putsat
    addi r7, r7, 1
    jmp .zeile
.zeilen_fertig:

    call kbd_getkey
    shri r10, r0, 8
    cmpi r10, K_UP
    jz .hoch
    cmpi r10, K_DOWN
    jz .runter
    cmpi r10, K_ENTER
    jz .haken
    cmpi r10, K_ESC
    jz .fertig
    jmp .zeichnen
.hoch:
    subi r6, r6, 1
    cmpi r6, 0
    jge .zeichnen
    movi r6, BDA_BLOCKN-1
    jmp .zeichnen
.runter:
    addi r6, r6, 1
    cmpi r6, BDA_BLOCKN
    jl .zeichnen
    movi r6, 0
    jmp .zeichnen
.haken:
    mov r1, r6
    call blk_umschalten
    jmp .zeichnen
.fertig:
    call firma_veroeffentlichen
    call pw_sichern                   ; Haken gelten sofort
    call setup_frame
    li r1, s_co_blkok
    call setup_message
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  Die feste Programmliste
;
;  Namen wie sie auf der Platte stehen -- gui_prog_starten() bekommt genau
;  diese Zeichenkette. Kommt ein Programm dazu, aendert sich nur diese
;  Tabelle und sonst nichts.
; ---------------------------------------------------------------------------
.align 4
prog_namen:
    .dw s_pn_coder, s_pn_prompt, s_pn_files, s_pn_monitor
    .dw s_pn_control, s_pn_settings, s_pn_paint, s_pn_word
    .dw s_pn_browser, s_pn_calc, s_pn_flappy, s_pn_py
    .dw s_pn_cc, s_pn_asm, s_pn_bench, s_pn_fenster

s_pn_coder:    .db "CODER.TBX", 0
s_pn_prompt:   .db "PROMPT.TBX", 0
s_pn_files:    .db "FILES.TBX", 0
s_pn_monitor:  .db "MONITOR.TBX", 0
s_pn_control:  .db "CONTROL.TBX", 0
s_pn_settings: .db "SETTINGS.TBX", 0
s_pn_paint:    .db "PAINT.TBX", 0
s_pn_word:     .db "WORD.TBX", 0
s_pn_browser:  .db "BROWSER.TBX", 0
s_pn_calc:     .db "CALC.TBX", 0
s_pn_flappy:   .db "FLAPPY.TBX", 0
s_pn_py:       .db "PY.TBX", 0
s_pn_cc:       .db "CC.TBX", 0
s_pn_asm:      .db "ASM.TBX", 0
s_pn_bench:    .db "BENCH.TBX", 0
s_pn_fenster:  .db "FENSTER.TBX", 0
.align 4

s_co_owner:    .db " Owner Text ", 0
s_co_frage:    .db "Owner Text (max 31):", 0
s_co_gesetzt:  .db "Owner text stored. It appears after the next start.", 0
s_co_abbruch:  .db "Cancelled. Nothing was changed.", 0
s_co_blkhead:  .db " Blocked Programs ", 0
s_co_blkkeys:  .db "ENTER toggles      ESC closes", 0
s_co_blkok:    .db "Blocked program list stored.", 0
.align 4

; ===========================================================================
;  A8 -- merken, dass die Knopfzelle gezogen wurde
;
;  Verhindern kann man es nicht: wer an disk/cmos.bin kommt, loescht alle
;  Sperren, und bei einem echten Mainboard ist das derselbe Handgriff. Was
;  man tun kann, ist es SICHTBAR machen. Echte Firmenrechner nennen das
;  Chassis Intrusion.
;
;  Erkannt wird es an einer Kennung im pruefsummierten Bereich. Ein geleertes
;  CMOS hat dort eine Null; das Serien-BIOS schreibt sie nie. (Auf 0x2F, das
;  im Pflichtenheft stand, geht es nicht: dieses Byte ist die Kennung des
;  Bausteins selbst, und der setzt sie beim Leeren gleich wieder richtig --
;  gemerkt haette man davon nichts.)
; ===========================================================================
intrusion_pruefen:
    push r6
    movi r10, CM_INTRUSION
    call cmos_read
    cmpi r0, PW_INTRUSION_MAGIC
    jz .in_ordnung

    ; Kennung fehlt. Das heisst aber nur dann "jemand hat die Knopfzelle
    ; gezogen", wenn dieser Rechner schon einmal eingerichtet WAR -- sonst
    ; ist es schlicht der erste Start. Den Unterschied kennt allein das
    ; NVRAM: es liegt in einer eigenen Datei und wird nicht mitgeloescht,
    ; wenn jemand nur cmos.bin wegnimmt. Genau deshalb ist es der richtige
    ; Zeuge. Ohne diese Unterscheidung meldete ein fabrikneuer Rechner beim
    ; allerersten Einschalten einen Einbruch.
    ldwa r6, NV_WAR_DA
    cmpi r6, 0
    jz .nur_neu

    movi r6, 1                        ; fuer diesen Start gemerkt
    stwa INTRUSION_FLAG, r6
    movi r1, EV_CLEARED
    call ev_log
    movi r10, CM_INTRUSION            ; und gleich wieder kennzeichnen
    movi r11, PW_INTRUSION_MAGIC
    call cmos_write
    call pw_sichern
    pop r6
    ret
.nur_neu:                             ; erster Start: still kennzeichnen
    movi r6, 0
    stwa INTRUSION_FLAG, r6
    movi r10, CM_INTRUSION
    movi r11, PW_INTRUSION_MAGIC
    call cmos_write
    call pw_sichern
    pop r6
    ret
.in_ordnung:
    movi r6, 0
    stwa INTRUSION_FLAG, r6
    pop r6
    ret

intrusion_frage:                      ; -> r0 = 1, wenn dieser Start betroffen war
    ldwa r0, INTRUSION_FLAG
    ret

; --- Die rote Meldung beim Start ------------------------------------------
intrusion_melden:
    push r6
    ldwa r6, INTRUSION_FLAG
    cmpi r6, 0
    jz .nichts
    movi r1, 0x4F                     ; weiss auf rot
    call vid_clear
    movi r1, 15
    movi r2, 10
    li r3, s_in_head
    movi r4, 0x4E
    call vid_putsat
    movi r1, 15
    movi r2, 12
    li r3, s_in_text
    movi r4, 0x4F
    call vid_putsat
    movi r1, 15
    movi r2, 14
    li r3, s_in_taste
    movi r4, 0x4F
    call vid_putsat
    call kbd_getkey
    movi r1, ATTR_NORMAL
    call vid_clear
.nichts:
    pop r6
    ret

s_in_head:    .db "CONFIGURATION CLEARED", 0
s_in_text:    .db "Configuration was cleared -- contact your administrator.", 0
s_in_taste:   .db "Press any key to continue.", 0
.align 4
