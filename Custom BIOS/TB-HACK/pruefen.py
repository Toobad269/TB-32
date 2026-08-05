#!/usr/bin/env python3
"""
Prueft TB-HACK auf der echten emulierten Maschine.

    python3 "Custom BIOS/TB-HACK/pruefen.py"

Gebaut wird vorher automatisch. Getestet wird das, was die Werkzeuge
versprechen -- und zwar so, wie ein Mensch es machen wuerde: Setup
aufmachen, im Monitor zu einer Adresse springen, ein Byte aendern und
nachsehen, ob es dort steht; einen Port lesen und beschreiben; einen Sektor
holen, aendern, zurueckschreiben und neu laden; den Startsektor umstellen
und den Rechner damit wirklich starten.

Der Rechner laeuft dabei wirklich -- kein Bauteil ist nachgebaut. Die
Knopfzelle ist eine eigene Datei im Temp-Ordner, die zwischen den Neustarts
stehen bleibt; genau daran haengt der Test, dass ein Startpatch einen
Neustart ueberlebt. Die Platte ist eine Kopie: der Sektoreditor schreibt
wirklich darauf, und das soll die echte Platte nicht treffen.
"""

import os
import shutil
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, ROOT)

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from hardware.machine import Machine
from tools.headless import KEYNAMES, screen_text, test_platte

GRUEN, ROT, WEG = "\033[92m", "\033[91m", "\033[0m"
PASST = 0
FEHLT = 0

# Im Reiter Hack: die Zeilennummern der neun Zeilen
Z_MONITOR, Z_PORTS, Z_CMOS, Z_SEKTOR = 0, 1, 2, 3
Z_BOOTSEK, Z_NOSIG, Z_PATCHON, Z_PATCHES = 4, 5, 6, 7

# Im Reiter Code: die Zeilennummern der sechs Zeilen
Z_CDLOAD, Z_CDLEN, Z_CDORG, Z_CDSHOW, Z_CDADDR = 0, 1, 2, 3, 4

# Der Text, den der Bootsektor beim Starten ausgibt, steht an dieser Stelle
# im Sektor 0 -- und damit ab 0x7C00 an dieser Adresse im Speicher. Ein
# Startpatch darauf ist auf dem Bildschirm sofort zu sehen.
BOOTTEXT = "Boot sector: loading kernel"


def pruefe(was, bedingung, bild=None):
    global PASST, FEHLT
    if bedingung:
        PASST += 1
        print(f"  [{GRUEN}  OK  {WEG}] {was}")
    else:
        FEHLT += 1
        print(f"  [{ROT} FEHLT{WEG}] {was}")
        if bild:
            for zeile in bild.split("\n"):
                if zeile.strip():
                    print(f"           | {zeile}")


class Lauf:
    """Ein laufender TB-32 mit TB-HACK im ROM."""

    def __init__(self, rom, cmos, platte):
        self.m = Machine(ROOT, rom=rom, disk=platte, cmos=cmos)
        self.m.power_on()
        self.dt = 1 / 60
        self.gesehen = ""
        self._n = 0

    def warte(self, sekunden):
        for _ in range(int(sekunden / self.dt)):
            self.m.run_slice(self.dt)
            self._n += 1
            if self._n % 3 == 0:
                self.gesehen += "\n" + self.bild()
            if not self.m.running:
                return

    def eingabe(self, text, warte_danach=0.4):
        for teil in text.split("|"):
            if teil.upper() in KEYNAMES:
                sc, a = KEYNAMES[teil.upper()]
                self.m.keyboard.push(a, sc)
                for _ in range(3):
                    self.m.run_slice(self.dt)
            else:
                for ch in teil:
                    self.m.keyboard.push(ord(ch), 0)
                    for _ in range(3):
                        self.m.run_slice(self.dt)
        self.warte(warte_danach)

    def bild(self):
        return "\n".join(screen_text(self.m))

    def speicher(self, adresse):
        """Ein Byte aus dem Arbeitsspeicher des laufenden Rechners.

        Der Bildschirm taugt nicht fuer alles: was der Bootsektor ausgibt,
        steht nur so lange da, bis der Kernel das Bild loescht -- und das
        sind hier weniger als ein Abtastschritt. Was wirklich im Speicher
        gelandet ist, laesst sich dagegen jederzeit nachsehen."""
        return self.m.bus.read8(adresse)

    def ins_setup(self, versuche=16):
        for _ in range(versuche):
            self.eingabe("DEL", 0.15)
            if "SETUP UTILITY" in self.bild():
                return True
        return False

    def zum_reiter_hack(self):
        """Von Main aus fuenf Reiter nach rechts."""
        for _ in range(5):
            self.eingabe("RIGHT", 0.0)
        self.warte(0.4)

    def zum_reiter_code(self):
        """Von Main aus sechs Reiter nach rechts -- Code ist der letzte."""
        for _ in range(6):
            self.eingabe("RIGHT", 0.0)
        self.warte(0.4)

    def datei_anbieten(self, daten):
        """Was der Wirtsrechner herausgibt, wenn das BIOS nach einer Datei
        fragt. Auf dem echten Geraet macht das der Dateidialog (pc.py setzt
        flash.waehler); hier legen wir die Datei einfach hin."""
        self.m.flash.waehler = lambda: daten

    def zur_zeile(self, n):
        """Sicher auf Zeile n des Reiters Hack.

        Nicht mit UP nach oben tasten: die Auswahl laeuft am Rand um, und der
        Reiter hat genau neun Zeilen -- neunmal UP landet exakt wieder da, wo
        man angefangen hat. Ein Reiterwechsel dagegen setzt die markierte
        Zeile auf 0 zurueck (setup_main, .tab_setzen), und das ist hier der
        einzige verlaessliche Nullpunkt. Links-dann-rechts landet dabei
        wieder auf demselben Reiter, egal auf welchem man steht."""
        self.eingabe("LEFT", 0.0)
        self.eingabe("RIGHT", 0.0)
        for _ in range(n):
            self.eingabe("DOWN", 0.0)
        self.warte(0.3)

    def hex_eingeben(self, wert):
        self.eingabe(wert, 0.2)
        self.eingabe("ENTER", 0.5)


def main():
    print("\nTB-HACK -- die Bastlerwerkzeuge im BIOS\n")

    from importlib import import_module
    sys.path.insert(0, HIER)
    bauen = import_module("bauen")
    rom, daten, summe = bauen.bauen(still=True)
    print(f"  Abbild: {len(daten)} Bytes, Pruefsumme {summe:08X}\n")

    tmp = tempfile.mkdtemp(prefix="tbhack_")
    chip = os.path.join(tmp, "bios.bin")
    shutil.copy(rom, chip)
    cmos = os.path.join(tmp, "cmos.bin")          # bleibt ueber Neustarts hinweg
    platte = os.path.join(tmp, "hd0.img")         # eigene Kopie: hier wird geschrieben
    shutil.copy(test_platte(), platte)

    with open(platte, "rb") as f:
        sektor0 = f.read(512)
    textoffset = sektor0.find(BOOTTEXT.encode())
    if textoffset < 0:
        raise SystemExit("Der Bootsektor sieht anders aus als erwartet.")
    textadresse = 0x7C00 + textoffset

    # === 1. Der Reiter ist da ============================================
    print("--- Der Reiter Hack --------------------------------------------")
    L = Lauf(chip, cmos, platte)
    pruefe("Das Setup oeffnet sich", L.ins_setup(), L.bild())
    L.zum_reiter_hack()
    b = L.bild()
    pruefe("Der Reiter Hack ist da", "Hack" in b, b)
    pruefe("Die vier Werkzeuge stehen darin",
           "Memory Monitor" in b and "Port Console" in b
           and "CMOS Editor" in b and "Load Disk Sector" in b, b)
    pruefe("... und die vier Startsachen darunter",
           "Boot Sector" in b and "Ignore Boot Signature" in b
           and "Apply Boot Patches" in b and "Edit Boot Patches" in b, b)

    # === 2. Der Hex-Monitor ==============================================
    print("\n--- Der Hex-Monitor --------------------------------------------")
    L.zur_zeile(Z_MONITOR)
    L.eingabe("ENTER", 0.6)
    b = L.bild()
    pruefe("Der Monitor geht auf", "TB-HACK MEMORY MONITOR" in b, b)
    pruefe("... mit Adressspalte und Spaltenkopf",
           "ADDRESS" in b and "0A 0B 0C 0D 0E 0F" in b, b)
    pruefe("... und zeigt die Interrupttabelle ab 0", "00000000 " in b, b)

    # Ins ROM springen: dort steht bei 0x0F000004 die Kennung "TBBI".
    L.eingabe("G", 0.4)
    L.hex_eingeben("F000000")
    b = L.bild()
    pruefe("G springt zu einer Adresse", "0F000000 " in b, b)
    pruefe("... und im ROM steht die Kennung TBBI",
           "TBBI" in b, b)

    # Ein Byte im freien Arbeitsspeicher aendern und nachsehen
    L.eingabe("G", 0.4)
    L.hex_eingeben("90000")
    L.eingabe("ENTER", 0.4)             # Byte unter dem Cursor aendern
    L.hex_eingeben("A5")
    b = L.bild()
    pruefe("ENTER aendert das Byte unter dem Cursor",
           "00090000 A5 " in b, b)
    pruefe("... und die Fusszeile zeigt es an",
           "Cursor at 00090000  =  A5" in b, b)

    # Der Cursor laeuft, und am Zeilenende rutscht die Seite mit
    L.eingabe("RIGHT", 0.0)
    L.eingabe("DOWN", 0.3)
    b = L.bild()
    pruefe("Die Pfeiltasten bewegen den Cursor",
           "Cursor at 00090011" in b, b)
    L.eingabe("PGDN", 0.4)
    b = L.bild()
    pruefe("PgDn blaettert eine Seite weiter", "00090100 " in b, b)
    L.eingabe("PGUP", 0.4)
    b = L.bild()
    pruefe("PgUp wieder zurueck", "00090000 " in b, b)

    L.eingabe("ESC", 0.6)
    pruefe("ESC fuehrt zurueck ins Setup", "SETUP UTILITY" in L.bild(), L.bild())

    # === 3. Die Portkonsole ==============================================
    print("\n--- Die Portkonsole --------------------------------------------")
    L.zur_zeile(Z_PORTS)
    L.eingabe("ENTER", 0.6)
    b = L.bild()
    pruefe("Die Portkonsole geht auf", "TB-HACK PORT CONSOLE" in b, b)

    L.eingabe("P", 0.4)                 # 0x00A0 ist das Thermometer
    L.hex_eingeben("A0")
    b = L.bild()
    pruefe("Ein Port laesst sich waehlen", "Port                00A0" in b, b)
    gelesen = None
    for zeile in b.split("\n"):
        if "Reads" in zeile:
            gelesen = int(zeile.split()[-1], 16)
    pruefe(f"... und liefert einen plausiblen Wert ({gelesen} Zehntelgrad)",
           gelesen is not None and 100 < gelesen < 1500, b)

    # 0x00A3 ist die Drosselgrenze: hineinschreiben und zurueckl esen
    L.eingabe("P", 0.4)
    L.hex_eingeben("A3")
    L.eingabe("W", 0.4)
    L.hex_eingeben("5B")                # 91 Grad
    b = L.bild()
    pruefe("W schreibt einen Wert in den Port",
           "Last value written  0000005B" in b, b)
    pruefe("... und der Baustein hat ihn wirklich uebernommen",
           "Reads               0000005B" in b, b)

    L.eingabe("ESC", 0.6)
    pruefe("ESC fuehrt zurueck ins Setup", "SETUP UTILITY" in L.bild(), L.bild())

    # === 4. Der CMOS-Editor ==============================================
    print("\n--- Der CMOS-Editor --------------------------------------------")
    L.zur_zeile(Z_CMOS)
    L.eingabe("ENTER", 0.6)
    b = L.bild()
    pruefe("Der CMOS-Editor geht auf", "TB-HACK CMOS EDITOR" in b, b)
    # Die Datenzeilen tragen hinter der Registernummer drei Leerzeichen --
    # daran sind sie vom Spaltenkopf zu unterscheiden, der genauso mit "00"
    # anfaengt.
    zeilen = [z.strip() for z in b.split("\n")
              if z.strip()[:5] in ("00   ", "10   ", "20   ", "30   ")]
    pruefe("... und zeigt alle vier Zeilen der Knopfzelle",
           len(zeilen) == 4, b)
    pruefe("... darunter das Kennbyte 5A auf Platz 0x2F",
           any(z.startswith("20   ") and z.endswith("5A") for z in zeilen), b)

    # Platz 0x12 ist der POST-Piep. Auf 0 setzen und nachsehen.
    for _ in range(0x12):
        L.eingabe("RIGHT", 0.0)
    L.warte(0.3)
    b = L.bild()
    pruefe("Der Cursor laesst sich auf ein Register stellen",
           "Register 12" in b, b)
    L.eingabe("ENTER", 0.4)
    L.hex_eingeben("00")
    b = L.bild()
    pruefe("ENTER aendert das Byte der Knopfzelle",
           "Register 12  =  00" in b, b)

    L.eingabe("ESC", 0.6)
    pruefe("ESC fuehrt zurueck ins Setup", "SETUP UTILITY" in L.bild(), L.bild())

    # === 5. Der Sektoreditor =============================================
    #     Holen, aendern, zurueckschreiben, neu holen -- die ganze Kette.
    print("\n--- Der Sektoreditor -------------------------------------------")
    L.zur_zeile(Z_SEKTOR)
    L.eingabe("ENTER", 0.5)
    L.hex_eingeben("0")                 # Sektor 0, der Bootsektor
    b = L.bild()
    pruefe("Ein Sektor laesst sich holen", "TB-HACK MEMORY MONITOR" in b, b)
    pruefe("... er landet im Puffer bei 4 MB", "00400000 " in b, b)
    pruefe("... die Fusszeile nennt die Sektornummer",
           "Buffer holds sector 0000" in b, b)

    # Ans Ende blaettern: dort stehen die 55 AA
    L.eingabe("G", 0.4)
    L.hex_eingeben("4001F0")
    b = L.bild()
    endzeile = next((z for z in b.split("\n") if "004001F0" in z), "")
    pruefe("Am Sektorende stehen die 55 AA", "55 AA" in endzeile, b)

    # Ein Byte im Polster aendern und zurueckschreiben
    L.eingabe("G", 0.4)
    L.hex_eingeben("4001F0")
    L.eingabe("ENTER", 0.4)
    L.hex_eingeben("7E")
    L.eingabe("W", 0.5)                 # Puffer zurueck auf die Platte
    L.eingabe("ENTER", 0.8)             # die Nachfrage bestaetigen
    pruefe("Der Puffer laesst sich zurueckschreiben",
           "Sector written" in L.gesehen, L.bild())

    with open(platte, "rb") as f:
        neu = f.read(512)
    pruefe("Das Byte steht wirklich auf der Platte", neu[0x1F0] == 0x7E)
    pruefe("... und der Rest des Sektors ist unangetastet",
           neu[:0x1F0] == sektor0[:0x1F0] and neu[0x1F1:] == sektor0[0x1F1:])

    # Frisch von der Platte holen -- steht die Aenderung noch da?
    L.eingabe("S", 0.5)
    L.hex_eingeben("0")
    L.eingabe("G", 0.4)
    L.hex_eingeben("4001F0")
    b = L.bild()
    pruefe("Neu geladen ist die Aenderung noch da",
           "004001F0 7E " in b, b)
    L.eingabe("ESC", 0.6)

    # === 6. Startpatches ==================================================
    print("\n--- Startpatches -----------------------------------------------")
    L.zur_zeile(Z_PATCHES)
    L.eingabe("ENTER", 0.6)
    b = L.bild()
    pruefe("Der Patcheditor geht auf", "TB-HACK BOOT PATCHES" in b, b)
    pruefe("... mit zwei Plaetzen zu je Adresse und Wert",
           "Patch 1  Address" in b and "Patch 2  Value" in b, b)

    L.eingabe("ENTER", 0.4)             # Patch 1, Adresse
    L.hex_eingeben(f"{textadresse:X}")
    L.eingabe("DOWN", 0.2)
    L.eingabe("ENTER", 0.4)             # Patch 1, Wert
    L.hex_eingeben("58")                # 'X'
    b = L.bild()
    pruefe("Adresse und Wert stehen im Editor",
           f"{textadresse:08X}" in b and "58" in b, b)
    L.eingabe("ESC", 0.6)

    L.zur_zeile(Z_PATCHON)
    L.eingabe("ENTER", 0.5)             # Anwenden einschalten
    pruefe("Anwenden laesst sich einschalten",
           "Apply Boot Patches" in L.bild() and "Enabled" in L.bild(), L.bild())
    L.eingabe("F10", 1.5)               # sichern und verlassen

    print("\n--- Neustart mit dem Patch -------------------------------------")
    P = Lauf(chip, cmos, platte)
    P.warte(8.0)
    pruefe("Das BIOS meldet den angewandten Patch",
           "Boot patches applied: 1" in P.gesehen, P.bild())
    pruefe("Das gepatchte Byte steht wirklich im Bootsektor",
           P.speicher(textadresse) == 0x58)
    pruefe("... und der Text daneben ist unangetastet",
           bytes(P.speicher(textadresse + i) for i in range(1, 12))
           == b"oot sector:")
    pruefe("Der Rechner startet trotzdem durch",
           "TOOBAD-OS" in P.gesehen, P.bild())

    # === 7. Der Startsektor ==============================================
    #     Auf einen leeren Sektor umstellen: ohne Signatur bleibt das BIOS
    #     stehen, mit abgeschalteter Pruefung springt es hinein.
    print("\n--- Startsektor und Signatur -----------------------------------")
    S = Lauf(chip, cmos, platte)
    S.ins_setup()
    S.zum_reiter_hack()
    S.zur_zeile(Z_PATCHON)
    S.eingabe("ENTER", 0.4)             # Patches wieder aus
    S.zur_zeile(Z_BOOTSEK)
    S.eingabe("ENTER", 0.5)
    S.hex_eingeben("2000")              # ein leerer Sektor weit hinten
    # setup_message haelt das Bild eine gute halbe Sekunde an, bevor die
    # Liste wieder gezeichnet wird -- so lange steht nur die Meldung da.
    S.warte(1.2)
    b = S.bild()
    pruefe("Der Startsektor laesst sich einstellen",
           "Boot Sector" in b and "2000" in b, b)
    S.eingabe("F10", 1.5)

    # Geprueft wird auf den Panikschirm, nicht auf die Zeile "no boot
    # signature": die steht nur den Bruchteil eines Abtastschritts lang da,
    # weil panic sofort danach das Bild loescht.
    N = Lauf(chip, cmos, platte)
    N.warte(8.0)
    pruefe("Ohne Signatur bleibt das BIOS stehen",
           "No bootable device found" in N.gesehen, N.bild())

    N2 = Lauf(chip, cmos, platte)
    N2.ins_setup()
    N2.zum_reiter_hack()
    N2.zur_zeile(Z_NOSIG)
    N2.eingabe("ENTER", 0.4)            # Signaturpruefung abschalten
    pruefe("Die Signaturpruefung laesst sich abschalten",
           "Ignore Boot Signature" in N2.bild() and "Enabled" in N2.bild(),
           N2.bild())
    N2.eingabe("F10", 1.5)

    # Kurz hinsehen: hinter dem leeren Sektor stehen lauter Nullen, und die
    # sind auf dem TB-32 gueltige Leerbefehle. Der Rechner laeuft also
    # weiter, kommt aber nie irgendwo an -- lange zuschauen lohnt nicht.
    N3 = Lauf(chip, cmos, platte)
    N3.warte(2.5)
    pruefe("Danach springt es auch ohne Signatur hinein",
           "No bootable device found" not in N3.gesehen, N3.bild())

    # === 8. F5 ist die Notbremse =========================================
    #     Der Startsektor steht noch auf 0x2000. Ohne einen Weg zurueck waere
    #     der Rechner damit endgueltig verstellt.
    print("\n--- F5 holt den Rechner zurueck --------------------------------")
    F = Lauf(chip, cmos, platte)
    F.ins_setup()
    F.zum_reiter_hack()
    b = F.bild()
    pruefe("Der verstellte Startsektor steht noch da", "2000" in b, b)
    F.eingabe("F5", 0.8)
    b = F.bild()
    pruefe("F5 setzt den Startsektor auf 0 zurueck",
           "Boot Sector" in b and "0000" in b and "2000" not in b, b)
    pruefe("... und schaltet Signatur und Patches wieder scharf",
           b.count("Disabled") >= 2, b)
    F.eingabe("F10", 1.5)

    Z = Lauf(chip, cmos, platte)
    Z.warte(8.0)
    pruefe("Danach startet der Rechner wieder normal",
           "TOOBAD-OS" in Z.gesehen
           and "No bootable device found" not in Z.gesehen, Z.bild())
    pruefe("... und ohne Patch steht der Bootsektor wieder im Original",
           Z.speicher(textadresse) == ord("B"))

    # === 9. Der Rueckuebersetzer =========================================
    #     Zuerst am ROM, weil sich das gegenpruefen laesst: die Sprungziele
    #     muessen genau die Adressen sein, die in der Symboltabelle stehen.
    print("\n--- Der Rueckuebersetzer am eigenen ROM ------------------------")
    symbole = {}
    with open(os.path.join(HIER, "TB-HACK.sym")) as f:
        for zeile in f:
            adr, name = zeile.split()
            symbole[name] = int(adr, 16)

    C = Lauf(chip, cmos, platte)
    C.ins_setup()
    C.zum_reiter_code()
    b = C.bild()
    pruefe("Der Reiter Code ist da", "Load Program from Host File" in b, b)
    pruefe("... und meldet, dass noch nichts geholt wurde",
           b.count("nothing loaded") == 2, b)

    C.zur_zeile(Z_CDADDR)
    C.eingabe("ENTER", 0.5)
    C.hex_eingeben(f"{symbole['bios_start']:X}")
    b = C.bild()
    pruefe("Das Listing geht auf", "TB-HACK CODE VIEWER" in b, b)
    pruefe("li sp, BIOS_STACK wird zu movi/movh",
           "movi    sp, 0xFFF0" in b and "movh    sp, 0x0007" in b, b)
    pruefe("cli und sti stehen ohne Argumente da",
           " cli" in b and " sti" in b, b)
    pruefe("out P_TIMER_HZ, r10 kommt mit Port und Register zurueck",
           "out     0x0010, r10" in b, b)
    for name in ("bda_init", "ivt_init", "post", "boot"):
        pruefe(f"call {name} zeigt auf {symbole[name]:08X}",
               f"call    0x{symbole[name]:08X}" in b, b)
    C.eingabe("ESC", 0.6)

    # === 10. Eine .TBX vom Wirtsrechner ==================================
    #     Ein gebautes Programm mit Kopf, dessen Befehle wir kennen -- so
    #     laesst sich jede Formatart einzeln nachrechnen.
    print("\n--- Eine .TBX vom Wirtsrechner ---------------------------------")
    from hardware.isa import encode_i, encode_r, encode_j, encode_c

    CODE = 0x00500008                    # CD_PUFFER + 8 Byte Kopf
    befehle = [
        encode_i(0x11, 1, 0, 0x1234),    # movi r1, 0x1234
        encode_r(0x20, 2, 3, 4),         # add  r2, r3, r4
        encode_i(0x1B, 5, 14, 0xFFFC),   # ldw  r5, [fp-4]
        encode_i(0x1E, 6, 15, 8),        # stw  [sp+8], r6
        encode_i(0x62, 7, 0, 0x0050),    # out  0x0050, r7
        encode_i(0x60, 8, 0, 0x00A0),    # in   r8, 0x00A0
        encode_i(0x64, 0, 0, 0x40),      # int  0x40
        encode_j(0x50, 2, -2),           # jnz  zurueck
        encode_c(0x42, 4),               # call vorwaerts
        encode_r(0x05),                  # ret
        0xFFFFFFFF,                      # kein Befehl -> .word
    ]
    tbx = b"PXBT" + (0x00200000).to_bytes(4, "little")
    tbx += b"".join(w.to_bytes(4, "little") for w in befehle)

    T = Lauf(chip, cmos, platte)
    T.datei_anbieten(tbx)
    T.ins_setup()
    T.zum_reiter_code()
    T.zur_zeile(Z_CDLOAD)
    T.eingabe("ENTER", 1.2)
    b = T.bild()
    pruefe("Die Datei kommt an", f"{len(tbx)} bytes" in b, b)
    pruefe("Der TBX-Kopf wird gelesen", "00200000" in b, b)

    T.zur_zeile(Z_CDSHOW)
    T.eingabe("ENTER", 0.8)
    b = T.bild()
    pruefe("Show Code zeigt das geholte Programm",
           "TB-HACK CODE VIEWER" in b and "TBX program" in b, b)
    erwartet = [
        ("movi    r1, 0x1234",       "movi mit Direktwert"),
        ("add     r2, r3, r4",       "add mit drei Registern"),
        ("ldw     r5, [fp-4]",       "ldw mit negativem Versatz und fp statt r14"),
        ("stw     [sp+8], r6",       "stw dreht Ziel und Quelle um"),
        ("out     0x0050, r7",       "out nennt erst den Port"),
        ("in      r8, 0x00A0",       "in nennt erst das Register"),
        ("int     0x40",             "int mit Nummer"),
        (f"jnz     0x{CODE + 7 * 4 - 8:08X}", "jnz rechnet rueckwaerts richtig"),
        (f"call    0x{CODE + 8 * 4 + 16:08X}", "call rechnet vorwaerts richtig"),
        (" ret",                     "ret ohne Argumente"),
        (".word   0xFFFFFFFF",       "kein Befehl wird als Wort gezeigt, nicht verschluckt"),
    ]
    for text, was in erwartet:
        pruefe(was, text in b, b)
    T.eingabe("ESC", 0.6)

    # Eine Datei ohne Kopf ist roher Code -- und wird auch so genannt
    R = Lauf(chip, cmos, platte)
    R.datei_anbieten(b"".join(w.to_bytes(4, "little") for w in befehle))
    R.ins_setup()
    R.zum_reiter_code()
    R.zur_zeile(Z_CDLOAD)
    R.eingabe("ENTER", 1.2)
    b = R.bild()
    pruefe("Eine Datei ohne Kennung heisst 'no TBX header'",
           "no TBX header" in b, b)
    R.zur_zeile(Z_CDSHOW)
    R.eingabe("ENTER", 0.8)
    pruefe("... und wird trotzdem ab dem ersten Byte gezeigt",
           "movi    r1, 0x1234" in R.bild(), R.bild())
    R.eingabe("ESC", 0.6)

    # Zu gross: mehr als ein Programm sein darf
    G = Lauf(chip, cmos, platte)
    G.datei_anbieten(b"\x00" * (0x80000 + 4))
    G.ins_setup()
    G.zum_reiter_code()
    G.zur_zeile(Z_CDLOAD)
    G.eingabe("ENTER", 1.2)
    pruefe("Eine zu grosse Datei wird abgelehnt",
           "larger than 512 KB" in G.gesehen, G.bild())

    # === 11. Der Chip nennt sich beim Namen ==============================
    print("\n--- Das Abbild selbst ------------------------------------------")
    pruefe("Der POST nennt TB-HACK", "TB-HACK BIOS" in Z.gesehen, Z.bild())
    with open(rom, "rb") as f:
        kopf = f.read(0x30)
    pruefe("Der Kopf traegt die Kennung TBBI", kopf[4:8] == b"TBBI")
    pruefe("... und den Namen, den das Mainboard zeigt",
           kopf[0x10:0x30].split(b"\0")[0] == b"TB-HACK BIOS v2.5.2")

    shutil.rmtree(tmp, ignore_errors=True)

    farbe = GRUEN if FEHLT == 0 else ROT
    print(f"\n{farbe}{PASST}/{PASST + FEHLT} Pruefungen bestanden{WEG}\n")
    return 1 if FEHLT else 0


if __name__ == "__main__":
    sys.exit(main())
