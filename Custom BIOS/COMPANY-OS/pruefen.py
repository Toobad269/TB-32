#!/usr/bin/env python3
"""
Prueft COMPANY-OS auf der echten emulierten Maschine.

    python3 "Custom BIOS/COMPANY-OS/pruefen.py"

Aufgebaut wie die Pruefung von TB-LOCK: der Rechner laeuft wirklich, mit
eigener Knopfzelle und eigenem NVRAM im Temp-Ordner, und der Test drueckt
dieselben Tasten wie ein Mensch.

Die Punkte der Abnahmeliste aus der README, so weit sie gebaut sind. Was
noch fehlt (Power-On-Passwort, Ereignisspeicher-Reiter, F12-Menue), steht
unten am Ende als offene Liste -- lieber ein ehrliches Loch als ein Test,
der etwas prueft, das es nicht gibt.
"""

import os
import re
import shutil
import subprocess
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

BDA_FIRMA = 0x500
BDA_POLICY = 0x524
BDA_BLOCK = 0x528
BDA_INVENT = 0x628


def pruefe(was, bedingung, bild=None):
    global PASST, FEHLT
    if bedingung:
        PASST += 1
        print(f"  [{GRUEN}  OK  {WEG}] {was}")
    else:
        FEHLT += 1
        print(f"  [{ROT} FEHLT{WEG}] {was}")
        if bild:
            for zeile in str(bild).split("\n"):
                if zeile.strip():
                    print(f"           | {zeile}")


class Lauf:
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
            if self.m.cpu.last_fault:
                raise SystemExit(f"CPU-Fehler: {self.m.cpu.last_fault}")
            if not self.m.running:
                return

    def eingabe(self, text, warte_danach=0.5):
        for teil in text.split("|"):
            if teil.upper() in KEYNAMES:
                sc, a = KEYNAMES[teil.upper()]
                self.m.keyboard.push(a, sc)
            else:
                for ch in teil:
                    self.m.keyboard.push(ord(ch), 0)
            for _ in range(3):
                self.m.run_slice(self.dt)
        self.warte(warte_danach)

    def bild(self):
        return "\n".join(screen_text(self.m))

    def ins_setup(self, versuche=16):
        for _ in range(versuche):
            self.eingabe("DEL", 0.15)
            if "SETUP UTILITY" in self.bild():
                return True
        return False

    def reiter(self, n):
        """Von Main aus n Reiter nach rechts."""
        for _ in range(n):
            self.eingabe("RIGHT", 0.0)
        self.warte(0.4)

    def text_bei(self, adresse, laenge=32):
        roh = bytes(self.m.bus.read_block(adresse, laenge))
        return roh.split(b"\x00", 1)[0].decode("latin-1")

    def wort_bei(self, adresse):
        return int.from_bytes(bytes(self.m.bus.read_block(adresse, 4)), "little")


def main():
    print("\nCOMPANY-OS -- Firmenrichtlinien im BIOS\n")

    sys.path.insert(0, HIER)
    from importlib import import_module
    bauen = import_module("bauen")
    rom, daten, summe = bauen.bauen(still=True)
    print(f"  Abbild: {len(daten)} Bytes, Pruefsumme {summe:08X}\n")

    tmp = tempfile.mkdtemp(prefix="companyos_")
    chip = os.path.join(tmp, "bios.bin")
    shutil.copy(rom, chip)
    cmos = os.path.join(tmp, "cmos.bin")
    platte = test_platte()

    # === 1. Der Reiter Company ==========================================
    print("--- Der Reiter Company -----------------------------------------")
    L = Lauf(chip, cmos, platte)
    pruefe("Der Rechner startet mit COMPANY-OS", L.ins_setup(), L.bild())
    L.reiter(5)
    b = L.bild()
    pruefe("Der Reiter Company ist da", "Company" in b and "Owner Tag" in b, b)
    pruefe("Alle neun Zeilen stehen darin",
           "Owner Text" in b and "Block Compiler" in b and "Block Network" in b
           and "Require Login Password" in b and "Boot From Internal Disk" in b
           and "Blocked Programs" in b and "Configuration Cleared" in b, b)
    pruefe("Ein fabrikneuer Rechner meldet KEINEN Einbruch",
           "Configuration Cleared" in b and "Yes" not in b.split("Configuration Cleared")[1][:40],
           b)

    # === 2. Owner Tag und Owner Text ====================================
    print("\n--- Eigentuemer-Eintrag ----------------------------------------")
    L.eingabe("ENTER", 0.4)                  # Owner Tag an
    pruefe("Owner Tag laesst sich einschalten", "Enabled" in L.bild(), L.bild())

    L.eingabe("DOWN", 0.0)
    L.eingabe("ENTER", 0.5)                  # Owner Text
    pruefe("Der Editor fragt nach dem Firmentext",
           "Owner Text (max 31)" in L.bild(), L.bild())
    L.eingabe("ACME GmbH", 0.2)
    pruefe("Der Text erscheint im Klartext, nicht als Sterne",
           "ACME GmbH" in L.bild() and "*****" not in L.bild(), L.bild())
    L.eingabe("ENTER", 0.8)
    pruefe("Der Text ist gespeichert", "Owner text stored" in L.gesehen, L.bild())
    L.eingabe("F10", 1.0)

    N = Lauf(chip, cmos, platte)
    N.warte(4.0)
    pruefe("Nach dem Neustart steht er im Speicher fuer das System",
           N.text_bei(BDA_FIRMA) == "ACME GmbH", N.text_bei(BDA_FIRMA))
    pruefe("Das Schalterwort traegt Bit 0", N.wort_bei(BDA_POLICY) & 1 == 1,
           hex(N.wort_bei(BDA_POLICY)))
    pruefe("Die Seriennummer steht im Inventar",
           N.text_bei(BDA_INVENT, 16).startswith("TB32-"), N.text_bei(BDA_INVENT, 16))
    pruefe("Der Startzaehler zaehlt mit", N.wort_bei(BDA_INVENT + 16) >= 2,
           N.wort_bei(BDA_INVENT + 16))

    # Owner Tag wieder aus -> der Text darf nicht mehr ankommen.
    # Dafuer ein FRISCHER Rechner: N ist oben durchgebootet, und nach dem
    # Bootvorgang fuehrt DEL nirgendwohin mehr. Genau daran ist dieser Test
    # beim ersten Anlauf gescheitert -- die Tastendruecke gingen ins
    # laufende System statt ins Setup.
    N2 = Lauf(chip, cmos, platte)
    pruefe("Setup laesst sich wieder oeffnen", N2.ins_setup(), N2.bild())
    N2.reiter(5)
    N2.eingabe("ENTER", 0.4)
    N2.eingabe("F10", 1.0)
    O = Lauf(chip, cmos, platte)
    O.warte(4.0)
    pruefe("Owner Tag aus -> das System bekommt keinen Text mehr",
           O.text_bei(BDA_FIRMA) == "", repr(O.text_bei(BDA_FIRMA)))

    # === 3. Programme sperren ===========================================
    print("\n--- Programme sperren ------------------------------------------")
    P = Lauf(chip, cmos, platte)
    P.ins_setup()
    P.reiter(5)
    for _ in range(6):
        P.eingabe("DOWN", 0.0)
    P.warte(0.3)
    P.eingabe("ENTER", 0.6)                  # Blocked Programs
    pruefe("Die Abhakliste geht auf",
           "Blocked Programs" in P.bild() and "CODER.TBX" in P.bild(), P.bild())
    P.eingabe("ENTER", 0.3)                  # CODER.TBX abhaken
    pruefe("Der Haken sitzt", "[X]" in P.bild(), P.bild())
    P.eingabe("ESC", 0.6)
    P.eingabe("F10", 1.0)

    Q = Lauf(chip, cmos, platte)
    Q.warte(4.0)
    pruefe("Das System bekommt den gesperrten Namen",
           Q.text_bei(BDA_BLOCK, 16) == "CODER.TBX", Q.text_bei(BDA_BLOCK, 16))

    # === 3b. Und zwar wirklich: ein Startversuch muss scheitern =========
    #
    # Der erste Anlauf hat nur geprueft, dass der Name im Speicher ankommt
    # und der Menueeintrag grau wird. Das ist NICHT dasselbe wie gesperrt:
    # der Start lief trotzdem durch, weil die Pruefung nur in
    # gui_prog_starten() stand -- an der Dateiverwaltung, am Coder und an
    # START auf der Kommandozeile ging sie vorbei. Diese Pruefung hier ist
    # die, auf die es ankommt.
    print("\n--- ... und der Start scheitert auch wirklich ------------------")
    blkcmos = os.path.join(tmp, "cmos_blk.bin")
    BL = Lauf(chip, blkcmos, platte)
    BL.ins_setup()
    BL.reiter(5)
    for _ in range(6):
        BL.eingabe("DOWN", 0.0)
    BL.warte(0.3)
    BL.eingabe("ENTER", 0.6)                 # Blocked Programs
    for _ in range(9):
        BL.eingabe("DOWN", 0.0)
    BL.warte(0.3)
    BL.eingabe("ENTER", 0.4)                 # CALC.TBX abhaken
    pruefe("CALC.TBX ist abgehakt", "[X] CALC.TBX" in BL.bild(), BL.bild())
    BL.eingabe("ESC", 0.5)
    BL.eingabe("F10", 1.0)
    BL.m.cmos.data[0x1D] = 1                 # in die Textkonsole starten
    BL.m.cmos.save()

    BS = Lauf(chip, blkcmos, platte)
    BS.warte(7.0)
    BS.eingabe("START CALC.TBX", 0.3)
    BS.eingabe("ENTER", 2.5)
    pruefe("START CALC.TBX wird abgewiesen",
           "Blocked by system policy" in BS.gesehen, BS.bild())
    pruefe("... und meldet NICHT 'nicht gefunden'",
           "not found" not in BS.bild(), BS.bild())

    BS.eingabe("CALC", 0.3)                  # auch ohne START davor
    BS.eingabe("ENTER", 2.0)
    pruefe("Auch der Aufruf ohne START wird abgewiesen",
           BS.bild().count("Blocked by system policy") >= 1
           or "Blocked by system policy" in BS.gesehen, BS.bild())

    # Gegenprobe: ein nicht gesperrtes Programm laeuft weiterhin
    BS.eingabe("VER", 0.3)
    BS.eingabe("ENTER", 1.5)
    pruefe("Ein nicht gesperrtes Programm laeuft weiter",
           "TOOBAD-OS" in BS.bild(), BS.bild())

    # === 4. Die groben Schalter =========================================
    print("\n--- Compiler und Netz ------------------------------------------")
    R = Lauf(chip, cmos, platte)
    R.ins_setup()
    R.reiter(5)
    R.eingabe("DOWN", 0.0)
    R.eingabe("DOWN", 0.0)
    R.warte(0.3)
    R.eingabe("ENTER", 0.4)                  # Block Compiler
    R.eingabe("DOWN", 0.0)
    R.warte(0.2)
    R.eingabe("ENTER", 0.4)                  # Block Network
    R.eingabe("F10", 1.0)
    S = Lauf(chip, cmos, platte)
    S.warte(4.0)
    pol = S.wort_bei(BDA_POLICY)
    pruefe("Bit 1 (kein Compiler) kommt beim System an", pol & 2 == 2, hex(pol))
    pruefe("Bit 2 (kein Netz) kommt beim System an", pol & 4 == 4, hex(pol))

    # === A1/A2: das Power-On-Passwort ===================================
    print("\n--- A1/A2: Power-On-Passwort -----------------------------------")
    pwcmos = os.path.join(tmp, "cmos_pw.bin")
    V = Lauf(chip, pwcmos, platte)
    V.ins_setup()
    V.reiter(4)                              # Reiter Password
    # Zeile 0 Supervisor (Anzeige), 1 setzen, 2 loeschen, 3 Power-On
    # (Anzeige), 4 setzen. Die Anzeigezeilen zaehlen mit -- ENTER auf einer
    # von ihnen tut nichts, und genau darauf bin ich beim ersten Anlauf
    # hereingefallen.
    for _ in range(4):
        V.eingabe("DOWN", 0.0)
    V.warte(0.3)
    V.eingabe("ENTER", 0.5)                  # Set / Change Power-On Password
    pruefe("Der Dialog fragt nach einem Power-On-Passwort",
           "Enter New Password" in V.bild(), V.bild())
    V.eingabe("start123", 0.2)
    V.eingabe("ENTER", 0.4)
    V.eingabe("start123", 0.2)
    V.eingabe("ENTER", 0.8)
    pruefe("Es ist eingerichtet", "Power-On password installed" in V.gesehen,
           V.bild())
    V.eingabe("F10", 1.0)

    W = Lauf(chip, pwcmos, platte)
    W.warte(6.0)
    pruefe("Der Rechner fragt VOR dem Booten",
           "This computer is locked" in W.gesehen, W.bild())
    pruefe("... und bootet nicht von allein durch",
           "A:\\>" not in W.bild() and "Mounting" not in W.bild(), W.bild())
    W.eingabe("start123", 0.2)
    W.eingabe("ENTER", 3.0)
    pruefe("Mit dem Passwort geht es weiter",
           "Mounting" in W.gesehen or "A:\\>" in W.gesehen, W.bild())

    # Zwei Fehlversuche, dann Reset -- der Zaehler darf NICHT von vorn
    # anfangen. Genau daran scheitert ein Zaehler im Register.
    X = Lauf(chip, pwcmos, platte)
    X.warte(5.0)
    for _ in range(2):
        X.eingabe("falsch", 0.2)
        X.eingabe("ENTER", 1.2)
    pruefe("Zwei Fehlversuche werden abgewiesen", "Wrong password" in X.gesehen,
           X.bild())
    Y = Lauf(chip, pwcmos, platte)           # Reset -- das ist der Prueffall
    Y.warte(5.0)
    Y.eingabe("nochmalfalsch", 0.2)
    Y.eingabe("ENTER", 2.0)
    pruefe("Der dritte Fehlversuch NACH einem Reset sperrt den Rechner",
           "Too many failed attempts" in Y.gesehen, Y.bild())

    # Das Supervisor-Passwort muss durchkommen, sonst sperrt sich der
    # Administrator selbst aus. Vorher den Zaehler zuruecksetzen -- der
    # Rechner ist gerade dicht.
    os.remove(pwcmos)
    Z2 = Lauf(chip, pwcmos, platte)
    Z2.ins_setup()
    Z2.reiter(4)
    Z2.eingabe("DOWN", 0.0)
    Z2.warte(0.2)
    Z2.eingabe("ENTER", 0.5)                 # Supervisor setzen
    Z2.eingabe("chef", 0.2)
    Z2.eingabe("ENTER", 0.4)
    Z2.eingabe("chef", 0.2)
    Z2.eingabe("ENTER", 0.8)
    for _ in range(3):
        Z2.eingabe("DOWN", 0.0)
    Z2.warte(0.2)
    Z2.eingabe("ENTER", 0.5)                 # Power-On setzen (Zeile 4)
    Z2.eingabe("benutzer", 0.2)
    Z2.eingabe("ENTER", 0.4)
    Z2.eingabe("benutzer", 0.2)
    Z2.eingabe("ENTER", 0.8)
    Z2.eingabe("F10", 1.0)

    Z3 = Lauf(chip, pwcmos, platte)
    Z3.warte(5.0)
    Z3.eingabe("chef", 0.2)                  # das des Administrators
    Z3.eingabe("ENTER", 3.0)
    pruefe("Das Supervisor-Passwort kommt durch das Power-On-Tor",
           "Mounting" in Z3.gesehen or "A:\\>" in Z3.gesehen, Z3.bild())

    # === A6: nur von der eigenen Platte starten =========================
    # Zwei Wege muessen zu sein: der ueber das Setup und der daran vorbei.
    print("\n--- A6: die Startquelle ist festgenagelt ------------------------")
    a6cmos = os.path.join(tmp, "cmos_a6.bin")
    A = Lauf(chip, a6cmos, platte)
    A.ins_setup()
    A.reiter(1)                              # Hardware
    A.eingabe("DOWN", 0.0)                   # Boot Device Priority
    A.warte(0.3)
    A.eingabe("ENTER", 0.4)
    pruefe("Ohne die Sperre laesst sich die Startquelle aendern",
           "Hard Disk 0" not in A.bild(), A.bild())
    A.eingabe("ENTER", 0.4)
    A.eingabe("ENTER", 0.4)                  # zurueck auf Hard Disk 0
    A.reiter(4)                              # Company (von Hardware aus)
    for _ in range(5):
        A.eingabe("DOWN", 0.0)
    A.warte(0.3)
    A.eingabe("ENTER", 0.4)                  # Boot From Internal Disk Only
    pruefe("Der Schalter laesst sich einschalten", "Enabled" in A.bild(), A.bild())
    A.eingabe("F10", 1.0)

    B = Lauf(chip, a6cmos, platte)
    B.ins_setup()
    B.reiter(1)
    B.eingabe("DOWN", 0.0)
    B.warte(0.3)
    B.eingabe("ENTER", 0.6)
    pruefe("Jetzt verweigert das Setup die Aenderung",
           "locked by system policy" in B.gesehen, B.bild())
    pruefe("... und die Quelle steht weiter auf der Platte",
           "Hard Disk 0" in B.bild(), B.bild())

    # Der Weg am Setup vorbei: von aussen in cmos.bin schreiben.
    B.m.cmos.data[0x10] = 2                  # "Network"
    B.m.cmos.save()
    C = Lauf(chip, a6cmos, platte)
    C.warte(6.0)
    pruefe("Ein von aussen verstelltes CMOS wird beim Start zurueckgesetzt",
           C.m.cmos.data[0x10] == 0, C.m.cmos.data[0x10])
    pruefe("... und es steht auf dem Schirm",
           "Boot source was changed" in C.gesehen, C.bild())

    # === B1/B2/B3/B6 ====================================================
    print("\n--- B1: Ereignisspeicher ---------------------------------------")
    evcmos = os.path.join(tmp, "cmos_ev.bin")
    E = Lauf(chip, evcmos, platte)
    E.warte(5.0)                             # ein Start schreibt EV_BOOT
    E2 = Lauf(chip, evcmos, platte)
    E2.ins_setup()
    E2.reiter(6)                             # Reiter Event Log
    b = E2.bild()
    pruefe("Der Reiter Event Log ist da", "Event Log" in b, b)
    pruefe("Der Start steht drin", "Started" in b, b)
    pruefe("Leere Plaetze zeigen einen Strich", "--" in b, b)
    # Ein falsches Passwort muss auftauchen
    E2.eingabe("ESC", 1.0)
    E3 = Lauf(chip, evcmos, platte)
    E3.ins_setup()
    E3.reiter(4)
    E3.eingabe("DOWN", 0.0)
    E3.warte(0.2)
    E3.eingabe("ENTER", 0.5)
    E3.eingabe("sup", 0.2)
    E3.eingabe("ENTER", 0.4)
    E3.eingabe("sup", 0.2)
    E3.eingabe("ENTER", 0.8)
    E3.eingabe("F10", 1.0)
    E4 = Lauf(chip, evcmos, platte)
    E4.ins_setup()
    E4.eingabe("daneben", 0.2)
    E4.eingabe("ENTER", 1.2)
    E4.eingabe("sup", 0.2)
    E4.eingabe("ENTER", 1.2)
    E4.reiter(6)
    pruefe("Ein falsches Passwort landet im Protokoll",
           "Wrong password" in E4.bild(), E4.bild())
    # Leeren
    for _ in range(8):
        E4.eingabe("DOWN", 0.0)
    E4.warte(0.3)
    E4.eingabe("ENTER", 0.8)
    pruefe("Der Ereignisspeicher laesst sich leeren",
           "Event log cleared" in E4.gesehen, E4.bild())

    print("\n--- B2: Secure Boot dreistufig ---------------------------------")
    S2 = Lauf(chip, os.path.join(tmp, "cmos_sb.bin"), platte)
    S2.ins_setup()
    S2.reiter(3)                             # Security
    S2.eingabe("ENTER", 0.4)
    pruefe("Stufe 1 heisst Audit", "Audit" in S2.bild(), S2.bild())
    S2.eingabe("ENTER", 0.4)
    pruefe("Stufe 2 heisst Enforce", "Enforce" in S2.bild(), S2.bild())
    S2.eingabe("ENTER", 0.4)
    pruefe("Stufe 3 ist wieder Disabled", "Disabled" in S2.bild(), S2.bild())
    # Audit einstellen, Abbild verbiegen -> warnen, aber starten
    S2.eingabe("ENTER", 0.4)                 # Audit
    S2.eingabe("DOWN", 0.0)
    S2.eingabe("DOWN", 0.3)
    S2.eingabe("ENTER", 0.8)                 # Trust Current Boot Image
    S2.eingabe("F10", 1.0)
    kaputt = os.path.join(tmp, "hd_audit.img")
    shutil.copy(platte, kaputt)
    with open(kaputt, "r+b") as f:
        f.seek(500)
        f.write(b"\xA5" * 8)
    S3 = Lauf(chip, os.path.join(tmp, "cmos_sb.bin"), kaputt)
    S3.warte(7.0)
    pruefe("Audit warnt, haelt aber nicht an",
           "Audit" in S3.gesehen and "Mounting" in S3.gesehen, S3.bild())

    print("\n--- B3/B6: Inventar und Startbild ------------------------------")
    I = Lauf(chip, cmos, platte)
    I.ins_setup()
    I.reiter(5)
    for _ in range(8):
        I.eingabe("DOWN", 0.0)
    I.warte(0.4)
    b = I.bild()
    pruefe("Seriennummer, Startzaehler und Betriebszeit stehen im Setup",
           "Serial Number" in b and "Power-On Count" in b
           and "Operating Time" in b, b)
    pruefe("Die Seriennummer ist gefuellt", "TB32-" in b, b)

    # Der Owner Tag wurde weiter oben absichtlich ausgeschaltet -- ohne ihn
    # darf beim Einschalten auch nichts stehen. Fuer B6 also erst wieder an.
    B5 = Lauf(chip, cmos, platte)
    B5.ins_setup()
    B5.reiter(5)
    B5.eingabe("ENTER", 0.4)
    B5.eingabe("F10", 1.0)
    B6 = Lauf(chip, cmos, platte)
    B6.warte(2.0)
    pruefe("Der Firmentext steht schon beim Einschalten da",
           "ACME GmbH" in B6.gesehen, B6.bild())

    # === B4 und die C-Punkte ============================================
    print("\n--- B4: das Startmenue -----------------------------------------")
    bmcmos = os.path.join(tmp, "cmos_bm.bin")
    M = Lauf(chip, bmcmos, platte)
    for _ in range(16):
        M.eingabe("F8", 0.15)
        if "Boot Menu" in M.bild():
            break
    pruefe("F8 oeffnet das Startmenue", "Boot Menu" in M.bild(), M.bild())
    pruefe("Die eigene Platte steht drin", "Hard Disk 0" in M.bild(), M.bild())
    pruefe("Floppy und Netz stehen als nicht vorhanden da",
           "not installed" in M.bild(), M.bild())
    M.eingabe("DOWN", 0.2)
    M.eingabe("ENTER", 0.8)
    pruefe("Eine fehlende Quelle sagt das auch",
           "not installed on this machine" in M.gesehen, M.bild())
    M.eingabe("ESC", 2.0)

    # Mit A6 verlangt das Menue das Supervisor-Passwort -- sonst waere die
    # Sperre mit einem Tastendruck erledigt.
    M2 = Lauf(chip, bmcmos, platte)
    M2.ins_setup()
    M2.reiter(4)
    M2.eingabe("DOWN", 0.0)
    M2.warte(0.2)
    M2.eingabe("ENTER", 0.5)
    M2.eingabe("chef", 0.2)
    M2.eingabe("ENTER", 0.4)
    M2.eingabe("chef", 0.2)
    M2.eingabe("ENTER", 0.8)
    M2.reiter(1)                             # Company
    for _ in range(5):
        M2.eingabe("DOWN", 0.0)
    M2.warte(0.3)
    M2.eingabe("ENTER", 0.4)                 # Boot From Internal Disk Only
    M2.eingabe("F10", 1.0)

    M3 = Lauf(chip, bmcmos, platte)
    for _ in range(16):
        M3.eingabe("F8", 0.15)
        if "Boot Menu is locked" in M3.bild():
            break
    pruefe("Mit A6 verlangt das Startmenue das Supervisor-Passwort",
           "Boot Menu is locked" in M3.bild(), M3.bild())
    pruefe("... und zeigt die Liste noch nicht",
           "Hard Disk 0" not in M3.bild(), M3.bild())
    M3.eingabe("chef", 0.2)
    M3.eingabe("ENTER", 1.0)
    pruefe("Mit dem Passwort geht es auf", "Boot Menu" in M3.bild(), M3.bild())

    print("\n--- C: Startverzoegerung und Reiter Exit -----------------------")
    C1 = Lauf(chip, os.path.join(tmp, "cmos_c.bin"), platte)
    C1.ins_setup()
    for _ in range(6):
        C1.eingabe("DOWN", 0.0)
    C1.warte(0.3)
    b = C1.bild()
    pruefe("Boot Delay steht im Reiter Main", "Boot Delay" in b, b)
    C1.eingabe("ENTER", 0.4)
    C1.eingabe("ENTER", 0.4)
    pruefe("Die Sekunden lassen sich hochzaehlen", "2 s" in C1.bild(), C1.bild())
    C1.reiter(8)                             # Reiter Exit
    b = C1.bild()
    pruefe("Der Reiter Exit ist da",
           "Save Changes and Exit" in b and "Discard Changes and Exit" in b, b)
    C1.eingabe("ENTER", 1.2)
    pruefe("Save Changes and Exit verlaesst das Setup wirklich",
           "SETUP UTILITY" not in C1.bild(), C1.bild())

    # === 5. A7: die Flash-Sperre ========================================
    print("\n--- A7: der Chip laesst sich aus dem System nicht brennen -------")
    T = Lauf(chip, cmos, platte)
    T.warte(6.0)
    vorher = open(chip, "rb").read()
    pruefe("Nach dem Booten ist der Chip gesperrt", T.m.flash.gesperrt)
    T.m.flash.puffer = b"\x00" * 4096
    for befehl, was in ((3, "brennen"), (6, "Einmal-Start"),
                        (8, "Flashwunsch"), (4, "Sicherung")):
        T.m.flash.port_out(0xB0, befehl)
        pruefe(f"... {was} prallt ab", T.m.flash.status == T.m.flash.GESPERRT,
               T.m.flash.status)
    pruefe("Der Chip ist Byte fuer Byte unveraendert",
           open(chip, "rb").read() == vorher)

    # === 6. build.py laesst den Chip in Ruhe ============================
    print("\n--- build.py ---------------------------------------------------")
    with tempfile.TemporaryDirectory() as bt:
        probe = os.path.join(bt, "bios.bin")
        shutil.copy(chip, probe)
        # fremdes_bios() ist die Stelle, auf die es ankommt -- ohne den
        # ganzen Bau anzuwerfen.
        sys.path.insert(0, ROOT)
        from build import fremdes_bios
        pruefe("build.py erkennt ein fremdes BIOS",
               fremdes_bios(probe) == "COMPANY-OS BIOS v1.0", fremdes_bios(probe))
        shutil.copy(os.path.join(ROOT, "firmware", "bios.bin"), probe)
        pruefe("... und das eigene erkennt es nicht als fremd",
               fremdes_bios(probe) is None, fremdes_bios(probe))

    # === 7. Das Serien-BIOS raeumt auf ==================================
    print("\n--- Zurueck auf das Serien-BIOS --------------------------------")
    serie = os.path.join(tmp, "serie.bin")
    shutil.copy(os.path.join(ROOT, "firmware", "bios.bin"), serie)
    U = Lauf(serie, cmos, platte)
    U.warte(4.0)
    pruefe("Kein Firmentext mehr im Speicher", U.text_bei(BDA_FIRMA) == "",
           repr(U.text_bei(BDA_FIRMA)))
    pruefe("Keine Sperrliste mehr", U.text_bei(BDA_BLOCK, 16) == "",
           repr(U.text_bei(BDA_BLOCK, 16)))

    shutil.rmtree(tmp, ignore_errors=True)

    print("\n--- Noch nicht gebaut, deshalb hier nicht geprueft -------------")
    for offen in ("B5     Netzwerkstart -- eigenes Projekt",
                  "C      Numlock und AC Power Recovery: keine Hardware dafuer da"):
        print(f"  [ offen ] {offen}")

    farbe = GRUEN if FEHLT == 0 else ROT
    print(f"\n{farbe}{PASST}/{PASST + FEHLT} Pruefungen bestanden{WEG}\n")
    return 1 if FEHLT else 0


if __name__ == "__main__":
    sys.exit(main())
