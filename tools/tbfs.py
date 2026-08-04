#!/usr/bin/env python3
"""
TBFS von außen: Dateien zwischen dem Mac und der virtuellen Festplatte
schieben. Das ist das Gegenstück zum Dateisystem in system/fs.c -- beide
müssen exakt dasselbe Format sprechen.

    python3 tools/tbfs.py list
    python3 tools/tbfs.py put datei.txt [NAME.TXT]
    python3 tools/tbfs.py get NAME.TXT [ziel]
    python3 tools/tbfs.py del NAME.TXT
    python3 tools/tbfs.py format
"""

import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "disk", "hd0.img")

SECTOR = 512
MAGIC = 0x54424653
SUPER = 512
DIR0 = 513
DIRSECS = 8
DATA = 576
ENTSIZE = 32
MAXFILES = 128


class TBFS:
    def __init__(self, path=IMG):
        self.path = path
        with open(path, "rb") as f:
            self.img = bytearray(f.read())
        self.total = len(self.img) // SECTOR
        # Welche Sektoren wurden angefasst? Nur die werden zurückgeschrieben.
        # Früher ging das ganze Abbild raus -- lief nebenher der Emulator und
        # hatte etwas gespeichert, war das danach wieder weg.
        self.dirty = set()

    # -- Hilfen ------------------------------------------------------------

    def _sec(self, n):
        return n * SECTOR

    def _ent(self, i):
        return self._sec(DIR0) + i * ENTSIZE

    def _u32(self, off):
        return struct.unpack_from("<I", self.img, off)[0]

    def _put32(self, off, v):
        struct.pack_into("<I", self.img, off, v & 0xFFFFFFFF)
        self.markiere(off // SECTOR)     # jeder Schreibzugriff meldet sich

    def name(self, i):
        raw = bytes(self.img[self._ent(i):self._ent(i) + 16])
        return raw.split(b"\x00")[0].decode("latin-1")

    def start(self, i):  return self._u32(self._ent(i) + 16)
    def size(self, i):   return self._u32(self._ent(i) + 20)
    def used(self, i):   return self._u32(self._ent(i) + 24) & 0xFF
    def typ(self, i):    return self._u32(self._ent(i) + 24) & 0xFF
    def parent(self, i): return ((self._u32(self._ent(i) + 24) >> 16) & 0xFFFF) - 1

    def set_info(self, i, typ, parent):
        self._put32(self._ent(i) + 24, (typ & 0xFF) | (((parent + 1) & 0xFFFF) << 16))

    def sectors_for(self, n):
        return (n + SECTOR - 1) // SECTOR

    def markiere(self, sektor, anzahl=1):
        for i in range(sektor, sektor + anzahl):
            self.dirty.add(i)

    def save(self):
        """Schreibt nur die geänderten Sektoren -- niemals das ganze Abbild."""
        if not os.path.exists(self.path):
            with open(self.path, "wb") as f:
                f.write(self.img)
            self.dirty.clear()
            return
        with open(self.path, "r+b") as f:
            for sektor in sorted(self.dirty):
                f.seek(sektor * SECTOR)
                f.write(self.img[sektor * SECTOR:(sektor + 1) * SECTOR])
            f.flush()
        self.dirty.clear()

    # -- Verwaltung --------------------------------------------------------

    def formatted(self):
        return self._u32(self._sec(SUPER)) == MAGIC

    def format(self):
        self.img[self._sec(SUPER):self._sec(SUPER) + SECTOR] = bytearray(SECTOR)
        self.markiere(SUPER)
        self._put32(self._sec(SUPER), MAGIC)
        self._put32(self._sec(SUPER) + 4, self.total)
        self._put32(self._sec(SUPER) + 8, DIR0)
        self._put32(self._sec(SUPER) + 12, DATA)
        self.img[self._sec(DIR0):self._sec(DIR0 + DIRSECS)] = bytearray(DIRSECS * SECTOR)
        self.markiere(DIR0, DIRSECS)
        self.save()

    def list(self, ordner=-1):
        out = []
        for i in range(MAXFILES):
            if self.typ(i) and self.parent(i) == ordner:
                out.append((self.name(i), self.size(i), self.start(i), self.typ(i)))
        return out

    def find(self, name, ordner=-1):
        for i in range(MAXFILES):
            if self.typ(i) and self.parent(i) == ordner \
               and self.name(i).lower() == name.lower():
                return i
        return -1

    def mkdir(self, name, ordner=-1):
        """Legt einen Ordner an (oder liefert den vorhandenen)."""
        if not self.formatted():
            self.format()
        i = self.find(name, ordner)
        if i >= 0:
            return i
        i = next((k for k in range(MAXFILES) if not self.typ(k)), -1)
        if i < 0:
            raise SystemExit("Verzeichnis ist voll")
        e = self._ent(i)
        self.img[e:e + ENTSIZE] = bytearray(ENTSIZE)
        self.markiere(DIR0, DIRSECS)
        raw = name.encode("latin-1")[:15]
        self.img[e:e + len(raw)] = raw
        self.set_info(i, 2, ordner)
        self.save()
        return i

    def pfad_ordner(self, pfad):
        """Legt \"SYSTEM/UNTER\" an und gibt den letzten Ordner zurück."""
        ordner = -1
        for teil in pfad.replace("\\", "/").split("/"):
            if teil:
                ordner = self.mkdir(teil, ordner)
        return ordner

    def alloc(self, n):
        start = DATA
        again = True
        while again:
            again = False
            for i in range(MAXFILES):
                if self.typ(i) != 1:
                    continue
                end = self.start(i) + self.sectors_for(self.size(i))
                if start < end and self.start(i) < start + n:
                    start = end
                    again = True
        if start + n > self.total:
            raise SystemExit("Kein Platz mehr auf der virtuellen Platte")
        return start

    def put(self, name, data, ordner=-1):
        if not self.formatted():
            self.format()
        name = name[:15]
        n = max(1, self.sectors_for(len(data)))
        idx = self.find(name, ordner)
        if idx >= 0 and self.sectors_for(self.size(idx)) >= n:
            start = self.start(idx)
        else:
            if idx >= 0:
                self.set_info(idx, 0, -1)
            else:
                idx = next((i for i in range(MAXFILES) if not self.typ(i)), -1)
                if idx < 0:
                    raise SystemExit("Verzeichnis ist voll")
            start = self.alloc(n)
        e = self._ent(idx)
        self.img[e:e + ENTSIZE] = bytearray(ENTSIZE)
        self.markiere(DIR0, DIRSECS)
        raw = name.encode("latin-1")[:15]
        self.img[e:e + len(raw)] = raw
        self._put32(e + 16, start)
        self._put32(e + 20, len(data))
        self.set_info(idx, 1, ordner)
        self._put32(e + 28, 0)
        blob = bytes(data).ljust(n * SECTOR, b"\x00")
        self.img[self._sec(start):self._sec(start) + len(blob)] = blob
        self.markiere(start, max(1, (len(blob) + SECTOR - 1) // SECTOR))
        self.save()
        return idx

    def get(self, name, ordner=-1):
        i = self.find(name, ordner)
        if i < 0:
            return None
        off = self._sec(self.start(i))
        return bytes(self.img[off:off + self.size(i)])

    def delete(self, name, ordner=-1):
        i = self.find(name, ordner)
        if i < 0:
            return False
        self.set_info(i, 0, -1)
        self.img[self._ent(i):self._ent(i) + 16] = bytearray(16)
        self.markiere(DIR0, DIRSECS)
        self.save()
        return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    fs = TBFS()
    befehl = sys.argv[1]

    if befehl == "list":
        if not fs.formatted():
            print("Die virtuelle Platte ist noch nicht formatiert.")
            return 0

        def zeige(ordner, tiefe):
            for n, groesse, st, typ in fs.list(ordner):
                einzug = "  " * tiefe
                if typ == 2:
                    print(f"{einzug}[{n}]")
                    zeige(fs.find(n, ordner), tiefe + 1)
                else:
                    print(f"{einzug}{n:18s}{groesse:10d}")
        zeige(-1, 0)
    elif befehl == "put":
        src = sys.argv[2]
        ziel = sys.argv[3] if len(sys.argv) > 3 else os.path.basename(src).upper()
        ziel = ziel.replace("\\", "/")
        if "/" in ziel:
            ordner = fs.pfad_ordner(ziel.rsplit("/", 1)[0])
            name = ziel.rsplit("/", 1)[1]
        else:
            ordner, name = -1, ziel
        with open(src, "rb") as f:
            fs.put(name, f.read(), ordner)
        print(f"{src} -> A:\\{ziel.replace('/', chr(92))}")
    elif befehl == "mkdir":
        fs.pfad_ordner(sys.argv[2])
        print(f"Ordner A:\\{sys.argv[2]} angelegt")
    elif befehl == "get":
        data = fs.get(sys.argv[2])
        if data is None:
            print("Datei nicht gefunden")
            return 1
        ziel = sys.argv[3] if len(sys.argv) > 3 else sys.argv[2]
        with open(ziel, "wb") as f:
            f.write(data)
        print(f"A:\\{sys.argv[2]} -> {ziel} ({len(data)} Bytes)")
    elif befehl == "del":
        print("gelöscht" if fs.delete(sys.argv[2]) else "nicht gefunden")
    elif befehl == "format":
        fs.format()
        print("Virtuelle Platte formatiert.")
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
