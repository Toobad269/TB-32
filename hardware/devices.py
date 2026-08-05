"""
Die Peripherie des Rechners: Grafikkarte, Tastatur, Festplatte, Timer,
Lautsprecher, Maus, CMOS-Uhr, Netzwerkkarte und Netzteil.

Jedes Gerät hängt an I/O-Ports (wie bei einem echten PC über IN/OUT) und
manche zusätzlich an einem Speicherbereich (memory-mapped, z.B. der
Bildspeicher der Grafikkarte).
"""

import os
import time
import struct
from collections import deque

from hardware.isa import (
    GFX_H, GFX_W, IRQ_KBD, IRQ_MOUSE, IRQ_TIMER, PORT_BLT_BG, PORT_BLT_CHR,
    PORT_BLT_CMD, PORT_BLT_COL, PORT_BLT_H, PORT_BLT_SRC, PORT_BLT_W,
    PORT_BLT_X, PORT_BLT_Y, PORT_CMOS_DATA, PORT_CMOS_IDX,
    PORT_NVRAM_IDX, PORT_NVRAM_DATA, PORT_DISK_ADDR,
    PORT_DISK_CMD, PORT_DISK_COUNT, PORT_DISK_LBA, PORT_DISK_SIZE,
    PORT_DISK_STATUS, PORT_FAN, PORT_FANMODE, PORT_GFX_DOPPEL,
    PORT_GFX_TAUSCH, PORT_BLT_ZOOM, PORT_DMA_SRC, PORT_DMA_DST,
    PORT_DMA_LEN, PORT_DMA_VAL, PORT_DMA_CMD, PORT_KBD_DATA,
    PORT_KBD_STATUS, PORT_MCUR_ON, PORT_MCUR_X, PORT_MCUR_Y, PORT_MOUSE_BTN,
    PORT_MOUSE_WHEEL, PORT_MOUSE_X, PORT_MOUSE_Y, PORT_SPK_FREQ,
    PORT_SPK_ON, PORT_TEMP, PORT_TEMP_LIMIT, PORT_TEMP_MAX, PORT_THROTTLE,
    PORT_TIMER_HZ, PORT_TIMER_TICKS, PORT_VGA_CURSOR, PORT_VGA_MODE,
    PORT_VGA_PALIDX, PORT_VGA_PALVAL, VRAM_GFX_SIZE, VRAM_TEXT_SIZE,
    PORT_FLASH_CMD, PORT_FLASH_SIZE, PORT_FLASH_ADDR, ROM_SIZE,
    PORT_BLT_ZIEL, PORT_BLT_ZIELB, PORT_BLT_ZIELH,
    IRQ_NET, PORT_NET_STATUS, PORT_NET_ADDR, PORT_NET_LEN, PORT_NET_CMD,
    PORT_NET_MAC_HI, PORT_NET_MAC_LO, PORT_NET_ZAEHLER, PORT_NET_ZINDEX,
)

SECTOR = 512

# Fuer jedes der 256 moeglichen Bitmuster einer Zeichensatzzeile: welche
# Spalten sind gesetzt? Einmal ausgerechnet statt achtmal je Buchstabe.
_GESETZT = tuple(tuple(c for c in range(8) if bits & (0x80 >> c))
                 for bits in range(256))


# ---------------------------------------------------------------------------
# Grafikkarte
# ---------------------------------------------------------------------------

class VGA:
    """80x25 Textmodus (Zeichen + Farbattribut) und 640x400 Grafikmodus."""

    MODE_TEXT = 0
    MODE_GFX = 1

    def __init__(self):
        self.text = bytearray(VRAM_TEXT_SIZE)
        # Zwei Bildseiten wie bei einer echten Grafikkarte.
        #   self.gfx       -- hierhin wird gemalt (Bus und Blitter sehen sie)
        #   self.gfx_sicht -- diese wird angezeigt
        # Ohne Doppelpufferung sind beide dasselbe Feld, dann ist jeder
        # Malbefehl sofort sichtbar -- so hat es der Schreibtisch immer
        # gemacht und so bleibt es auch.
        self._seite_a = bytearray(VRAM_GFX_SIZE)
        self._seite_b = None            # wird erst bei Bedarf angelegt
        self.gfx = self._seite_a        # hierhin wird gemalt
        self.gfx_sicht = self._seite_a  # diese wird angezeigt
        self.doppel = 0
        self.mode = self.MODE_TEXT
        self.cursor = 0
        self.pal_index = 0
        self.palette = self._default_palette()
        self.dirty = True
        self.bus = None
        # Register des 2D-Beschleunigers
        self.blt = {"x": 0, "y": 0, "w": 0, "h": 0, "col": 15,
                    "chr": 32, "src": 0, "bg": 256, "zoom": 1,
                    "ziel": 0, "zielb": 0, "zielh": 0}
        self._glyph_cache = {}          # (Bitmuster, Farbe, Hintergrund) -> 8 Bytes
        self._zeile_cache = {}          # (Farbe, Breite) -> fertige Zeile
        self.mcur_x = 320
        self.mcur_y = 200
        self.mcur_on = 0
        self.clear_text()

    def _default_palette(self):
        """Die 16 klassischen PC-Farben, danach ein Farbwürfel und Graustufen."""
        pal = [
            0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
            0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
            0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
            0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
        ]
        for r in range(6):
            for g in range(6):
                for b in range(6):
                    pal.append((r * 51 << 16) | (g * 51 << 8) | (b * 51))
        while len(pal) < 256:
            v = min(255, (len(pal) - 232) * 10 + 8)
            pal.append((v << 16) | (v << 8) | v)
        return pal[:256]

    def clear_text(self, attr=0x07):
        for i in range(0, VRAM_TEXT_SIZE, 2):
            self.text[i] = 0x20
            self.text[i + 1] = attr
        self.dirty = True

    # -- 2D-Beschleuniger ---------------------------------------------------

    def _blit(self, cmd):
        """Führt ein Zeichenkommando aus. Arbeitet direkt auf dem Bildspeicher,
        deshalb ist es hunderte Male schneller als Pixel-für-Pixel über den Bus
        -- genau wie der Blitter einer echten Grafikkarte."""
        b = self.blt
        x, y, w, h = b["x"], b["y"], b["w"], b["h"]
        col = b["col"] & 0xFF
        # Ziel: der Bildschirm, oder ein Puffer im Arbeitsspeicher. Alles
        # Weitere rechnet mit GFX_W_L/GFX_H_L -- deshalb werden die hier zu
        # Groessen des Ziels, und der Rest des Blitters merkt keinen
        # Unterschied.
        if b["ziel"]:
            GFX_W_L, GFX_H_L = b["zielb"], b["zielh"]
            if GFX_W_L <= 0 or GFX_H_L <= 0:
                return
            fb = memoryview(self.bus.ram)[b["ziel"]:b["ziel"] + GFX_W_L * GFX_H_L]
        else:
            GFX_W_L, GFX_H_L = GFX_W, GFX_H
            fb = self.gfx
            self.dirty = True

        if cmd == 1:                                   # gefüllte Fläche
            if w <= 0 or h <= 0:
                return
            x0, y0 = max(0, x), max(0, y)
            x1, y1 = min(GFX_W_L, x + w), min(GFX_H_L, y + h)
            if x1 <= x0 or y1 <= y0:
                return
            breit = x1 - x0
            schl = (col, breit)
            row = self._zeile_cache.get(schl)
            if row is None:
                row = bytes([col]) * breit
                if len(self._zeile_cache) > 512:
                    self._zeile_cache.clear()
                self._zeile_cache[schl] = row
            if breit == GFX_W_L:
                # ganze Zeilen: ein einziger Schreibvorgang statt h Stueck
                fb[y0 * GFX_W_L:y1 * GFX_W_L] = row * (y1 - y0)
            else:
                off = y0 * GFX_W_L + x0
                for _ in range(y1 - y0):
                    fb[off:off + breit] = row
                    off += GFX_W_L

        elif cmd == 2:                                 # Rahmen
            for xx in range(max(0, x), min(GFX_W_L, x + w)):
                if 0 <= y < GFX_H_L:
                    fb[y * GFX_W_L + xx] = col
                if 0 <= y + h - 1 < GFX_H_L:
                    fb[(y + h - 1) * GFX_W_L + xx] = col
            for yy in range(max(0, y), min(GFX_H_L, y + h)):
                if 0 <= x < GFX_W_L:
                    fb[yy * GFX_W_L + x] = col
                if 0 <= x + w - 1 < GFX_W_L:
                    fb[yy * GFX_W_L + x + w - 1] = col

        elif cmd == 3:                                 # Zeichen aus dem Zeichensatz
            if self.bus is None:
                return
            code = b["chr"] & 0xFF
            if code < 32 or code > 127:
                code = 32
            base = b["src"] + (code - 32) * 8
            bg = b["bg"]
            muster = self.bus.read_block(base, 8)
            zoom = b["zoom"]
            # Vergroessert malen kann jetzt die Karte. Vorher musste ein
            # Programm dafuer 8*8*zoom*zoom Punkte einzeln schreiben -- bei
            # Zoom 3 sind das 576 Schreibvorgaenge fuer EINE Ziffer.
            if zoom > 1:
                br = 8 * zoom
                if 0 <= x <= GFX_W_L - br and 0 <= y <= GFX_H_L - br:
                    cache = self._glyph_cache
                    for r in range(8):
                        schl = (muster[r], col, bg, zoom)
                        zeile = cache.get(schl)
                        if zeile is None:
                            bits = muster[r]
                            if bg < 256:
                                zeile = bytes((col if bits & (0x80 >> (c // zoom))
                                               else bg) for c in range(br))
                            else:
                                zeile = None       # durchsichtig: unten Punkte
                            if zeile is not None:
                                if len(cache) > 4096:
                                    cache.clear()
                                cache[schl] = zeile
                        off = (y + r * zoom) * GFX_W_L + x
                        if zeile is not None:
                            for _ in range(zoom):
                                fb[off:off + br] = zeile
                                off += GFX_W_L
                        else:
                            bits = muster[r]
                            if bits:
                                punkte = bytes([col]) * zoom
                                for c in _GESETZT[bits]:
                                    o2 = off + c * zoom
                                    for _ in range(zoom):
                                        fb[o2:o2 + zoom] = punkte
                                        o2 += GFX_W_L
                return
            # Der schnelle Weg: Buchstabe liegt ganz auf dem Schirm, also ohne
            # Randpruefung je Bildpunkt. Mit Hintergrundfarbe wird jede der
            # acht Zeilen als fertige Acht-Byte-Folge in einem Rutsch gesetzt;
            # die Folgen wiederholen sich staendig und stehen deshalb im
            # Zwischenspeicher. Vorher waren es 64 einzelne Schreibvorgaenge.
            if 0 <= x <= GFX_W_L - 8 and 0 <= y <= GFX_H_L - 8:
                off = y * GFX_W_L + x
                if bg < 256:
                    cache = self._glyph_cache
                    for r in range(8):
                        schl = (muster[r], col, bg)
                        zeile = cache.get(schl)
                        if zeile is None:
                            bits = muster[r]
                            zeile = bytes(col if bits & (0x80 >> c) else bg
                                          for c in range(8))
                            if len(cache) > 4096:
                                cache.clear()
                            cache[schl] = zeile
                        fb[off:off + 8] = zeile
                        off += GFX_W_L
                else:
                    for r in range(8):
                        bits = muster[r]
                        if bits:
                            for c in _GESETZT[bits]:
                                fb[off + c] = col
                        off += GFX_W_L
                return
            for row in range(8):
                bits = muster[row]
                yy = y + row
                if not (0 <= yy < GFX_H_L):
                    continue
                off = yy * GFX_W_L
                for cx in range(8):
                    xx = x + cx
                    if not (0 <= xx < GFX_W_L):
                        continue
                    if bits & (0x80 >> cx):
                        fb[off + xx] = col
                    elif bg < 256:
                        fb[off + xx] = bg

        elif cmd == 4:                                 # Bild aus dem RAM
            if self.bus is None:
                return
            data = self.bus.read_block(b["src"], w * h)
            # Liegt das Bild ganz auf dem Schirm, geht jede Zeile in einem
            # Stueck. Nur Zeilen, in denen wirklich ein durchsichtiger Punkt
            # (255) vorkommt, muessen einzeln behandelt werden -- das "in"
            # sucht danach im Maschinencode und ist praktisch umsonst.
            if 0 <= x and x + w <= GFX_W_L and 0 <= y and y + h <= GFX_H_L:
                off = y * GFX_W_L + x
                i = 0
                for _ in range(h):
                    zeile = data[i:i + w]
                    if 255 in zeile:
                        for c, v in enumerate(zeile):
                            if v != 255:
                                fb[off + c] = v
                    else:
                        fb[off:off + w] = zeile
                    off += GFX_W_L
                    i += w
                return
            i = 0
            for yy in range(y, y + h):
                if 0 <= yy < GFX_H_L:
                    for xx in range(x, x + w):
                        if 0 <= xx < GFX_W_L:
                            v = data[i + (xx - x)]
                            if v != 255:               # 255 = durchsichtig
                                fb[yy * GFX_W_L + xx] = v
                i += w

        elif cmd == 6:                                 # Zeichenkette aus dem RAM
            # Eine ganze Zeile in EINEM Befehl. Vorher schickte das
            # Betriebssystem je Buchstabe einen eigenen Malbefehl -- eine
            # Editorseite sind 1600 Stueck, und jeder kostet den Prozessor
            # Dutzende Befehle. Jetzt ist es einer je Farbabschnitt.
            #   CHR = Adresse des Textes, W = Laenge, SRC = Zeichensatz
            # (der Zeichensatz bleibt so, wo er immer steht)
            if self.bus is None or w <= 0:
                return
            txt = self.bus.read_block(b["chr"], min(w, 256))
            bg = b["bg"]
            cache = self._glyph_cache
            font = b["src"]
            zoom = b["zoom"]
            br = 8 * zoom
            if not (0 <= y <= GFX_H_L - br):
                return
            for i in range(len(txt)):
                zx = x + i * br
                if zx < 0 or zx > GFX_W_L - br:
                    continue
                code = txt[i]
                if code < 32 or code > 127:
                    code = 32
                muster = self.bus.read_block(font + (code - 32) * 8, 8)
                off = y * GFX_W_L + zx
                if bg < 256:
                    for r in range(8):
                        schl = (muster[r], col, bg, zoom)
                        zeile = cache.get(schl)
                        if zeile is None:
                            bits = muster[r]
                            zeile = bytes(col if bits & (0x80 >> (c // zoom)) else bg
                                          for c in range(br))
                            if len(cache) > 4096:
                                cache.clear()
                            cache[schl] = zeile
                        for _ in range(zoom):
                            fb[off:off + br] = zeile
                            off += GFX_W_L
                else:
                    for r in range(8):
                        bits = muster[r]
                        if bits:
                            if zoom == 1:
                                for c in _GESETZT[bits]:
                                    fb[off + c] = col
                            else:
                                punkte = bytes([col]) * zoom
                                for c in _GESETZT[bits]:
                                    o2 = off + c * zoom
                                    for _ in range(zoom):
                                        fb[o2:o2 + zoom] = punkte
                                        o2 += GFX_W_L
                        off += GFX_W_L * zoom

        elif cmd == 7:                                 # Bild skaliert aus dem RAM
            # Nachster-Nachbar-Verfahren: fuer jede Zielzeile wird die
            # passende Quellzeile ausgesucht, fuer jede Zielspalte die
            # passende Quellspalte. Ohne Kommazahlen -- die Schrittweite
            # steht als Bruch aus Quell- und Zielgroesse fest. Genau so
            # skalieren Grafikkarten seit jeher.
            #   SRC = Bildpunkte, CHR = Quellbreite | Quellhoehe<<16
            #   X,Y = Ziel-Ecke, W,H = Zielgroesse
            if self.bus is None or w <= 0 or h <= 0:
                return
            qb = b["chr"] & 0xFFFF
            qh = (b["chr"] >> 16) & 0xFFFF
            if qb <= 0 or qh <= 0:
                return
            quelle = self.bus.read_block(b["src"], qb * qh)
            # Spaltenzuordnung einmal ausrechnen, nicht je Zeile
            spalten = [(i * qb) // w for i in range(w)]
            for zy in range(h):
                yy = y + zy
                if not (0 <= yy < GFX_H_L):
                    continue
                qz = (zy * qh) // h
                zeile = quelle[qz * qb:qz * qb + qb]
                if not zeile:
                    continue
                neu = bytes(zeile[c] for c in spalten)
                x0 = x
                if x0 < 0:
                    neu = neu[-x0:]
                    x0 = 0
                if x0 + len(neu) > GFX_W_L:
                    neu = neu[:GFX_W_L - x0]
                if not neu:
                    continue
                off = yy * GFX_W_L + x0
                if 255 in neu:
                    for c in range(len(neu)):
                        if neu[c] != 255:
                            fb[off + c] = neu[c]
                else:
                    fb[off:off + len(neu)] = neu

        elif cmd == 5:                                 # Bereich kopieren
            sx, sy = b["chr"], b["src"]                # Quelle in CHR/SRC
            for r in range(h):
                s = (sy + r) * GFX_W_L + sx
                d = (y + r) * GFX_W_L + x
                if 0 <= sy + r < GFX_H_L and 0 <= y + r < GFX_H_L:
                    fb[d:d + w] = fb[s:s + w]

    def doppel_setzen(self, an):
        """Schaltet die zweite Bildseite ein oder aus."""
        if an and not self.doppel:
            if self._seite_b is None:
                self._seite_b = bytearray(VRAM_GFX_SIZE)
            # Die jeweils ANDERE der beiden festen Seiten wird zur Rueckseite.
            # (Frueher stand hier eine Merkvariable, die nach dem Ausschalten
            # auf dieselbe Seite zeigte -- beim zweiten Start eines Spiels
            # waren dann beide Seiten dasselbe Feld und es flackerte wieder.)
            andere = self._seite_b if self.gfx is self._seite_a else self._seite_a
            andere[:] = self.gfx         # sonst blitzt beim ersten Tausch
            self.gfx_sicht = self.gfx    # das vorletzte Bild auf
            self.gfx = andere
            self.doppel = 1
        elif not an and self.doppel:
            self.gfx_sicht[:] = self.gfx
            self.gfx = self.gfx_sicht
            self.doppel = 0
        self.dirty = True

    def tauschen(self, wie=1):
        """Fertiges Bild sichtbar machen.

        wie = 1: die beiden Seiten **wechseln**. Schnell (nur zwei Zeiger),
                 aber die neue Rueckseite enthaelt das vorletzte Bild --
                 wer nur Teile malt, muss danach alles neu malen. Fuer
                 Spiele, die ohnehin jedes Bild komplett zeichnen.
        wie = 2: die Rueckseite auf die Vorderseite **kopieren**. Kostet
                 einen Speicherblock, dafuer bleibt die Rueckseite stehen --
                 damit funktionieren Teil-Neuzeichnungen. Genau das braucht
                 der Schreibtisch, der immer nur ein Fenster neu malt."""
        if not self.doppel:
            return
        if wie == 2:
            self.gfx_sicht[:] = self.gfx
            self.dirty = True
            return
        self.gfx, self.gfx_sicht = self.gfx_sicht, self.gfx
        # Die neue Rueckseite enthaelt noch das vorletzte Bild. Wer alles
        # neu malt, merkt davon nichts; wer nur Teile malt, will es haben.
        self.dirty = True

    def port_out(self, port, value):
        # Die Namen stehen jetzt oben in der Datei. Hier eine import-Zeile zu
        # haben sah harmlos aus -- sie lief aber bei JEDEM Portzugriff, und
        # ein Malbefehl schreibt sechs davon.
        if port == PORT_BLT_ZIEL:
            self.blt["ziel"] = value
            return
        if port == PORT_BLT_ZIELB:
            self.blt["zielb"] = value
            return
        if port == PORT_BLT_ZIELH:
            self.blt["zielh"] = value
            return
        if port == PORT_BLT_X:    self.blt["x"] = value if value < 0x8000 else value - 0x10000
        elif port == PORT_BLT_Y:  self.blt["y"] = value if value < 0x8000 else value - 0x10000
        elif port == PORT_BLT_W:  self.blt["w"] = value
        elif port == PORT_BLT_H:  self.blt["h"] = value
        elif port == PORT_BLT_COL: self.blt["col"] = value
        elif port == PORT_BLT_CHR: self.blt["chr"] = value
        elif port == PORT_BLT_SRC: self.blt["src"] = value
        elif port == PORT_BLT_ZOOM: self.blt["zoom"] = max(1, min(16, value))
        elif port == PORT_BLT_BG:  self.blt["bg"] = value
        elif port == PORT_BLT_CMD: self._blit(value)
        elif port == PORT_GFX_DOPPEL: self.doppel_setzen(value)
        elif port == PORT_GFX_TAUSCH: self.tauschen(value)
        elif port == PORT_MCUR_X:  self.mcur_x = value; self.dirty = True
        elif port == PORT_MCUR_Y:  self.mcur_y = value; self.dirty = True
        elif port == PORT_MCUR_ON: self.mcur_on = value; self.dirty = True
        elif port == PORT_VGA_MODE:
            # Ein Moduswechsel setzt den Blitter zurueck. Sonst schriebe der
            # Schreibtisch in Riesenschrift weiter, wenn ein Programm mit
            # gesetzter Vergroesserung abstuerzt.
            self.blt["zoom"] = 1
            self.mode = value & 1
            if value & 0x100:                 # Bit 8 gesetzt = Bildspeicher löschen
                if self.mode == self.MODE_TEXT:
                    self.clear_text()
                else:
                    self.gfx[:] = b"\x00" * VRAM_GFX_SIZE
                    if self.doppel:
                        self.gfx_sicht[:] = self.gfx
            self.dirty = True
        elif port == PORT_VGA_CURSOR:
            self.cursor = value & 0xFFFF
            self.dirty = True
        elif port == PORT_VGA_PALIDX:
            self.pal_index = value & 0xFF
        elif port == PORT_VGA_PALVAL:
            self.palette[self.pal_index] = value & 0xFFFFFF
            self.dirty = True

    def port_in(self, port):
        if port == PORT_VGA_MODE:
            return self.mode
        if port == PORT_VGA_CURSOR:
            return self.cursor
        # Lesbar, damit das Betriebssystem den Blitter-Zustand beim
        # Prozesswechsel sichern kann: er gehoert dem Programm, das gerade
        # malt, nicht dem naechsten.
        if port == PORT_BLT_ZIEL:
            return self.blt["ziel"]
        if port == PORT_BLT_ZIELB:
            return self.blt["zielb"]
        if port == PORT_BLT_ZIELH:
            return self.blt["zielh"]
        if port == PORT_BLT_SRC:
            return self.blt["src"]
        return 0


# ---------------------------------------------------------------------------
# Tastatur
# ---------------------------------------------------------------------------

class Keyboard:
    """Puffert Tastendrücke wie ein PS/2-Controller und löst IRQ 1 aus."""

    def __init__(self, cpu_ref):
        self.buffer = deque(maxlen=32)
        self.cpu = cpu_ref
        self.shift = False
        self.ctrl = False
        self.alt = False

    def push(self, ascii_code, scancode=0):
        self.buffer.append(((scancode & 0xFF) << 8) | (ascii_code & 0xFF))
        if self.cpu[0]:
            self.cpu[0].raise_irq(IRQ_KBD)

    def port_in(self, port):
        if port == PORT_KBD_DATA:
            return self.buffer.popleft() if self.buffer else 0
        if port == PORT_KBD_STATUS:
            return 1 if self.buffer else 0
        return 0


    def port_out(self, port, value):
        pass


# Sondertasten: liegen als Scancode im oberen Byte, ASCII-Teil ist 0
KEY_ESC, KEY_ENTER, KEY_BACKSPACE, KEY_TAB = 0x01, 0x1C, 0x0E, 0x0F
KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT = 0x48, 0x50, 0x4B, 0x4D
KEY_F1, KEY_F2, KEY_F10, KEY_DEL = 0x3B, 0x3C, 0x44, 0x53
KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN = 0x47, 0x4F, 0x49, 0x51
KEY_INS, KEY_F5 = 0x52, 0x3F


# ---------------------------------------------------------------------------
# Festplatte
# ---------------------------------------------------------------------------

class Disk:
    """Blockgerät mit 512-Byte-Sektoren. Überträgt per DMA direkt in den RAM."""

    def __init__(self, path, size_sectors=8192):
        self.path = path
        self.size_sectors = size_sectors
        self.lba = 0
        self.count = 1
        self.addr = 0
        self.status = 0
        self.busy_until = 0.0
        self.led = False

        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(b"\x00" * (size_sectors * SECTOR))
        self.file = open(path, "r+b")
        self.file.seek(0, os.SEEK_END)
        self.size_sectors = max(1, self.file.tell() // SECTOR)

    def close(self):
        try:
            self.file.flush()
            self.file.close()
        except Exception:
            pass

    def attach_bus(self, bus):
        self.bus = bus


    def port_out(self, port, value):
        if port == PORT_DISK_LBA:
            self.lba = value
        elif port == PORT_DISK_COUNT:
            # 16 Bit Sektorzähler: bis 32 MB am Stück. Mit 8 Bit (wie bei den
            # ganz frühen Controllern) wären bei 128 KB Schluss -- zu wenig,
            # seit der C-Compiler auf dem Gerät selbst läuft.
            self.count = max(1, value & 0xFFFF)
        elif port == PORT_DISK_ADDR:
            self.addr = value
        elif port == PORT_DISK_CMD:
            self._command(value)

    def port_in(self, port):
        if port == PORT_DISK_STATUS:
            return self.status
        if port == PORT_DISK_SIZE:
            return self.size_sectors
        return 0

    def _command(self, cmd):
        self.led = True
        self.busy_until = time.time() + 0.03
        if self.lba + self.count > self.size_sectors:
            self.status = 1                       # außerhalb der Platte
            return
        try:
            self.file.seek(self.lba * SECTOR)
            if cmd == 1:                          # lesen
                data = self.file.read(self.count * SECTOR)
                self.bus.write_block(self.addr, data)
            elif cmd == 2:                        # schreiben
                data = self.bus.read_block(self.addr, self.count * SECTOR)
                self.file.write(data)
                self.file.flush()
            else:
                self.status = 2
                return
            self.status = 0
        except Exception:
            self.status = 3


# ---------------------------------------------------------------------------
# Timer
# ---------------------------------------------------------------------------

class Timer:
    """Programmierbarer Intervall-Zähler: löst regelmäßig IRQ 0 aus."""

    def __init__(self, cpu_ref):
        self.cpu = cpu_ref
        self.hz = 0
        self.ticks = 0
        self._accum = 0.0


    def port_out(self, port, value):
        if port == PORT_TIMER_HZ:
            self.hz = value & 0xFFFF
            self._accum = 0.0

    def port_in(self, port):
        if port == PORT_TIMER_TICKS:
            return self.ticks & 0xFFFFFFFF
        return 0

    def advance(self, dt):
        if not self.hz:
            return
        self._accum += dt * self.hz
        n = int(self._accum)
        if n > 0:
            self._accum -= n
            self.ticks += n
            if self.cpu[0]:
                self.cpu[0].raise_irq(IRQ_TIMER)


# ---------------------------------------------------------------------------
# Lautsprecher
# ---------------------------------------------------------------------------

class Speaker:
    def __init__(self):
        self.freq = 440
        self.on = False
        self.changed = True


    def port_out(self, port, value):
        if port == PORT_SPK_FREQ:
            self.freq = max(20, value & 0xFFFF)
            self.changed = True
        elif port == PORT_SPK_ON:
            self.on = bool(value & 1)
            self.changed = True

    def port_in(self, port):
        return int(self.on)


# ---------------------------------------------------------------------------
# Maus
# ---------------------------------------------------------------------------

class Mouse:
    def __init__(self, cpu_ref):
        self.cpu = cpu_ref
        self.x = GFX_W // 2
        self.y = GFX_H // 2
        self.buttons = 0
        self.wheel = 0            # aufgelaufene Rasten, positiv = nach oben
        self.enabled = False

    def move(self, x, y, buttons):
        changed = (x != self.x or y != self.y or buttons != self.buttons)
        self.x, self.y, self.buttons = x, y, buttons
        if changed and self.enabled and self.cpu[0]:
            self.cpu[0].raise_irq(IRQ_MOUSE)

    def port_in(self, port):
        if port == PORT_MOUSE_WHEEL:
            # Einmal lesen heisst abholen -- danach steht der Zaehler wieder
            # auf null, wie bei einem echten Radzaehler.
            w, self.wheel = self.wheel, 0
            return w & 0xFFFFFFFF
        if port == PORT_MOUSE_X:
            return self.x
        if port == PORT_MOUSE_Y:
            return self.y
        if port == PORT_MOUSE_BTN:
            return self.buttons
        return 0


    def port_out(self, port, value):
        if port == PORT_MOUSE_BTN:
            self.enabled = bool(value & 1)


# ---------------------------------------------------------------------------
# CMOS + Echtzeituhr
# ---------------------------------------------------------------------------

CMOS_SEC, CMOS_MIN, CMOS_HOUR = 0x00, 0x02, 0x04
CMOS_WDAY, CMOS_DAY, CMOS_MONTH, CMOS_YEAR = 0x06, 0x07, 0x08, 0x09
CMOS_BOOTDEV   = 0x10
CMOS_QUICKBOOT = 0x11
CMOS_BEEP      = 0x12
CMOS_CPUSPEED  = 0x13
CMOS_RAMSIZE   = 0x14
CMOS_VERBOSE   = 0x15
CMOS_CHECKSUM  = 0x2E
CMOS_MAGIC     = 0x2F


class CMOS:
    """Batteriegepufferter Speicher: Uhr + die im BIOS-Setup gemachten
    Einstellungen. Landet als Datei auf der echten Platte -- das ist die
    Knopfzelle auf dem Mainboard."""

    # Die Uhr geht ab jetzt selbst: gespeichert wird der Unterschied zur Uhr
    # des Wirts, in Sekunden. Vorher war die Uhr des TB-32 nur eine Kopie der
    # Mac-Uhr -- man konnte sie im BIOS-Setup gar nicht stellen, weil jeder
    # Lesezugriff den geschriebenen Wert sofort wieder ueberbuegelte.
    OFFSET_REG = 0x30                        # vier Byte, vorzeichenbehaftet

    def __init__(self, path):
        self.path = path
        self.data = bytearray(64)
        self.index = 0
        self.offset = 0
        if os.path.exists(path):
            with open(path, "rb") as f:
                d = f.read(64)
                self.data[:len(d)] = d
        if self.data[CMOS_MAGIC] != 0x5A:
            self.reset_defaults()
        self.offset = int.from_bytes(
            self.data[self.OFFSET_REG:self.OFFSET_REG + 4], "little", signed=True)

    def reset_defaults(self):
        self.data = bytearray(64)
        self.data[CMOS_BOOTDEV] = 0          # 0 = Festplatte
        self.data[CMOS_QUICKBOOT] = 0
        self.data[CMOS_BEEP] = 1
        self.data[CMOS_CPUSPEED] = 2         # Index in die Taktliste
        self.data[CMOS_RAMSIZE] = 16
        self.data[CMOS_VERBOSE] = 1
        self.data[CMOS_MAGIC] = 0x5A
        self.save()

    def save(self):
        self._offset_sichern()
        s = sum(self.data[0x10:0x2E]) & 0xFF
        self.data[CMOS_CHECKSUM] = s
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "wb") as f:
            f.write(bytes(self.data))

    def _jetzt(self):
        return time.localtime(time.time() + self.offset)

    def _offset_sichern(self):
        self.data[self.OFFSET_REG:self.OFFSET_REG + 4] = \
            int(self.offset).to_bytes(4, "little", signed=True)

    def _uhr_stellen(self, feld, wert):
        """Ein Feld der Uhr setzen -- so, wie man an einem echten
        Uhrenbaustein dreht: die Uhr laeuft danach von dort weiter."""
        t = self._jetzt()
        teile = [t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec]
        if feld == 0:
            wert = 2000 + (wert % 100)           # CM_YEAR haelt 0..99
        teile[feld] = wert
        try:
            ziel = time.mktime((teile[0], teile[1], teile[2], teile[3],
                                teile[4], teile[5], 0, 0, -1))
        except (ValueError, OverflowError):
            return                                # z. B. 31. Februar: ignorieren
        self.offset = ziel - time.time()
        self._offset_sichern()

    def _refresh_clock(self):
        t = self._jetzt()
        self.data[CMOS_SEC] = t.tm_sec
        self.data[CMOS_MIN] = t.tm_min
        self.data[CMOS_HOUR] = t.tm_hour
        self.data[CMOS_WDAY] = t.tm_wday
        self.data[CMOS_DAY] = t.tm_mday
        self.data[CMOS_MONTH] = t.tm_mon
        self.data[CMOS_YEAR] = t.tm_year % 100


    def port_out(self, port, value):
        if port == PORT_CMOS_IDX:
            self.index = value & 0x3F
        elif port == PORT_CMOS_DATA:
            if self.index == 0x3F:               # Sonderregister: speichern
                self.save()
            elif self.index == CMOS_SEC:
                self._uhr_stellen(5, value & 0xFF)
            elif self.index == CMOS_MIN:
                self._uhr_stellen(4, value & 0xFF)
            elif self.index == CMOS_HOUR:
                self._uhr_stellen(3, value & 0xFF)
            elif self.index == CMOS_DAY:
                self._uhr_stellen(2, value & 0xFF)
            elif self.index == CMOS_MONTH:
                self._uhr_stellen(1, value & 0xFF)
            elif self.index == CMOS_YEAR:
                self._uhr_stellen(0, value & 0xFF)
            else:
                self.data[self.index] = value & 0xFF

    def port_in(self, port):
        if port == PORT_CMOS_IDX:
            return self.index
        if port == PORT_CMOS_DATA:
            if self.index <= CMOS_YEAR:
                self._refresh_clock()
            return self.data[self.index]
        return 0


# ---------------------------------------------------------------------------
# NVRAM -- der zweite batteriegepufferte Speicher
# ---------------------------------------------------------------------------

class NVRAM:
    """256 Byte, die einen Neustart überleben. Eigene Datei, eigene Ports.

    Warum ein zweiter Baustein und nicht einfach ein größeres CMOS: Die 64
    Byte an der Uhr sind ein Stück echter PC-Geschichte -- Adressbreite,
    Prüfsumme und Registerbelegung hängen daran, und ein Firmen-BIOS braucht
    Platz für Dinge, die dort nie hineinpassen (32 Byte Firmentext, acht
    Ereignisse, Inventarangaben). Echte Mainboards haben genau denselben
    Schritt gemacht.

    Gespeichert wird sofort bei jedem Schreibzugriff. Das ist unbequemer als
    ein Sammelbefehl wie CM_SAVE, aber hier richtig: der Ereignisspeicher
    protokolliert Dinge wie „Secure Boot hat angehalten", und danach kommt
    kein geordnetes Sichern mehr.
    """

    GROESSE = 256

    def __init__(self, path):
        self.path = path
        self.data = bytearray(self.GROESSE)
        self.index = 0
        if os.path.exists(path):
            with open(path, "rb") as f:
                d = f.read(self.GROESSE)
                self.data[:len(d)] = d

    def save(self):
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            with open(self.path, "wb") as f:
                f.write(self.data)
        except OSError:
            pass                      # ein volles Laufwerk darf den PC nicht anhalten

    def port_out(self, port, value):
        if port == PORT_NVRAM_IDX:
            self.index = value & 0xFF
        elif port == PORT_NVRAM_DATA:
            self.data[self.index] = value & 0xFF
            self.save()

    def port_in(self, port):
        if port == PORT_NVRAM_IDX:
            return self.index
        if port == PORT_NVRAM_DATA:
            return self.data[self.index]
        return 0


# ---------------------------------------------------------------------------
# Temperatur und Kühlung
# ---------------------------------------------------------------------------

class Thermal:
    # Wärmemodell des Rechners.
    #
    # Der Prozessor erzeugt Wärme proportional zu Takt und Auslastung, das
    # Gehäuse gibt sie an die Umgebung ab -- umso schneller, je höher der
    # Lüfter dreht. Wird es trotzdem zu heiß, drosselt der Chipsatz den Takt,
    # bis die Temperatur wieder fällt. Genau dieses Verhalten (thermal
    # throttling) haben alle heutigen Prozessoren; reicht auch das nicht,
    # schaltet die Hardware zum Selbstschutz ab.

    UMGEBUNG = 22.0          # Raumtemperatur in Grad
    KRITISCH = 105.0         # darüber schaltet der Rechner ab

    def __init__(self):
        self.temp = self.UMGEBUNG
        self.temp_max = self.UMGEBUNG
        self.fan = 40                 # Prozent
        self.fan_mode = 0             # 0 = automatisch, 1 = leise, 2 = voll
        self.limit = 85               # Drosselgrenze in Grad
        self.throttle = 0             # aktuelle Drosselung in Prozent
        self.notaus = False

    def advance(self, dt, auslastung, takt_mhz):
        # auslastung: 0..1 -- wie viel der Prozessor wirklich gerechnet hat
        if dt <= 0:
            return

        if self.fan_mode == 0:                       # automatisch regeln
            ziel = 25 + (self.temp - 40) * 2.5
            self.fan = int(max(20, min(100, ziel)))
        elif self.fan_mode == 1:
            self.fan = 20
        elif self.fan_mode == 2:
            self.fan = 100

        heizung = 2.5 * takt_mhz * auslastung + 0.4
        kuehlung = (self.temp - self.UMGEBUNG) * (0.05 + self.fan / 100.0 * 0.25)
        self.temp += (heizung - kuehlung) * dt
        if self.temp < self.UMGEBUNG:
            self.temp = self.UMGEBUNG
        if self.temp > self.temp_max:
            self.temp_max = self.temp

        if self.temp > self.limit:                   # zu heiß -> drosseln
            ziel = min(80, int((self.temp - self.limit) * 12))
            if ziel > self.throttle:
                self.throttle = ziel
            elif self.throttle > 0:
                self.throttle -= 1
        elif self.throttle > 0:
            self.throttle = max(0, self.throttle - 2)

        if self.temp >= self.KRITISCH:
            self.notaus = True

    def port_in(self, port):
        if port == PORT_TEMP:       return int(self.temp * 10)
        if port == PORT_FAN:        return self.fan
        if port == PORT_THROTTLE:   return self.throttle
        if port == PORT_TEMP_LIMIT: return self.limit
        if port == PORT_FANMODE:    return self.fan_mode
        if port == PORT_TEMP_MAX:   return int(self.temp_max * 10)
        return 0


    def port_out(self, port, value):
        if port == PORT_FAN:
            self.fan = max(0, min(100, value))
            self.fan_mode = 3                        # von Hand eingestellt
        elif port == PORT_TEMP_LIMIT:
            self.limit = max(40, min(100, value))
        elif port == PORT_FANMODE:
            self.fan_mode = value & 3
        elif port == PORT_TEMP_MAX:
            self.temp_max = self.temp                # Höchstwert zurücksetzen


# ---------------------------------------------------------------------------
# Netzteil / Power-Management
# ---------------------------------------------------------------------------

class Power:
    def __init__(self):
        self.request = None      # None | "off" | "reboot"


    def port_out(self, port, value):
        if value == 1:
            self.request = "off"
        elif value == 2:
            self.request = "reboot"

    def port_in(self, port):
        return 0


# ---------------------------------------------------------------------------
# Blockkopierer (DMA)
#
# Ein Prozessor, der 256 KB Byte fuer Byte umschaufelt, braucht dafuer eine
# Million Befehle -- bei 3 MHz eine Drittelsekunde. Kein Rueckgaengig, keine
# Bildablage, kein Textumbruch waere damit fluessig. Echte Rechner haben
# deshalb seit jeher einen eigenen Baustein, der den Speicher am Prozessor
# vorbei umschaufelt. Genau das ist das hier.
#
# Er sieht denselben Adressraum wie die CPU -- Quelle und Ziel duerfen also
# auch der Bildspeicher sein.
# ---------------------------------------------------------------------------

class DMA:
    def __init__(self):
        self.bus = None
        self.src = 0
        self.dst = 0
        self.laenge = 0
        self.wert = 0
        self.bytes_gesamt = 0        # nur zum Nachschauen, wie viel schon lief

    def port_out(self, port, value):
        if port == PORT_DMA_SRC:   self.src = value
        elif port == PORT_DMA_DST: self.dst = value
        elif port == PORT_DMA_LEN: self.laenge = value
        elif port == PORT_DMA_VAL: self.wert = value & 0xFF
        elif port == PORT_DMA_CMD: self._los(value)

    def port_in(self, port):
        if port == PORT_DMA_LEN:
            return self.laenge
        return 0

    def _los(self, cmd):
        if self.bus is None or self.laenge <= 0:
            return
        n = min(self.laenge, 1 << 24)          # Notbremse gegen Tippfehler
        if cmd == 1:
            self.bus.write_block(self.dst, self.bus.read_block(self.src, n))
            self.bytes_gesamt += n
            return
        if cmd == 2:
            self.bus.write_block(self.dst, bytes([self.wert]) * n)
            self.bytes_gesamt += n
            return
        # --- Suchbefehle ---------------------------------------------------
        # Wie die Zeichenkettenbefehle echter Prozessoren: der Baustein liest
        # den Block am Stueck und meldet, wie weit eine Farbe reicht. Das
        # Fuellwerkzeug in Paint braucht sonst je Bildpunkt einen eigenen
        # Lesebefehl -- fuer eine Flaeche waren das eine halbe Minute.
        # Das Ergebnis steht danach im Laengenregister.
        muster = bytes([self.wert])
        if cmd == 3:                     # wie viele Bytes ab SRC sind == VAL?
            blk = self.bus.read_block(self.src, n)
            self.laenge = len(blk) - len(blk.lstrip(muster))
        elif cmd == 4:                   # wo steht ab SRC das erste == VAL?
            blk = self.bus.read_block(self.src, n)
            self.laenge = blk.find(muster)
            if self.laenge < 0:
                self.laenge = 0xFFFFFFFF
        elif cmd == 7:                                 # Bild skaliert aus dem RAM
            # Nachster-Nachbar-Verfahren: fuer jede Zielzeile wird die
            # passende Quellzeile ausgesucht, fuer jede Zielspalte die
            # passende Quellspalte. Ohne Kommazahlen -- die Schrittweite
            # steht als Bruch aus Quell- und Zielgroesse fest. Genau so
            # skalieren Grafikkarten seit jeher.
            #   SRC = Bildpunkte, CHR = Quellbreite | Quellhoehe<<16
            #   X,Y = Ziel-Ecke, W,H = Zielgroesse
            if self.bus is None or w <= 0 or h <= 0:
                return
            qb = b["chr"] & 0xFFFF
            qh = (b["chr"] >> 16) & 0xFFFF
            if qb <= 0 or qh <= 0:
                return
            quelle = self.bus.read_block(b["src"], qb * qh)
            # Spaltenzuordnung einmal ausrechnen, nicht je Zeile
            spalten = [(i * qb) // w for i in range(w)]
            for zy in range(h):
                yy = y + zy
                if not (0 <= yy < GFX_H):
                    continue
                qz = (zy * qh) // h
                zeile = quelle[qz * qb:qz * qb + qb]
                if not zeile:
                    continue
                neu = bytes(zeile[c] for c in spalten)
                x0 = x
                if x0 < 0:
                    neu = neu[-x0:]
                    x0 = 0
                if x0 + len(neu) > GFX_W:
                    neu = neu[:GFX_W - x0]
                if not neu:
                    continue
                off = yy * GFX_W + x0
                if 255 in neu:
                    for c in range(len(neu)):
                        if neu[c] != 255:
                            fb[off + c] = neu[c]
                else:
                    fb[off:off + len(neu)] = neu

        elif cmd == 5:                   # wie viele Bytes VOR SRC sind == VAL?
            anfang = self.src - n + 1
            blk = self.bus.read_block(anfang, n)
            self.laenge = len(blk) - len(blk.rstrip(muster))
        self.bytes_gesamt += n


# ---------------------------------------------------------------------------
# Der ROM-Baustein und der Schlitz daneben
#
# Auf einem echten Mainboard sitzt das BIOS auf einem gesockelten Flash-Chip.
# Man kann ihn neu beschreiben -- und genau dabei kann man ein Board
# unbrauchbar machen. Deshalb haben Boards seit Jahren "BIOS Flashback": eine
# Datei auf einem USB-Stick, ein Knopf, fertig, ganz ohne laufendes System.
#
# Dieses Geraet ist beides zusammen: Befehl 1 laesst den WIRTSRECHNER eine
# Datei aussuchen (das ist der USB-Stick), Befehl 3 brennt sie in den Chip.
#
# Absichtlich dumm: geprueft wird hier nichts. Ob das Abbild eine gueltige
# Kennung und Pruefsumme hat, entscheidet die Firmware -- so wie auf einem
# echten Board, wo das Flash-Werkzeug im BIOS sitzt und nicht im Chip.
#
# Gebrannt wird die Datei, nicht der laufende Chip. Wer den Speicher
# ueberschreibt, aus dem die CPU gerade ihre Befehle holt, stuerzt im selben
# Augenblick ab; echte Flash-Programme kopieren sich dafuer erst ins RAM.
# Hier gilt schlicht: das neue BIOS gilt ab dem naechsten Einschalten.
# ---------------------------------------------------------------------------

class Flash:
    OK          = 0
    KEIN_PUFFER = 1
    ABGEBROCHEN = 2
    ZU_GROSS    = 3
    KEINE_SICHERUNG = 4
    SCHREIBFEHLER   = 5
    GESPERRT        = 6

    def __init__(self, rom_path):
        self.rom_path = rom_path
        self.backup_path = os.path.splitext(rom_path)[0] + ".backup.bin"
        self.bus = None
        self.waehler = None          # setzt pc.py: oeffnet den Dateidialog
        self.puffer = b""
        self.ziel = 0
        self.laenge = 0
        self.status = self.OK
        self.gebrannt = 0            # nur zum Nachschauen in Tests
        # Das Abbild fuer EINEN Start. Es liegt im Board, nicht auf der
        # Platte: ein Testabbild soll nicht ewig herumliegen, und ein
        # misslungener Versuch darf hoechstens einen Neustart kosten.
        self.einmal = None
        # Ein angemeldeter dauerhafter Flashvorgang. Die Firmware fragt beim
        # naechsten Start in Rot nach, bevor irgendetwas geschrieben wird.
        self.wunsch = False
        # Das Sperr-Latch (Befehl 10). Solange es steht, verweigern Brennen
        # und Zuruecksetzen den Dienst -- und zwar HIER im Baustein, nicht im
        # Setup. Das ist der Unterschied, auf den es ankommt: P_FLASH_CMD ist
        # ein ganz normaler Port, und der TB-32 kennt keine Portrechte. Ein
        # Schalter im Setup wuerde nur das Setup binden; jedes Programm im
        # laufenden System koennte weiter brennen. Ein Latch im Bauteil bindet
        # alle. Geloescht wird es ausschliesslich durch power_on(), also durch
        # einen echten Neustart -- genauso macht es das Lock-Bit eines echten
        # Chipsatzes.
        self.gesperrt = False

    def port_out(self, port, value):
        if port == PORT_FLASH_ADDR:
            self.ziel = value
        elif port == PORT_FLASH_SIZE:
            self.laenge = value
        elif port == PORT_FLASH_CMD:
            self.status = self._befehl(value)

    def port_in(self, port):
        if port == PORT_FLASH_SIZE:
            return len(self.puffer)
        return self.status

    def _befehl(self, cmd):
        if cmd == 1:                                   # Datei vom Wirt holen
            self.puffer = b""
            if self.waehler is None:
                return self.ABGEBROCHEN
            daten = self.waehler()
            if not daten:
                return self.ABGEBROCHEN
            self.puffer = daten
            return self.OK

        if cmd == 2:                                   # Puffer in den RAM
            if not self.puffer or self.bus is None:
                return self.KEIN_PUFFER
            self.bus.write_block(self.ziel, self.puffer)
            return self.OK

        if cmd == 10:                                  # Chip sperren, bis zum Neustart
            self.gesperrt = True
            return self.OK

        if cmd == 11:                                  # ist er gesperrt?
            return 1 if self.gesperrt else 0

        if cmd == 3:                                   # brennen
            if self.gesperrt:
                return self.GESPERRT
            if not self.puffer:
                return self.KEIN_PUFFER
            if len(self.puffer) > ROM_SIZE:
                return self.ZU_GROSS
            try:
                if os.path.exists(self.rom_path):
                    with open(self.rom_path, "rb") as f:
                        alt = f.read()
                    with open(self.backup_path, "wb") as f:
                        f.write(alt)
                with open(self.rom_path, "wb") as f:
                    f.write(self.puffer)
            except OSError:
                return self.SCHREIBFEHLER
            self.gebrannt += 1
            self.wunsch = False
            return self.OK

        if cmd == 5:                                   # Puffer aus dem RAM
            if self.bus is None or self.laenge <= 0 or self.laenge > ROM_SIZE:
                return self.ZU_GROSS
            self.puffer = bytes(self.bus.read_block(self.ziel, self.laenge))
            return self.OK

        if cmd == 6:                                   # nur fuer den naechsten Start
            # Auch das faellt unter die Sperre. Sonst waere sie in einem Zug
            # zu umgehen: Abbild fuer den naechsten Start anmelden, Reset --
            # und beim Hochfahren ist das Latch ja wieder offen.
            if self.gesperrt:
                return self.GESPERRT
            if not self.puffer:
                return self.KEIN_PUFFER
            self.einmal = self.puffer
            return self.OK

        if cmd == 7:                                   # alles wieder abmelden
            self.einmal = None
            self.wunsch = False
            return self.OK

        if cmd == 8:                                   # dauerhaft flashen wollen
            if self.gesperrt:                          # derselbe Weg ueber den Neustart
                return self.GESPERRT
            if not self.puffer:
                return self.KEIN_PUFFER
            self.wunsch = True
            return self.OK

        if cmd == 9:                                   # liegt ein Wunsch an?
            return 1 if self.wunsch else 0

        if cmd == 4:                                   # Sicherung zurueck
            if self.gesperrt:
                return self.GESPERRT
            if not os.path.exists(self.backup_path):
                return self.KEINE_SICHERUNG
            try:
                with open(self.backup_path, "rb") as f:
                    alt = f.read()
                with open(self.rom_path, "wb") as f:
                    f.write(alt)
            except OSError:
                return self.SCHREIBFEHLER
            return self.OK

        return self.KEIN_PUFFER


# ---------------------------------------------------------------------------
# Netzwerkkarte
# ---------------------------------------------------------------------------

class Netzkarte:
    """TB-NET: schickt und empfängt Rahmen. Mehr weiß sie nicht.

    Eine echte Netzwerkkarte versteht nichts von IP, von Namen oder von
    Webseiten. Sie kennt genau zwei Dinge: „schick diese Bytes raus" und
    „hier sind Bytes angekommen". Alles andere — Adressen, Verbindungen,
    Protokolle — macht das Betriebssystem. Deshalb steht hier auch nicht mehr.

    Der Draht ist auf dem Mac eine UDP-Multicast-Gruppe über den Rückkanal
    (127.0.0.1). Zwei laufende TB-32 hören dieselbe Gruppe und sehen deshalb
    gegenseitig ihre Rahmen — wie zwei Rechner an einem Hub. Die Pakete
    verlassen den Mac nicht.

    Auf dem Pi ersetzt später die echte Karte den Draht. Die Ports bleiben,
    und damit bleibt auch der ganze TB-32-Code unverändert.
    """

    GRUPPE = "239.32.32.32"
    PORT = 32032
    karten = 0                         # laufende Nummer für die Adresse
    MAXRAHMEN = 1518                   # wie bei Ethernet: 1500 Nutzdaten + Kopf

    def __init__(self, cpu_ref, mac=None):
        self.cpu = cpu_ref
        self.bus = None
        self.addr = 0
        self.len = 0
        self.zindex = 0
        self.empfangen = 0
        self.gesendet = 0
        self.eingang = deque(maxlen=64)
        self.sock = None
        # Eigene Adresse: 02:TB:.. -- das Bit 0x02 im ersten Byte heißt
        # "selbst vergeben", genau dafür ist es da. Der Rest kommt aus der
        # Prozessnummer, damit zwei Fenster auf demselben Mac sich nicht
        # dieselbe Adresse geben, und aus einem Zähler, damit auch zwei
        # Karten im selben Prozess (Tests) sich unterscheiden.
        Netzkarte.karten += 1
        self.mac = mac or bytes([0x02, 0x54, 0x42,
                                 (os.getpid() >> 8) & 0xFF,
                                 os.getpid() & 0xFF,
                                 Netzkarte.karten & 0xFF])
        self._anschliessen()

    def _anschliessen(self):
        """Steckt das Kabel ein. Geht es nicht, bleibt die Karte stumm."""
        import socket
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            if hasattr(socket, "SO_REUSEPORT"):
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            s.bind(("", self.PORT))
            # Ausdruecklich ueber 127.0.0.1 -- nicht ueber "irgendeine
            # Schnittstelle". Mit INADDR_ANY sucht macOS sich die Karte des
            # Standardwegs (WLAN) aus; die Rahmen gehen dann hinaus und
            # kommen auf demselben Rechner nie an. Mit dem Rueckkanal sehen
            # sich zwei TB-32 auf demselben Mac zuverlaessig.
            lo = socket.inet_aton("127.0.0.1")
            s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                         struct.pack("4s4s", socket.inet_aton(self.GRUPPE), lo))
            s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, lo)
            s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
            s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
            s.setblocking(False)
            self.sock = s
        except OSError:
            self.sock = None               # kein Netz: Bit 0 bleibt 0

    def close(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    # -- vom Draht abholen; ruft die Maschine jede Zeitscheibe auf ----------
    def poll(self):
        if self.sock is None:
            return
        while True:
            try:
                daten, _ = self.sock.recvfrom(2048)
            except (BlockingIOError, OSError):
                return
            if len(daten) < 14:
                continue
            if daten[6:12] == self.mac:    # der eigene Rahmen kommt zurück
                continue
            ziel = daten[:6]
            if ziel != self.mac and ziel != b"\xff\xff\xff\xff\xff\xff":
                continue                   # nicht an uns gerichtet
            self.eingang.append(daten[:self.MAXRAHMEN])
            self.empfangen += 1
            if self.cpu[0] is not None:
                self.cpu[0].raise_irq(IRQ_NET)

    # -- Ports -------------------------------------------------------------
    def port_out(self, port, value):
        if port == PORT_NET_ADDR:
            self.addr = value
        elif port == PORT_NET_LEN:
            self.len = value
        elif port == PORT_NET_ZINDEX:
            self.zindex = value
        elif port == PORT_NET_CMD:
            self._befehl(value)

    def _befehl(self, cmd):
        if cmd == 1:                       # senden
            n = max(14, min(self.len, self.MAXRAHMEN))
            rahmen = bytearray(self.bus.read_block(self.addr, n))
            rahmen[6:12] = self.mac        # der Absender kommt von der Karte
            if self.sock is not None:
                try:
                    self.sock.sendto(bytes(rahmen), (self.GRUPPE, self.PORT))
                    self.gesendet += 1
                except OSError:
                    pass
        elif cmd == 2:                     # den nächsten Rahmen abholen
            if self.eingang:
                r = self.eingang.popleft()
                self.bus.write_block(self.addr, r)
                self.len = len(r)
            else:
                self.len = 0
        elif cmd == 3:                     # Warteschlange leeren
            self.eingang.clear()

    def port_in(self, port):
        if port == PORT_NET_STATUS:
            return (1 if self.sock is not None else 0) | \
                   (2 if self.eingang else 0)
        if port == PORT_NET_LEN:
            return self.len
        if port == PORT_NET_MAC_HI:
            return (self.mac[0] << 8) | self.mac[1]
        if port == PORT_NET_MAC_LO:
            return int.from_bytes(self.mac[2:6], "big")
        if port == PORT_NET_ZAEHLER:
            return self.gesendet if self.zindex == 1 else self.empfangen
        return 0
