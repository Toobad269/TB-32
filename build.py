#!/usr/bin/env python3
"""
Baut die komplette Software für den virtuellen PC:

    firmware/bios.asm    ->  firmware/bios.bin      (BIOS-ROM)
    system/boot.asm      ->  Sektor 0 der Platte    (Bootsektor)
    system/kernel.asm    ->  \SYSTEM\KERNEL.BIN     (Betriebssystem)

Das ist das Gegenstück zu einem echten Build: Assemblieren, dann das
Festplatten-Image zusammensetzen.
"""

import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from tools.assembler import assemble_text, AsmError
from tools.tcc import compile_source, CompileError
from hardware.isa import ROM_SIZE

SECTOR = 512
DISK_SECTORS = 16384          # 8 MiB Festplatte


def asm_file(src, dst=None, symfile=None):
    with open(src, "r", encoding="utf-8") as f:
        text = f.read()
    data, symbols, base = assemble_text(text, os.path.dirname(os.path.abspath(src)))
    if dst:
        with open(dst, "wb") as f:
            f.write(data)
    if symfile:
        with open(symfile, "w") as f:
            for name, addr in sorted(symbols.items(), key=lambda kv: kv[1]):
                f.write(f"{addr:08X} {name}\n")
    return data, symbols


def bios_kopf_stempeln(pfad):
    """Trägt Länge und Prüfsumme in den Kopf eines BIOS-Abbildes ein.

    Das Mainboard prüft beides beim Einschalten (`Machine.rom_pruefen`) und
    greift sonst zur Sicherung. Der Assembler kann die Summe nicht selbst
    einsetzen -- sie hängt vom fertigen Abbild ab, also erst hier."""
    with open(pfad, "rb") as f:
        roh = bytearray(f.read())
    if bytes(roh[4:8]) != b"TBBI":
        raise SystemExit(
            f"{pfad}: kein TBBI-Kopf an Position 4. Ein BIOS beginnt mit "
            f"einem Sprung, dann 'TBBI', Länge, Prüfsumme -- siehe Doku 16.")
    while len(roh) % 4:
        roh.append(0)
    roh[8:12] = len(roh).to_bytes(4, "little")
    roh[12:16] = b"\x00\x00\x00\x00"
    h = 0x1234
    for i in range(0, len(roh), 4):
        h = (h * 31 + int.from_bytes(roh[i:i + 4], "little")) & 0xFFFFFFFF
    roh[12:16] = h.to_bytes(4, "little")
    with open(pfad, "wb") as f:
        f.write(roh)
    return bytes(roh), h


def build():
    fw = os.path.join(ROOT, "firmware")
    sysdir = os.path.join(ROOT, "system")
    diskdir = os.path.join(ROOT, "disk")
    os.makedirs(diskdir, exist_ok=True)

    # --- 1. BIOS -----------------------------------------------------------
    bios, syms = asm_file(os.path.join(fw, "bios.asm"),
                          os.path.join(fw, "bios.bin"),
                          os.path.join(fw, "bios.sym"))
    if len(bios) > ROM_SIZE:
        raise SystemExit("BIOS passt nicht ins ROM!")
    bios, summe = bios_kopf_stempeln(os.path.join(fw, "bios.bin"))
    print(f"  BIOS    {len(bios):6d} Bytes  ({len(bios)*100//ROM_SIZE}% des ROMs, "
          f"Prüfsumme {summe:08X})")

    # Das kleinste BIOS, das den Rechner startet -- Vorlage für ein eigenes.
    # Es wird bei jedem Bau mit übersetzt, damit es nicht verrottet.
    minpfad = os.path.join(fw, "minimal.asm")
    if os.path.exists(minpfad):
        mini, _ = asm_file(minpfad, os.path.join(fw, "minimal.bin"))
        mini, msum = bios_kopf_stempeln(os.path.join(fw, "minimal.bin"))
        print(f"  BIOS    {len(mini):6d} Bytes  (minimal.bin, "
              f"Prüfsumme {msum:08X})")

    # --- 2. Bootsektor -----------------------------------------------------
    bootpath = os.path.join(sysdir, "boot.asm")
    boot = b""
    if os.path.exists(bootpath):
        boot, _ = asm_file(bootpath, os.path.join(sysdir, "boot.bin"))
        if len(boot) > SECTOR:
            raise SystemExit(f"Bootsektor zu groß: {len(boot)} > {SECTOR}")
        print(f"  Boot    {len(boot):6d} Bytes  (max {SECTOR})")

    # --- 3. Kernel: erst C übersetzen, dann assemblieren -------------------
    kernel = b""
    cpath = os.path.join(sysdir, "kernel.c")
    if os.path.exists(cpath):
        with open(cpath, encoding="utf-8") as f:
            csrc = f.read()
        casm = compile_source(csrc, sysdir)
        with open(os.path.join(sysdir, "start.asm"), encoding="utf-8") as f:
            start = f.read()
        combined = start + "\n" + casm
        with open(os.path.join(sysdir, "kernel.asm"), "w", encoding="utf-8") as f:
            f.write(combined)
        data, symbols, base = assemble_text(combined, sysdir)
        kernel = data
        with open(os.path.join(sysdir, "kernel.bin"), "wb") as f:
            f.write(kernel)
        with open(os.path.join(sysdir, "kernel.sym"), "w") as f:
            for name, addr in sorted(symbols.items(), key=lambda kv: kv[1]):
                f.write(f"{addr:08X} {name}\n")
        print(f"  Kernel  {len(kernel):6d} Bytes  "
              f"({(len(kernel)+SECTOR-1)//SECTOR} Sektoren, "
              f"{len(casm.splitlines())} Zeilen Assembler aus C)")
        # Der Kernel wird nach 0x10000 geladen. Ab SECBUF liegen die festen
        # Puffer des Dateisystems -- waechst der Kernel dort hinein,
        # ueberschreibt er im Betrieb sein eigenes Verzeichnis. Das sieht man
        # erst viel spaeter und an ganz anderer Stelle, also hier hart pruefen.
        KERNEL_LOAD = 0x00010000
        SECBUF      = 0x000B0000
        if KERNEL_LOAD + len(kernel) > SECBUF:
            raise SystemExit(
                f"Kernel ist {len(kernel)} Bytes gross und reicht bis "
                f"0x{KERNEL_LOAD + len(kernel):06X} -- ab 0x{SECBUF:06X} "
                f"liegen die Puffer des Dateisystems (siehe system/fs.c)!")

    # --- 4. Bootsektor auf die Platte legen --------------------------------
    #
    # WICHTIG: nur der eine Sektor wird geschrieben, nicht das ganze Abbild.
    # Frueher las build.py die Platte komplett ein, aenderte Kernel und
    # Bootsektor und schrieb alles zurueck -- lief nebenher der Emulator,
    # waren dessen inzwischen gespeicherte Dateien danach weg.
    #
    # Der Kernel steht hier nicht mehr: er kommt weiter unten als ganz
    # normale Datei \SYSTEM\KERNEL.BIN aufs Laufwerk, und der Bootsektor
    # sucht ihn dort. Feste Kernelsektoren gibt es nicht mehr.
    img_path = os.path.join(diskdir, "hd0.img")
    if not os.path.exists(img_path) or os.path.getsize(img_path) < DISK_SECTORS * SECTOR:
        vorhanden = b""
        if os.path.exists(img_path):
            with open(img_path, "rb") as f:
                vorhanden = f.read()
        with open(img_path, "wb") as f:
            f.write(vorhanden.ljust(DISK_SECTORS * SECTOR, b"\x00"))

    with open(img_path, "r+b") as f:
        if boot:
            sec = bytearray(SECTOR)
            sec[:len(boot)] = boot
            sec[SECTOR - 2] = 0x55        # Bootsignatur, wie beim echten PC
            sec[SECTOR - 1] = 0xAA
            f.seek(0)
            f.write(sec)
        f.flush()

    # --- 5. Programme übersetzen und einsortiert auf die Platte legen ------
    #
    # \SYSTEM  Werkzeuge (liegen im Suchpfad, sind von überall aufrufbar)
    # \PROGS   Hilfsprogramme
    # \SOURCE  Quelltexte, auch der des Compilers selbst
    WERKZEUGE = {"CC.TBX", "ASM.TBX", "PY.TBX"}
    # Programme des Systems: sie stehen im Startmenue und liegen in
    # \SYSTEM\PROGS. Alles andere landet in \PROGS.
    SYSTEMPROGRAMME = {"CALC.TBX", "FLAPPY.TBX", "FENSTER.TBX"}
    # Bibliotheken werden nur eingebunden, nie fuer sich uebersetzt.
    NUR_BIBLIOTHEK = ("proglib.c", "gfxlib.c")
    # Diese Quelltexte kommen mit aufs Laufwerk, aber ohne fertiges Programm --
    # sie sind zum Selberuebersetzen auf dem TB-32 gedacht.
    NUR_QUELLTEXT = ("crash.c",)

    progdir = os.path.join(ROOT, "programs")
    if os.path.isdir(progdir):
        from tools.tbfs import TBFS
        with open(os.path.join(progdir, "prog_start.asm"), encoding="utf-8") as f:
            progstart = f.read()
        fs = TBFS(img_path)
        if not fs.formatted():
            fs.format()
        system_dir = fs.pfad_ordner("SYSTEM")
        # \SYSTEM\PROGS -- die Programme, die zum System gehoeren. Wie
        # System32 bei Windows: es sind Dateien, aber Loeschen kostet das
        # Passwort. Was der Benutzer selbst hineinlegt, gehoert nach \PROGS.
        sysprogs_dir = fs.pfad_ordner("SYSTEM/PROGS")
        progs_dir = fs.pfad_ordner("PROGS")
        source_dir = fs.pfad_ordner("SOURCE")

        namen = []
        for datei in sorted(os.listdir(progdir)):
            if not datei.endswith(".c"):
                continue
            if datei in NUR_BIBLIOTHEK or datei in NUR_QUELLTEXT:
                continue
            with open(os.path.join(progdir, datei), encoding="utf-8") as f:
                src = f.read()
            asm = compile_source(src, progdir)
            code, _, _ = assemble_text(progstart + "\n" + asm, progdir)
            name = datei[:-2].upper()[:11] + ".TBX"
            if name in WERKZEUGE:
                ziel, wo = system_dir, "SYSTEM"
            elif name in SYSTEMPROGRAMME:
                ziel, wo = sysprogs_dir, "SYSTEM\\PROGS"
            else:
                ziel, wo = progs_dir, "PROGS"
            fs.put(name, code, ziel)
            namen.append(f"{wo}\\{name}")
        if namen:
            print("  Programme " + ", ".join(namen))
        print("  Zum Selberuebersetzen  " +
              ", ".join("SOURCE\\" + d[:-2].upper() + ".C" for d in NUR_QUELLTEXT))

        # Der Quelltext des Compilers gehört mit auf die Platte -- ohne ihn
        # könnte sich der Rechner seinen Compiler nicht selbst neu bauen.
        for quelle in ("cc.c", "proglib.c", "gfxlib.c", "calc.c", "crash.c"):
            with open(os.path.join(progdir, quelle), "rb") as f:
                fs.put(quelle.upper(), f.read(), source_dir)

        # Das System selbst. KERNEL.BIN ist hier keine Kopie und keine
        # Dekoration: der Bootsektor sucht genau diese Datei und startet
        # genau diese Bytes. Loescht man sie und startet neu, bleibt der
        # Rechner mit "\SYSTEM\KERNEL.BIN fehlt" stehen -- so wie ein echter
        # Rechner ohne Betriebssystem. Zurueck holt man sie mit build.py.
        sysdateien = 0
        for quelle, ziel in ((os.path.join(sysdir, "kernel.bin"), "KERNEL.BIN"),
                             (os.path.join(ROOT, "firmware", "bios.bin"), "BIOS.BIN"),
                             (os.path.join(sysdir, "kernel.sym"), "KERNEL.SYM")):
            if os.path.exists(quelle):
                with open(quelle, "rb") as f:
                    fs.put(ziel, f.read(), system_dir)
                sysdateien += 1
        if sysdateien:
            print(f"  System    {sysdateien} Dateien in SYSTEM\\ sichtbar gemacht")

        # KEIN Benutzerkonto. Wer den Rechner frisch baut, richtet ihn beim
        # ersten Start selbst ein: Name, Passwort, fertig. Frueher legte
        # build.py hier ein offenes Konto "user" an -- praktisch beim
        # Entwickeln, aber wer das Projekt herunterlaedt, sass danach in
        # einem fremden Konto, das er nie angelegt hatte.
        #
        # Die Testwerkzeuge brauchen weiterhin einen angemeldeten Rechner;
        # die legen sich das Konto selbst an (test_konto in tools/headless.py).

        # Alles aus diskfiles/ wird 1:1 auf die Platte gespiegelt
        diskdir_src = os.path.join(ROOT, "diskfiles")
        anzahl = 0
        for wurzel, _, dateien in os.walk(diskdir_src):
            rel = os.path.relpath(wurzel, diskdir_src)
            ordner = -1 if rel == "." else fs.pfad_ordner(rel)
            for d in sorted(dateien):
                if d.startswith("."):
                    continue
                with open(os.path.join(wurzel, d), "rb") as f:
                    fs.put(d.upper(), f.read(), ordner)
                anzahl += 1
        if anzahl:
            print(f"  Dateien   {anzahl} aus diskfiles/ übernommen")

    print(f"  Platte  {os.path.getsize(img_path)//1024:6d} KiB -> {img_path}")


if __name__ == "__main__":
    print("Baue TOOBAD TB-32 ...")
    try:
        build()
    except AsmError as e:
        print(f"\nASSEMBLER-FEHLER: {e}")
        sys.exit(1)
    print("Fertig.")
