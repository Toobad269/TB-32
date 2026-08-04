TOOBAD-OS 1.0 -- QUICK START
============================

Type HELP for the command list, WIN for the desktop.

DIRECTORIES ON THIS DISK
  \SYSTEM    the tools: C compiler, assembler, Python interpreter
             (programs here are found from anywhere -- this is the PATH)
  \PROGS     utilities: memory test, benchmark
  \SOURCE    source code, including the compiler's own source

MOVING AROUND
  DIR              list this directory
  CD SOURCE        enter a directory
  CD ..            go up one level
  CD \             back to the root
  MD <name>        create a directory

WRITING PROGRAMS -- all three run on this machine

  C:          CD SOURCE
              EDIT MYPROG.C
              CC MYPROG.C MY.TBX
              MY

  Assembly:   ASM MYPROG.ASM MY.TBX
              MY

  Python:     PY MYPROG.PY

THE COMPILER BUILDS ITSELF
  CD SOURCE
  CC CC.C CC2.TBX          the compiler compiles its own source
  CC2 CC.C CC3.TBX         the result does it again
  FC CC2.TBX CC3.TBX       -> identical, byte for byte

USEFUL KEYS
  Mouse wheel   scroll back through earlier output
  PgUp          same, from the keyboard
  DEL at boot   enter BIOS setup
