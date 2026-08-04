; ===========================================================================
;  Startcode für eigenständige TOOBAD-OS-Programme (.TBX)
;
;  So eine Datei liegt auf der Platte, wird vom Kernel an PROG_ADDR geladen
;  und ab dem ersten Byte gestartet. Sie kennt den Kernel nicht -- sie spricht
;  nur über INT 0x40 mit ihm.
; ===========================================================================

.org 0x00200000

prog_entry:
    call main
    movi r0, 4                         ; Systemaufruf 4 = beenden.
    int 0x40                           ; Im Hintergrund kehrt das nie zurueck --
    ret                                ; im Vordergrund geht es zur Shell zurueck.

; sc(nummer, a1, a2, a3, a4) -- der Weg zum Betriebssystem
sc:
    mov r10, r1
    mov r1, r2
    mov r2, r3
    mov r3, r4
    mov r4, r5
    mov r0, r10
    int 0x40
    ret

; ---------------------------------------------------------------------------
;  Hardware ohne Umweg
;
;  Ports sind auf dem TB-32 nicht geschuetzt -- ein Programm darf sie selbst
;  bedienen. Der Weg ueber int 0x40 kostet dagegen das Sichern von 15
;  Registern und eine lange Fallunterscheidung im Kernel. Bei vierzig
;  Malbefehlen je Bild ist das der Unterschied zwischen ruckeln und laufen.
;
;  CC auf dem Geraet erzeugt fuer portout/portin dieselben zwei Befehle
;  direkt an der Aufrufstelle -- beide Compiler kommen also aufs Gleiche.
; ---------------------------------------------------------------------------

; portout(port, wert) -- outr schreibt rd an den Port in ra, also r2 nach r1
portout:
    outr r2, r1
    ret

; portin(port)
portin:
    inr r0, r1
    ret
