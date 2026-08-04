#!/usr/bin/env python3
"""
TOOBAD TB-32  --  der virtuelle PC

Diese Datei ist das GEHÄUSE: Monitor, Tastatur, Maus, Lautsprecher.
Sie enthält keinerlei Logik des Rechners selbst -- die läuft komplett als
Maschinencode auf der emulierten CPU (siehe hardware/ und firmware/).

    python3 pc.py               normal starten
    python3 pc.py --scale 3     größer
    python3 pc.py --turbo       so schnell wie der Mac kann
"""

import os
import sys
import time
import struct
import subprocess

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")
import pygame

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from hardware.machine import Machine, CPU_SPEED_NAMES
from hardware.devices import VGA
from hardware import devices as dev
from hardware.isa import GFX_W, GFX_H

CHAR_W, CHAR_H = 8, 16
CLIP_BUF = 0x00130000            # Zwischenablage von TOOBAD-OS (siehe gui.c)
SCR_COLS, SCR_ROWS = 80, 25
SCREEN_W, SCREEN_H = SCR_COLS * CHAR_W, SCR_ROWS * CHAR_H     # 640 x 400
SCR_LINE_CELLS = SCR_COLS * 2                                 # Bytes je Textzeile

# ---------------------------------------------------------------------------
# CP437 -> Unicode: der Zeichensatz, den PCs seit 1981 im Textmodus benutzen
# ---------------------------------------------------------------------------

CP437 = (
    "\x00☺☻♥♦♣♠•◘○◙♂♀♪♫☼"
    "►◄↕‼¶§▬↨↑↓→←∟↔▲▼"
    " !\"#$%&'()*+,-./"
    "0123456789:;<=>?"
    "@ABCDEFGHIJKLMNO"
    "PQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmno"
    "pqrstuvwxyz{|}~⌂"
    "ÇüéâäàåçêëèïîìÄÅ"
    "ÉæÆôöòûùÿÖÜ¢£¥₧ƒ"
    "áíóúñÑªº¿⌐¬½¼¡«»"
    "░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
    "└┴┬├─┼╞╟╚╔╩╦╠═╬╧"
    "╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
    "αßΓπΣσµτΦΘΩδ∞φε∩"
    "≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ "
)

# Zeichen, die wir lieber selbst malen als dem Font zu überlassen --
# so sehen Rahmen und Blöcke garantiert lückenlos aus.
BLOCK_GLYPHS = {
    0xDB: (0.0, 0.0, 1.0, 1.0),      # Vollblock
    0xDC: (0.0, 0.5, 1.0, 0.5),      # untere Hälfte
    0xDF: (0.0, 0.0, 1.0, 0.5),      # obere Hälfte
    0xDD: (0.0, 0.0, 0.5, 1.0),      # linke Hälfte
    0xDE: (0.5, 0.0, 0.5, 1.0),      # rechte Hälfte
}


class Monitor:
    """Zeichnet den Bildspeicher der virtuellen Grafikkarte auf den echten."""

    def __init__(self, scale):
        self.scale = scale
        self.surface = pygame.Surface((SCREEN_W, SCREEN_H))
        self.font = self._load_font()
        self.glyph_cache = {}
        self.prev_text = None
        self.blink = True

    def _load_font(self):
        for name in ("Menlo", "DejaVu Sans Mono", "Consolas", "Courier New", "monospace"):
            try:
                f = pygame.font.SysFont(name, CHAR_H - 2)
                if f.size("M")[0] > 0:
                    return f
            except Exception:
                continue
        return pygame.font.Font(None, CHAR_H)

    def glyph(self, code, fg, bg, palette):
        key = (code, fg, bg)
        cached = self.glyph_cache.get(key)
        if cached is not None:
            return cached
        surf = pygame.Surface((CHAR_W, CHAR_H))
        bgc = palette[bg]
        fgc = palette[fg]
        surf.fill(((bgc >> 16) & 0xFF, (bgc >> 8) & 0xFF, bgc & 0xFF))
        color = ((fgc >> 16) & 0xFF, (fgc >> 8) & 0xFF, fgc & 0xFF)

        if code in BLOCK_GLYPHS:
            x, y, w, h = BLOCK_GLYPHS[code]
            surf.fill(color, pygame.Rect(int(x * CHAR_W), int(y * CHAR_H),
                                         int(w * CHAR_W), int(h * CHAR_H)))
        elif code == 0xB0:                       # Schattierung: Punktraster
            for yy in range(0, CHAR_H, 2):
                for xx in range((yy // 2) % 2, CHAR_W, 2):
                    surf.set_at((xx, yy), color)
        elif code not in (0, 32):
            ch = CP437[code] if code < len(CP437) else "?"
            img = self.font.render(ch, True, color)
            surf.blit(img, ((CHAR_W - img.get_width()) // 2,
                            (CHAR_H - img.get_height()) // 2))
        self.glyph_cache[key] = surf
        return surf

    def draw_text(self, vga, force=False, text=None, cursor=True):
        if text is None:
            text = vga.text
        prev = self.prev_text
        pal = vga.palette
        for i in range(0, SCR_COLS * SCR_ROWS * 2, 2):
            if not force and prev is not None and \
               prev[i] == text[i] and prev[i + 1] == text[i + 1]:
                continue
            cell = i >> 1
            attr = text[i + 1]
            g = self.glyph(text[i], attr & 0x0F, (attr >> 4) & 0x0F, pal)
            self.surface.blit(g, ((cell % SCR_COLS) * CHAR_W,
                                  (cell // SCR_COLS) * CHAR_H))
        self.prev_text = bytes(text)

        cur = vga.cursor
        if cursor and self.blink and cur < SCR_COLS * SCR_ROWS:
            x, y = (cur % SCR_COLS) * CHAR_W, (cur // SCR_COLS) * CHAR_H
            attr = text[cur * 2 + 1]
            c = vga.palette[attr & 0x0F]
            pygame.draw.rect(self.surface, ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF),
                             pygame.Rect(x, y + CHAR_H - 3, CHAR_W, 2))
            self.prev_text = None                # Cursorzelle nächstes Mal neu malen

    def draw_gfx(self, vga):
        img = pygame.image.frombuffer(bytes(vga.gfx_sicht), (GFX_W, GFX_H), "P")
        img.set_palette([((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)
                         for c in vga.palette])
        self.surface.blit(img, (0, 0))
        if vga.mcur_on:
            self.draw_pointer(vga, vga.mcur_x, vga.mcur_y)
        self.prev_text = None

    # Mauszeiger: die Grafikkarte legt ihn über das Bild, wie ein echter
    # Hardware-Cursor. Das Betriebssystem muss ihn deshalb nicht wegradieren.
    POINTER = [
        "X           ", "XX          ", "XoX         ", "XooX        ",
        "XoooX       ", "XooooX      ", "XoooooX     ", "XooooooX    ",
        "XoooooooX   ", "XooooooooX  ", "XoooooXXXXX ", "XooXooX     ",
        "XoX XooX    ", "XX  XooX    ", "X    XooX   ", "     XooX   ",
        "      XX    ",
    ]

    def draw_pointer(self, vga, mx, my):
        for dy, row in enumerate(self.POINTER):
            for dx, ch in enumerate(row):
                if ch == " ":
                    continue
                x, y = mx + dx, my + dy
                if 0 <= x < GFX_W and 0 <= y < GFX_H:
                    self.surface.set_at((x, y), (0, 0, 0) if ch == "X"
                                        else (255, 255, 255))

    def render(self, vga, target, force=False, history=None, view=None):
        if history is not None:
            self.draw_text(vga, True, text=history, cursor=False)
        elif vga.mode == VGA.MODE_TEXT:
            self.draw_text(vga, force)
        else:
            self.draw_gfx(vga)
        if view is None:
            view = target.get_rect()
        if view.size == (SCREEN_W, SCREEN_H):
            target.blit(self.surface, view.topleft)
        else:
            target.blit(pygame.transform.scale(self.surface, view.size),
                        view.topleft)


class PCSpeaker:
    """Der kleine Piepser auf dem Mainboard -- eine reine Rechteckwelle."""

    RATE = 22050

    def __init__(self):
        self.ok = False
        self.cache = {}
        self.current = None
        try:
            pygame.mixer.init(frequency=self.RATE, size=-16, channels=1, buffer=512)
            self.channel = pygame.mixer.Channel(0)
            self.ok = True
        except Exception:
            pass

    def tone(self, freq):
        snd = self.cache.get(freq)
        if snd is None:
            n = self.RATE // 4
            period = max(2, self.RATE // max(20, freq))
            amp = 6000
            data = bytearray()
            for i in range(n):
                v = amp if (i % period) < period // 2 else -amp
                data += struct.pack("<h", v)
            snd = pygame.mixer.Sound(buffer=bytes(data))
            self.cache[freq] = snd
        return snd

    def update(self, spk):
        if not self.ok or not spk.changed:
            return
        spk.changed = False
        if spk.on:
            self.channel.play(self.tone(spk.freq), loops=-1)
            self.current = spk.freq
        else:
            self.channel.stop()
            self.current = None


# --- Tastatur: pygame-Tasten auf PC-Scancodes abbilden ---------------------

# Sondertasten kommen über KEYDOWN, alle druckbaren Zeichen über TEXTINPUT.
# Grund: SDL liefert bei KEYDOWN das Zeichen noch nicht zuverlässig mit --
# je nach Tastaturlayout steht event.unicode dort leer oder trägt das
# Zeichen des vorigen Anschlags. Das Text-Ereignis ist die richtige Quelle
# und kennt auch Umlaute und Sonderzeichen des eingestellten Layouts.
SCANCODES = {
    pygame.K_ESCAPE: dev.KEY_ESC, pygame.K_RETURN: dev.KEY_ENTER,
    pygame.K_KP_ENTER: dev.KEY_ENTER,
    pygame.K_BACKSPACE: dev.KEY_BACKSPACE, pygame.K_TAB: dev.KEY_TAB,
    pygame.K_UP: dev.KEY_UP, pygame.K_DOWN: dev.KEY_DOWN,
    pygame.K_LEFT: dev.KEY_LEFT, pygame.K_RIGHT: dev.KEY_RIGHT,
    pygame.K_F1: dev.KEY_F1, pygame.K_F2: dev.KEY_F2,
    pygame.K_F5: dev.KEY_F5, pygame.K_F10: dev.KEY_F10,
    pygame.K_DELETE: dev.KEY_DEL, pygame.K_HOME: dev.KEY_HOME,
    pygame.K_END: dev.KEY_END, pygame.K_PAGEUP: dev.KEY_PGUP,
    pygame.K_PAGEDOWN: dev.KEY_PGDN, pygame.K_INSERT: dev.KEY_INS,
}

ASCII_FALLBACK = {
    pygame.K_RETURN: 13, pygame.K_KP_ENTER: 13, pygame.K_BACKSPACE: 8,
    pygame.K_TAB: 9, pygame.K_ESCAPE: 27,
}

# Diese Tasten wiederholen sich, solange man sie haelt.
WIEDERHOLBAR = (pygame.K_BACKSPACE, pygame.K_DELETE, pygame.K_UP,
                pygame.K_DOWN, pygame.K_LEFT, pygame.K_RIGHT,
                pygame.K_PAGEUP, pygame.K_PAGEDOWN)
WDH_START = 0.40        # so lange muss man halten, bis es losgeht
WDH_TAKT  = 0.03        # danach so oft je Sekunde

# Bedenkzeit beim Einschalten -- die Sekunden zwischen Knopfdruck und
# erstem Bild, in denen ein echtes Board Netzteil, Lüfter und Chipsatz
# hochfährt.
#
# Sie steht hier im GEHÄUSE und nicht in der Firmware. Das ist der ganze
# Punkt: so gilt sie für **jedes** BIOS. Wer sein eigenes flasht, kann sie
# nicht weglassen -- auch nicht aus Versehen. Ein BIOS, das sofort in den
# Bootsektor springt, bekommt sie trotzdem.
#
# Tasten, die in dieser Zeit gedrückt werden, gehen nicht verloren: sie
# warten und werden dem Rechner gereicht, sobald er läuft. Wer also gleich
# beim Einschalten DEL drückt, landet im Setup -- vorausgesetzt, sein BIOS
# hat eins.
EINSCHALT_HALT_S = 5.0

# ---------------------------------------------------------------------------
# Zurückblättern im Fenster
#
# Das BIOS hebt jede Zeile, die oben aus dem Bild läuft, in einem Ringpuffer
# im Arbeitsspeicher auf (siehe firmware/video.asm, sb_push). Von dort holt
# sich das Fenster die alten Zeilen -- genauso, wie ein Terminal-Programm auf
# dem Mac seinen Scrollback verwaltet. Der virtuelle Rechner merkt davon
# nichts und läuft ungestört weiter.
# ---------------------------------------------------------------------------

SB_BASE = 0x00100000
SB_LINES = 512
SB_LINESIZE = 160
BDA_SBHEAD = 0x000004A8
BDA_SBCOUNT = 0x000004AC


def _u32(ram, addr):
    return int.from_bytes(ram[addr:addr + 4], "little")


def sb_count(m):
    return min(SB_LINES, _u32(m.bus.ram, BDA_SBCOUNT))


def sb_line(m, i):
    """Zeile i der Historie (0 = älteste) als 160 Bytes Zellen."""
    ram = m.bus.ram
    if _u32(ram, BDA_SBCOUNT) >= SB_LINES:
        pos = (_u32(ram, BDA_SBHEAD) + i) % SB_LINES
    else:
        pos = i
    off = SB_BASE + pos * SB_LINESIZE
    return ram[off:off + SB_LINESIZE]


def build_history_view(m, offset):
    """Baut ein volles 80x25-Bild: Historie plus aktueller Bildschirm,
    um <offset> Zeilen zurückgeblättert."""
    count = sb_count(m)
    total = count + SCR_ROWS
    start = max(0, total - SCR_ROWS - offset)
    leer = bytes([0x20, 0x07] * SCR_COLS)

    out = bytearray()
    for r in range(SCR_ROWS):
        idx = start + r
        if idx < count:
            out += sb_line(m, idx)
        elif idx < total:
            k = idx - count
            out += m.vga.text[k * SB_LINESIZE:(k + 1) * SB_LINESIZE]
        else:
            out += leer

    hinweis = (f" SCROLLBACK   line {start + 1} of {total}   "
               f"mouse wheel or Shift+PgUp/PgDn   any key returns ")
    hinweis = hinweis[:SCR_COLS].ljust(SCR_COLS)
    zeile = 24 * SCR_LINE_CELLS
    for i, ch in enumerate(hinweis):
        out[zeile + i * 2] = ord(ch) & 0xFF
        out[zeile + i * 2 + 1] = 0x70
    return out


def bildflaeche(fenster):
    """Groesstmoegliches 640x400-Rechteck im Fenster, mittig."""
    bw, bh = fenster
    f = min(bw / SCREEN_W, bh / SCREEN_H)
    w, h = max(1, int(SCREEN_W * f)), max(1, int(SCREEN_H * f))
    return pygame.Rect((bw - w) // 2, (bh - h) // 2, w, h)


_symbole = {}


def kernel_symbol(name):
    """Adresse einer Kernelvariablen aus system/kernel.sym.

    Die Tabelle entsteht beim Bauen und wird hier einmal eingelesen. So kann
    das Fenster in den Arbeitsspeicher des virtuellen Rechners hineinlangen --
    genau das braucht die gemeinsame Zwischenablage."""
    if not _symbole:
        try:
            with open(os.path.join(ROOT, "system", "kernel.sym")) as f:
                for zeile in f:
                    adr, sym = zeile.split()
                    _symbole[sym] = int(adr, 16)
        except Exception:
            return None
    return _symbole.get(name)


def gast_clipboard(m):
    """Liest die Zwischenablage von TOOBAD-OS aus dem Arbeitsspeicher."""
    a = kernel_symbol("clip_len")
    if a is None:
        return ""
    try:
        laenge = struct.unpack_from("<i", m.bus.ram, a)[0]
        if laenge <= 0 or laenge > 8192:
            return ""
        return bytes(m.bus.ram[CLIP_BUF:CLIP_BUF + laenge]).decode("latin-1")
    except Exception:
        return ""


def gast_clipboard_setzen(m, text):
    """Schreibt Text direkt in die Zwischenablage von TOOBAD-OS.

    Frueher wurden die Zeichen als Tastendruecke eingeschleust -- das war
    langsam, ging nur im Editor und verschluckte alles ausser Buchstaben.
    Direkt in den Puffer geschrieben, funktioniert Einfuegen ueberall dort,
    wo das System selbst einfuegt, samt Zeilenumbruechen."""
    a = kernel_symbol("clip_len")
    if a is None:
        return False
    roh = bytearray()
    for zeichen in text[:8000]:
        code = ord(zeichen)
        if code == 10 or code == 9 or 32 <= code < 127:
            roh.append(10 if code == 10 else code)
    try:
        m.bus.ram[CLIP_BUF:CLIP_BUF + len(roh)] = roh
        struct.pack_into("<i", m.bus.ram, a, len(roh))
        return True
    except Exception:
        return False


def mac_clipboard_get():
    """Text aus der macOS-Zwischenablage holen (leer, wenn es nicht klappt)."""
    try:
        return subprocess.run(["pbpaste"], capture_output=True, timeout=1,
                              text=True).stdout
    except Exception:
        return ""


def mac_clipboard_set(text):
    try:
        subprocess.run(["pbcopy"], input=text, text=True, timeout=1)
        return True
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Das Startbild des Mainboards
#
# Es gehört bewusst NICHT der Firmware. Ein Startbild, das im BIOS steckt,
# ist genau dann weg, wenn jemand sein eigenes flasht -- und dann gibt es
# auch keine Stelle mehr, an der man DEL drücken könnte. Deshalb malt es
# hier das Board, direkt in den Textbildspeicher, bevor die CPU überhaupt
# Strom bekommt. So sieht der Start bei JEDEM BIOS gleich aus.
#
# Das Einzige, was vom BIOS kommt, ist sein Name -- er steht in dessen Kopf
# auf Position 0x10 (siehe hardware/machine.py, rom_name).
# ---------------------------------------------------------------------------

WISCH_S  = 1.2          # so lange läuft das Blau von oben nach unten
NAME_S   = 1.5          # ab hier steht der Name da
PROMPT_S = 2.0          # ab hier die Zeile mit DEL

BILD_BLAU = 0x17        # grau auf blau
BILD_NAME = 0x1F        # weiß auf blau
BILD_HINT = 0x1B        # hellcyan auf blau


def rom_bytes(pfad):
    """Den BIOS-Chip auslesen -- nur für den Namen im Startbild."""
    try:
        with open(pfad, "rb") as f:
            return f.read()
    except OSError:
        return b""


def bild_schreiben(vga, y, text, attr):
    """Eine Zeile mittig in den Textbildspeicher legen."""
    x = max(0, (SCR_COLS - len(text)) // 2)
    for i, ch in enumerate(text[:SCR_COLS - x]):
        zelle = (y * SCR_COLS + x + i) * 2
        vga.text[zelle] = ord(ch) & 0xFF
        vga.text[zelle + 1] = attr


def startbild(vga, name, t, test):
    """Malt den Stand des Startbilds zum Zeitpunkt t (Sekunden seit dem
    Knopfdruck). Wird jedes Bild neu aufgerufen -- billiger als es zu
    merken, und der Monitor zeichnet ohnehin nur geänderte Zellen."""
    zeilen = SCR_ROWS if t >= WISCH_S else int(SCR_ROWS * t / WISCH_S)
    for y in range(zeilen):
        for x in range(SCR_COLS):
            zelle = (y * SCR_COLS + x) * 2
            vga.text[zelle] = 0x20
            vga.text[zelle + 1] = BILD_BLAU
    if t >= NAME_S:
        bild_schreiben(vga, SCR_ROWS // 2 - 1, name, BILD_NAME)
        if test:
            bild_schreiben(vga, SCR_ROWS // 2, "TEST IMAGE -- runs once", BILD_HINT)
    if t >= PROMPT_S:
        bild_schreiben(vga, SCR_ROWS // 2 + 1,
                       "Press DEL to enter SETUP", BILD_HINT)


def bios_datei_waehlen():
    """Öffnet den Dateidialog des Macs und gibt den Inhalt zurück.

    Das ist der USB-Stick beim BIOS-Flashback: eine Datei vom Wirtsrechner
    wird dem Board hingehalten. Deshalb sitzt es hier im Gehäuse und nicht
    im TB-32 -- ein Programm, das den Chip beschreibt, aus dem es gerade
    selbst seine Befehle holt, wäre keine gute Idee.

    Rückgabe: Bytes oder None (abgebrochen, oder nicht lesbar)."""
    # Ohne Dateityp-Filter: der wäre nur eine Bequemlichkeit, und wenn macOS
    # die Endung nicht kennt, kann man seine eigene Datei plötzlich nicht mehr
    # auswählen. Ob das Abbild taugt, entscheidet ohnehin die Firmware -- und
    # sie sagt es deutlich.
    skript = ('POSIX path of (choose file with prompt '
              '"BIOS-Abbild zum Flashen auswählen (.bin)")')
    try:
        r = subprocess.run(["osascript", "-e", skript],
                           capture_output=True, text=True, timeout=300)
    except Exception:
        return None
    pfad = r.stdout.strip()
    if r.returncode != 0 or not pfad:
        return None                       # abgebrochen
    try:
        with open(pfad, "rb") as f:
            return f.read()
    except OSError:
        return None


def main():
    scale = 2
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
    turbo = "--turbo" in sys.argv

    pygame.init()
    pygame.display.set_caption("TOOBAD TB-32")
    screen = pygame.display.set_mode((SCREEN_W * scale, SCREEN_H * scale),
                                     pygame.RESIZABLE)
    # Das Bild behaelt sein Seitenverhaeltnis; daneben bleiben schwarze Raender.
    view = bildflaeche(screen.get_size())
    clock = pygame.time.Clock()

    pygame.key.start_text_input()        # Zeichen über das Text-Ereignis holen
    monitor = Monitor(scale)
    speaker = PCSpeaker()
    m = Machine(ROOT)
    m.gehaeuse = True                         # Neustarts gehen durchs Startbild
    m.flash.waehler = bios_datei_waehlen      # Setup > Firmware > Flash BIOS

    overlay = False
    # Auch der allererste Start geht durch die Bedenkzeit -- sonst wäre der
    # Rechner beim Programmstart schon an, und genau das soll er nicht sein.
    kaltstart = time.perf_counter() + EINSCHALT_HALT_S
    vorrat = []                          # Tasten aus der Bedenkzeit
    bios_name = Machine.rom_name(rom_bytes(m.rom_path)) or "UNNAMED BIOS"
    m.vga.mode = VGA.MODE_TEXT
    m.vga.clear_text(0x00)
    m.vga.cursor = SCR_COLS * SCR_ROWS   # kein blinkender Strich im Startbild
    scrollback = 0                       # 0 = live, sonst Zeilen zurück
    overlay_font = pygame.font.SysFont("Menlo", 13)
    blink_timer = 0.0
    ips_measured = 0
    # Tastenwiederholung: welche Sondertaste gerade gehalten wird und wann
    # sie das naechste Mal ausloest. Zeichen kommen ueber das Text-Ereignis
    # und wiederholen sich auf dem Mac ohnehin nicht -- hier geht es um
    # Loeschen und die Pfeile, wo Halten wirklich gebraucht wird.
    halten = None
    halten_zeit = 0.0
    maus_bits = 0                # Bit 0 links, 1 Mitte, 2 rechts
    ips_measured = 0
    rest_ms = 6.0                        # Startschaetzung fuers Zeichnen
    cpu_ms = 0.0
    rahmen_start = time.perf_counter()
    last = time.perf_counter()
    running = True

    while running:
        now = time.perf_counter()
        dt = min(0.05, now - last)
        last = now

        # Hat sich das System selbst neu gestartet? Dann faehrt es genauso
        # hoch wie nach dem Einschalten -- mit Startbild und Bedenkzeit.
        if m.neustart_wunsch and not m.running and kaltstart is None:
            m.neustart_wunsch = False
            kaltstart = now + EINSCHALT_HALT_S
            bios_name = (Machine.rom_name(m.flash.einmal
                         if m.flash.einmal is not None
                         else rom_bytes(m.rom_path)) or "UNNAMED BIOS")
            m.vga.mode = VGA.MODE_TEXT
            m.vga.clear_text(0x00)
            m.vga.cursor = SCR_COLS * SCR_ROWS

        # Die Bedenkzeit ist um: jetzt kommt Strom auf das Board.
        if kaltstart is not None and now >= kaltstart:
            kaltstart = None
            m.power_on()
            monitor.prev_text = None
            if m.rom_gerettet:
                print("Das BIOS-Abbild war unbrauchbar -- die Sicherung "
                      "wurde zurückgespielt (Dual BIOS).")
            # Was in der Bedenkzeit getippt wurde, bekommt der Rechner
            # jetzt. Erst jetzt, weil ein Tastatur-Interrupt bei stehender
            # CPU verpuffen würde -- die Taste läge dann im Baustein und
            # niemand holte sie ab.
            for a, sc in vorrat:
                m.keyboard.push(a, sc)
            vorrat = []

        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False
            elif e.type == pygame.VIDEORESIZE:
                view = bildflaeche((e.w, e.h))
                screen.fill((0, 0, 0))
                monitor.prev_text = None
            elif e.type == pygame.MOUSEWHEEL:
                if m.vga.mode == VGA.MODE_TEXT:
                    scrollback = max(0, min(sb_count(m), scrollback + e.y * 3))
                    monitor.prev_text = None
                else:
                    m.mouse.wheel += e.y          # Rad an die Oberfläche
            elif e.type == pygame.KEYDOWN:
                mods = pygame.key.get_mods()
                if mods & pygame.KMOD_SHIFT and e.key in (pygame.K_PAGEUP,
                                                          pygame.K_PAGEDOWN):
                    schritt = 20 if e.key == pygame.K_PAGEUP else -20
                    scrollback = max(0, min(sb_count(m), scrollback + schritt))
                    monitor.prev_text = None
                    continue
                if scrollback:                       # jede Taste holt uns zurück
                    scrollback = 0
                    monitor.prev_text = None
                if e.key == pygame.K_q and (mods & pygame.KMOD_META or mods & pygame.KMOD_CTRL):
                    running = False
                    continue
                if e.key == pygame.K_F12:
                    overlay = not overlay
                    continue
                if e.key == pygame.K_F11:
                    pygame.display.toggle_fullscreen()
                    continue
                # Einfuegen: Cmd+V und Strg+V tun dasselbe.
                #
                # Liegt auf dem Mac etwas in der Zwischenablage, wandert es
                # zuerst in die Zwischenablage von TOOBAD-OS -- danach faengt
                # das System selbst an zu arbeiten und fuegt es ein. Frueher
                # gab es hier zwei verschiedene Tasten fuer zwei verschiedene
                # Ablagen, und wer auf dem Mac kopiert und dann Strg+V
                # gedrueckt hat, bekam schlicht nichts.
                if e.key == pygame.K_v and (mods & pygame.KMOD_META
                                            or mods & pygame.KMOD_CTRL):
                    text = mac_clipboard_get()
                    if text:
                        gast_clipboard_setzen(m, text)
                    m.keyboard.push(22, 0)       # Strg+V an das System
                    continue
                # Cmd+C nimmt die Auswahl aus TOOBAD-OS mit zum Mac.
                # Cmd+C nimmt die Auswahl aus TOOBAD-OS mit zum Mac. Strg+C
                # kopiert im Gast -- damit danach auch etwas dasteht, holen
                # wir es gleich mit herueber, sobald der Gast fertig ist.
                if e.key == pygame.K_c and (mods & pygame.KMOD_META):
                    text = gast_clipboard(m)
                    if text:
                        mac_clipboard_set(text)
                    continue
                if e.key == pygame.K_r and (mods & pygame.KMOD_CTRL):
                    m.power_on()                     # Reset-Knopf am Gehäuse
                    monitor.prev_text = None
                    continue
                # Nur Sondertasten -- Buchstaben und Ziffern kommen als
                # Text-Ereignis, siehe unten.
                if (mods & pygame.KMOD_CTRL) and pygame.K_a <= e.key <= pygame.K_z:
                    m.keyboard.push(e.key - pygame.K_a + 1, 0)
                    continue
                sc = SCANCODES.get(e.key, 0)
                ch = ASCII_FALLBACK.get(e.key, 0)
                if sc or ch:
                    if not m.running:
                        # DEL in der Bedenkzeit ist der klassische Griff ins
                        # Setup. Aufheben statt wegwerfen -- weiterreichen
                        # kann man ihn erst, wenn die CPU laeuft.
                        if kaltstart is not None:
                            vorrat.append((ch, sc))
                        continue
                    m.keyboard.push(ch, sc)
                    if e.key in WIEDERHOLBAR:
                        halten = (e.key, sc, ch)
                        halten_zeit = now + WDH_START
            elif e.type == pygame.KEYUP:
                if halten and e.key == halten[0]:
                    halten = None
            elif e.type == pygame.TEXTINPUT:
                # Der Einschaltknopf am Gehäuse. Er muss hier stehen und
                # nicht bei KEYDOWN: dort ist `unicode` je nach Layout leer
                # oder trägt noch das Zeichen des vorigen Anschlags -- ein
                # Umlaut kommt nur über das Text-Ereignis zuverlässig an.
                #
                # Und er wirkt nicht sofort: erst kommt die Bedenkzeit,
                # wie bei einem echten Gerät zwischen Knopfdruck und erstem
                # Bild.
                if not m.running:
                    if kaltstart is None:
                        if e.text in ("ü", "Ü"):
                            kaltstart = now + EINSCHALT_HALT_S
                            # Der Name kommt aus dem Chip, der JETZT drin
                            # steckt -- er kann seit dem letzten Start ein
                            # anderer sein.
                            bios_name = (Machine.rom_name(m.flash.einmal
                                         if m.flash.einmal is not None
                                         else rom_bytes(m.rom_path))
                                         or "UNNAMED BIOS")
                            m.vga.mode = VGA.MODE_TEXT
                            m.vga.clear_text(0x00)
                            m.vga.cursor = SCR_COLS * SCR_ROWS
                    else:
                        for zeichen in e.text:          # in der Bedenkzeit
                            code = ord(zeichen)         # getippt: aufheben
                            if 32 <= code < 127:
                                vorrat.append((code, 0))
                    continue
                if scrollback:
                    scrollback = 0
                    monitor.prev_text = None
                for zeichen in e.text:
                    code = ord(zeichen)
                    if 32 <= code < 127:
                        m.keyboard.push(code, 0)
            elif e.type == pygame.MOUSEMOTION:
                mx = min(GFX_W - 1, max(0, (e.pos[0] - view.x) * GFX_W // view.w))
                my = min(GFX_H - 1, max(0, (e.pos[1] - view.y) * GFX_H // view.h))
                m.mouse.move(mx, my, maus_bits)
            elif e.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP):
                # Den Zustand aus den EREIGNISSEN führen, nicht aus
                # get_pressed(). Letzteres liefert je nach Plattform beim
                # Loslassen noch den alten Stand -- und der Rechtsklick kam
                # deshalb gar nicht erst beim TB-32 an.
                # pygame zählt 1 = links, 2 = Mitte, 3 = rechts;
                # der TB-32 erwartet Bit 0, 1, 2.
                if e.button in (1, 2, 3):
                    bit = 1 << (e.button - 1)
                    # Auf dem Mac ist Ctrl+Klick der übliche Rechtsklick --
                    # und bei manchen Trackpads der einzige, der ankommt.
                    if e.button == 1 and (pygame.key.get_mods() & pygame.KMOD_CTRL):
                        bit = 4
                    if e.type == pygame.MOUSEBUTTONDOWN:
                        maus_bits |= bit
                    else:
                        maus_bits &= ~bit
                    m.mouse.move(m.mouse.x, m.mouse.y, maus_bits)

        # Gehaltene Taste nachliefern
        if halten and now >= halten_zeit:
            m.keyboard.push(halten[2], halten[1])
            halten_zeit = now + WDH_TAKT

        # Zeitbudget der CPU: alles, was vom Bild uebrig bleibt.
        #
        # Frueher standen hier feste 8 von 16,7 Millisekunden -- also weniger
        # als die Haelfte, egal wie schnell das Zeichnen tatsaechlich war. Wir
        # messen jetzt, wie lange der Rest eines Bildes wirklich dauert
        # (Zeichnen, Ereignisse, Ton), und geben der CPU den Rest bis knapp
        # unter die Bilddauer. Damit bleibt das Fenster genauso fluessig,
        # der virtuelle Rechner wird aber deutlich schneller.
        budget_ms = 16.7 - rest_ms - 1.0
        if budget_ms < 5.0:  budget_ms = 5.0      # der CPU immer etwas goennen
        if budget_ms > 14.0: budget_ms = 14.0     # dem Fenster immer etwas lassen
        rahmen_start = time.perf_counter()
        n = m.run_slice(dt * (8 if turbo else 1),
                        max_ms=budget_ms + 2.0 if turbo else budget_ms)
        cpu_ms = (time.perf_counter() - rahmen_start) * 1000.0
        ips_measured = int(n / dt) if dt > 0 else 0

        if kaltstart is not None:
            startbild(m.vga, bios_name, EINSCHALT_HALT_S - (kaltstart - now),
                      m.flash.einmal is not None)

        blink_timer += dt
        if blink_timer > 0.5:
            blink_timer = 0.0
            monitor.blink = not monitor.blink

        speaker.update(m.speaker)
        if scrollback and m.vga.mode == VGA.MODE_TEXT:
            monitor.render(m.vga, screen, history=build_history_view(m, scrollback),
                           view=view)
        else:
            if scrollback:
                scrollback = 0                       # Grafikmodus: kein Blättern
            monitor.render(m.vga, screen, view=view)

        if overlay:
            soll = m.ips // 1000
            lines = [
                f"CPU {CPU_SPEED_NAMES[m.cmos.data[0x13]]}   "
                f"real {ips_measured//1000} von {soll} kIPS"
                f"{'   (Wirt zu langsam)' if ips_measured < soll * 800 else ''}",
                f"PC 0x{m.cpu.pc:08X}  SP 0x{m.cpu.r[15]:08X}  "
                f"{'HALT' if m.cpu.halted else 'RUN '}",
                f"Befehle gesamt: {m.total_instructions:,}",
                f"FPS {clock.get_fps():.0f}   Platte: "
                f"{'LESEN/SCHREIBEN' if m.disk.led else 'bereit'}",
                f"CPU {m.thermal.temp:.1f} \u00b0C   Lüfter {m.thermal.fan}%"
                + (f"   DROSSELT {m.thermal.throttle}%" if m.thermal.throttle else ""),
            ]
            if m.cpu.last_fault:
                lines.append(m.cpu.last_fault)
            y = 4
            for ln in lines:
                img = overlay_font.render(ln, True, (255, 255, 0), (0, 0, 0))
                screen.blit(img, (6, y))
                y += 16

        # --- Ausgeschaltet: der Bildschirm ist schwarz -----------------
        #
        # Frueher blieb das letzte Bild stehen und darueber lag ein roter
        # Balken. Das war praktisch, aber falsch: ein Monitor an einem
        # ausgeschalteten Rechner zeigt nichts. Jetzt ist er wirklich
        # schwarz -- und waehrend des Kaltstarts bleibt er es auch, so wie
        # in den Sekunden, in denen ein echter PC schon laeuft, aber noch
        # kein Bild schickt.
        # Ausgeschaltet heißt schwarz. Läuft dagegen gerade die Bedenkzeit,
        # steht das Startbild im Bildspeicher und der Monitor hat es eben
        # schon gezeichnet -- dann bleibt hier nichts zu tun.
        if not m.running and kaltstart is None:
            screen.fill((0, 0, 0))

        pygame.display.flip()
        # Wie viel Zeit hat alles ausser der CPU gebraucht? Das ist die
        # Grundlage fuer das Budget des naechsten Bildes -- geglaettet, damit
        # ein einzelner Ausreisser nicht sofort durchschlaegt.
        gesamt_ms = (time.perf_counter() - rahmen_start) * 1000.0
        rest_ms = rest_ms * 0.8 + (gesamt_ms - cpu_ms) * 0.2
        clock.tick(60)

    m.shutdown()
    pygame.quit()


if __name__ == "__main__":
    main()
