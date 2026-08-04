/* ==========================================================================
   TBFS -- das Dateisystem von TOOBAD-OS

   Aufbau der Festplatte:
       Sektor    0        Bootsektor
       Sektor    1..511   Kernel (bis 256 KB)
       Sektor  512        Superblock (Kennung, Zaehler)
       Sektor  513..520   Verzeichnis (128 Eintraege a 32 Byte)
       Sektor  576..      Daten der Dateien

   Ein Verzeichniseintrag:
       Byte  0..15   Name (mit 0 abgeschlossen)
       Byte 16..19   erster Sektor der Datei
       Byte 20..23   Groesse in Bytes
       Byte 24..27   unten der Typ (1 = Datei, 2 = Ordner),
                     oben der Elternordner + 1 (0 = Hauptverzeichnis)
       Byte 28..31   Uhrzeit der letzten Aenderung

   Ordner brauchen so keinen eigenen Speicherplatz auf der Platte: sie sind
   nur Eintraege, auf die sich andere Eintraege als Eltern beziehen.
   ========================================================================== */

#define FS_MAGIC     0x54424653      /* "TBFS" als Zahl */
#define FS_SUPER     512
#define FS_DIRSEC0   513
#define FS_DIRSECS   8
#define FS_DATA      576
#define FS_ENTSIZE   32
#define FS_MAXFILES  128

/* Diese Puffer lagen frueher direkt hinter dem Kernel (ab 0x30000). Als der
   Kernel ueber 128 KB wuchs, ist er hineingewachsen und hat sich das
   Verzeichnis selbst ueberschrieben -- Fenstertitel standen ploetzlich als
   Dateinamen da. Jetzt liegen sie oberhalb der Prozess-Stacks, mit reichlich
   Luft fuer den Kernel darunter. build.py prueft den Abstand bei jedem Bauen. */
#define SECBUF       0x000B0000      /* ein Sektor Arbeitsspeicher */
#define DIRBUF       0x000B1000      /* das Verzeichnis im RAM */
#define FILEBUF      0x000C0000      /* Arbeitspuffer fuer Dateiinhalte */
#define FILEBUF_MAX  65536

#define FT_FILE  1
#define FT_DIR    2

int fs_ready = 0;
/* Zaehler, der sich bei jeder Aenderung am Verzeichnis erhoeht. Die
   Oberflaeche merkt daran, dass ihre gemerkte Dateiliste veraltet ist --
   sonst muesste sie bei jedem Neuzeichnen alle 128 Eintraege durchsuchen. */
int fs_gen = 0;
int cwd = 0 - 1;                     /* aktueller Ordner, -1 = Hauptverzeichnis */

int mem_get(int addr)            { int* p; p = (int*)addr; return *p; }
void mem_put(int addr, int v)    { int* p; p = (int*)addr; *p = v; }

int  ent_addr(int i)             { return DIRBUF + i * FS_ENTSIZE; }
char* ent_name(int i)            { return (char*)ent_addr(i); }
int  ent_start(int i)            { return mem_get(ent_addr(i) + 16); }
int  ent_size(int i)             { return mem_get(ent_addr(i) + 20); }
int  ent_used(int i)             { return mem_get(ent_addr(i) + 24) & 255; }
int  ent_type(int i)             { return mem_get(ent_addr(i) + 24) & 255; }
int  ent_parent(int i)           { return ((mem_get(ent_addr(i) + 24) >> 16) & 65535) - 1; }
int  ent_time(int i)             { return mem_get(ent_addr(i) + 28); }

void ent_setstart(int i, int v)  { mem_put(ent_addr(i) + 16, v); }
void ent_setsize(int i, int v)   { mem_put(ent_addr(i) + 20, v); }
void ent_setused(int i, int v)   { mem_put(ent_addr(i) + 24, v); }
/* Versteckt: Bit 8 im Info-Wort. Die Art steht im untersten Byte, der
   Elternordner ab Bit 16 -- dazwischen war Platz. So sieht man USER.DAT
   nicht mehr in jeder Liste, ohne dass sich am Aufbau etwas aendert. */
int  ent_versteckt(int i)  { return (mem_get(ent_addr(i) + 24) >> 8) & 1; }
void ent_verstecken(int i) { mem_put(ent_addr(i) + 24,
                                     mem_get(ent_addr(i) + 24) | 256); }

void ent_setinfo(int i, int typ, int parent) {
    mem_put(ent_addr(i) + 24, (typ & 255) | (((parent + 1) & 65535) << 16));
}
void ent_settime(int i, int v)   { mem_put(ent_addr(i) + 28, v); }

int sectors_for(int bytes) { return (bytes + 511) / 512; }

void fs_load_dir() {
    fs_gen++;
    sys_diskread(FS_DIRSEC0, FS_DIRSECS, DIRBUF);
}
void fs_save_dir() {
    fs_gen++;
    sys_diskwrite(FS_DIRSEC0, FS_DIRSECS, DIRBUF);
}

/* Legt ein leeres Dateisystem an. */
void fs_format() {
    int i;
    memset((char*)SECBUF, 0, 512);
    mem_put(SECBUF, FS_MAGIC);
    mem_put(SECBUF + 4, sys_disksize());
    mem_put(SECBUF + 8, FS_DIRSEC0);
    mem_put(SECBUF + 12, FS_DATA);
    sys_diskwrite(FS_SUPER, 1, SECBUF);

    memset((char*)DIRBUF, 0, FS_DIRSECS * 512);
    fs_save_dir();
    fs_ready = 1;
}

/* Haengt das Dateisystem ein; formatiert automatisch, wenn die Platte neu ist.
   Rueckgabe: 1 = war schon formatiert, 0 = wurde jetzt formatiert */
int fs_mount() {
    sys_diskread(FS_SUPER, 1, SECBUF);
    if (mem_get(SECBUF) != FS_MAGIC) {
        fs_format();
        return 0;
    }
    fs_load_dir();
    fs_ready = 1;
    return 1;
}

/* Sucht einen Eintrag in einem bestimmten Ordner. */
int fs_find_in(char* name, int ordner) {
    int i;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) continue;
        if (ent_parent(i) != ordner) continue;
        if (stricmp(ent_name(i), name) == 0) return i;
    }
    return 0 - 1;
}

int fs_find(char* name) { return fs_find_in(name, cwd); }

/* Programme werden der Reihe nach gesucht: im aktuellen Ordner, dann in
   \SYSTEM, dann in \SYSTEM\PROGS, dann in \PROGS. Das ist der Suchpfad
   des Systems -- dasselbe, was unter DOS in der Umgebungsvariablen PATH
   steht. Die Reihenfolge ist wichtig: was zum System gehoert, gewinnt gegen
   eine gleichnamige Datei in \PROGS. Sonst kann man das System mit einer
   untergeschobenen Datei aushebeln. */
int fs_find_prog(char* name) {
    int i; int d; int sys;
    i = fs_find_in(name, cwd);
    if (i >= 0) return i;
    sys = fs_find_in("SYSTEM", 0 - 1);
    d = sys;
    if (d >= 0 && ent_type(d) == FT_DIR) {
        i = fs_find_in(name, d);
        if (i >= 0) return i;
    }
    d = sys >= 0 ? fs_find_in("PROGS", sys) : 0 - 1;
    if (d >= 0 && ent_type(d) == FT_DIR) {
        i = fs_find_in(name, d);
        if (i >= 0) return i;
    }
    d = fs_find_in("PROGS", 0 - 1);
    if (d >= 0 && ent_type(d) == FT_DIR) {
        i = fs_find_in(name, d);
        if (i >= 0) return i;
    }
    return 0 - 1;
}

/* --- Ordner ------------------------------------------------------------- */

int fs_mkdir(char* name) {
    int idx;
    if (fs_find(name) >= 0) return 0 - 2;
    idx = fs_free_slot();
    if (idx < 0) return 0 - 1;
    memset(ent_name(idx), 0, FS_ENTSIZE);
    strncpy(ent_name(idx), name, 16);
    ent_setstart(idx, 0);
    ent_setsize(idx, 0);
    ent_setinfo(idx, FT_DIR, cwd);
    ent_settime(idx, sys_clock());
    fs_save_dir();
    return 0;
}

/* Wechselt den Ordner. "\" = Hauptverzeichnis, ".." = eine Ebene hoch. */
int fs_chdir(char* name) {
    int i;
    if (name[0] == 0) return 0;
    if (name[0] == 92 || (name[0] == '/' && name[1] == 0)) { cwd = 0 - 1; return 0; }
    if (name[0] == '.' && name[1] == '.') {
        if (cwd >= 0) cwd = ent_parent(cwd);
        return 0;
    }
    if (name[0] == '.' && name[1] == 0) return 0;
    i = fs_find(name);
    if (i < 0) return 0 - 1;
    if (ent_type(i) != FT_DIR) return 0 - 2;
    cwd = i;
    return 0;
}

/* Ordner loeschen -- nur wenn er leer ist. */
int fs_rmdir(char* name) {
    int i; int j;
    i = fs_find(name);
    if (i < 0) return 0 - 1;
    if (ent_type(i) != FT_DIR) return 0 - 2;
    for (j = 0; j < FS_MAXFILES; j++)
        if (ent_type(j) != 0 && ent_parent(j) == i) return 0 - 3;
    ent_setinfo(i, 0, 0 - 1);
    memset(ent_name(i), 0, 16);
    fs_save_dir();
    return 0;
}

/* Baut den Pfad des aktuellen Ordners, z.B.  A:\SYSTEM  */
void fs_path(char* out) {
    char teile[8];
    int kette[8];
    int n; int i; int k;
    n = 0;
    k = cwd;
    while (k >= 0 && n < 8) { kette[n] = k; n++; k = ent_parent(k); }
    strcpy(out, "A:");
    for (i = n - 1; i >= 0; i--) {
        strcat(out, "\\");
        strcat(out, ent_name(kette[i]));
    }
    if (n == 0) strcat(out, "\\");
}

int fs_free_slot() {
    int i;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) return i;
    }
    return 0 - 1;
}

/* Sucht die erste freie Luecke, in die <n> Sektoren passen. */
int fs_alloc(int n) {
    int start; int i; int kollision; int ende;
    start = FS_DATA;
    kollision = 1;
    while (kollision) {
        kollision = 0;
        for (i = 0; i < FS_MAXFILES; i++) {
            if (ent_type(i) != FT_FILE) continue;
            ende = ent_start(i) + sectors_for(ent_size(i));
            if (start < ende && ent_start(i) < start + n) {
                start = ende;
                kollision = 1;
            }
        }
    }
    if (start + n > sys_disksize()) return 0 - 1;
    return start;
}

/* Schreibt <bytes> Bytes ab Adresse <addr> in die Datei <name>. */
int fs_write(char* name, int addr, int bytes) {
    int idx; int n; int start; int i;
    n = sectors_for(bytes);
    if (n == 0) n = 1;
    idx = fs_find(name);
    if (idx >= 0) {
        if (sectors_for(ent_size(idx)) >= n) {
            start = ent_start(idx);              /* passt in den alten Platz */
        } else {
            i = ent_parent(idx);
            ent_setinfo(idx, 0, 0 - 1);          /* alten Platz freigeben */
            start = fs_alloc(n);
            if (start < 0) return 0 - 1;
            ent_setstart(idx, start);
            ent_setinfo(idx, FT_FILE, i);
        }
    } else {
        idx = fs_free_slot();
        if (idx < 0) return 0 - 2;
        start = fs_alloc(n);
        if (start < 0) return 0 - 1;
        memset(ent_name(idx), 0, FS_ENTSIZE);
        strncpy(ent_name(idx), name, 16);
        ent_setstart(idx, start);
        ent_setinfo(idx, FT_FILE, cwd);
    }
    ent_setsize(idx, bytes);
    ent_settime(idx, sys_clock());
    sys_diskwrite(start, n, addr);
    fs_save_dir();
    return 0;
}

/* Liest einen bekannten Eintrag. */
int fs_read_idx(int idx, int addr, int maxbytes) {
    int bytes;
    if (idx < 0 || ent_type(idx) != FT_FILE) return 0 - 1;
    bytes = ent_size(idx);
    if (bytes > maxbytes) bytes = maxbytes;
    sys_diskread(ent_start(idx), sectors_for(bytes), addr);
    return bytes;
}

/* Wie fs_read, sucht aber auch im Systemordner. */
int fs_read_prog(char* name, int addr, int maxbytes) {
    return fs_read_idx(fs_find_prog(name), addr, maxbytes);
}

/* Liest eine Datei nach <addr>. Rueckgabe: Anzahl Bytes oder -1 */
int fs_read(char* name, int addr, int maxbytes) {
    int idx; int bytes;
    idx = fs_find(name);
    if (idx < 0) return 0 - 1;
    bytes = ent_size(idx);
    if (bytes > maxbytes) bytes = maxbytes;
    sys_diskread(ent_start(idx), sectors_for(bytes), addr);
    return bytes;
}

/* Eine Datei oder einen Ordner in einen anderen Ordner haengen.

   Im TBFS steht der Elternordner im selben Wort wie der Typ -- verschieben
   heisst also nur, dieses eine Feld zu aendern. Es werden keine Sektoren
   angefasst, die Datei bleibt genau da liegen, wo sie liegt. Genau so
   arbeiten auch grosse Dateisysteme: der Ordner ist eine Zuordnung, nicht
   der Ort auf der Platte.

   Rueckgabe: 0 ok, -1 Ziel belegt, -2 Ordner in sich selbst, -3 ungueltig */
int fs_move(int idx, int ziel) {
    int p;
    if (idx < 0 || ent_type(idx) == 0) return 0 - 3;
    if (ent_parent(idx) == ziel) return 0;
    if (fs_find_in(ent_name(idx), ziel) >= 0) return 0 - 1;
    /* Ein Ordner darf nicht in sich selbst oder in eines seiner eigenen
       Kinder wandern -- sonst haengt der Ast in der Luft. */
    if (ent_type(idx) == FT_DIR) {
        p = ziel;
        while (p >= 0) {
            if (p == idx) return 0 - 2;
            p = ent_parent(p);
        }
    }
    ent_setinfo(idx, ent_type(idx), ziel);
    ent_settime(idx, sys_clock());
    fs_save_dir();
    return 0;
}

/* Eine Bibliotheksdatei lesen: erst im aktuellen Ordner, dann in \SOURCE.
   Damit findet der Compiler proglib.c und gfxlib.c auch dann, wenn der
   Quelltext woanders liegt -- vorher musste man dafuer im selben Ordner
   sitzen, und wer das nicht wusste, bekam nur "undefined function". */
int fs_read_lib(char* name, int addr, int maxbytes) {
    int i; int ord;
    i = fs_find(name);
    if (i < 0) {
        ord = fs_find_in("SOURCE", 0 - 1);
        if (ord >= 0) i = fs_find_in(name, ord);
    }
    if (i < 0) return 0 - 1;
    return fs_read_idx(i, addr, maxbytes);
}

/* Der Papierkorb.

   Loeschen heisst ab jetzt: in den Ordner \RECYCLED verschieben. Erst
   wer DORT loescht, loescht wirklich. Das ist keine Bequemlichkeit --
   Colin hatte an einem Abend versehentlich die ganze Platte geleert, und
   eigene Quelltexte holt kein build.py zurueck.

   Der Ordner heisst RECYCLED und nicht PAPIERKORB: alles, was auf dem
   Bildschirm des TB-32 landet, ist englisch -- Dateinamen eingeschlossen.
   (Windows 95 nannte seinen genauso.)

   Der Ordner wird beim ersten Bedarf angelegt. Liegt eine Datei gleichen
   Namens schon drin, wird sie ueberschrieben -- wie bei jedem Papierkorb. */
int papierkorb_ordner() {
    int i; int idx;
    i = fs_find_in("RECYCLED", 0 - 1);
    if (i >= 0) return i;
    idx = fs_free_slot();
    if (idx < 0) return 0 - 1;
    memset(ent_name(idx), 0, FS_ENTSIZE);
    strncpy(ent_name(idx), "RECYCLED", 16);
    ent_setstart(idx, 0);
    ent_setsize(idx, 0);
    ent_setinfo(idx, FT_DIR, 0 - 1);      /* liegt immer im Hauptverzeichnis */
    ent_settime(idx, sys_clock());
    fs_save_dir();
    return idx;
}

/* --- Was zum System gehoert, ist geschuetzt -------------------------------
   Wie System32 bei Windows oder /usr bei Unix: die Dateien liegen offen da,
   aber sie loeschen kostet das Passwort. Der Sinn ist nicht Misstrauen --
   ein einziges verlorenes KERNEL.BIN, und der Rechner startet nie wieder.

   sudo_bis merkt sich, bis wann eine Erlaubnis gilt. Genau wie das echte
   sudo: einmal das Passwort, dann eine Weile Ruhe. */
int sudo_bis = 0;
int konto_offen();                   /* steht in kernel.c */

int in_system(int idx) {
    int sys; int k; int tiefe;
    sys = fs_find_in("SYSTEM", 0 - 1);
    if (sys < 0) return 0;
    k = idx;
    tiefe = 0;
    while (k >= 0 && tiefe < 8) {
        if (k == sys) return 1;
        k = ent_parent(k);
        tiefe = tiefe + 1;
    }
    return 0;
}

/* 1 = darf, 0 = braucht erst das Passwort. Ein Rechner ohne Passwort ist
   offen -- dann fragt auch hier niemand. */
int darf_system(int idx) {
    if (in_system(idx) == 0) return 1;
    if (konto_offen()) return 1;
    if (sudo_bis > sys_ticks()) return 1;
    return 0;
}

int fs_endgueltig_loeschen(char* name) {
    int idx;
    idx = fs_find(name);
    if (idx < 0) return 0 - 1;
    if (ent_type(idx) == FT_DIR) return 0 - 2;
    if (darf_system(idx) == 0) return 0 - 3;
    ent_setinfo(idx, 0, 0 - 1);
    memset(ent_name(idx), 0, 16);
    fs_save_dir();
    return 0;
}

int fs_delete(char* name) {
    int idx; int korb; int alt;
    idx = fs_find(name);
    if (idx < 0) return 0 - 1;
    if (ent_type(idx) == FT_DIR) return 0 - 2;
    if (darf_system(idx) == 0) return 0 - 3;   /* geschuetzt */

    korb = papierkorb_ordner();
    /* Wer im Papierkorb loescht, meint es ernst. */
    if (korb < 0 || cwd == korb) return fs_endgueltig_loeschen(name);

    alt = fs_find_in(name, korb);
    if (alt >= 0) {                       /* gleicher Name liegt schon drin */
        ent_setinfo(alt, 0, 0 - 1);
        memset(ent_name(alt), 0, 16);
    }
    ent_setinfo(idx, ent_type(idx), korb);
    fs_save_dir();
    return 0;
}

int fs_rename(char* alt, char* neu) {
    int idx;
    idx = fs_find(alt);
    if (idx < 0) return 0 - 1;
    if (fs_find(neu) >= 0) return 0 - 2;
    memset(ent_name(idx), 0, 16);
    strncpy(ent_name(idx), neu, 16);
    fs_save_dir();
    return 0;
}

int fs_count() {
    int i; int n;
    n = 0;
    for (i = 0; i < FS_MAXFILES; i++) if (ent_type(i) == FT_FILE) n++;
    return n;
}

int fs_used_sectors() {
    int i; int n;
    n = 0;
    for (i = 0; i < FS_MAXFILES; i++)
        if (ent_type(i) == FT_FILE) n += sectors_for(ent_size(i));
    return n;
}
