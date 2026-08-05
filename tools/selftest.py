#!/usr/bin/env python3
"""
Full test of the virtual PC.

Rebuilds everything, powers the machine on, and checks step by step
whether every layer really works -- from the CPU to the window system.
Every test runs on the real emulated machine, nothing is simulated.

    python3 tools/selftest.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from hardware.machine import Machine
from hardware import devices as dev
from tools.headless import KEYNAMES, screen_text, test_cmos, test_platte

GRUEN, ROT, GELB, WEG = "\033[92m", "\033[91m", "\033[93m", "\033[0m"


def tippe(m, text):
    """Converts a string into keystrokes."""
    keys = []
    for teil in text.split("|"):
        if teil.upper() in KEYNAMES:
            keys.append(KEYNAMES[teil.upper()])
        else:
            for ch in teil:
                keys.append((0, ord(ch)))
    return keys


class Lauf:
    """A machine that runs in the background and that you can type into."""

    def __init__(self, rom=None):
        self.m = Machine(ROOT, rom=rom, disk=test_platte(), cmos=test_cmos())
        self.m.power_on()
        self.dt = 1 / 60
        # Everything that has ever been on screen since power-on. With Quick
        # Boot enabled, POST is over after a quarter second -- a look at a
        # fixed point in time would then miss it.
        self.gesehen = ""
        self._zaehler = 0

    def warte(self, sekunden):
        for _ in range(int(sekunden / self.dt)):
            self.m.run_slice(self.dt)
            self._zaehler += 1
            if self._zaehler % 3 == 0:          # about 20 times per second
                self.gesehen = self.gesehen + "\n" + self.bild()
            if not self.m.running:
                return

    def eingabe(self, text, warte_danach=1.0):
        for sc, a in tippe(self.m, text):
            self.m.keyboard.push(a, sc)
            for _ in range(3):
                self.m.run_slice(self.dt)
        self.warte(warte_danach)

    def bild(self):
        return "\n".join(screen_text(self.m))


def bios_summe():
    """The checksum stored in the header of the built BIOS."""
    with open(os.path.join(ROOT, "firmware", "bios.bin"), "rb") as f:
        return int.from_bytes(f.read(16)[12:16], "little")


def flash_test():
    """Actually rewrite the BIOS chip -- on a copy.

    This tests the whole chain: pick a file, check the signature and
    checksum, flash it, create a backup, boot from it, restore it.
    And the case that really matters: a corrupted image must NOT
    be flashed."""
    fw = os.path.join(ROOT, "firmware")
    with tempfile.TemporaryDirectory() as tmp:
        chip = os.path.join(tmp, "bios.bin")
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        with open(os.path.join(fw, "minimal.bin"), "rb") as f:
            klein = f.read()
        with open(chip, "rb") as f:
            gross = f.read()

        # --- A corrupted image must be rejected -------------------
        kaputt = bytearray(klein)
        kaputt[400] ^= 0xFF                     # checksum no longer matches
        L = Lauf(rom=chip)
        L.m.flash.waehler = lambda: bytes(kaputt)
        for _ in range(14):
            L.eingabe("DEL", 0.15)
            if "SETUP UTILITY" in L.bild():
                break
        for _ in range(4):
            L.eingabe("RIGHT", 0.0)
        L.warte(0.4)
        L.eingabe("DOWN", 0.0)
        L.eingabe("DOWN", 0.3)
        L.eingabe("ENTER", 0.8)
        pruefe("Corrupted image is rejected",
               "Checksum does not match" in L.gesehen, L.bild())
        with open(chip, "rb") as f:
            pruefe("... and the chip remains untouched", f.read() == gross)

        # --- Flash the good image -------------------------------------
        L.m.flash.waehler = lambda: klein
        L.eingabe("ENTER", 0.6)                 # Flash BIOS from File
        L.eingabe("ENTER", 0.8)                 # confirm the prompt
        pruefe("Good image is flashed", "Flash complete" in L.gesehen, L.bild())
        with open(chip, "rb") as f:
            pruefe("Chip now contains the new BIOS", f.read() == klein)
        with open(os.path.join(tmp, "bios.backup.bin"), "rb") as f:
            pruefe("Backup of the old BIOS created", f.read() == gross)

        # --- And it boots with it too -----------------------------------
        # How to tell: the minimal BIOS has no self-test at all, so the
        # "TOOBAD BIOS" startup screen is missing -- and yet the prompt
        # is there at the end. Looking for its own message would be
        # pointless: it's only on screen for microseconds, because the
        # boot sector loads right after.
        N = Lauf(rom=chip)
        N.warte(4.0)
        pruefe("The machine boots with the self-flashed BIOS",
               "A:\\>" in N.bild() and "TOOBAD BIOS" not in N.gesehen, N.bild())

        # --- One-time boot: testing without touching the chip ---------------
        # After the flash test above, the chip still carries the small
        # BIOS -- restore the real one first, or the test would check
        # against itself.
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        T = Lauf(rom=chip)
        T.warte(6.0)
        T.m.bus.write_block(0x00760000, klein)
        T.m.flash.port_out(0xB2, 0x00760000)
        T.m.flash.port_out(0xB1, len(klein))
        T.m.flash.port_out(0xB0, 5)
        pruefe("Image fetched from RAM",
               T.m.flash.status == 0 and len(T.m.flash.puffer) == len(klein))
        T.m.flash.port_out(0xB0, 6)
        T.m.power_on()
        T.warte(4.0)
        pruefe("Restart runs with the test image",
               T.m.bios_test and "A:\\>" in T.bild(), T.bild())
        with open(chip, "rb") as f:
            pruefe("... and the chip remained untouched", f.read() == gross)
        T.m.power_on()
        T.warte(6.0)
        pruefe("The next boot uses the real BIOS again",
               not T.m.bios_test and "A:\\>" in T.bild(), T.bild())

        # --- Permanent flash: the firmware asks in red ------------
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        F = Lauf(rom=chip)
        F.warte(6.0)
        F.m.bus.write_block(0x00760000, klein)
        F.m.flash.port_out(0xB2, 0x00760000)
        F.m.flash.port_out(0xB1, len(klein))
        F.m.flash.port_out(0xB0, 5)
        F.m.flash.port_out(0xB0, 8)
        F.m.power_on()
        F.warte(1.5)
        pruefe("Firmware asks in red before flashing",
               "FLASH BIOS" in F.bild() and "permanent" in F.bild(), F.bild())
        with open(chip, "rb") as f:
            pruefe("... and hasn't written anything yet", f.read() == gross)
        F.eingabe("ENTER", 3.0)
        with open(chip, "rb") as f:
            pruefe("ENTER flashes the chip", f.read() == klein)
        shutil.copy(os.path.join(fw, "bios.bin"), chip)

        # --- Dual BIOS: a destroyed chip is automatically replaced ------
        with open(chip, "wb") as f:
            f.write(b"\x00" * 64)               # chip destroyed
        R = Machine(ROOT, rom=chip, disk=test_platte(), cmos=test_cmos())
        R.power_on()
        pruefe("Destroyed chip: the board falls back to the backup",
               R.rom_gerettet)
        with open(chip, "rb") as f:
            pruefe("... and writes the old BIOS back", f.read() == gross)
        R.shutdown()


ergebnisse = []


def pruefe(name, bedingung, detail=""):
    ergebnisse.append((name, bool(bedingung)))
    zeichen = f"{GRUEN}  OK  {WEG}" if bedingung else f"{ROT} FAIL {WEG}"
    print(f"  [{zeichen}] {name}" + (f"   {detail}" if detail and not bedingung else ""))


_symbole = {}


def symbole():
    """The names from the kernel with their addresses -- this lets the
    self-test check what the browser actually understood, instead of
    guessing letters from the screen."""
    if not _symbole:
        with open(os.path.join(ROOT, "system", "kernel.sym")) as f:
            for zeile in f:
                teile = zeile.split()
                if len(teile) >= 2:
                    _symbole[teile[1]] = int(teile[0], 16)
    return _symbole


def wort(m, adresse):
    return int.from_bytes(m.bus.read_block(adresse, 4), "little", signed=True)


def seitentext(m, sym):
    """The displayed lines as a single block of text."""
    n = wort(m, sym["br_anzahl"])
    aus = []
    for i in range(min(n, 40)):
        roh = m.bus.read_block(0x00190000 + i * 100, 100)
        aus.append(roh.split(b"\0")[0].decode("latin1"))
    return "\n".join(aus)


def linkzeile(m, sym):
    """The first line that contains a link."""
    n = wort(m, sym["br_anzahl"])
    for i in range(min(n, 40)):
        if wort(m, sym["br_link"] + i * 4) >= 0:
            return i
    return 0 - 1


def main():
    print("Rebuilding the system ...")
    r = subprocess.run([sys.executable, "build.py"], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        return 1
    for zeile in r.stdout.splitlines()[1:-1]:
        print("   ", zeile.strip())

    print("\n--- Tools --------------------------------------------------")
    r = subprocess.run([sys.executable, "tools/ctest.py", "--selftest"],
                       cwd=ROOT, capture_output=True, text=True)
    bestanden = "11/11" in r.stdout or r.stdout.strip().endswith("tests passed")
    pruefe("C compiler passes all language tests", "FAILED" not in r.stdout)

    print("\n--- Power-on and BIOS ---------------------------------------")
    L = Lauf()
    # POST now takes its time: memory counts up visibly, each check
    # appears individually. Without Quick Boot it takes about 1.5 s.
    L.warte(2.0)
    bild = L.gesehen                     # everything the POST displayed
    pruefe("CPU starts at the reset vector and executes ROM code",
           "TOOBAD BIOS" in bild)
    pruefe("Memory test finds 16384 KB", "16384 KB" in bild)
    pruefe("Hard disk detected", "16384 sectors" in bild)
    pruefe("Graphics card reported", "TB-VGA" in bild)

    print("\n--- BIOS setup (CMOS) ------------------------------------------")
    L2 = Lauf()
    # Offer DEL repeatedly: with Quick Boot the time window is only a
    # quarter second wide, without it takes two seconds.
    for _ in range(14):
        L2.eingabe("DEL", 0.15)
        if "SETUP UTILITY" in L2.bild():
            break
    L2.warte(0.4)
    bild = L2.bild()
    pruefe("DEL opens the setup", "SETUP UTILITY" in bild)
    pruefe("Settings are displayed", "Quick Boot" in bild)
    pruefe("Clock from the CMOS is running", "System Time" in bild)
    pruefe("Tab bar present",
           "Hardware" in bild and "Cooling" in bild and "Security" in bild)
    # Switch tabs: right twice -> Cooling with the readings
    L2.eingabe("RIGHT", 0.3)
    L2.eingabe("RIGHT", 0.5)
    bild = L2.bild()
    pruefe("Cooling tab shows readings",
           "Fan Control" in bild and "CPU Temperature" in bild)
    L2.eingabe("RIGHT", 0.5)
    bild = L2.bild()
    pruefe("Security tab shows Secure Boot", "Secure Boot" in bild)
    # Two keys in the same frame: the interrupt controller only has one bit
    # per source, so the handler must clear the chip out. If that went
    # wrong, the keyboard would lag one keystroke behind.
    L2.eingabe("LEFT", 0.0)
    L2.eingabe("LEFT", 0.6)
    pruefe("Two keys in the same frame both arrive", "Hardware" in L2.bild()
           and "ÉÍÍ Hardware" in L2.bild().replace("\u2554", "É"))
    L2.eingabe("RIGHT", 0.0)
    L2.eingabe("RIGHT", 0.0)
    L2.eingabe("RIGHT", 0.6)
    bild = L2.bild()
    pruefe("Firmware tab shows the BIOS chip",
           "BIOS Image Size" in bild and "Flash BIOS from File" in bild)
    pruefe("Chip reports its own checksum",
           f"{bios_summe():08X}" in bild, bild)
    L2.eingabe("ESC", 0.8)

    print("\n--- Flashing the BIOS (on a copy of the chip) -------------------")
    flash_test()

    print("\n--- Boot process ------------------------------------------------")
    L.warte(3.0)
    bild = L.bild()
    pruefe("Boot sector loaded and started", "TOOBAD-OS" in bild)
    pruefe("File system mounted", "Mounting file system" in bild)
    pruefe("Shell reports in", "A:\\>" in bild)

    print("\n--- Operating system ---------------------------------------------")
    L.eingabe("ver|ENTER", 0.7)
    pruefe("Command 'ver'", "TB-32" in L.bild())

    L.eingabe("mem|ENTER", 0.7)
    pruefe("Command 'mem' reads the BIOS data", "Total physical memory" in L.bild())

    L.eingabe("cls|ENTER", 0.4)
    L.eingabe("dir|ENTER", 1.0)
    bild = L.bild()
    pruefe("Directory shows the folders on the disk",
           "SYSTEM" in bild and "PROGS" in bild and "SOURCE" in bild)
    L.eingabe("CD PROGS|ENTER", 0.6)
    L.eingabe("DIR|ENTER", 1.0)
    bild = L.bild()
    pruefe("Folder change and contents",
           "A:\\PROGS" in bild and "BENCH.TBX" in bild and "MEMTEST.TBX" in bild)
    L.eingabe("CD \\|ENTER", 0.6)

    print("\n--- File system (write, read, delete) --------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("edit TEST.TXT|ENTER", 1.2)
    L.eingabe("Self-test writes here.", 1.0)
    L.eingabe("F2", 1.2)
    L.eingabe("ESC", 1.0)
    L.eingabe("type TEST.TXT|ENTER", 1.0)
    pruefe("Editor saves and the file can be read back",
           "Self-test writes here." in L.bild())
    L.eingabe("del TEST.TXT|ENTER", 0.8)
    pruefe("Delete file", "File deleted" in L.bild())

    # The start menu now shows what's in \SYSTEM\PROGS and \PROGS --
    # seven entries at a time, the rest via arrow. The numbers come from
    # gui.c, so a new menu entry doesn't break three tests again.
    import re as _re
    _gui = open(os.path.join(ROOT, "system", "gui.c")).read()
    MENU_SICHT = int(_re.search(r"define MENU_SICHT\s+(\d+)", _gui).group(1))
    MENU_ZH, BAR_Y = 14, 378
    MENU_HOEHE = (MENU_SICHT + 2) * MENU_ZH + 16
    MENU_TOP = BAR_Y - MENU_HOEHE

    def _klick(x, y, danach=1.0):
        L.m.mouse.move(x, y, 0); L.warte(0.2)
        L.m.mouse.move(x, y, 1); L.warte(0.3)
        L.m.mouse.move(x, y, 0); L.warte(danach)

    def menue(eintrag):
        """Click the Start button, then the nth entry -- scrolling if needed."""
        _klick(25, 387, 0.5)
        rollen = 0
        if eintrag >= MENU_SICHT:
            rollen = eintrag - MENU_SICHT + 1
        for _ in range(rollen):                      # down arrow
            _klick(172,
                   MENU_TOP + 6 + (MENU_SICHT - 1) * MENU_ZH, 0.3)
        _klick(60, MENU_TOP + 6 + (eintrag - rollen) * MENU_ZH)

    def menue_prog(name):
        """Start a program from the start menu.

        The menu builds its list from \\SYSTEM\\PROGS and \\PROGS -- so an
        entry's number is only known at runtime. Here the same order is
        recomputed instead of guessing a number."""
        from tools.tbfs import TBFS
        fs = TBFS(L.m.disk_path)
        sysdir = fs.find("SYSTEM", -1)
        liste = []
        for ordner in (fs.find("PROGS", sysdir), fs.find("PROGS", -1)):
            if ordner < 0:
                continue
            for i in range(128):
                if fs.typ(i) != 1 or fs.parent(i) != ordner:
                    continue
                if (fs._u32(fs._ent(i) + 24) >> 8) & 1:
                    continue
                if not fs.name(i).upper().endswith(".TBX"):
                    continue
                liste.append(fs.name(i).upper())
        MENU_FEST = int(_re.search(r"define MENU_FEST\s+(\d+)", _gui).group(1))
        menue(MENU_FEST + liste.index(name.upper()))

    def menue_fest(k):
        """0 = Power options, 1 = Exit desktop -- these are always at the bottom."""
        _klick(25, 387, 0.5)
        _klick(60, MENU_TOP + 8 + (MENU_SICHT + k) * MENU_ZH)

    # \SYSTEM\PROGS: the programs that belong to the system. They appear in
    # the start menu and are protected against deletion.
    L.eingabe("cd SYSTEM|ENTER", 0.6)
    L.eingabe("dir|ENTER", 0.8)
    pruefe("SYSTEM has its own program folder", "PROGS" in L.bild(),
           L.bild())
    L.eingabe("cd \\|ENTER", 0.6)

    print("\n--- Network ---------------------------------------------------")
    L.eingabe("net|ENTER", 0.8)
    bild = L.bild()
    pruefe("Network card reports in", "TB-NET" in bild and "Link" in bild)
    pruefe("Card states its own address", "02:54:42" in bild, bild)

    # A frame from outside: a second card on the same wire sends a
    # broadcast. If it arrives, the whole chain works -- wire, card, driver.
    from hardware.devices import Netzkarte

    class _Speicher:
        def __init__(self): self.b = bytearray(2048)
        def read_block(self, a, n): return bytes(self.b[a:a + n])
        def write_block(self, a, d): self.b[a:a + len(d)] = d

    fremd = Netzkarte([None])
    fremd.bus = _Speicher()
    rahmen = b"\xff" * 6 + b"\x00" * 6 + b"\x77\x42" + b"Selbsttest" + b"\x00" * 30
    fremd.bus.b[:len(rahmen)] = rahmen
    fremd.addr, fremd.len = 0, len(rahmen)
    fremd._befehl(1)
    L.warte(0.8)
    L.eingabe("net|ENTER", 0.8)
    bild = L.bild()
    empfangen = 0
    for zeile in bild.splitlines():
        if "Frames received" in zeile:
            empfangen = int(zeile.split()[-1])
    pruefe("Frame from outside arrives", empfangen >= 1, bild)

    # ARP and ICMP: ping a second machine. This proves the whole chain
    # works end to end -- "who has 10.0.0.x", reply, IP header with
    # checksum, ICMP echo, and the reply coming back.
    L.eingabe("net ip|ENTER", 0.8)
    meine = ""
    for zeile in L.bild().splitlines():
        if "IP address" in zeile:
            meine = zeile.split()[-1]
    pruefe("Card has its own IP address", meine.startswith("10.0.0."), L.bild())

    P = Lauf()                                   # the machine on the other end
    P.warte(6.0)
    P.eingabe("net ip|ENTER", 0.8)
    gegen = ""
    for zeile in P.bild().splitlines():
        if "IP address" in zeile:
            gegen = zeile.split()[-1]

    # Both must run in turns, otherwise neither is listening while the
    # other one asks.
    for sc, a in tippe(L.m, "ping " + gegen + "|ENTER"):
        L.m.keyboard.push(a, sc)
        for _ in range(3):
            L.m.run_slice(L.dt); P.m.run_slice(P.dt)
    for _ in range(int(9.0 / L.dt)):
        L.m.run_slice(L.dt)
        P.m.run_slice(P.dt)
    pruefe("PING gets a reply", "4 of 4 answered" in L.bild(), L.bild())
    P.eingabe("net arp|ENTER", 0.8)
    pruefe("The other side has us in its address table", meine in P.bild(), P.bild())
    fremd.close()

    # --- The router and DNS ------------------------------------------------
    # The router is what connects the TB-32 to the outside world. So the
    # test can run without internet access, it's given a tiny name
    # service here that answers every query with the same address.
    import socket as _socket
    import subprocess as _sub
    import threading as _threading

    stub = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM)
    stub.bind(("127.0.0.1", 0))
    stub_port = stub.getsockname()[1]
    stub.settimeout(20.0)

    def _namensdienst():
        """Answers every A query with 93.184.216.34."""
        try:
            frage, absender = stub.recvfrom(512)
        except OSError:
            return
        # mirror the header, set the response bit, announce one answer
        antwort = bytearray(frage[:2]) + bytes([0x81, 0x80, 0, 1, 0, 1, 0, 0, 0, 0])
        antwort += frage[12:]                       # the same question back
        antwort += bytes([0xC0, 0x0C,               # pointer to the name
                          0, 1, 0, 1,               # type A, class IN
                          0, 0, 0, 60,              # TTL
                          0, 4, 93, 184, 216, 34])
        stub.sendto(bytes(antwort), absender)

    _threading.Thread(target=_namensdienst, daemon=True).start()
    router = _sub.Popen([sys.executable, "-u", os.path.join(ROOT, "router.py"),
                         "--dns", f"127.0.0.1:{stub_port}"],
                        cwd=ROOT, stdout=_sub.DEVNULL, stderr=_sub.DEVNULL)
    try:
        L.warte(1.0)
        L.eingabe("ping 10.0.0.254|ENTER", 6.0)
        pruefe("The router answers PING", "answered" in L.bild()
               and "0 of 4" not in L.bild(), L.bild())
        L.eingabe("host test.example|ENTER", 6.0)
        pruefe("DNS: a name resolves to an address",
               "93.184.216.34" in L.bild(), L.bild())

        # TCP: a real connection to a real server -- handshake, sequence
        # numbers, acknowledgments, teardown. The server runs here on the
        # Mac, so the test can run without internet access.
        from http.server import BaseHTTPRequestHandler, HTTPServer

        class _Seite(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path.startswith("/a"):
                    inhalt = (b"<html><head><title>Test</title></head><body>"
                              b"<h1>First page</h1><p>A paragraph with a "
                              b"<a href=\"/b\">link</a> in it.</p></body></html>")
                elif self.path.startswith("/b"):
                    inhalt = b"<html><body><h1>Second page</h1></body></html>"
                else:
                    inhalt = b"<html><body>TB-32 can do TCP</body></html>"
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.send_header("Content-Length", str(len(inhalt)))
                self.end_headers()
                self.wfile.write(inhalt)

            def log_message(self, *_):
                pass

        web = HTTPServer(("127.0.0.1", 0), _Seite)
        _threading.Thread(target=web.serve_forever, daemon=True).start()
        L.eingabe(f"fetch 127.0.0.1:{web.server_port} /|ENTER", 14.0)
        pruefe("TCP: a page is really fetched",
               "TB-32 can do TCP" in L.bild(), L.bild())
        pruefe("TCP: the server responds with 200",
               "200 OK" in L.bild(), L.bild())
        # --- The proxy --------------------------------------------------
        # It's the path to HTTPS: the TB-32 tells it in plain HTTP what to
        # fetch. This tests the chain (unencrypted, so the test can run
        # without internet access) -- that it can also do TLS is Python's
        # job and needs no proof inside the TB-32.
        with _socket.socket() as _s:
            _s.bind(("127.0.0.1", 0))
            proxy_port = _s.getsockname()[1]
        vermittler = _sub.Popen([sys.executable, "-u",
                                 os.path.join(ROOT, "proxy.py"), str(proxy_port)],
                                cwd=ROOT, stdout=_sub.DEVNULL, stderr=_sub.DEVNULL)
        L.warte(1.5)
        L.eingabe(f"net proxy 127.0.0.1:{proxy_port}|ENTER", 1.0)
        pruefe("Proxy is set", "8080" in L.bild()
               or str(proxy_port) in L.bild(), L.bild())
        L.eingabe(f"fetch 127.0.0.1:{web.server_port} /a|ENTER", 14.0)
        pruefe("The proxy fetches the page",
               "First page" in L.bild(), L.bild())
        L.eingabe("net proxy off|ENTER", 1.0)
        vermittler.terminate()
        vermittler.wait(timeout=5)

        # --- The browser ---------------------------------------------------
        # Two pages on the Mac: the first links to the second. This tests
        # what makes a browser a browser -- reading HTML, wrapping text,
        # rendering, and following a link.
        L.eingabe("WIN|ENTER", 3.0)
        menue(1)                                     # Start > Browser
        L.warte(1.5)
        L.eingabe("|".join(["BACKSPACE"] * 20), 0.5)   # clear the address bar
        L.eingabe(f"127.0.0.1:{web.server_port}/a|ENTER", 1.0)
        sym = symbole()
        # Wait until the page is loaded -- a fixed wait time would be
        # dishonest here: how long DNS, connection, and transfer take
        # depends on how the host machine is doing that day.
        anzahl = 0
        for _ in range(int(25.0 / L.dt)):
            L.m.run_slice(L.dt)
            anzahl = wort(L.m, sym["br_anzahl"])
            if anzahl > 0:
                break
        L.warte(1.0)
        anzahl = wort(L.m, sym["br_anzahl"])
        pruefe("Browser renders a page", anzahl > 0, f"Lines: {anzahl}")
        pruefe("Browser recognizes the heading",
               "First page" in seitentext(L.m, sym), seitentext(L.m, sym))
        pruefe("Browser recognizes the link",
               wort(L.m, sym["br_linkanzahl"]) >= 1)

        # Click the line with the link. Where the line sits on screen isn't
        # guessed but computed from the window itself: win_x/win_y of the
        # topmost window plus the layout from app_browser.
        zeile = linkzeile(L.m, sym)
        oben = wort(L.m, sym["win_top"])
        wx = wort(L.m, sym["win_x"] + oben * 4)
        wy = wort(L.m, sym["win_y"] + oben * 4)
        titel_h = int(_re.search(r"define TITLE_H\s+(\d+)",
                                 open(os.path.join(ROOT, "system", "gui.c")).read()).group(1))
        if zeile >= 0:
            x, y = wx + 30, wy + titel_h + 36 + zeile * 10 + 4
            L.m.mouse.move(x, y, 0); L.warte(0.3)
            L.m.mouse.move(x, y, 1); L.warte(0.3)
            L.m.mouse.move(x, y, 0); L.warte(0.5)
        for _ in range(int(25.0 / L.dt)):
            L.m.run_slice(L.dt)
            if "Second page" in seitentext(L.m, sym):
                break
        pruefe("A click on the link fetches the next page",
               "Second page" in seitentext(L.m, sym), seitentext(L.m, sym))
        web.shutdown()

        # --- The window server ---------------------------------------------------
        # A program from disk gets its own window on the desktop. This
        # tests the whole chain: creating a window, painting into its own
        # buffer, receiving events, closing itself.
        def fremde():
            return [wort(L.m, sym["win_type"] + i * 4) for i in range(6)].count(18)

        menue_prog("PROMPT.TBX")                     # the command line
        L.warte(2.5)
        pruefe("The command line is its own program", fremde() >= 1)
        vorher = fremde()

        L.eingabe("start fenster.tbx /b|ENTER", 1.0)
        for _ in range(int(30.0 / L.dt)):
            L.m.run_slice(L.dt)
            if fremde() > vorher:
                break
        pruefe("Program gets its own window", fremde() > vorher)
        typen = [wort(L.m, sym["win_type"] + i * 4) for i in range(6)]
        nr = wort(L.m, sym["win_top"])
        gemalt = sum(L.m.bus.read_block(0x00800000 + nr * 0x00040000, 4000))
        pruefe("It paints into its own buffer", gemalt > 0, f"Sum {gemalt}")
        pruefe("The screen stays intact",
               sum(L.m.vga.gfx[:64000]) > 0)
        L.eingabe("ESC", 5.0)                        # the program terminates itself
        pruefe("And it cleans up its own window", fremde() == vorher,
               str([wort(L.m, sym["win_type"] + i * 4) for i in range(6)]))

        # And a real program: the calculator, formerly fullscreen.
        menue_prog("PROMPT.TBX")                     # bring input back to the front
        L.warte(1.5)
        L.eingabe("start calc.tbx /b|ENTER", 1.0)
        for _ in range(int(30.0 / L.dt)):
            L.m.run_slice(L.dt)
            if fremde() > vorher:
                break
        pruefe("The calculator runs in the window", fremde() > vorher)
        pruefe("The desktop remains standing",
               sum(L.m.vga.gfx[:64000]) > 0)
        L.eingabe("ESC", 5.0)

        menue_fest(1)                                # back to the console
        L.warte(2.0)
    finally:
        router.terminate()
        router.wait(timeout=5)
        stub.close()

    print("\n--- Programs from disk -----------------------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("START MEMTEST.TBX|ENTER", 7.0)
    bild = L.bild()
    pruefe("Program loads and runs through correctly",
           "PASS" in bild, "Memory test should report PASS")
    L.eingabe("ENTER", 1.2)                       # quit the program
    pruefe("Program returns cleanly to the shell",
           L.bild().rstrip().endswith("A:\\>"))

    print("\n--- Self-built tools on the device ---------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("CD SOURCE|ENTER", 0.6)
    L.eingabe("ASM HELLO.ASM HELLO.TBX|ENTER", 3.0)
    bild = L.bild()
    pruefe("Assembler runs on the TB-32 itself", "Created HELLO.TBX" in bild)
    L.eingabe("HELLO|ENTER", 2.0)
    pruefe("Self-assembled program runs",
           "Hello from a program written ON the TB-32" in L.bild())
    L.eingabe("ENTER", 1.0)

    L.eingabe("CD SOURCE|ENTER", 0.6)
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("CC T2.C T2.TBX|ENTER", 5.0)
    pruefe("C compiler compiles on the TB-32 itself",
           "Created T2.TBX" in L.bild())
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("T2|ENTER", 4.0)
    bild = L.bild()
    pruefe("Self-compiled C program computes correctly",
           "285" in bild and "a after pointer write = 42" in bild)
    pruefe("Pointers, arrays, and logic in the generated code",
           "and-ok or-ok not-ok" in bild and "strlen = 19" in bild
           and "ABCDE" in bild)
    L.eingabe("ENTER", 1.0)

    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("PY TEST.PY|ENTER", 6.0)
    bild = L.bild()
    pruefe("Python interpreter computes correctly",
           "7 * 6 = 42" in bild and "10! = 3628800" in bild)
    pruefe("Python handles lists and loops",
           "[3, 1, 4, 1, 5, 9, 2, 6]" in bild and "Largest: 9" in bild)

    L.eingabe("DEL T2.TBX|ENTER", 0.8)
    L.eingabe("CD \\|ENTER", 0.6)

    print("\n--- Scrolling back (Scrollback) --------------------------------")
    L.eingabe("PGUP", 1.2)
    pruefe("PgUp opens the screen history", "SCROLLBACK" in L.bild())
    L.eingabe("ESC", 1.0)
    pruefe("ESC returns to the prompt", "SCROLLBACK" not in L.bild())

    print("\n--- Multitasking -----------------------------------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("START BENCH.TBX /B|ENTER", 0.8)
    L.eingabe("TASKLIST|ENTER", 1.2)
    bild = L.bild()
    pruefe("Background process in the process list",
           "shell" in bild and "BENCH" in bild)
    pruefe("Context switches take place", "Context switches" in bild)
    pruefe("Process really keeps running in parallel",
           "Running" in bild or "Ready" in bild)
    L.eingabe("TASKKILL 1|ENTER", 1.2)
    pruefe("Process can be terminated", "terminated" in L.bild())

    print("\n--- Terminal and editor in a window -----------------------------")
    # The command line is now a program: the shell stays in the kernel,
    # the window comes from disk.
    L.eingabe("WIN|ENTER", 3.0)
    menue_prog("PROMPT.TBX")
    L.warte(2.0)
    ram = L.m.bus.ram
    term = "".join(chr(ram[0x00120000 + i * 2]) for i in range(70 * 3))
    pruefe("Command line runs as a window", "command prompt" in term.lower())
    for ch in "VER":
        L.m.keyboard.push(ord(ch), 0); L.warte(0.2)
    L.m.keyboard.push(13, dev.KEY_ENTER); L.warte(2.0)
    term = "".join(chr(ram[0x00120000 + i * 2]) for i in range(70 * 8))
    pruefe("Commands in the terminal window", "TOOBAD-OS Version" in term)

    L.eingabe("start coder.tbx /b|ENTER", 1.0)
    for _ in range(int(25.0 / L.dt)):
        L.m.run_slice(L.dt)
        if 18 in [wort(L.m, symbole()["win_type"] + i * 4) for i in range(6)]:
            break
    pruefe("The Coder runs as its own program",
           18 in [wort(L.m, symbole()["win_type"] + i * 4) for i in range(6)])
    # ESC deliberately no longer exits the desktop (an accidentally
    # pressed key used to throw you out of your work and into the text
    # console). The way out is now the last menu entry "Exit desktop".
    menue_fest(1)                                # Exit desktop
    L.warte(1.5)

    print("\n--- Graphics and window system -----------------------------------")
    L.eingabe("WIN|ENTER", 2.5)
    vga = L.m.vga
    pruefe("Graphics mode active", vga.mode == 1)
    pruefe("Screen has been painted", sum(vga.gfx[:64000]) > 0)
    pruefe("Hardware mouse cursor enabled", vga.mcur_on == 1)
    for x, y in ((25, 387), (60, MENU_TOP + 6)):  # Start -> File Manager
        L.m.mouse.move(x, y, 0); L.warte(0.2)
        L.m.mouse.move(x, y, 1); L.warte(0.3)
        L.m.mouse.move(x, y, 0); L.warte(0.8)
    pruefe("Start menu opens a window",
           sum(vga.gfx[100 * 640:300 * 640]) > 0)
    menue_fest(1)                                # Exit desktop
    L.warte(1.5)
    pruefe("Return to text mode", vga.mode == 0)

    print("\n--- Shutting down ------------------------------------------------")
    L.eingabe("SHUTDOWN|ENTER", 3.0)
    pruefe("Operating system shuts the machine down", not L.m.running)

    gut = sum(1 for _, ok in ergebnisse if ok)
    alle = len(ergebnisse)
    farbe = GRUEN if gut == alle else (GELB if gut > alle * 0.8 else ROT)
    print(f"\n{farbe}{gut}/{alle} checks passed{WEG}")
    L.m.shutdown()
    return 0 if gut == alle else 1


if __name__ == "__main__":
    sys.exit(main())
