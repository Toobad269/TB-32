#!/usr/bin/env python3
"""
TOOBAD TB-32  --  the virtual PC

This file is the CASE: monitor, keyboard, mouse, speaker.
It contains none of the computer's own logic -- that runs entirely as
machine code on the emulated CPU (see hardware/ and firmware/).

    python3 pc.py               start normally
    python3 pc.py --scale 3     bigger
    python3 pc.py --turbo       as fast as the Mac can go
    python3 pc.py --kein-netz   without router and proxy
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
CLIP_BUF = 0x00130000            # clipboard of TOOBAD-OS (see gui.c)
SCR_COLS, SCR_ROWS = 80, 25
SCREEN_W, SCREEN_H = SCR_COLS * CHAR_W, SCR_ROWS * CHAR_H     # 640 x 400
SCR_LINE_CELLS = SCR_COLS * 2                                 # bytes per line of text

# ---------------------------------------------------------------------------
# CP437 -> Unicode: the character set PCs have used in text mode since 1981
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

# Characters we'd rather draw ourselves than leave to the font --
# this way borders and blocks are guaranteed to look seamless.
BLOCK_GLYPHS = {
    0xDB: (0.0, 0.0, 1.0, 1.0),      # full block
    0xDC: (0.0, 0.5, 1.0, 0.5),      # lower half
    0xDF: (0.0, 0.0, 1.0, 0.5),      # upper half
    0xDD: (0.0, 0.0, 0.5, 1.0),      # left half
    0xDE: (0.5, 0.0, 0.5, 1.0),      # right half
}


class Monitor:
    """Draws the virtual graphics card's framebuffer onto the real one."""

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
        elif code == 0xB0:                       # shading: dot pattern
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
            self.prev_text = None                # redraw cursor cell next time

    def draw_gfx(self, vga):
        img = pygame.image.frombuffer(bytes(vga.gfx_sicht), (GFX_W, GFX_H), "P")
        img.set_palette([((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)
                         for c in vga.palette])
        self.surface.blit(img, (0, 0))
        if vga.mcur_on:
            self.draw_pointer(vga, vga.mcur_x, vga.mcur_y)
        self.prev_text = None

    # Mouse pointer: the graphics card overlays it on the image, like a real
    # hardware cursor. The operating system therefore doesn't need to erase it.
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
    """The little beeper on the mainboard -- a plain square wave."""

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


# --- Keyboard: mapping pygame keys to PC scancodes ---------------------

# Special keys come through KEYDOWN, all printable characters through TEXTINPUT.
# Reason: SDL doesn't reliably supply the character with KEYDOWN --
# depending on the keyboard layout, event.unicode is empty there or carries
# the character of the previous keystroke. The text event is the correct source
# and also knows umlauts and special characters of the configured layout.
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

# These keys repeat as long as they're held.
WIEDERHOLBAR = (pygame.K_BACKSPACE, pygame.K_DELETE, pygame.K_UP,
                pygame.K_DOWN, pygame.K_LEFT, pygame.K_RIGHT,
                pygame.K_PAGEUP, pygame.K_PAGEDOWN)
WDH_START = 0.40        # how long you must hold before it starts repeating
WDH_TAKT  = 0.03        # then this many times per second

# Power-on delay -- the seconds between pressing the button and the
# first frame, during which a real board's power supply, fan, and chipset
# spin up.
#
# It lives here in the CASE and not in the firmware. That's the whole
# point: this way it applies to **every** BIOS. Anyone flashing their own
# can't leave it out -- not even by accident. A BIOS that jumps straight
# into the boot sector still gets it.
#
# Keys pressed during this time aren't lost: they wait and get handed to
# the machine as soon as it's running. So anyone who presses DEL right at
# power-on lands in Setup -- provided their BIOS has one.
EINSCHALT_HALT_S = 5.0

# ---------------------------------------------------------------------------
# Scrolling back in the window
#
# The BIOS keeps every line that scrolls off the top of the screen in a ring
# buffer in RAM (see firmware/video.asm, sb_push). The window fetches the old
# lines from there -- just like a terminal program on the Mac manages its
# scrollback. The virtual machine notices none of this and keeps running
# undisturbed.
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
    """Line i of the history (0 = oldest) as 160 bytes of cells."""
    ram = m.bus.ram
    if _u32(ram, BDA_SBCOUNT) >= SB_LINES:
        pos = (_u32(ram, BDA_SBHEAD) + i) % SB_LINES
    else:
        pos = i
    off = SB_BASE + pos * SB_LINESIZE
    return ram[off:off + SB_LINESIZE]


def build_history_view(m, offset):
    """Builds a full 80x25 frame: history plus the current screen,
    scrolled back by <offset> lines."""
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
    """Largest possible 640x400 rectangle in the window, centered."""
    bw, bh = fenster
    f = min(bw / SCREEN_W, bh / SCREEN_H)
    w, h = max(1, int(SCREEN_W * f)), max(1, int(SCREEN_H * f))
    return pygame.Rect((bw - w) // 2, (bh - h) // 2, w, h)


_symbole = {}


def kernel_symbol(name):
    """Address of a kernel variable from system/kernel.sym.

    The table is generated at build time and read in here once. This lets
    the window reach into the virtual machine's RAM --
    exactly what the shared clipboard needs."""
    if not _symbole:
        try:
            with open(os.path.join(ROOT, "system", "kernel.sym")) as f:
                for zeile in f:
                    adr, sym = zeile.split()
                    _symbole[sym] = int(adr, 16)
        except Exception:
            return None
    return _symbole.get(name)


ED_BUF = 0x000D0000              # text buffer of the Coder (see system/edit.c)
APP_EDITOR = 8                   # window type of the Coder (see system/gui.c)


WT_BUF = 0x00770000              # window text that the system prepares


def fenstertext_erbitten(m):
    """Asks TOOBAD-OS for the text of the topmost window.

    In graphics mode the screen holds pixels, not text -- the
    case can't read anything there. So it asks: it sets `wt_wunsch`,
    and the desktop puts the text down one loop iteration later.

    This way every program answers the question for itself. A new window
    needs one line in the system -- and none here, where pc.py would
    otherwise have to know far too much about the system."""
    a = kernel_symbol("wt_wunsch")
    if a is None:
        return False
    struct.pack_into("<i", m.bus.ram, a, 1)
    return True


def fenstertext_holen(m):
    """Is the requested text ready? Then return it as a string."""
    a = kernel_symbol("wt_wunsch")
    b = kernel_symbol("wt_len")
    if a is None or b is None:
        return None
    if struct.unpack_from("<i", m.bus.ram, a)[0] != 0:
        return None                       # not answered yet
    n = struct.unpack_from("<i", m.bus.ram, b)[0]
    if n <= 0 or n > 60000:
        return ""
    return bytes(m.bus.ram[WT_BUF:WT_BUF + n]).decode("latin-1")


def alles_kopieren(m):
    """Ctrl+K: everything visible into the Mac's clipboard -- silently.

    This belongs in the CASE and not in the system. In the BIOS and in Setup
    no operating system is running at all that could handle a key; from here
    it works everywhere, no matter what software is currently on the CPU.

    In text mode, the whole screen, line by line. In graphics mode, if the
    Coder is on top, its FULL text -- not just the visible portion, since
    that's exactly the point of copying.

    No feedback: no message to the guest, no blinking. Whoever presses the
    key knows what they wanted."""
    if m.vga.mode != VGA.MODE_TEXT:
        return None                       # the system will be asked, see above

    t = m.vga.text
    zeilen = []
    for y in range(SCR_ROWS):
        z = "".join(CP437[t[(y * SCR_COLS + x) * 2]] if t[(y * SCR_COLS + x) * 2] < len(CP437)
                    else " " for x in range(SCR_COLS))
        zeilen.append(z.rstrip())
    while zeilen and zeilen[-1] == "":
        zeilen.pop()
    return "\n".join(zeilen)


def gast_clipboard(m):
    """Reads TOOBAD-OS's clipboard from RAM."""
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
    """Writes text directly into TOOBAD-OS's clipboard.

    Previously the characters were injected as keystrokes -- that was
    slow, only worked in the editor, and swallowed everything except
    letters. Written directly into the buffer, pasting works anywhere
    the system itself pastes, including line breaks."""
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
    """Get text from the macOS clipboard (empty if it doesn't work)."""
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
# The mainboard's splash screen
#
# It deliberately does NOT belong to the firmware. A splash screen baked
# into the BIOS is gone the moment someone flashes their own -- and then
# there's no longer any place where you could press DEL. So it's the board
# that draws this here, directly into the text framebuffer, before the CPU
# even gets power. This way the boot looks the same for EVERY BIOS.
#
# The only thing that comes from the BIOS is its name -- it sits in its
# header at position 0x10 (see hardware/machine.py, rom_name).
# ---------------------------------------------------------------------------

WISCH_S  = 1.2          # how long the blue sweeps from top to bottom
NAME_S   = 1.5          # from here on the name is shown
PROMPT_S = 2.0          # from here on the line with DEL

BILD_BLAU = 0x17        # gray on blue
BILD_NAME = 0x1F        # white on blue
BILD_HINT = 0x1B        # light cyan on blue


def rom_bytes(pfad):
    """Read the BIOS chip -- only for the name in the splash screen."""
    try:
        with open(pfad, "rb") as f:
            return f.read()
    except OSError:
        return b""


def bild_schreiben(vga, y, text, attr):
    """Place a line centered in the text framebuffer."""
    x = max(0, (SCR_COLS - len(text)) // 2)
    for i, ch in enumerate(text[:SCR_COLS - x]):
        zelle = (y * SCR_COLS + x + i) * 2
        vga.text[zelle] = ord(ch) & 0xFF
        vga.text[zelle + 1] = attr


def startbild(vga, name, t, test):
    """Draws the state of the splash screen at time t (seconds since the
    button was pressed). Called anew every frame -- cheaper than
    remembering it, and the monitor only redraws changed cells anyway."""
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
    """Opens the Mac's file dialog and returns the contents.

    This is the USB stick for the BIOS flashback: a file from the host
    machine is handed to the board. That's why it lives here in the case
    and not in the TB-32 -- a program that writes to the very chip it's
    currently fetching its own instructions from would not be a good idea.

    Returns: bytes or None (cancelled, or unreadable)."""
    # No file-type filter: that would only be a convenience, and if macOS
    # doesn't recognize the extension, you'd suddenly no longer be able to
    # select your own file. Whether the image is any good is decided by the
    # firmware anyway -- and it says so clearly.
    skript = ('POSIX path of (choose file with prompt '
              '"Select BIOS image to flash (.bin)")')
    try:
        r = subprocess.run(["osascript", "-e", skript],
                           capture_output=True, text=True, timeout=300)
    except Exception:
        return None
    pfad = r.stdout.strip()
    if r.returncode != 0 or not pfad:
        return None                       # cancelled
    try:
        with open(pfad, "rb") as f:
            return f.read()
    except OSError:
        return None


def netz_starten():
    """Starts the router and proxy together, so a single call is enough.

    Both are pure Python and run in their own threads in the same process.
    The proxy port acts as the gatekeeper check: if a second TOOBAD window
    is already running, it's taken -- then this one starts neither proxy
    nor router. Two routers on the same wire would talk over each other
    when answering ARP.

    With --kein-netz everything stays off; then the TB-32 only talks to
    other TB-32s on the same machine."""
    if "--kein-netz" in sys.argv:
        print("Network: off (--kein-netz)")
        return
    import proxy
    import router
    if proxy.im_hintergrund(8080) is None:
        print("Network: already running in another window")
        return
    if router.im_hintergrund() is None:
        print("Network: couldn't plug in the wire")
        return
    print("Network: router 10.0.0.254, proxy on 8080 -- both are running")


def main():
    scale = 2
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
    turbo = "--turbo" in sys.argv

    netz_starten()

    pygame.init()
    pygame.display.set_caption("TOOBAD TB-32")
    screen = pygame.display.set_mode((SCREEN_W * scale, SCREEN_H * scale),
                                     pygame.RESIZABLE)
    # The image keeps its aspect ratio; black bars remain on the sides.
    view = bildflaeche(screen.get_size())
    clock = pygame.time.Clock()

    pygame.key.start_text_input()        # get characters via the text event
    monitor = Monitor(scale)
    speaker = PCSpeaker()
    m = Machine(ROOT)
    m.gehaeuse = True                         # restarts go through the splash screen
    m.flash.waehler = bios_datei_waehlen      # Setup > Firmware > Flash BIOS

    overlay = False
    # Even the very first startup goes through the power-on delay -- otherwise
    # the machine would already be on when the program starts, and that's
    # exactly what it shouldn't be.
    kaltstart = time.perf_counter() + EINSCHALT_HALT_S
    vorrat = []                          # keys from the power-on delay
    kopie_wartet = 0                     # how long to wait for the window text
    bios_name = Machine.rom_name(rom_bytes(m.rom_path)) or "UNNAMED BIOS"
    m.vga.mode = VGA.MODE_TEXT
    m.vga.clear_text(0x00)
    m.vga.cursor = SCR_COLS * SCR_ROWS   # no blinking cursor in the splash screen
    scrollback = 0                       # 0 = live, otherwise lines back
    overlay_font = pygame.font.SysFont("Menlo", 13)
    blink_timer = 0.0
    ips_measured = 0
    # Key repeat: which special key is currently held and when it next
    # fires. Characters come through the text event and don't repeat on
    # the Mac anyway -- this is about delete and the arrows, where holding
    # down is actually needed.
    halten = None
    halten_zeit = 0.0
    maus_bits = 0                # bit 0 left, 1 middle, 2 right
    ips_measured = 0
    rest_ms = 6.0                        # initial estimate for drawing
    cpu_ms = 0.0
    rahmen_start = time.perf_counter()
    last = time.perf_counter()
    running = True

    while running:
        now = time.perf_counter()
        dt = min(0.05, now - last)
        last = now

        # Did the system restart itself? Then it boots up just like after
        # power-on -- with the splash screen and the power-on delay.
        if m.neustart_wunsch and not m.running and kaltstart is None:
            m.neustart_wunsch = False
            kaltstart = now + EINSCHALT_HALT_S
            bios_name = (Machine.rom_name(m.flash.einmal
                         if m.flash.einmal is not None
                         else rom_bytes(m.rom_path)) or "UNNAMED BIOS")
            m.vga.mode = VGA.MODE_TEXT
            m.vga.clear_text(0x00)
            m.vga.cursor = SCR_COLS * SCR_ROWS

        # The power-on delay is over: now the board gets power.
        if kaltstart is not None and now >= kaltstart:
            kaltstart = None
            m.power_on()
            monitor.prev_text = None
            if m.rom_gerettet:
                print("The BIOS image was unusable -- the backup "
                      "was restored (Dual BIOS).")
            # Whatever was typed during the power-on delay, the machine gets
            # now. Only now, because a keyboard interrupt would fizzle with
            # the CPU stopped -- the key would then sit in the chip and
            # nobody would pick it up.
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
                    m.mouse.wheel += e.y          # wheel event to the UI
            elif e.type == pygame.KEYDOWN:
                mods = pygame.key.get_mods()
                if mods & pygame.KMOD_SHIFT and e.key in (pygame.K_PAGEUP,
                                                          pygame.K_PAGEDOWN):
                    schritt = 20 if e.key == pygame.K_PAGEUP else -20
                    scrollback = max(0, min(sb_count(m), scrollback + schritt))
                    monitor.prev_text = None
                    continue
                if scrollback:                       # any key brings us back
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
                # Paste: Cmd+V and Ctrl+V do the same thing.
                #
                # If something is on the Mac's clipboard, it first moves
                # into TOOBAD-OS's clipboard -- after that the system itself
                # starts working and pastes it. There used to be two
                # different keys here for two different clipboards, and
                # anyone who copied on the Mac and then pressed Ctrl+V
                # simply got nothing.
                if e.key == pygame.K_v and (mods & pygame.KMOD_META
                                            or mods & pygame.KMOD_CTRL):
                    text = mac_clipboard_get()
                    if text:
                        gast_clipboard_setzen(m, text)
                    m.keyboard.push(22, 0)       # Ctrl+V to the system
                    continue
                # Cmd+C takes the selection from TOOBAD-OS along to the Mac.
                # Cmd+C takes the selection from TOOBAD-OS along to the Mac. Ctrl+C
                # copies in the guest -- so that there's something there
                # afterward, we bring it over right away as soon as the guest is done.
                if e.key == pygame.K_c and (mods & pygame.KMOD_META):
                    text = gast_clipboard(m)
                    if text:
                        mac_clipboard_set(text)
                    continue
                # Ctrl+K -- copy everything, without saying a word about
                # it. Comes BEFORE the general Ctrl+letter rule,
                # otherwise it would go to the guest as a control character.
                if e.key == pygame.K_k and (mods & pygame.KMOD_CTRL
                                            or mods & pygame.KMOD_META):
                    text = alles_kopieren(m)
                    if text is None:              # graphics mode: ask the system
                        if fenstertext_erbitten(m):
                            kopie_wartet = now + 1.0
                    elif text:
                        mac_clipboard_set(text)
                    continue
                if e.key == pygame.K_r and (mods & pygame.KMOD_CTRL):
                    m.power_on()                     # reset button on the case
                    monitor.prev_text = None
                    continue
                # Only special keys -- letters and digits come through the
                # text event, see below.
                if (mods & pygame.KMOD_CTRL) and pygame.K_a <= e.key <= pygame.K_z:
                    m.keyboard.push(e.key - pygame.K_a + 1, 0)
                    continue
                sc = SCANCODES.get(e.key, 0)
                ch = ASCII_FALLBACK.get(e.key, 0)
                if sc or ch:
                    if not m.running:
                        # DEL during the power-on delay is the classic way into
                        # Setup. Hold onto it instead of discarding it -- it
                        # can only be passed along once the CPU is running.
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
                # The power button on the case. It has to live here and
                # not in KEYDOWN: there, `unicode` is empty depending on the
                # layout, or still carries the character of the previous
                # keystroke -- an umlaut only arrives reliably through the
                # text event.
                #
                # And it doesn't take effect immediately: first comes the
                # power-on delay, just like on a real device between
                # pressing the button and the first frame.
                if not m.running:
                    if kaltstart is None:
                        if e.text in ("ü", "Ü"):
                            kaltstart = now + EINSCHALT_HALT_S
                            # The name comes from the chip that's currently
                            # plugged in -- it may be a different one since
                            # the last boot.
                            bios_name = (Machine.rom_name(m.flash.einmal
                                         if m.flash.einmal is not None
                                         else rom_bytes(m.rom_path))
                                         or "UNNAMED BIOS")
                            m.vga.mode = VGA.MODE_TEXT
                            m.vga.clear_text(0x00)
                            m.vga.cursor = SCR_COLS * SCR_ROWS
                    else:
                        for zeichen in e.text:          # during the power-on delay
                            code = ord(zeichen)         # typed: hold onto it
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
                # Track state from the EVENTS, not from
                # get_pressed(). Depending on the platform, the latter still
                # returns the old state on release -- which meant the right
                # click never even reached the TB-32.
                # pygame counts 1 = left, 2 = middle, 3 = right;
                # the TB-32 expects bit 0, 1, 2.
                if e.button in (1, 2, 3):
                    bit = 1 << (e.button - 1)
                    # On the Mac, Ctrl+click is the usual right click --
                    # and on some trackpads the only one that comes through.
                    if e.button == 1 and (pygame.key.get_mods() & pygame.KMOD_CTRL):
                        bit = 4
                    if e.type == pygame.MOUSEBUTTONDOWN:
                        maus_bits |= bit
                    else:
                        maus_bits &= ~bit
                    m.mouse.move(m.mouse.x, m.mouse.y, maus_bits)

        # Deliver the held key again
        if halten and now >= halten_zeit:
            m.keyboard.push(halten[2], halten[1])
            halten_zeit = now + WDH_TAKT

        # CPU time budget: whatever is left over from the frame.
        #
        # This used to be a fixed 8 out of 16.7 milliseconds -- so less
        # than half, no matter how fast drawing actually was. We now
        # measure how long the rest of a frame really takes
        # (drawing, events, sound), and give the CPU the remainder up to
        # just under the frame duration. This keeps the window just as
        # smooth, but the virtual machine becomes noticeably faster.
        budget_ms = 16.7 - rest_ms - 1.0
        if budget_ms < 5.0:  budget_ms = 5.0      # always give the CPU something
        if budget_ms > 14.0: budget_ms = 14.0     # always leave the window something
        rahmen_start = time.perf_counter()
        n = m.run_slice(dt * (8 if turbo else 1),
                        max_ms=budget_ms + 2.0 if turbo else budget_ms)
        cpu_ms = (time.perf_counter() - rahmen_start) * 1000.0
        ips_measured = int(n / dt) if dt > 0 else 0

        # Is the requested window text ready by now?
        if kopie_wartet:
            t = fenstertext_holen(m)
            if t is not None:
                if t:
                    mac_clipboard_set(t)
                kopie_wartet = 0
            elif now > kopie_wartet:
                kopie_wartet = 0          # the system isn't answering, fine
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
                scrollback = 0                       # graphics mode: no scrolling
            monitor.render(m.vga, screen, view=view)

        if overlay:
            soll = m.ips // 1000
            lines = [
                f"CPU {CPU_SPEED_NAMES[m.cmos.data[0x13]]}   "
                f"real {ips_measured//1000} of {soll} kIPS"
                f"{'   (host too slow)' if ips_measured < soll * 800 else ''}",
                f"PC 0x{m.cpu.pc:08X}  SP 0x{m.cpu.r[15]:08X}  "
                f"{'HALT' if m.cpu.halted else 'RUN '}",
                f"Total instructions: {m.total_instructions:,}",
                f"FPS {clock.get_fps():.0f}   Disk: "
                f"{'READ/WRITE' if m.disk.led else 'ready'}",
                f"CPU {m.thermal.temp:.1f} \u00b0C   Fan {m.thermal.fan}%"
                + (f"   THROTTLING {m.thermal.throttle}%" if m.thermal.throttle else ""),
            ]
            if m.cpu.last_fault:
                lines.append(m.cpu.last_fault)
            y = 4
            for ln in lines:
                img = overlay_font.render(ln, True, (255, 255, 0), (0, 0, 0))
                screen.blit(img, (6, y))
                y += 16

        # --- Powered off: the screen is black -----------------
        #
        # It used to be that the last frame stayed on screen with a red
        # bar on top of it. That was practical, but wrong: a monitor on a
        # powered-off machine shows nothing. Now it's really
        # black -- and it stays that way during the power-on delay too, just
        # like during the seconds where a real PC is already running but
        # not yet sending a picture.
        # Powered off means black. If the power-on delay is running instead,
        # the splash screen is already in the framebuffer and the monitor
        # has just drawn it -- then there's nothing left to do here.
        if not m.running and kaltstart is None:
            screen.fill((0, 0, 0))

        pygame.display.flip()
        # How much time did everything except the CPU take? That's the
        # basis for the next frame's budget -- smoothed, so that a single
        # outlier doesn't immediately throw things off.
        gesamt_ms = (time.perf_counter() - rahmen_start) * 1000.0
        rest_ms = rest_ms * 0.8 + (gesamt_ms - cpu_ms) * 0.2
        clock.tick(60)

    m.shutdown()
    pygame.quit()


if __name__ == "__main__":
    main()
