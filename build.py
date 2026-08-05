#!/usr/bin/env python3
"""
Builds the complete software for the virtual PC:

    firmware/bios.asm    ->  firmware/bios.bin      (BIOS ROM)
    system/boot.asm      ->  sector 0 of the disk    (boot sector)
    system/kernel.asm    ->  \SYSTEM\KERNEL.BIN     (operating system)

This is the counterpart to a real build: assemble, then put together the
disk image.
"""

import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from tools.assembler import assemble_text, AsmError
from tools.tcc import compile_source, CompileError
from hardware.isa import ROM_SIZE

SECTOR = 512
DISK_SECTORS = 16384          # 8 MiB disk


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
    """Writes length and checksum into the header of a BIOS image.

    The mainboard checks both at power-on (`Machine.rom_pruefen`) and falls
    back to the backup otherwise. The assembler can't fill in the checksum
    itself -- it depends on the finished image, so it only happens here."""
    with open(pfad, "rb") as f:
        roh = bytearray(f.read())
    if bytes(roh[4:8]) != b"TBBI":
        raise SystemExit(
            f"{pfad}: no TBBI header at offset 4. A BIOS begins with "
            f"a jump, then 'TBBI', length, checksum -- see Doc 16.")
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
        raise SystemExit("BIOS does not fit into ROM!")
    bios, summe = bios_kopf_stempeln(os.path.join(fw, "bios.bin"))
    print(f"  BIOS    {len(bios):6d} bytes  ({len(bios)*100//ROM_SIZE}% of ROM, "
          f"checksum {summe:08X})")

    # The smallest BIOS that boots the machine -- a template for building
    # your own. It gets compiled with every build so it doesn't rot.
    minpfad = os.path.join(fw, "minimal.asm")
    if os.path.exists(minpfad):
        mini, _ = asm_file(minpfad, os.path.join(fw, "minimal.bin"))
        mini, msum = bios_kopf_stempeln(os.path.join(fw, "minimal.bin"))
        print(f"  BIOS    {len(mini):6d} bytes  (minimal.bin, "
              f"checksum {msum:08X})")

    # --- 2. Boot sector -----------------------------------------------------
    bootpath = os.path.join(sysdir, "boot.asm")
    boot = b""
    if os.path.exists(bootpath):
        boot, _ = asm_file(bootpath, os.path.join(sysdir, "boot.bin"))
        if len(boot) > SECTOR:
            raise SystemExit(f"Boot sector too large: {len(boot)} > {SECTOR}")
        print(f"  Boot    {len(boot):6d} bytes  (max {SECTOR})")

    # --- 3. Kernel: compile C first, then assemble -------------------
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
        print(f"  Kernel  {len(kernel):6d} bytes  "
              f"({(len(kernel)+SECTOR-1)//SECTOR} sectors, "
              f"{len(casm.splitlines())} lines of assembly from C)")
        # The kernel is loaded at 0x10000. Starting at SECBUF are the
        # filesystem's fixed buffers -- if the kernel grows into that
        # region, it overwrites its own directory at runtime. That only
        # shows up much later and in a completely different place, so
        # check it strictly here.
        KERNEL_LOAD = 0x00010000
        SECBUF      = 0x000B0000
        if KERNEL_LOAD + len(kernel) > SECBUF:
            raise SystemExit(
                f"The kernel is {len(kernel)} bytes and reaches up to "
                f"0x{KERNEL_LOAD + len(kernel):06X} -- starting at 0x{SECBUF:06X} "
                f"are the filesystem's buffers (see system/fs.c)!")

    # --- 4. Write the boot sector to the disk --------------------------------
    #
    # IMPORTANT: only that one sector is written, not the whole image.
    # build.py used to read in the entire disk, modify the kernel and boot
    # sector, and write everything back -- if the emulator was running
    # alongside, its files saved in the meantime were gone afterward.
    #
    # The kernel no longer lives here: it goes onto the drive further below
    # as a completely normal file \SYSTEM\KERNEL.BIN, and the boot sector
    # looks for it there. There are no more fixed kernel sectors.
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
            sec[SECTOR - 2] = 0x55        # boot signature, just like on a real PC
            sec[SECTOR - 1] = 0xAA
            f.seek(0)
            f.write(sec)
        f.flush()

    # --- 5. Compile programs and sort them onto the disk ------
    #
    # \SYSTEM  tools (on the search path, callable from anywhere)
    # \PROGS   utility programs
    # \SOURCE  source files, including the compiler's own
    WERKZEUGE = {"CC.TBX", "ASM.TBX", "PY.TBX"}
    # System programs: they appear in the start menu and live in
    # \SYSTEM\PROGS. Everything else ends up in \PROGS.
    SYSTEMPROGRAMME = {"PROMPT.TBX", "MONITOR.TBX", "CONTROL.TBX",
                       "SETTINGS.TBX", "FILES.TBX"}

    # Libraries are only ever included, never compiled on their own.
    NUR_BIBLIOTHEK = ("proglib.c", "gfxlib.c")
    # These source files are copied onto the drive, but without a compiled
    # program -- they're meant to be compiled by hand on the TB-32 itself.
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
        # \SYSTEM\PROGS -- the programs that belong to the system. Like
        # System32 on Windows: they are files, but deleting them costs the
        # password. Whatever the user puts there themselves belongs in \PROGS.
        sysprogs_dir = fs.pfad_ordner("SYSTEM/PROGS")
        progs_dir = fs.pfad_ordner("PROGS")
        source_dir = fs.pfad_ordner("SOURCE")

        # --- Every program gets its own place in memory -------
        # Up until now, every program loaded at the same address. As long as
        # only one ran at a time, that was fine; since several run
        # simultaneously in windows, the second one would overwrite the
        # first one's code. Now each gets its place assigned at build time --
        # that's how early systems did it too, long before there was memory
        # management.
        # Tools (CC, ASM, PY) always run alone and are large -- they keep the
        # old location and get plenty of room.
        # Window programs run SIDE BY SIDE and each get their own small slot.
        WERKZEUG_BASIS = 0x00200000         # 512 KB for one of the tools
        PROG_BASIS = 0x00280000
        PROG_SLOT = 0x00020000              # 128 KB per window program
        PROG_MAXSLOTS = 16
        slot_nr = 0

        namen = []
        for datei in sorted(os.listdir(progdir)):
            if not datei.endswith(".c"):
                continue
            if datei in NUR_BIBLIOTHEK or datei in NUR_QUELLTEXT:
                continue
            with open(os.path.join(progdir, datei), encoding="utf-8") as f:
                src = f.read()
            asm = compile_source(src, progdir)
            name = datei[:-2].upper()[:11] + ".TBX"
            if name in WERKZEUGE:
                basis = WERKZEUG_BASIS
                platz = 0x00080000
            else:
                if slot_nr >= PROG_MAXSLOTS:
                    raise SystemExit(
                        f"More than {PROG_MAXSLOTS} window programs -- "
                        f"memory doesn't have that many slots.")
                basis = PROG_BASIS + slot_nr * PROG_SLOT
                platz = PROG_SLOT
                slot_nr += 1
            vorspann = f".equ PROG_BASE, 0x{basis:08X}\n"
            code, _, _ = assemble_text(vorspann + progstart + "\n" + asm, progdir)
            if len(code) > platz:
                raise SystemExit(
                    f"{datei} is {len(code)} bytes, but the slot only holds "
                    f"{platz}.")
            if name in WERKZEUGE:
                ziel, wo = system_dir, "SYSTEM"
            elif name in SYSTEMPROGRAMME:
                ziel, wo = sysprogs_dir, "SYSTEM\\PROGS"
            else:
                ziel, wo = progs_dir, "PROGS"
            # A file of the same name in the OTHER folder has to go.
            # Otherwise both copies exist after a move, and the search path
            # finds the old version -- the machine would launch yesterday's
            # program and nobody would understand why the change had no
            # effect.
            for anderer in (system_dir, sysprogs_dir, progs_dir):
                if anderer == ziel:
                    continue
                alt = fs.find(name, anderer)
                if alt >= 0:
                    fs.delete(name, anderer)
                    print(f"  Cleaned up  old version of {name} removed")
            fs.put(name, code, ziel)
            namen.append(f"{wo}\\{name}@{basis >> 20}M")
        if namen:
            print("  Programs " + ", ".join(namen))
        print("  To compile yourself  " +
              ", ".join("SOURCE\\" + d[:-2].upper() + ".C" for d in NUR_QUELLTEXT))

        # The compiler's source code belongs on the disk too -- without it
        # the machine couldn't rebuild its own compiler.
        for quelle in ("cc.c", "proglib.c", "gfxlib.c", "calc.c", "crash.c"):
            with open(os.path.join(progdir, quelle), "rb") as f:
                fs.put(quelle.upper(), f.read(), source_dir)

        # The system itself. KERNEL.BIN here is not a copy and not
        # decoration: the boot sector looks for exactly this file and boots
        # exactly these bytes. Delete it and reboot, and the machine stops
        # with "\SYSTEM\KERNEL.BIN missing" -- just like a real machine
        # without an operating system. You get it back with build.py.
        sysdateien = 0
        for quelle, ziel in ((os.path.join(sysdir, "kernel.bin"), "KERNEL.BIN"),
                             (os.path.join(ROOT, "firmware", "bios.bin"), "BIOS.BIN"),
                             (os.path.join(sysdir, "kernel.sym"), "KERNEL.SYM")):
            if os.path.exists(quelle):
                with open(quelle, "rb") as f:
                    fs.put(ziel, f.read(), system_dir)
                sysdateien += 1
        if sysdateien:
            print(f"  System    {sysdateien} files made visible in SYSTEM\\")

        # NO user account. Anyone building the machine fresh sets it up
        # themselves on first boot: name, password, done. build.py used to
        # create an open "user" account here -- convenient during
        # development, but anyone downloading the project would then find
        # themselves in a stranger's account they never created.
        #
        # The test tools still need a logged-in machine; they create the
        # account themselves (test_konto in tools/headless.py).

        # Everything from diskfiles/ is mirrored 1:1 onto the disk
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
            print(f"  Files     {anzahl} copied from diskfiles/")

    print(f"  Disk    {os.path.getsize(img_path)//1024:6d} KiB -> {img_path}")


if __name__ == "__main__":
    print("Building TOOBAD TB-32 ...")
    try:
        build()
    except AsmError as e:
        print(f"\nASSEMBLER ERROR: {e}")
        sys.exit(1)
    print("Done.")
