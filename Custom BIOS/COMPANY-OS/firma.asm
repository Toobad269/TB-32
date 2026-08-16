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

; ===========================================================================
;  A6 -- nur von der eigenen Platte starten
;
;  Der klassische erste Angriff auf einen fremden Rechner ist, ein eigenes
;  System von woanders zu starten und die Platte in Ruhe auszulesen. Sperrt
;  man das nicht, waren alle anderen Sperren umsonst: sie stehen ja im
;  System, das dann gar nicht erst hochkommt.
;
;  Beim TB-32 ist die Lage heute besonders: `boot` liest immer Sektor 0 der
;  Platte, und Floppy wie Netz stehen im Setup als "not installed". Es GIBT
;  also noch keine zweite Startquelle. Was dieses Bit deshalb tut, ist die
;  Einstellung festzunageln -- zweifach:
;
;    1. Das Setup laesst "Boot Device Priority" nicht mehr aendern.
;    2. Steht beim Start trotzdem etwas anderes als die Platte drin, wird es
;       zurueckgesetzt und protokolliert. Das faengt den Weg ab, der am Setup
;       vorbeifuehrt: jemand schreibt von aussen in cmos.bin.
;
;  Kommt spaeter der Netzwerkstart dazu (B5), bewacht dasselbe Bit ihn schon.
; ===========================================================================
boot_quelle_sichern:
    push r6
    movi r1, POL_INTDISK
    call pol_frage
    cmpi r0, 0
    jz .egal

    movi r10, CM_BOOTDEV
    call cmos_read
    cmpi r0, 0
    jz .egal                          ; steht ohnehin auf der Platte

    movi r10, CM_BOOTDEV              ; zurueck auf die eigene Platte
    movi r11, 0
    call cmos_write
    call pw_sichern
    movi r1, EV_BOOTSRC
    call ev_log
    li r1, s_a6_zurueck
    call pw_melden
.egal:
    pop r6
    ret

s_a6_zurueck: .db "Boot source was changed -- reset to the internal disk.", 0
.align 4

; ===========================================================================
;  B1 -- den Ereignisspeicher anzeigen
;
;  Die Zeilen stehen mit dem NEUESTEN oben. Der Ring zeigt mit NV_LOGHEAD auf
;  den naechsten freien Platz, also ist head-1 der juengste Eintrag.
; ===========================================================================
ev_zeile_zeigen:                      ; r1 = Zeile 0..7, r2 = Attribut
    push r6
    push r7
    push r8
    mov r8, r2                        ; Attribut retten
    mov r6, r1

    movi r1, NV_LOGHEAD
    call nv_read
    subi r0, r0, 1                    ; juengster Eintrag
    sub r0, r0, r6                    ; r6 Schritte zurueck
    andi r7, r0, 7
    muli r7, r7, NV_LOGLEN
    addi r7, r7, NV_LOG               ; r7 = Adresse des Eintrags

    mov r1, r7
    call nv_read
    cmpi r0, 0
    jz .leer

    push r0
    mov r1, r0
    call ev_name
    mov r1, r0
    mov r2, r8
    call vid_puts
    pop r0

    li r1, s_ev_bei
    mov r2, r8
    call vid_puts
    addi r1, r7, 3                    ; Tag
    call nv_read
    mov r1, r0
    mov r2, r8
    call vid_putn
    li r1, s_ev_punkt
    mov r2, r8
    call vid_puts
    addi r1, r7, 4                    ; Monat
    call nv_read
    mov r1, r0
    mov r2, r8
    call vid_putn
    li r1, s_ev_luecke
    mov r2, r8
    call vid_puts
    addi r1, r7, 1                    ; Stunde
    call nv_read
    mov r1, r0
    mov r2, r8
    call vid_putn
    li r1, s_ev_doppel
    mov r2, r8
    call vid_puts
    addi r1, r7, 2                    ; Minute
    call nv_read
    cmpi r0, 10
    jge .keine_null
    push r0
    li r1, s_ev_null
    mov r2, r8
    call vid_puts
    pop r0
.keine_null:
    mov r1, r0
    mov r2, r8
    call vid_putn
    jmp .fertig
.leer:
    li r1, s_ev_leer
    mov r2, r8
    call vid_puts
.fertig:
    pop r8
    pop r7
    pop r6
    ret

; --- Klartext zu einer Ereignisart (r1 = Art) -> r0 = Zeiger --------------
ev_name:
    cmpi r1, EV_BADPW
    jz .badpw
    cmpi r1, EV_CLEARED
    jz .cleared
    cmpi r1, EV_FLASH
    jz .flash
    cmpi r1, EV_SECURE
    jz .secure
    cmpi r1, EV_DEFAULTS
    jz .defaults
    cmpi r1, EV_LOCKOUT
    jz .lockout
    cmpi r1, EV_BOOT
    jz .boot
    cmpi r1, EV_BOOTSRC
    jz .bootsrc
    li r0, s_ev_unbekannt
    ret
.badpw:    li r0, s_ev_badpw
    ret
.cleared:  li r0, s_ev_cleared
    ret
.flash:    li r0, s_ev_flash
    ret
.secure:   li r0, s_ev_secure
    ret
.defaults: li r0, s_ev_defaults
    ret
.lockout:  li r0, s_ev_lockout
    ret
.boot:     li r0, s_ev_boot
    ret
.bootsrc:  li r0, s_ev_bootsrc
    ret

ev_leeren:
    push r6
    movi r6, NV_LOG
.loop:
    cmpi r6, NV_LOG+64
    jae .fertig
    mov r1, r6
    movi r2, 0
    call nv_write
    addi r6, r6, 1
    jmp .loop
.fertig:
    movi r1, NV_LOGHEAD
    movi r2, 0
    call nv_write
    pop r6
    ret

s_ev_leer:      .db "--", 0
s_ev_bei:       .db "   ", 0
s_ev_punkt:     .db ".", 0
s_ev_luecke:    .db "  ", 0
s_ev_doppel:    .db ":", 0
s_ev_null:      .db "0", 0
s_ev_badpw:     .db "Wrong password", 0
s_ev_cleared:   .db "Configuration cleared", 0
s_ev_flash:     .db "BIOS flashed", 0
s_ev_secure:    .db "Secure Boot halted", 0
s_ev_defaults:  .db "Setup defaults loaded", 0
s_ev_lockout:   .db "Locked out", 0
s_ev_boot:      .db "Started", 0
s_ev_bootsrc:   .db "Boot source reset", 0
s_ev_unbekannt: .db "Unknown", 0
.align 4

; ===========================================================================
;  B6 -- das Startbild der Firma
;
;  Den BIOS-Namen malt das Mainboard schon in die Bildmitte, bevor ueberhaupt
;  Code laeuft. Darunter kommt der Firmentext -- aus demselben Speicher wie
;  der Eigentuemer-Eintrag, also ohne eine einzige zusaetzliche Einstellung.
;  Kleine Arbeit, grosse Wirkung: der Rechner sieht ab dem Einschaltknopf
;  nach seinem Eigentuemer aus und nicht erst ab dem Schreibtisch.
; ===========================================================================
firma_startbild:
    push r6
    push r7
    movi r1, POL_OWNER
    call pol_frage
    cmpi r0, 0
    jz .nichts

    li r1, NV_FIRMA
    li r2, TXT_BUF
    movi r3, 31
    call nv_str_lesen
    li r10, TXT_BUF                   ; leerer Text? Dann nichts malen
    ldb r0, [r10]
    cmpi r0, 0
    jz .nichts

    li r1, TXT_BUF                    ; mittig setzen
    call firma_laenge
    mov r6, r0
    shri r6, r6, 1
    movi r7, SCR_W/2
    sub r7, r7, r6
    cmpi r7, 0
    jge .x_ok
    movi r7, 0
.x_ok:
    mov r1, r7
    movi r2, 14
    li r3, TXT_BUF
    movi r4, ATTR_BRIGHT
    call vid_putsat
.nichts:
    pop r7
    pop r6
    ret

firma_laenge:                         ; r1 = Zeiger -> r0 = Laenge
    push r6
    mov r6, r1
    movi r0, 0
.loop:
    add r10, r6, r0
    ldb r11, [r10]
    cmpi r11, 0
    jz .fertig
    addi r0, r0, 1
    cmpi r0, 40
    jl .loop
.fertig:
    pop r6
    ret
.align 4

; ===========================================================================
;  B4 -- das Startmenue
;
;  Einmal von woanders starten, OHNE die Einstellung zu aendern. Bei echten
;  Rechnern liegt es auf F12 oder F8; hier muss es F8 sein, denn F11 und F12
;  gehoeren dem Fenster (Vollbild und Einblendung) und erreichen den
;  virtuellen Rechner gar nicht.
;
;  Steht A6 ("nur von der eigenen Platte"), verlangt das Menue vorher das
;  Supervisor-Passwort. Damit ist es das Werkzeug des Administrators und
;  nicht das Schlupfloch, das die Sperre aushebelt -- ohne diese Abfrage
;  waere A6 mit einem Tastendruck erledigt.
; ===========================================================================
.equ BM_X,         22
.equ BM_Y,         8
.equ BM_W,         36
.equ BM_H,         10

boot_menue:
    push r6
    push r7

    movi r1, POL_INTDISK              ; bei gesetzter Sperre erst das Passwort
    call pol_frage
    cmpi r0, 0
    jz .offen
    call pw_gesetzt
    cmpi r0, 0
    jz .offen                         ; kein Passwort gesetzt: nichts zu fragen
    movi r1, ATTR_NORMAL
    call vid_clear
    li r1, s_bm_locked
    call pw_pruefen
    cmpi r0, 1
    jz .offen
    li r1, s_pw_wrong
    call pw_melden
    movi r1, ATTR_NORMAL
    call vid_clear
    pop r7
    pop r6
    ret

.offen:
    movi r6, 0                        ; markierte Zeile
.zeichnen:
    movi r1, ATTR_NORMAL
    call vid_clear
    movi r1, BM_X
    movi r2, BM_Y
    movi r3, BM_W
    movi r4, BM_H
    movi r5, A_SEL
    call vid_fillrect
    movi r1, BM_X
    movi r2, BM_Y
    movi r3, BM_W
    movi r4, BM_H
    movi r5, A_SEL
    call vid_box
    movi r1, BM_X+3
    movi r2, BM_Y
    li r3, s_bm_head
    movi r4, A_SEL
    call vid_putsat
    movi r1, BM_X+3
    movi r2, BM_Y+BM_H-2
    li r3, s_bm_keys
    movi r4, A_SEL
    call vid_putsat

    movi r7, 0
.zeile:
    cmpi r7, 3
    jge .zeilen_fertig
    shli r10, r7, 2
    li r11, bm_namen
    add r10, r10, r11
    ldw r3, [r10]
    movi r1, BM_X+3
    addi r2, r7, BM_Y+2
    movi r4, A_SEL
    cmp r7, r6
    jnz .normal
    movi r4, A_BG
.normal:
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
    jz .waehlen
    cmpi r10, K_ESC
    jz .raus
    jmp .zeichnen
.hoch:
    subi r6, r6, 1
    cmpi r6, 0
    jge .zeichnen
    movi r6, 2
    jmp .zeichnen
.runter:
    addi r6, r6, 1
    cmpi r6, 3
    jl .zeichnen
    movi r6, 0
    jmp .zeichnen

.waehlen:
    cmpi r6, 0
    jz .raus                          ; die eigene Platte: einfach weiter
    li r1, s_bm_fehlt                 ; Floppy und Netz gibt es (noch) nicht
    call pw_melden
    jmp .zeichnen
.raus:
    movi r1, ATTR_NORMAL
    call vid_clear
    pop r7
    pop r6
    ret

.align 4
bm_namen:  .dw s_bm_disk, s_bm_floppy, s_bm_net
s_bm_head:   .db " Boot Menu ", 0
s_bm_keys:   .db "ENTER starts      ESC cancels", 0
s_bm_disk:   .db "Hard Disk 0  (internal)", 0
s_bm_floppy: .db "Floppy       (not installed)", 0
s_bm_net:    .db "Network      (not installed)", 0
s_bm_fehlt:  .db "That boot source is not installed on this machine.", 0
s_bm_locked: .db " Boot Menu is locked ", 0
.align 4

; ===========================================================================
;  C -- die Startverzoegerung
;
;  Sekunden warten, bevor gebootet wird, damit man DEL sicher trifft. Auf
;  langsamen Anzeigen hilft das wirklich, und bei einem Rechner mit Quick
;  Boot ist die Bedenkzeit sonst eine Viertelsekunde.
; ===========================================================================
boot_verzoegern:
    push r6
    push r7
    movi r10, CM_BOOTDELAY
    call cmos_read
    cmpi r0, 0
    jz .nichts
    cmpi r0, 9
    jle .ok
    movi r0, 9                        ; ein verbogenes CMOS nicht ewig warten lassen
.ok:
    muli r6, r0, 100                  ; Ticks: 100 je Sekunde
    ldwa r7, BDA_TICKS
    add r6, r6, r7
.warten:
    ldwa r7, BDA_TICKS
    cmp r7, r6
    jae .nichts
    hlt
    jmp .warten
.nichts:
    pop r7
    pop r6
    ret
.align 4

; ===========================================================================
;  Die Sektorsperre -- die Firmware laesst gesperrte Programme gar nicht
;  erst in den Arbeitsspeicher
;
;  Vorher sass die Sperre im Kernel (prog_run). Das ist eine Bitte, keine
;  Mauer: wer den Kernel austauscht -- und das kann auf diesem Rechner jeder,
;  der einen Compiler hat --, ist sie los. Und das Programm lag dabei schon
;  im RAM; verweigert wurde erst der Sprung hinein.
;
;  Hier ist es umgekehrt. Beim Start loest die Firmware jeden gesperrten
;  Namen zu seinem Sektorbereich auf und merkt sich das Paar. Danach
;  verweigert INT 0x13 -- der Dienst, ueber den JEDE Dateilesung des Systems
;  laeuft -- jeden Lesezugriff, der diesen Bereich beruehrt. Das Programm
;  kommt nicht in den Speicher, egal welches Betriebssystem darueber laeuft
;  und egal ob es ueber das Startmenue, die Dateiverwaltung oder START
;  gerufen wird.
;
;  Was das NICHT kann, und das gehoert dazu: Die Tabelle entsteht beim Start
;  aus dem Verzeichnis. Verschiebt das System die Datei danach, zeigt der
;  gemerkte Bereich ins Leere, bis zum naechsten Neustart. Die Sperre im
;  Kernel bleibt deshalb bestehen -- sie kennt Namen statt Sektoren und faengt
;  genau diesen Fall ab. Zwei Schichten, jede mit einer anderen Schwaeche.
; ===========================================================================

.align 4

; --- fs_name_gleich(r1 = Eintrag, r2 = Name) -> r0 = 1 bei Gleichheit -----
fs_name_gleich:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1
    mov r7, r2
    movi r8, 0
.loop:
    cmpi r8, FS_E_START               ; das Namensfeld ist 16 Byte lang
    jge .ja
    add r10, r6, r8
    ldb r0, [r10]
    add r10, r7, r8
    ldb r9, [r10]
    cmp r0, r9
    jnz .nein
    cmpi r0, 0
    jz .ja
    addi r8, r8, 1
    jmp .loop
.ja:
    movi r0, 1
    jmp .fertig
.nein:
    movi r0, 0
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  fs_suchen(r1 = Name, r2 = Elternordner+1, r3 = 1 wenn ein Ordner gesucht
;            wird) -> r0 = Eintragsnummer, oder -1
;
;  Das Verzeichnis muss vorher bei SEC_PUFFER liegen.
; ---------------------------------------------------------------------------
fs_suchen:
    push r6
    push r7
    push r8
    push r9
    mov r7, r1
    mov r8, r2
    mov r9, r3
    li r6, SEC_PUFFER
    movi r0, 0
.loop:
    cmpi r0, FS_MAXFILES
    jge .nichts
    push r0
    mov r1, r6
    mov r2, r7
    call fs_name_gleich
    mov r10, r0
    pop r0
    cmpi r10, 0
    jz .weiter

    ldw r10, [r6+FS_E_INFO]
    shri r11, r10, 16                 ; Elternordner+1
    cmp r11, r8
    jnz .weiter
    andi r10, r10, 0xFF               ; Art
    cmpi r9, 0
    jz .keine_ordnerpruefung
    cmpi r10, FS_FT_DIR
    jnz .weiter
    jmp .gefunden
.keine_ordnerpruefung:
    cmpi r10, FS_FT_DIR
    jz .weiter                        ; Ordner sind hier nicht gemeint
.gefunden:
    pop r9
    pop r8
    pop r7
    pop r6
    ret
.weiter:
    addi r6, r6, FS_ENTSIZE
    addi r0, r0, 1
    jmp .loop
.nichts:
    li r0, 0xFFFFFFFF
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  blk_sektoren_bauen -- beim Start die Tabelle fuellen
; ---------------------------------------------------------------------------
blk_sektoren_bauen:
    push r6
    push r7
    push r8
    push r9

    li r6, BLK_SEK                    ; Tabelle leeren
    movi r7, 0
.leeren:
    cmpi r7, BLK_SEKN*8
    jae .geleert
    add r10, r6, r7
    movi r11, 0
    stw [r10], r11
    addi r7, r7, 4
    jmp .leeren
.geleert:

    li r1, FS_DIRSEC0                 ; Verzeichnis holen
    movi r2, FS_DIRSECS
    li r3, SEC_PUFFER
    call disk_read
    cmpi r0, 0
    jnz .raus

    ; Die beiden Programmordner einmal nachschlagen
    li r1, s_d_system
    movi r2, 0
    movi r3, 1
    call fs_suchen
    addi r8, r0, 1                    ; r8 = \SYSTEM als Elternnummer (0 = keiner)
    cmpi r0, 0
    jge .sys_ok
    movi r8, 0
.sys_ok:
    cmpi r8, 0
    jz .sysprogs_fehlt
    li r1, s_d_progs
    mov r2, r8
    movi r3, 1
    call fs_suchen
    addi r8, r0, 1                    ; r8 = \SYSTEM\PROGS
    cmpi r0, 0
    jge .sysprogs_ok
.sysprogs_fehlt:
    movi r8, 0
.sysprogs_ok:

    li r1, s_d_progs                  ; r9 = \PROGS im Hauptverzeichnis
    movi r2, 0
    movi r3, 1
    call fs_suchen
    addi r9, r0, 1
    cmpi r0, 0
    jge .progs_ok
    movi r9, 0
.progs_ok:

    movi r6, 0                        ; r6 = Programmnummer
    movi r7, 0                        ; r7 = naechster Tabellenplatz
.programm:
    cmpi r6, BDA_BLOCKN
    jge .raus
    mov r1, r6
    call blk_gesetzt
    cmpi r0, 0
    jz .naechstes

    mov r1, r6
    call blk_name
    mov r1, r0

    push r1                           ; erst in \SYSTEM\PROGS
    mov r2, r8
    movi r3, 0
    call fs_suchen
    pop r1
    cmpi r0, 0
    jge .eintragen

    push r1                           ; dann in \PROGS
    mov r2, r9
    movi r3, 0
    call fs_suchen
    pop r1
    cmpi r0, 0
    jge .eintragen

    movi r2, 0                        ; zuletzt im Hauptverzeichnis
    movi r3, 0
    call fs_suchen
    cmpi r0, 0
    jl .naechstes

.eintragen:
    muli r10, r0, FS_ENTSIZE
    li r11, SEC_PUFFER
    add r10, r10, r11                 ; r10 = Eintrag
    ldw r0, [r10+FS_E_START]          ; Startsektor
    ldw r11, [r10+FS_E_SIZE]          ; Groesse in Byte
    addi r11, r11, 511                ; aufrunden auf ganze Sektoren
    shri r11, r11, 9
    cmpi r11, 0
    jnz .laenge_ok
    movi r11, 1
.laenge_ok:
    shli r1, r7, 3
    li r2, BLK_SEK
    add r1, r1, r2
    stw [r1], r0
    stw [r1+4], r11
    addi r7, r7, 1
    cmpi r7, BLK_SEKN
    jge .raus
.naechstes:
    addi r6, r6, 1
    jmp .programm
.raus:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; ---------------------------------------------------------------------------
;  blk_sektor_frage(r1 = LBA, r2 = Anzahl) -> r0 = 1, wenn gesperrt
;
;  Zwei Bereiche ueberschneiden sich, wenn jeder vor dem Ende des anderen
;  anfaengt. Genau das steht hier, sonst nichts.
; ---------------------------------------------------------------------------
blk_sektor_frage:
    push r6
    push r7
    push r8
    push r9
    mov r8, r1                        ; Anfang der Anfrage
    add r9, r1, r2                    ; Ende (ausschliesslich)
    li r6, BLK_SEK
    movi r7, 0
.loop:
    cmpi r7, BLK_SEKN
    jge .frei
    ldw r0, [r6]                      ; Startsektor
    ldw r1, [r6+4]                    ; Anzahl
    cmpi r1, 0
    jz .frei                          ; leerer Platz = Ende der Tabelle
    add r1, r0, r1                    ; Ende des gesperrten Bereichs
    cmp r8, r1
    jge .weiter                       ; Anfrage faengt erst danach an
    cmp r0, r9
    jge .weiter                       ; Anfrage hoert schon davor auf
    movi r0, 1
    jmp .fertig
.weiter:
    addi r6, r6, 8
    addi r7, r7, 1
    jmp .loop
.frei:
    movi r0, 0
.fertig:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

s_d_system:  .db "SYSTEM", 0
s_d_progs:   .db "PROGS", 0
.align 4
