/* ==========================================================================
   ASM  --  Assembler fuer den TB-32, der AUF dem TB-32 laeuft

   Damit ist der Rechner selbsttragend: man schreibt im EDIT ein Programm,
   ruft ASM auf und bekommt eine fertige .TBX-Datei, die man starten kann.
   Der Mac wird dafuer nicht mehr gebraucht.

   Aufruf im Betriebssystem:
       ASM QUELLE.ASM ZIEL.TBX

   Zwei Durchgaenge wie jeder echte Assembler:
       Durchgang 1  Groesse jeder Zeile bestimmen, Sprungmarken merken
       Durchgang 2  jetzt sind alle Marken bekannt -> Maschinencode erzeugen
   ========================================================================== */

#include "proglib.c"

#define SRC_BUF    0x00280000        /* Quelltext, bis 64 KB */
#define OUT_BUF    0x00292000        /* erzeugter Maschinencode */
#define ROH_BUF    0x002C0000        /* Quelltext VOR dem Einbinden */
#define INC_BUF    0x002D0000        /* eine eingebundene Datei */
#define SRC_MAX    65536
#define OUT_MAX    65536
#define INC_MAX    32768

#define LOAD_ADDR  0x00200000        /* hierhin laedt das OS Programme */

/* 512 statt 256: const.inc allein bringt 158 Konstanten mit, dazu die
   Marken eines BIOS. Mit 256 war beim ersten ernsthaften Versuch Schluss --
   und zwar lautlos, add_sym hat den Rest einfach verworfen. */
#define MAXSYM     512
#define NAMELEN    24

/* Befehlsformate */
#define F_NONE  0
#define F_R     1
#define F_RR    2
#define F_RRR   3
#define F_RI    4
#define F_RRI   5
#define F_MEM   6
#define F_J     7
#define F_C     8
#define F_I     9
#define F_IR    10

char sym_name[MAXSYM * NAMELEN];
int  sym_addr[MAXSYM];
int  sym_count = 0;

char mn_name[128 * 8];
int  mn_op[128];
int  mn_fmt[128];
int  mn_cond[128];
int  mn_count = 0;

int pass;
int pc;
int out_len;
int errors;
int src_len;
int line_no;

char token[64];
char line[256];

/* --- Tabellenverwaltung -------------------------------------------------- */

char* mn_at(int i) { return (char*)((int)mn_name + i * 8); }
char* sym_at(int i) { return (char*)((int)sym_name + i * NAMELEN); }

void add_mn(char* name, int op, int fmt, int cond) {
    strncpy(mn_at(mn_count), name, 8);
    mn_op[mn_count] = op;
    mn_fmt[mn_count] = fmt;
    mn_cond[mn_count] = cond;
    mn_count++;
}

void init_table() {
    add_mn("nop",  0x00, F_NONE, 0);
    add_mn("hlt",  0x01, F_NONE, 0);
    add_mn("cli",  0x02, F_NONE, 0);
    add_mn("sti",  0x03, F_NONE, 0);
    add_mn("iret", 0x04, F_NONE, 0);
    add_mn("ret",  0x05, F_NONE, 0);
    add_mn("brk",  0x06, F_NONE, 0);

    add_mn("mov",  0x10, F_RR, 0);
    add_mn("movi", 0x11, F_RI, 0);
    add_mn("movh", 0x13, F_RI, 0);

    add_mn("ldb",  0x18, F_MEM, 0);
    add_mn("ldsb", 0x19, F_MEM, 0);
    add_mn("ldh",  0x1A, F_MEM, 0);
    add_mn("ldw",  0x1B, F_MEM, 0);
    add_mn("stb",  0x1C, F_MEM, 0);
    add_mn("sth",  0x1D, F_MEM, 0);
    add_mn("stw",  0x1E, F_MEM, 0);

    add_mn("add",  0x20, F_RRR, 0);
    add_mn("sub",  0x21, F_RRR, 0);
    add_mn("mul",  0x22, F_RRR, 0);
    add_mn("div",  0x23, F_RRR, 0);
    add_mn("mod",  0x24, F_RRR, 0);
    add_mn("and",  0x25, F_RRR, 0);
    add_mn("or",   0x26, F_RRR, 0);
    add_mn("xor",  0x27, F_RRR, 0);
    add_mn("shl",  0x28, F_RRR, 0);
    add_mn("shr",  0x29, F_RRR, 0);
    add_mn("sar",  0x2A, F_RRR, 0);
    add_mn("not",  0x2B, F_RR, 0);
    add_mn("neg",  0x2C, F_RR, 0);
    add_mn("cmp",  0x2D, F_RR, 0);
    add_mn("tst",  0x2E, F_RR, 0);
    add_mn("udiv", 0x2F, F_RRR, 0);
    add_mn("umod", 0x3F, F_RRR, 0);

    add_mn("addi", 0x30, F_RRI, 0);
    add_mn("subi", 0x31, F_RRI, 0);
    add_mn("muli", 0x32, F_RRI, 0);
    add_mn("divi", 0x33, F_RRI, 0);
    add_mn("modi", 0x34, F_RRI, 0);
    add_mn("andi", 0x35, F_RRI, 0);
    add_mn("ori",  0x36, F_RRI, 0);
    add_mn("xori", 0x37, F_RRI, 0);
    add_mn("shli", 0x38, F_RRI, 0);
    add_mn("shri", 0x39, F_RRI, 0);
    add_mn("sari", 0x3A, F_RRI, 0);
    add_mn("cmpi", 0x3D, F_RI, 0);
    add_mn("tsti", 0x3E, F_RI, 0);

    add_mn("push", 0x40, F_R, 0);
    add_mn("pop",  0x41, F_R, 0);
    add_mn("call", 0x42, F_C, 0);
    add_mn("callr",0x43, F_R, 0);
    add_mn("pushf",0x44, F_NONE, 0);
    add_mn("popf", 0x45, F_NONE, 0);

    add_mn("jmp",  0x50, F_J, 0);
    add_mn("jz",   0x50, F_J, 1);
    add_mn("je",   0x50, F_J, 1);
    add_mn("jnz",  0x50, F_J, 2);
    add_mn("jne",  0x50, F_J, 2);
    add_mn("jc",   0x50, F_J, 3);
    add_mn("jb",   0x50, F_J, 3);
    add_mn("jnc",  0x50, F_J, 4);
    add_mn("jae",  0x50, F_J, 4);
    add_mn("jn",   0x50, F_J, 5);
    add_mn("jnn",  0x50, F_J, 6);
    add_mn("jbe",  0x50, F_J, 9);
    add_mn("ja",   0x50, F_J, 10);
    add_mn("jl",   0x50, F_J, 11);
    add_mn("jge",  0x50, F_J, 12);
    add_mn("jle",  0x50, F_J, 13);
    add_mn("jg",   0x50, F_J, 14);
    add_mn("jmpr", 0x51, F_R, 0);

    add_mn("in",   0x60, F_RI, 0);
    add_mn("inr",  0x61, F_RR, 0);
    add_mn("out",  0x62, F_IR, 0);
    add_mn("outr", 0x63, F_RR, 0);
    add_mn("int",  0x64, F_I, 0);
}

int find_mn(char* name) {
    int i;
    for (i = 0; i < mn_count; i++)
        if (stricmp(mn_at(i), name) == 0) return i;
    return 0 - 1;
}

/* Der Vergleich des ersten Zeichens vor dem strcmp ist kein Schoenheits-
   fehler: bei 512 Symbolen und zwei Durchgaengen sind das sonst Millionen
   Zeichenvergleiche, und der Assembler sieht aus, als haenge er. */
/* Lokale Marken (.loop, .done, .copy) gehoeren zur zuletzt genannten
   globalen Marke. Ohne das ist `.copy` in video.asm ueberall dieselbe --
   der Assembler uebersetzt anstandslos, und die Spruenge landen in einer
   voellig anderen Funktion. Genau so ein Fehler faellt erst auf, wenn ein
   fertiges BIOS nicht mehr startet. */
char last_global[NAMELEN];
char voll_puffer[NAMELEN * 2];

char* voller_name(char* n) {
    int i; int j;
    if (n[0] != '.') return n;
    i = 0;
    while (last_global[i] && i < NAMELEN - 1) { voll_puffer[i] = last_global[i]; i++; }
    j = 0;
    while (n[j] && i < NAMELEN * 2 - 2) { voll_puffer[i] = n[j]; i++; j++; }
    voll_puffer[i] = 0;
    return voll_puffer;
}

int find_sym(char* name) {
    int i; char c;
    c = name[0];
    for (i = 0; i < sym_count; i++)
        if (sym_name[i * NAMELEN] == c && strcmp(sym_at(i), name) == 0) return i;
    return 0 - 1;
}

void add_sym(char* name, int addr) {
    int i;
    i = find_sym(name);
    if (i >= 0) { sym_addr[i] = addr; return; }
    if (sym_count >= MAXSYM) { fehler("too many symbols"); return; }
    strncpy(sym_at(sym_count), name, NAMELEN);
    sym_addr[sym_count] = addr;
    sym_count++;
}

/* --- Fehlermeldungen ----------------------------------------------------- */

void fehler(char* text) {
    if (pass != 2) return;
    printc("  Line ", RED);
    printnc(line_no, RED);
    printc(": ", RED);
    printc(text, RED);
    nl();
    errors++;
}

/* --- Ausgabe ------------------------------------------------------------- */

void emit_byte(int b) {
    if (pass == 2 && out_len < OUT_MAX) byte_put(OUT_BUF + out_len, b);
    out_len++;
    pc++;
}

void emit32(int w) {
    emit_byte(w & 255);
    emit_byte((w >> 8) & 255);
    emit_byte((w >> 16) & 255);
    emit_byte((w >> 24) & 255);
}

/* --- Zerlegen der Zeile -------------------------------------------------- */

int lpos;

void skip_space() {
    while (line[lpos] == ' ' || line[lpos] == 9) lpos++;
}

/* Holt das naechste Wort nach token[]. Gibt 0 zurueck, wenn die Zeile zu Ende ist. */
int next_token() {
    int n;
    skip_space();
    n = 0;
    if (line[lpos] == 0 || line[lpos] == ';') { token[0] = 0; return 0; }
    if (line[lpos] == ',') { lpos++; skip_space(); }
    if (line[lpos] == 0 || line[lpos] == ';') { token[0] = 0; return 0; }

    if (line[lpos] == 34) {                  /* Zeichenkette am Stueck lesen */
        token[0] = 34;
        n = 1;
        lpos++;
        while (line[lpos] && line[lpos] != 34) {
            if (n < 60) {
                if (line[lpos] == 92 && line[lpos + 1]) {    /* \n \t \\ */
                    lpos++;
                    if (line[lpos] == 'n') token[n] = 10;
                    else if (line[lpos] == 't') token[n] = 9;
                    else if (line[lpos] == '0') token[n] = 0;
                    else token[n] = line[lpos];
                } else {
                    token[n] = line[lpos];
                }
                n++;
            }
            lpos++;
        }
        if (line[lpos] == 34) lpos++;
        token[n] = 34;
        n++;
        token[n] = 0;
        return n;
    }

    while (line[lpos] && line[lpos] != ' ' && line[lpos] != 9 &&
           line[lpos] != ',' && line[lpos] != ';') {
        if (n < 62) { token[n] = line[lpos]; n++; }
        lpos++;
    }
    token[n] = 0;
    return n;
}

/* Ein ganzes Argument bis zum Komma -- Leerzeichen mittendrin gehoeren
   dazu. next_token() trennt an Leerzeichen und liefert von
   `IVT_BASE + IRQ_TIMER*4` nur `IVT_BASE`; der Rest fiel lautlos unter den
   Tisch, und die Interrupttabelle stand voller Nullen. */
int next_arg() {
    int n;
    skip_space();
    if (line[lpos] == ',') { lpos++; skip_space(); }
    if (line[lpos] == 0 || line[lpos] == ';') { token[0] = 0; return 0; }
    n = 0;
    while (line[lpos] && line[lpos] != ',' && line[lpos] != ';') {
        if (n < 62) { token[n] = line[lpos]; n++; }
        lpos++;
    }
    while (n > 0 && (token[n - 1] == ' ' || token[n - 1] == 9)) n--;
    token[n] = 0;
    return n;
}

/* --- Werte auswerten ----------------------------------------------------- */

int reg_of(char* t) {
    int n;
    if (stricmp(t, "sp") == 0) return 15;
    if (stricmp(t, "fp") == 0) return 14;
    if (stricmp(t, "at") == 0) return 13;
    if (t[0] != 'r' && t[0] != 'R') return 0 - 1;
    n = atoi(t + 1);
    if (n < 0 || n > 15) return 0 - 1;
    return n;
}

/* Zahl, Zeichen ('A') oder Sprungmarke, optional mit + oder - dahinter */
int wert_name(char* t) {
    int v; int i; int neg; int teil;
    v = 0;
    i = 0;

    if (t[0] == 39) {                            /* Zeichenkonstante 'X' */
        if (t[1] == 92) {                        /* Escape \n \t \0 */
            if (t[2] == 'n') return 10;
            if (t[2] == 't') return 9;
            if (t[2] == '0') return 0;
            if (t[2] == 'r') return 13;
            return t[2];
        }
        return t[1];
    }

    if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        i = 2;
        while (t[i]) {
            v = v * 16;
            if (isdigit(t[i])) v = v + t[i] - '0';
            else v = v + (toupper(t[i]) - 'A' + 10);
            i++;
        }
        return v;
    }

    if (isdigit(t[0]) || t[0] == '-') return atoi(t);

    /* Ein einzelner Name -- Sprungmarke oder .equ-Konstante. */
    v = find_sym(voller_name(t));
    if (v < 0) {
        if (pass == 2) fehler("unknown label");
        return 0;
    }
    v = sym_addr[v];
    neg = 0;
    teil = 0;
    i = 0;
    t[i] = teil;
    return v;
}

/* --- Ausdruecke:  IVT_BASE + IRQ_TIMER*4  ------------------------------
   Punkt vor Strich. Ohne das rechnet der Assembler von links nach rechts,
   und aus IVT_BASE + IRQ_TIMER*4 wird (IVT_BASE + IRQ_TIMER)*4 -- die
   Interrupttabelle landete dann an einer voellig falschen Adresse, und
   zwar ohne jede Fehlermeldung.

   Klammern gibt es absichtlich nicht. In der ganzen Firmware kommt keine
   einzige vor, und ohne sie bleibt der Zerteiler zwanzig Zeilen lang.   */

int   ex_pos;
char* ex_txt;
char  ex_tok[64];
int   ex_op;                       /* Operator hinter dem letzten Stueck */
int   ex_ebene;                    /* Klammertiefe, fuer den Zwischenspeicher */
char  ex_puffer[4 * 80];

int ex_stueck() {
    int n; int c;
    n = 0;
    while (ex_txt[ex_pos] == ' ' || ex_txt[ex_pos] == 9) ex_pos++;
    if (ex_txt[ex_pos] == '-' || ex_txt[ex_pos] == '+') {
        ex_tok[n] = ex_txt[ex_pos]; n++; ex_pos++;
    }
    while (ex_txt[ex_pos] && ex_txt[ex_pos] != ' ' && ex_txt[ex_pos] != 9 &&
           ex_txt[ex_pos] != '+' && ex_txt[ex_pos] != '-' &&
           ex_txt[ex_pos] != '*' && ex_txt[ex_pos] != '/') {
        if (n < 62) { ex_tok[n] = ex_txt[ex_pos]; n++; }
        ex_pos++;
    }
    ex_tok[n] = 0;
    while (ex_txt[ex_pos] == ' ' || ex_txt[ex_pos] == 9) ex_pos++;
    c = ex_txt[ex_pos];
    if (c) ex_pos++;
    return c;
}

/* Ein Faktor: eine Zahl, ein Name -- oder ein geklammerter Ausdruck.
   Der Operator dahinter landet in ex_op. */
int ex_faktor() {
    int n; int tiefe; int wert; int alt_pos;
    char* alt_txt; char* puf;

    while (ex_txt[ex_pos] == ' ' || ex_txt[ex_pos] == 9) ex_pos++;
    if (ex_txt[ex_pos] == '(') {
        ex_pos++;
        tiefe = 1;
        n = 0;
        puf = ex_puffer + ex_ebene * 80;
        while (ex_txt[ex_pos]) {
            if (ex_txt[ex_pos] == '(') tiefe++;
            if (ex_txt[ex_pos] == ')') {
                tiefe--;
                if (tiefe == 0) { ex_pos++; break; }
            }
            if (n < 78) { puf[n] = ex_txt[ex_pos]; n++; }
            ex_pos++;
        }
        puf[n] = 0;
        alt_txt = ex_txt;
        alt_pos = ex_pos;
        if (ex_ebene < 3) {
            ex_ebene++;
            wert = value_of(puf);
            ex_ebene--;
        } else {
            fehler("expression too deeply nested");
            wert = 0;
        }
        ex_txt = alt_txt;
        ex_pos = alt_pos;
        ex_op = ex_txt[ex_pos];
        if (ex_op) ex_pos++;
        return wert;
    }

    ex_op = ex_stueck();
    return wert_name(ex_tok);
}

/* Punkt vor Strich. Ohne das rechnet der Assembler von links nach rechts,
   und aus IVT_BASE + IRQ_TIMER*4 wird (IVT_BASE + IRQ_TIMER)*4 -- die
   Interrupttabelle landete dann an einer voellig falschen Adresse, und
   zwar ohne jede Fehlermeldung. */
int value_of(char* t) {
    int summe; int term; int op; int b; int d;
    if (t[0] == 39) return wert_name(t);       /* 'X' -- da steht kein Ausdruck */
    ex_txt = t;
    ex_pos = 0;
    summe = 0;
    op = '+';
    term = ex_faktor();
    while (1) {
        b = ex_op;
        if (b == '*') { term = term * ex_faktor(); continue; }
        if (b == '/') {
            d = ex_faktor();
            if (d == 0) { fehler("division by zero"); d = 1; }
            term = term / d;
            continue;
        }
        if (op == '+') summe = summe + term;
        else           summe = summe - term;
        if (b == 0) return summe;
        op = b;
        term = ex_faktor();
    }
}



/* --- Speicheroperand:  [r2+8]  oder  [r2]  -------------------------------
   Der Offset landet in mem_off, das Basisregister ist der Rueckgabewert. */

int mem_off;

int mem_operand(char* t) {
    int i; int j; int basis;
    char inner[40];

    mem_off = 0;
    i = 0;
    while (t[i] && t[i] != '[') i++;
    if (t[i] != '[') { fehler("expected [ ]"); return 0 - 1; }
    i++;
    j = 0;
    while (t[i] && t[i] != ']' && t[i] != '+' && t[i] != '-') {
        if (j < 38) { inner[j] = t[i]; j++; }
        i++;
    }
    inner[j] = 0;
    basis = reg_of(inner);
    if (basis < 0) { fehler("expected register in [ ]"); return 0 - 1; }

    if (t[i] == '+' || t[i] == '-') {
        j = 0;
        if (t[i] == '-') { inner[j] = '-'; j++; }
        i++;
        while (t[i] && t[i] != ']') {
            if (j < 38) { inner[j] = t[i]; j++; }
            i++;
        }
        inner[j] = 0;
        mem_off = value_of(inner);
    }
    return basis;
}

/* --- Kodierung ----------------------------------------------------------
   Muss Byte fuer Byte zu hardware/isa.py passen. Wer hier etwas aendert,
   aendert es dort mit -- sonst laeuft der erzeugte Code nur noch auf einem
   der beiden Emulatoren. */

int enc_r(int op, int rd, int ra, int rb) {
    return (op << 24) | ((rd & 15) << 20) | ((ra & 15) << 16) | ((rb & 15) << 12);
}

int enc_i(int op, int rd, int ra, int imm) {
    return (op << 24) | ((rd & 15) << 20) | ((ra & 15) << 16) | (imm & 65535);
}

int enc_j(int op, int cond, int off) {
    return (op << 24) | ((cond & 15) << 20) | (off & 1048575);
}

int enc_c(int op, int off) {
    return (op << 24) | (off & 16777215);
}

/* --- Eine Zeile uebersetzen ---------------------------------------------- */

void do_line() {
    int idx; int op; int fmt; int rd; int ra; int rb; int imm; int ziel; int off;
    char first[64];
    int i;

    lpos = 0;
    if (next_token() == 0) return;
    strcpy(first, token);

    /* Sprungmarke am Zeilenanfang? */
    i = strlen(first);
    if (first[i - 1] == ':') {
        first[i - 1] = 0;
        if (first[0] != '.') strncpy(last_global, first, NAMELEN);
        if (pass == 1) add_sym(voller_name(first), pc);
        if (next_token() == 0) return;
        strcpy(first, token);
    }

    /* Direktiven */
    if (first[0] == '.') {
        if (stricmp(first, ".db") == 0) {
            while (next_token()) {
                if (token[0] == 34) {                 /* Zeichenkette */
                    i = 1;
                    while (token[i] && token[i] != 34) { emit_byte(token[i]); i++; }
                } else {
                    emit_byte(value_of(token) & 255);
                }
            }
            return;
        }
        if (stricmp(first, ".dw") == 0) {
            while (next_arg()) emit32(value_of(token));
            return;
        }
        if (stricmp(first, ".space") == 0) {
            next_arg();
            i = value_of(token);
            while (i > 0) { emit_byte(0); i--; }
            return;
        }
        /* .org <adresse> -- ab hier zaehlt der Programmzaehler anders.
           Ein BIOS liegt bei 0x0F000000; ohne .org zeigte jede Sprung-
           adresse darin in den Programmbereich bei 0x00200000. */
        if (stricmp(first, ".org") == 0) {
            next_arg();
            pc = value_of(token);
            return;
        }
        /* .equ NAME, wert -- benannte Konstante. Sie muss schon in
           Durchgang 1 stehen: sonst kennt Durchgang 1 die Groesse einer
           Zeile nicht, die sie benutzt. */
        if (stricmp(first, ".equ") == 0) {
            next_token();
            strcpy(first, token);
            next_arg();
            if (pass == 1) add_sym(first, value_of(token));
            return;
        }
        /* .include ist schon vor den Durchgaengen erledigt worden --
           siehe includes_aufloesen(). Hier nur noch ueberlesen. */
        if (stricmp(first, ".include") == 0) return;
        if (stricmp(first, ".align") == 0) {
            next_arg();
            i = value_of(token);
            if (i < 1) i = 4;
            while (pc % i) emit_byte(0);
            return;
        }
        fehler("unknown directive");
        return;
    }

    /* Pseudo-Befehl: 32-Bit-Wert laden */
    if (stricmp(first, "li") == 0) {
        next_token();
        rd = reg_of(token);
        next_arg();
        imm = value_of(token);
        emit32(enc_i(0x11, rd, 0, imm & 65535));
        emit32(enc_i(0x13, rd, 0, (imm >> 16) & 65535));
        return;
    }

    /* Pseudo-Befehle fuer absolute Adressen:
           ldwa r5, BDA_CURX   ->   li at, BDA_CURX ; ldw r5, [at]
           stwa BDA_CURX, r5   ->   li at, BDA_CURX ; stw r5, [at]
       Sie benutzen r13 ("at") als Hilfsregister, genau wie der Assembler
       auf dem Mac. Ohne sie laesst sich keine Firmware uebersetzen -- die
       greift staendig auf feste Adressen im BIOS-Datenbereich zu. */
    if (stricmp(first, "ldwa") == 0 || stricmp(first, "ldha") == 0 ||
        stricmp(first, "ldba") == 0 || stricmp(first, "stwa") == 0 ||
        stricmp(first, "stha") == 0 || stricmp(first, "stba") == 0) {
        if (first[0] == 's' || first[0] == 'S') {
            next_arg();                          /* stwa ADRESSE, rs */
            imm = value_of(token);
            next_token();
            rd = reg_of(token);
        } else {
            next_token();                        /* ldwa rd, ADRESSE */
            rd = reg_of(token);
            next_arg();
            imm = value_of(token);
        }
        if (rd < 0) { fehler("expected register"); return; }
        emit32(enc_i(0x11, 13, 0, imm & 65535));
        emit32(enc_i(0x13, 13, 0, (imm >> 16) & 65535));
        /* Den Opcode aus der eigenen Tabelle holen statt ihn hinzuschreiben.
           Ein hier abgetippter Zahlenwert waere genau die Sorte Fehler, die
           erst auffaellt, wenn ein fertiges BIOS nicht mehr startet. */
        first[strlen(first) - 1] = 0;            /* ldwa -> ldw */
        idx = find_mn(first);
        if (idx < 0) { fehler("unknown instruction"); return; }
        emit32(enc_i(mn_op[idx], rd, 13, 0));
        return;
    }

    idx = find_mn(first);
    if (idx < 0) { fehler("unknown instruction"); return; }
    op = mn_op[idx];
    fmt = mn_fmt[idx];

    if (fmt == F_NONE) { emit32(enc_r(op, 0, 0, 0)); return; }

    if (fmt == F_R) {
        next_token();
        rd = reg_of(token);
        if (rd < 0) { fehler("expected register"); return; }
        emit32(enc_r(op, rd, 0, 0));
        return;
    }

    if (fmt == F_RR) {
        next_token();
        rd = reg_of(token);
        next_token();
        ra = reg_of(token);
        if (rd < 0 || ra < 0) { fehler("expected register"); return; }
        emit32(enc_r(op, rd, ra, 0));
        return;
    }

    if (fmt == F_RRR) {
        next_token();
        rd = reg_of(token);
        next_token();
        ra = reg_of(token);
        next_token();
        rb = reg_of(token);
        if (rd < 0 || ra < 0 || rb < 0) { fehler("expected register"); return; }
        emit32(enc_r(op, rd, ra, rb));
        return;
    }

    if (fmt == F_RI) {
        next_token();
        rd = reg_of(token);
        next_arg();
        imm = value_of(token);
        if (rd < 0) { fehler("expected register"); return; }
        emit32(enc_i(op, rd, 0, imm & 65535));
        return;
    }

    if (fmt == F_RRI) {
        next_token();
        rd = reg_of(token);
        next_token();
        ra = reg_of(token);
        next_arg();
        imm = value_of(token);
        if (rd < 0 || ra < 0) { fehler("expected register"); return; }
        emit32(enc_i(op, rd, ra, imm & 65535));
        return;
    }

    if (fmt == F_MEM) {
        if (first[0] == 's' || first[0] == 'S') {   /* stw [r2+8], r1 */
            next_token();
            ra = mem_operand(token);
            off = mem_off;
            next_token();
            rd = reg_of(token);
        } else {                                    /* ldw r1, [r2+8] */
            next_token();
            rd = reg_of(token);
            next_token();
            ra = mem_operand(token);
            off = mem_off;
        }
        if (rd < 0 || ra < 0) { fehler("expected register"); return; }
        emit32(enc_i(op, rd, ra, off & 65535));
        return;
    }

    if (fmt == F_I) {
        next_arg();
        imm = value_of(token);
        emit32(enc_i(op, 0, 0, imm & 65535));
        return;
    }

    if (fmt == F_IR) {                              /* out 0x80, r1 */
        next_arg();
        imm = value_of(token);
        next_token();
        rd = reg_of(token);
        if (rd < 0) { fehler("expected register"); return; }
        emit32(enc_i(op, rd, 0, imm & 65535));
        return;
    }

    if (fmt == F_J) {
        next_arg();
        ziel = pc;
        if (pass == 2) ziel = value_of(token);
        off = (ziel - pc) / 4;
        emit32(enc_j(op, mn_cond[idx], off));
        return;
    }

    if (fmt == F_C) {
        next_arg();
        ziel = pc;
        if (pass == 2) ziel = value_of(token);
        off = (ziel - pc) / 4;
        emit32(enc_c(op, off));
        return;
    }

    fehler("format not supported");
}

/* --- .include aufloesen, bevor die Durchgaenge laufen -------------------
   Der Quelltext kommt roh in den einen Puffer, das Ergebnis in den anderen.
   Jede Zeile, die mit .include beginnt, wird durch den Inhalt der genannten
   Datei ersetzt. Eine Ebene tief -- mehr braucht ein BIOS nicht, und
   Rekursion in 64 KB Puffern endet sonst schnell im Nichts.

   Gesucht wird mit fileread_lib: erst im aktuellen Ordner, dann in SOURCE.
   Genau so findet auch der Compiler seine Bibliotheken.                  */

int includes_aufloesen(int roh_len) {
    int i; int j; int k; int n; int m; int start;
    char* roh; char* ziel; char* inc;
    char name[24];

    roh = (char*)ROH_BUF;
    ziel = (char*)SRC_BUF;
    inc = (char*)INC_BUF;
    i = 0;
    j = 0;

    while (i < roh_len) {
        start = i;
        while (i < roh_len && roh[i] != 10) i++;
        if (i < roh_len) i++;                 /* das Zeilenende gehoert dazu */

        k = start;
        while (k < i && (roh[k] == ' ' || roh[k] == 9)) k++;
        if (roh[k] == '.' && roh[k+1] == 'i' && roh[k+2] == 'n' &&
            roh[k+3] == 'c' && roh[k+4] == 'l') {
            while (k < i && roh[k] != 34) k++;       /* zum ersten " */
            k++;
            n = 0;
            while (k < i && roh[k] != 34 && n < 20) { name[n] = roh[k]; n++; k++; }
            name[n] = 0;
            m = fileread_lib(name, INC_BUF, INC_MAX);
            if (m < 0) {
                printc("  Cannot include: ", RED);
                printc(name, RED);
                nl();
                return 0 - 1;
            }
            n = 0;
            while (n < m && j < SRC_MAX - 1) { ziel[j] = inc[n]; j++; n++; }
            if (j < SRC_MAX - 1) { ziel[j] = 10; j++; }
            continue;
        }

        k = start;
        while (k < i && j < SRC_MAX - 1) { ziel[j] = roh[k]; j++; k++; }
    }
    ziel[j] = 0;
    return j;
}

/* --- Durchgang ueber den ganzen Quelltext -------------------------------- */

void run_pass(int nr) {
    int i; int j;
    char* src;
    pass = nr;
    pc = LOAD_ADDR;
    out_len = 0;
    line_no = 1;
    src = (char*)SRC_BUF;

    i = 0;
    while (i < src_len) {
        j = 0;
        while (i < src_len && src[i] != 10) {
            if (j < 250 && src[i] != 13) { line[j] = src[i]; j++; }
            i++;
        }
        i++;
        line[j] = 0;
        do_line();
        line_no++;
        if ((line_no & 15) == 0) sc(28, i * 100 / src_len, 0, 0, 0);
    }
}

/* ========================================================================== */

int main() {
    int n; int i; char* src;
    char quelle[24];
    char ziel[24];

    print("TB-32 Assembler 1.0\n");

    src = (char*)0x00008200;   /* hier legt das OS die Befehlszeile ab */
    i = 0;
    while (src[i] == ' ') i++;
    n = 0;
    while (src[i] && src[i] != ' ') { quelle[n] = src[i]; n++; i++; }
    quelle[n] = 0;
    while (src[i] == ' ') i++;
    n = 0;
    while (src[i] && src[i] != ' ') { ziel[n] = src[i]; n++; i++; }
    ziel[n] = 0;

    if (quelle[0] == 0 || ziel[0] == 0) {
        print("Usage:  ASM <source.ASM> <target.TBX>\n\n");
        print("Writes a runnable program from assembly source.\n");
        print("Example:\n");
        print("  CODER MYPROG.ASM\n");
        print("  ASM MYPROG.ASM MYPROG.TBX\n");
        print("  START MYPROG.TBX\n");
        return 1;
    }

    n = fileread(quelle, ROH_BUF, SRC_MAX);
    if (n < 0) {
        printc("Source file not found: ", RED);
        print(quelle);
        nl();
        return 1;
    }
    n = includes_aufloesen(n);
    if (n < 0) return 1;
    src_len = n;

    print("Assembling ");
    printc(quelle, BRIGHT);
    print(" (");
    printn(n);
    print(" bytes) ...\n");

    init_table();
    sym_count = 0;
    errors = 0;

    run_pass(1);
    print("  Pass 1: ");
    printnc(sym_count, BRIGHT);
    print(" labels found\n");

    run_pass(2);
    print("  Pass 2: ");
    printnc(out_len, BRIGHT);
    print(" bytes of machine code\n");

    if (errors > 0) {
        printc("\nAssembly failed, errors: ", RED);
        printn(errors);
        print("\nNo output file written.\n");
        return 1;
    }

    if (filewrite(ziel, OUT_BUF, out_len) != 0) {
        printc("Could not write output file (disk full?)\n", RED);
        return 1;
    }

    printc("\nCreated ", GREEN);
    printc(ziel, BRIGHT);
    print("   Run it with: START ");
    print(ziel);
    nl();
    return 0;
}
