; Ein Programm direkt auf dem TB-32 geschrieben und uebersetzt.
; Systemaufrufe: r0 = Funktion, dann int 0x40
;   1 = Text ausgeben (r1 = Adresse, r2 = Farbe)
;   2 = auf Taste warten
;   3 = Bildschirm loeschen (r1 = Farbe)
;   4 = Programm beenden

start:
    movi r0, 3
    movi r1, 7
    int 0x40

    li r1, gruss
    movi r2, 10
    movi r0, 1
    int 0x40

    li r1, zeile2
    movi r2, 15
    movi r0, 1
    int 0x40

    movi r0, 2                ; auf Taste warten
    int 0x40

    movi r0, 4                ; beenden
    int 0x40
    ret

gruss:
    .db "Hello from a program written ON the TB-32!", 10, 0
zeile2:
    .db "Assembled by ASM.TBX, which runs on this machine too.", 10, 0
