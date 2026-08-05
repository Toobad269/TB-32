; ===========================================================================
;  TB-32 Systemkonstanten -- wird von BIOS, Bootloader und Kernel eingebunden
; ===========================================================================

; --- Speicherkarte ---------------------------------------------------------
.equ IVT_BASE,      0x00000000      ; 256 Interrupt-Vektoren a 4 Byte
.equ BDA_BASE,      0x00000400      ; BIOS-Datenbereich
.equ BOOT_ADDR,     0x00007C00      ; hierhin kommt der Bootsektor
.equ BIOS_STACK,    0x0007FFF0      ; Stack der Firmware
.equ KERNEL_ADDR,   0x00010000      ; hierhin ladt der Bootloader den Kernel
.equ VRAM_TEXT,     0x02000000
.equ VRAM_GFX,      0x02100000
.equ ROM_BASE,      0x0F000000

; --- BIOS-Datenbereich (Variablen der Firmware) ----------------------------
.equ BDA_CURX,      0x00000400
.equ BDA_CURY,      0x00000404
.equ BDA_ATTR,      0x00000408
.equ BDA_TICKS,     0x0000040C
.equ BDA_BOOTDEV,   0x00000410
.equ BDA_KEYHEAD,   0x00000414
.equ BDA_KEYTAIL,   0x00000418
.equ BDA_KEYBUF,    0x00000420      ; 32 Eintrage a 4 Byte
.equ BDA_MEMKB,     0x000004A0
.equ BDA_DISKSEC,   0x000004A4
; --- Was das BIOS dem Betriebssystem ueber den Rechner sagt ---------------
; Bei echten PCs heisst das SMBIOS: die Firmware legt eine Tabelle in den
; Speicher, das System liest sie aus.
.equ BDA_FIRMA,     0x00000500      ; 32 Byte Eigentuemer-Eintrag
.equ BDA_POLICY,    0x00000524      ; Schalterwort, Belegung wie CM_POLICY
.equ BDA_BLOCK,     0x00000528      ; gesperrte Programme im Klartext,
                                    ; 16 Plaetze a 16 Byte, leerer Name = Ende
.equ BDA_INVENT,    0x00000628      ; Inventar: Seriennummer (16), Starts (4),
                                    ; Betriebsminuten (4)
.equ BDA_BLOCKN,    16              ; so viele Plaetze
.equ BDA_BLOCKLEN,  256             ; 16 * 16
.equ BDA_INVLEN,    24
; Das Pflichtenheft hatte acht Plaetze bis 0x5A8 vorgesehen, weil das Setup
; ab 0x600 anfaengt. Sechzehn Programme brauchen aber sechzehn Plaetze, sonst
; koennte man mehr sperren, als beim System ankommt -- eine Sperre, die
; stillschweigend danebengeht, ist schlimmer als keine. Also den Rat aus dem
; Pflichtenheft befolgt und den Arbeitsplatz der Firmware nach 0x700
; verschoben. Von 0x500 bis 0x6FF gehoert jetzt alles dem, was das BIOS dem
; Betriebssystem erzaehlt; darueber faengt sein eigenes Kratzpapier an.

; --- Der zweite batteriegepufferte Speicher (NVRAM, 256 Byte) -------------
; Die Knopfzelle an der Uhr ist voll. Was mehr Platz braucht -- Firmentext,
; Ereignisspeicher, Inventar -- liegt hier.
.equ P_NVRAM_IDX,   0x0072
.equ P_NVRAM_DATA,  0x0073

.equ NV_MAGIC,      0x00            ; 0x4E = eingerichtet
.equ NV_FIRMA,      0x01            ; 32 Byte Firmentext
.equ NV_SERIAL,     0x21            ; 16 Byte Seriennummer
.equ NV_BOOTS,      0x31            ; 4 Byte: Anzahl Starts
.equ NV_MINUTES,    0x35            ; 4 Byte: Betriebsminuten
.equ NV_LOGHEAD,    0x39            ; naechster Platz im Ringpuffer (0..7)
.equ NV_LOG,        0x40            ; 8 Eintraege a 8 Byte = 64 Byte
.equ NV_LOGLEN,     8
.equ NV_LOGCNT,     8
.equ NV_NAME_MAGIC, 0x4E

; Ereignisarten fuer den Ereignisspeicher
.equ EV_BADPW,      1               ; falsches Passwort
.equ EV_CLEARED,    2               ; CMOS war geleert
.equ EV_FLASH,      3               ; BIOS geflasht
.equ EV_SECURE,     4               ; Secure Boot hat angehalten
.equ EV_DEFAULTS,   5               ; Standardwerte geladen
.equ EV_LOCKOUT,    6               ; nach zu vielen Fehlversuchen gesperrt
.equ EV_BOOT,       7               ; normal gestartet

; --- Flash-Baustein: die Sperre -------------------------------------------
.equ FLASH_LOCK,    10              ; Chip sperren, loest nur der Neustart
.equ FLASH_ISLOCK,  11              ; nachfragen: 1 = gesperrt

.equ BDA_SCRATCH,   0x000004B0      ; 64 Byte Kratzpapier fur Zahlenausgabe
; Eigener Platz fuer das Setup. NICHT in BDA_SCRATCH legen -- dort formatiert
; vid_putn seine Ziffern hin, und dann waere der gemerkte Reiter nach der
; ersten ausgegebenen Zahl weg.
.equ SETUP_TAB,     0x00000700      ; aktiver Reiter
.equ SETUP_ROW,     0x00000704      ; markierte Zeile
.equ SETUP_SAVE,    0x00000710      ; 16 Byte CMOS-Sicherung fuer ESC
; TB-LOCK: die beiden Eingabepuffer fuer das Passwort. Sie liegen hinter
; SETUP_SAVE (0x610..0x61F) und damit weiterhin auf dem Kratzpapier, das
; allein der Firmware gehoert -- der Kernel faengt erst bei 0x10000 an.
.equ PW_BUF1,       0x00000720      ; 32 Byte: das eingetippte Passwort
.equ PW_BUF2,       0x00000740      ; 32 Byte: die Wiederholung
; Zustand der gemeinsamen Eingaberoutine. Drei Speicherworte statt dreier
; Register: die Tippschleife braucht schon vier, und der TB-32 hat sechzehn.
.equ TE_MAX,        0x00000760      ; hoechstens so viele Zeichen
.equ TE_SICHTBAR,   0x00000764      ; 0 = Sterne, 1 = Text zeigen
.equ TE_BUF,        0x00000768      ; wohin getippt wird
.equ TXT_BUF,       0x00000770      ; 32 Byte Arbeitsplatz fuer den Firmentext
.equ NV_WAR_DA,     0x00000790      ; war das NVRAM schon eingerichtet?
.equ INTRUSION_FLAG,0x00000794      ; wurde die Knopfzelle gezogen? (nur dieser Start)
.equ BDA_SBHEAD,    0x000004A8      ; Ringpuffer der Bildschirmhistorie
.equ BDA_SBCOUNT,   0x000004AC

; --- Bildschirmhistorie ("scrollback") -------------------------------------
.equ SB_BASE,       0x00100000      ; 512 Zeilen a 160 Byte = 80 KB
.equ SB_LINES,      512
.equ SB_LINESIZE,   160

; --- Bildschirm ------------------------------------------------------------
.equ SCR_W,         80
.equ SCR_H,         25
.equ SCR_CELLS,     2000

; --- Farbattribute (Hintergrund<<4 | Vordergrund) --------------------------
.equ C_BLACK,       0x00
.equ C_BLUE,        0x01
.equ C_GREEN,       0x02
.equ C_CYAN,        0x03
.equ C_RED,         0x04
.equ C_MAGENTA,     0x05
.equ C_BROWN,       0x06
.equ C_GRAY,        0x07
.equ C_DGRAY,       0x08
.equ C_LBLUE,       0x09
.equ C_LGREEN,      0x0A
.equ C_LCYAN,       0x0B
.equ C_LRED,        0x0C
.equ C_LMAGENTA,    0x0D
.equ C_YELLOW,      0x0E
.equ C_WHITE,       0x0F

.equ ATTR_NORMAL,   0x07
.equ ATTR_BRIGHT,   0x0F
.equ ATTR_TITLE,    0x1F            ; weiss auf blau
.equ ATTR_HILIGHT,  0x70            ; schwarz auf grau (Auswahlbalken)
.equ ATTR_OK,       0x0A
.equ ATTR_ERR,      0x0C

; --- I/O-Ports -------------------------------------------------------------
.equ P_PIC_ACK,     0x0000
.equ P_PIC_MASK,    0x0001
.equ P_TIMER_HZ,    0x0010
.equ P_TIMER_TICKS, 0x0011
.equ P_KBD_DATA,    0x0020
.equ P_KBD_STATUS,  0x0021
.equ P_DISK_LBA,    0x0030
.equ P_DISK_COUNT,  0x0031
.equ P_DISK_ADDR,   0x0032
.equ P_DISK_CMD,    0x0033
.equ P_DISK_STATUS, 0x0034
.equ P_DISK_SIZE,   0x0035
.equ P_VGA_MODE,    0x0040
.equ P_VGA_CURSOR,  0x0041
.equ P_VGA_PALIDX,  0x0042
.equ P_VGA_PALVAL,  0x0043
; --- 2D-Beschleuniger (Blitter) -------------------------------------------
.equ P_BLT_X,       0x0044
.equ P_BLT_Y,       0x0045
.equ P_BLT_W,       0x0046
.equ P_BLT_H,       0x0047
.equ P_BLT_COL,     0x0048
.equ P_BLT_CMD,     0x0049
.equ P_BLT_CHR,     0x004A
.equ P_BLT_SRC,     0x004B
.equ P_BLT_BG,      0x004C
.equ P_SPK_FREQ,    0x0050
.equ P_SPK_ON,      0x0051
.equ P_MOUSE_X,     0x0060
.equ P_MOUSE_Y,     0x0061
.equ P_MOUSE_BTN,   0x0062
.equ P_CMOS_IDX,    0x0070
.equ P_CMOS_DATA,   0x0071
.equ P_DEBUG,       0x0080
.equ P_POWER,       0x0090
; --- Kuehlung und Temperatur (siehe hardware/devices.py, class Thermal) ---
.equ P_TEMP,        0x00A0            ; aktuelle Temperatur in Zehntelgrad
.equ P_FAN,         0x00A1            ; Luefterdrehzahl 0..100 Prozent
.equ P_THROTTLE,    0x00A2            ; aktuelle Drosselung in Prozent
.equ P_TEMPLIMIT,   0x00A3            ; ab wann gedrosselt wird, in Grad
.equ P_FANMODE,     0x00A4            ; 0 automatisch, 1 leise, 2 volle Drehzahl
.equ P_TEMPMAX,     0x00A5            ; hoechste je gemessene Temperatur
; --- ROM-Baustein neu beschreiben (siehe Doku 16) -------------------------
.equ P_FLASH_CMD,   0x00B0            ; 1 Datei holen, 2 in den RAM, 3 brennen,
                                      ; 4 Sicherung zurueckspielen
.equ P_FLASH_SIZE,  0x00B1            ; lesen: Bytes im Puffer (0 = keine)
.equ P_FLASH_ADDR,  0x00B2            ; Zieladresse fuer Befehl 2

; --- Kopf eines BIOS-Abbildes ---------------------------------------------
;     0x00 Sprung ueber den Kopf
;     0x04 "TBBI"
;     0x08 Laenge in Byte
;     0x0C Pruefsumme
;     0x10 Name, 32 Byte, mit Nullbyte abgeschlossen
;     0x30 hier faengt der Code an
;
;  Der Name ist nicht Schmuck: das MAINBOARD liest ihn und schreibt ihn beim
;  Einschalten in die Bildmitte, bevor die CPU ueberhaupt laeuft. Jedes BIOS
;  legt so seinen eigenen Namen fest, und das Startbild sieht trotzdem
;  ueberall gleich aus.
.equ BIOSHDR_MAGIC, 0x04
.equ BIOSHDR_LEN,   0x08
.equ BIOSHDR_SUM,   0x0C
.equ BIOSHDR_NAME,  0x10
.equ BIOSHDR_ENDE,  0x30
.equ BIOS_MAGIC,    0x49424254        ; "TBBI" als Zahl

; --- Interruptnummern ------------------------------------------------------
.equ INT_DIV0,      0x00
.equ INT_BADOP,     0x06
.equ IRQ_TIMER,     0x08
.equ IRQ_KBD,       0x09
.equ IRQ_MOUSE,     0x0C
.equ INT_VIDEO,     0x10
.equ INT_DISK,      0x13
.equ INT_KBD,       0x16
.equ INT_TIME,      0x1A
.equ INT_SYS,       0x40

; --- CMOS-Register ---------------------------------------------------------
.equ CM_SEC,        0x00
.equ CM_MIN,        0x02
.equ CM_HOUR,       0x04
.equ CM_WDAY,       0x06
.equ CM_DAY,        0x07
.equ CM_MONTH,      0x08
.equ CM_YEAR,       0x09
.equ CM_BOOTDEV,    0x10
.equ CM_QUICKBOOT,  0x11
.equ CM_BEEP,       0x12
.equ CM_CPUSPEED,   0x13
.equ CM_RAMSIZE,    0x14
.equ CM_VERBOSE,    0x15
.equ CM_FANMODE,    0x16            ; 0 automatisch, 1 leise, 2 volle Drehzahl
.equ CM_TEMPLIMIT,  0x17            ; Drosselgrenze in Grad (60..100, Fuenferschritte)
.equ CM_SECURE,     0x18            ; Secure Boot: 0 aus, 1 an
.equ CM_SUM0,       0x19            ; gemerkte Pruefsumme des Startabbildes,
.equ CM_SUM1,       0x1A            ; vier Byte, niedrigstes zuerst
.equ CM_SUM2,       0x1B
.equ CM_SUM3,       0x1C
.equ CM_BOOTMODE,   0x1D            ; 0 = Schreibtisch, 1 = Textkonsole
; TB-LOCK: das Supervisor-Passwort. Ab 0x20, nicht darunter -- setup_backup
; sichert 0x10..0x1F fuer ESC, ein Passwort dort waere beim Verlassen ohne
; Speichern wieder weg. Unter 0x2E bleibt es, damit die Knopfzelle es in ihre
; eigene Pruefsumme einrechnet.
.equ CM_PWFLAG,     0x20            ; 0 = keins gesetzt, 1 = gesetzt
.equ CM_PWSUM0,     0x21            ; Pruefsumme des Passworts, vier Byte,
.equ CM_PWSUM1,     0x22            ; niedrigstes zuerst
.equ CM_PWSUM2,     0x23
.equ CM_PWSUM3,     0x24

; --- COMPANY-OS: Richtlinien, Sperrliste, zweites Passwort ----------------
; Elf Plaetze sind unter der Pruefsumme frei (0x1E-0x1F und 0x25-0x2D).
; Neun davon gehen hier drauf, zwei bleiben Reserve. Was oberhalb von 0x2D
; laege, fiele aus der Pruefsumme heraus -- es ueberlebte zwar den Neustart,
; aber ein leergeraeumtes CMOS fiele niemandem mehr auf.
.equ CM_INTRUSION,  0x1E            ; 0xC7 = von COMPANY-OS eingerichtet
.equ CM_POLICY,     0x25            ; Schalterwort, Bits siehe unten
.equ CM_BLOCK0,     0x26            ; gesperrte Programme, Bits 0-7
.equ CM_BLOCK1,     0x27            ; ... Bits 8-15
.equ CM_PWUFLAG,    0x28            ; Power-On-Passwort: 0 keins, 1 gesetzt
.equ CM_PWUSUM0,    0x29            ; dessen Pruefsumme, vier Byte
.equ CM_PWUSUM1,    0x2A
.equ CM_PWUSUM2,    0x2B
.equ CM_PWUSUM3,    0x2C
.equ CM_PWTRIES,    0x2D            ; Fehlversuche, ueberlebt den Reset

.equ CM_SAVE,       0x3F            ; Schreiben loest "in Datei sichern" aus

; Bits im Schalterwort CM_POLICY. Dieselbe Belegung liegt spaeter unter
; BDA_POLICY im Speicher, damit das Betriebssystem sie ohne Umrechnen liest.
.equ POL_OWNER,     0x01            ; Bit 0: Eigentuemer-Eintrag anzeigen
.equ POL_NOCC,      0x02            ; Bit 1: kein Compiler
.equ POL_NONET,     0x04            ; Bit 2: kein Netz
.equ POL_NEEDPW,    0x08            ; Bit 3: Anmeldung verlangt ein Passwort
.equ POL_INTDISK,   0x10            ; Bit 4: nur von der eigenen Platte starten

.equ PW_INTRUSION_MAGIC, 0xC7
.equ PW_MAXTRIES,   3               ; Fehlversuche, dann bleibt der Rechner stehen

; --- Tastatur-Scancodes ----------------------------------------------------
.equ K_ESC,         0x01
.equ K_BACKSPACE,   0x0E
.equ K_TAB,         0x0F
.equ K_ENTER,       0x1C
.equ K_F1,          0x3B
.equ K_F2,          0x3C
.equ K_F5,          0x3F
.equ K_F10,         0x44
.equ K_HOME,        0x47
.equ K_UP,          0x48
.equ K_PGUP,        0x49
.equ K_LEFT,        0x4B
.equ K_RIGHT,       0x4D
.equ K_END,         0x4F
.equ K_DOWN,        0x50
.equ K_PGDN,        0x51
.equ K_INS,         0x52
.equ K_DEL,         0x53

; --- CP437-Rahmenzeichen ---------------------------------------------------
.equ BX_TL,         0xC9           ; doppelte Linie: Ecke oben links
.equ BX_TR,         0xBB
.equ BX_BL,         0xC8
.equ BX_BR,         0xBC
.equ BX_H,          0xCD
.equ BX_V,          0xBA
.equ BX_SH,         0xB0           ; Schattierung
.equ BX_BLOCK,      0xDB
