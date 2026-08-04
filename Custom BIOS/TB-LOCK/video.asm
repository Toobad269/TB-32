; ===========================================================================
;  Bildschirm-Bibliothek der Firmware
;
;  Aufrufkonvention (gilt im ganzen Projekt):
;     r0        Rueckgabewert
;     r1..r5    Argumente
;     r6..r9    muessen von der gerufenen Funktion gesichert werden
;     r10..r12  Kratzregister, duerfen jederzeit zerstoert werden
;     r13 (at)  Hilfsregister des Assemblers -- nach ldwa/stwa immer futsch
;     r14 (fp)  Framepointer,  r15 (sp) Stackpointer
;
;  Der Textbildschirm liegt ab VRAM_TEXT: je Zelle 1 Byte Zeichen + 1 Byte
;  Farbe (Hintergrund<<4 | Vordergrund) -- exakt wie bei einer echten
;  VGA-Karte im Textmodus.
; ===========================================================================

; --- Cursorposition an die Grafikkarte melden ------------------------------
vid_sync:
    ldwa r10, BDA_CURY
    muli r10, r10, SCR_W
    ldwa r11, BDA_CURX
    add r10, r10, r11
    out P_VGA_CURSOR, r10
    ret

; --- vid_setcursor(r1 = x, r2 = y) ----------------------------------------
vid_setcursor:
    stwa BDA_CURX, r1
    stwa BDA_CURY, r2
    call vid_sync
    ret

; --- vid_clearrow(r1 = y, r2 = attribut) ----------------------------------
vid_clearrow:
    push r6
    push r7
    muli r6, r1, SCR_W*2
    li r10, VRAM_TEXT
    add r6, r6, r10
    movi r7, SCR_W
.loop:
    movi r10, 0x20
    stb [r6], r10
    stb [r6+1], r2
    addi r6, r6, 2
    subi r7, r7, 1
    cmpi r7, 0
    jnz .loop
    pop r7
    pop r6
    ret

; --- vid_clear(r1 = attribut): ganzen Bildschirm loeschen -----------------
vid_clear:
    push r6
    push r7
    stwa BDA_ATTR, r1
    li r6, VRAM_TEXT
    li r7, SCR_CELLS
.loop:
    movi r10, 0x20
    stb [r6], r10
    stb [r6+1], r1
    addi r6, r6, 2
    subi r7, r7, 1
    cmpi r7, 0
    jnz .loop
    movi r10, 0
    stwa BDA_CURX, r10
    stwa BDA_CURY, r10
    call vid_sync
    pop r7
    pop r6
    ret

; --- vid_scroll(): alles eine Zeile hoch ----------------------------------
vid_scroll:
    push r6
    push r7
    push r8
    call sb_push                      ; oberste Zeile in die Historie retten
    li r6, VRAM_TEXT
    li r7, VRAM_TEXT + SCR_W*2
    li r8, (SCR_H-1)*SCR_W*2/4
.copy:
    ldw r10, [r7]
    stw [r6], r10
    addi r6, r6, 4
    addi r7, r7, 4
    subi r8, r8, 1
    cmpi r8, 0
    jnz .copy
    movi r1, SCR_H-1
    ldwa r2, BDA_ATTR
    call vid_clearrow
    pop r8
    pop r7
    pop r6
    ret

; --- sb_push(): die oberste Bildschirmzeile in den Ringpuffer legen -------
;     So sammelt der Rechner alles, was oben aus dem Bild gelaufen ist --
;     genau das, was ein Terminal zum Zurueckblaettern braucht.
sb_push:
    push r6
    push r7
    push r8
    ldwa r6, BDA_SBHEAD
    muli r6, r6, SB_LINESIZE
    li r7, SB_BASE
    add r6, r6, r7                    ; r6 = Ziel im Ringpuffer
    li r7, VRAM_TEXT                  ; r7 = Quelle (Bildschirmzeile 0)
    movi r8, SB_LINESIZE/4
.copy:
    ldw r10, [r7]
    stw [r6], r10
    addi r6, r6, 4
    addi r7, r7, 4
    subi r8, r8, 1
    cmpi r8, 0
    jnz .copy

    ldwa r6, BDA_SBHEAD               ; Schreibzeiger weiterdrehen
    addi r6, r6, 1
    cmpi r6, SB_LINES
    jl .nowrap
    movi r6, 0
.nowrap:
    stwa BDA_SBHEAD, r6

    ldwa r6, BDA_SBCOUNT              ; Fuellstand bis zum Maximum zaehlen
    cmpi r6, SB_LINES
    jge .voll
    addi r6, r6, 1
    stwa BDA_SBCOUNT, r6
.voll:
    pop r8
    pop r7
    pop r6
    ret

; --- sb_line(r1 = Zeilennummer, r2 = Zieladresse) -------------------------
;     Zeile 0 ist die aelteste noch gespeicherte Zeile.
sb_line:
    push r6
    push r7
    push r8
    ldwa r6, BDA_SBCOUNT
    cmpi r6, SB_LINES
    jl .direkt
    ldwa r6, BDA_SBHEAD               ; Puffer ist voll -> ab Schreibzeiger
    add r6, r6, r1
    cmpi r6, SB_LINES
    jl .fertig
    subi r6, r6, SB_LINES
    jmp .fertig
.direkt:
    mov r6, r1
.fertig:
    muli r6, r6, SB_LINESIZE
    li r7, SB_BASE
    add r6, r6, r7                    ; r6 = Quelle
    mov r7, r2                        ; r7 = Ziel
    movi r8, SB_LINESIZE/4
.copy:
    ldw r10, [r6]
    stw [r7], r10
    addi r6, r6, 4
    addi r7, r7, 4
    subi r8, r8, 1
    cmpi r8, 0
    jnz .copy
    pop r8
    pop r7
    pop r6
    ret

; --- vid_nextline(): Zeilenumbruch, scrollt bei Bedarf --------------------
vid_nextline:
    push r6
    ldwa r6, BDA_CURY
    addi r6, r6, 1
    cmpi r6, SCR_H
    jl .store
    call vid_scroll
    movi r6, SCR_H-1
.store:
    stwa BDA_CURY, r6
    movi r10, 0
    stwa BDA_CURX, r10
    call vid_sync
    pop r6
    ret

; --- vid_putc(r1 = zeichen, r2 = attribut) --------------------------------
vid_putc:
    push r6
    push r7
    cmpi r1, 10
    jz .newline
    cmpi r1, 13
    jz .cr
    cmpi r1, 8
    jz .backspace
    cmpi r1, 9
    jz .tab

    ldwa r6, BDA_CURY                 ; Zieladresse ausrechnen
    muli r6, r6, SCR_W
    ldwa r7, BDA_CURX
    add r6, r6, r7
    shli r6, r6, 1
    li r10, VRAM_TEXT
    add r6, r6, r10
    stb [r6], r1
    stb [r6+1], r2

    addi r7, r7, 1                    ; Cursor weiter
    cmpi r7, SCR_W
    jl .savex
    call vid_nextline
    jmp .done
.savex:
    stwa BDA_CURX, r7
    call vid_sync
    jmp .done

.newline:
    call vid_nextline
    jmp .done
.cr:
    movi r10, 0
    stwa BDA_CURX, r10
    call vid_sync
    jmp .done
.backspace:
    ldwa r7, BDA_CURX
    cmpi r7, 0
    jz .done
    subi r7, r7, 1
    stwa BDA_CURX, r7
    ldwa r6, BDA_CURY                 ; Zeichen unter dem Cursor loeschen
    muli r6, r6, SCR_W
    add r6, r6, r7
    shli r6, r6, 1
    li r10, VRAM_TEXT
    add r6, r6, r10
    movi r10, 0x20
    stb [r6], r10
    stb [r6+1], r2
    call vid_sync
    jmp .done
.tab:
    ldwa r7, BDA_CURX
    addi r7, r7, 8
    andi r7, r7, 0xFFF8
    cmpi r7, SCR_W
    jl .tabsave
    call vid_nextline
    jmp .done
.tabsave:
    stwa BDA_CURX, r7
    call vid_sync
.done:
    pop r7
    pop r6
    ret

; --- vid_puts(r1 = zeiger auf 0-terminierten Text, r2 = attribut) ---------
vid_puts:
    push r6
    push r7
    mov r6, r1
    mov r7, r2
.loop:
    ldb r1, [r6]
    cmpi r1, 0
    jz .done
    mov r2, r7
    call vid_putc
    addi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

; --- vid_putsat(r1 = x, r2 = y, r3 = text, r4 = attribut) -----------------
vid_putsat:
    push r6
    push r7
    mov r6, r3
    mov r7, r4
    call vid_setcursor
    mov r1, r6
    mov r2, r7
    call vid_puts
    pop r7
    pop r6
    ret

; --- vid_putn(r1 = zahl, r2 = attribut): dezimal, ohne Vorzeichen ---------
vid_putn:
    push r6
    push r7
    push r8
    mov r6, r1
    mov r7, r2
    li r8, BDA_SCRATCH+31
    movi r10, 0
    stb [r8], r10
    cmpi r6, 0
    jnz .loop
    subi r8, r8, 1                    ; Sonderfall: die Zahl 0
    movi r10, 0x30
    stb [r8], r10
    jmp .print
.loop:
    cmpi r6, 0
    jz .print
    movi r11, 10
    umod r10, r6, r11
    udiv r6, r6, r11
    addi r10, r10, 0x30
    subi r8, r8, 1
    stb [r8], r10
    jmp .loop
.print:
    mov r1, r8
    mov r2, r7
    call vid_puts
    pop r8
    pop r7
    pop r6
    ret

; --- vid_puthex(r1 = zahl, r2 = attribut, r3 = stellen) -------------------
vid_puthex:
    push r6
    push r7
    push r8
    mov r6, r1
    mov r7, r2
    mov r8, r3
    subi r8, r8, 1
    shli r8, r8, 2                    ; Bitposition der obersten Stelle
.loop:
    cmpi r8, 0
    jl .done
    movi r11, 15
    shr r10, r6, r8
    and r10, r10, r11
    cmpi r10, 10
    jl .digit
    addi r10, r10, 0x41-10
    jmp .emit
.digit:
    addi r10, r10, 0x30
.emit:
    mov r1, r10
    mov r2, r7
    push r8
    call vid_putc
    pop r8
    subi r8, r8, 4
    jmp .loop
.done:
    pop r8
    pop r7
    pop r6
    ret

; --- vid_putat(r1 = x, r2 = y, r3 = zeichen, r4 = attribut) ---------------
vid_putat:
    push r6
    muli r6, r2, SCR_W
    add r6, r6, r1
    shli r6, r6, 1
    li r10, VRAM_TEXT
    add r6, r6, r10
    stb [r6], r3
    stb [r6+1], r4
    pop r6
    ret

; --- vid_hline(r1 = x, r2 = y, r3 = laenge, r4 = zeichen, r5 = attribut) --
vid_hline:
    push r6
    push r7
    mov r6, r3
    muli r7, r2, SCR_W
    add r7, r7, r1
    shli r7, r7, 1
    li r10, VRAM_TEXT
    add r7, r7, r10
.loop:
    cmpi r6, 0
    jz .done
    stb [r7], r4
    stb [r7+1], r5
    addi r7, r7, 2
    subi r6, r6, 1
    jmp .loop
.done:
    pop r7
    pop r6
    ret

; --- vid_fillrect(r1=x, r2=y, r3=breite, r4=hoehe, r5=attribut) -----------
vid_fillrect:
    push r6
    push r7
    push r8
    push r9
    mov r6, r2                        ; aktuelle Zeile
    mov r7, r4                        ; noch zu fuellende Zeilen
    mov r8, r1                        ; x
    mov r9, r3                        ; Breite
.rows:
    cmpi r7, 0
    jz .done
    mov r1, r8
    mov r2, r6
    mov r3, r9
    movi r4, 0x20
    call vid_hline
    addi r6, r6, 1
    subi r7, r7, 1
    jmp .rows
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret

; --- vid_box(r1=x, r2=y, r3=breite, r4=hoehe, r5=attribut) ----------------
;     Zeichnet einen Doppelrahmen im CP437-Stil.
vid_box:
    push r6
    push r7
    push r8
    push r9
    mov r6, r1
    mov r7, r2
    mov r8, r3
    mov r9, r4

    ; obere Kante
    addi r1, r6, 1
    mov r2, r7
    subi r3, r8, 2
    movi r4, BX_H
    call vid_hline
    mov r1, r6
    mov r2, r7
    movi r3, BX_TL
    mov r4, r5
    call vid_putat
    add r1, r6, r8
    subi r1, r1, 1
    mov r2, r7
    movi r3, BX_TR
    mov r4, r5
    call vid_putat

    ; untere Kante
    addi r1, r6, 1
    add r2, r7, r9
    subi r2, r2, 1
    subi r3, r8, 2
    movi r4, BX_H
    call vid_hline
    mov r1, r6
    add r2, r7, r9
    subi r2, r2, 1
    movi r3, BX_BL
    mov r4, r5
    call vid_putat
    add r1, r6, r8
    subi r1, r1, 1
    add r2, r7, r9
    subi r2, r2, 1
    movi r3, BX_BR
    mov r4, r5
    call vid_putat

    ; Seiten
    addi r10, r7, 1
.sides:
    add r11, r7, r9
    subi r11, r11, 1
    cmp r10, r11
    jge .done
    push r10
    mov r1, r6
    mov r2, r10
    movi r3, BX_V
    mov r4, r5
    call vid_putat
    pop r10
    push r10
    add r1, r6, r8
    subi r1, r1, 1
    mov r2, r10
    movi r3, BX_V
    mov r4, r5
    call vid_putat
    pop r10
    addi r10, r10, 1
    jmp .sides
.done:
    pop r9
    pop r8
    pop r7
    pop r6
    ret
