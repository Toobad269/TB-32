/* ==========================================================================
   CC  --  ein C-Compiler, der AUF dem TB-32 laeuft

   Damit schliesst sich der Kreis: der Rechner uebersetzt jetzt selbst
   Hochsprachen-Quelltext in Maschinencode. Kein Mac mehr noetig.

       EDIT PROG.C
       CC PROG.C PROG.TBX
       PROG

   Bauart: Ein-Durchgang-Compiler mit direkter Codeerzeugung -- genau so
   waren die ersten C-Compiler gebaut. Wo eine Adresse beim Erzeugen noch
   nicht bekannt ist (Sprung nach vorn, Aufruf einer spaeter definierten
   Funktion, Groesse des Stackrahmens), wird eine Luecke gelassen und
   spaeter nachgetragen. Das nennt man Backpatching.

   Sprachumfang:
       int, char, Zeiger (int*, char*), Arrays, globale und lokale Variablen
       Funktionen mit bis zu 5 Parametern, return, Rekursion
       if/else, while, for, break, continue, Bloecke
       + - * / % , == != < <= > >= , && || ! , & | ^ ~ << >> , = += -= *= /=
       ++ -- , Adresse-von (&x), Dereferenzierung (*p), Indizierung a[i]
       Zeichenketten, Zeichen ('A'), Kommentare
       eingebaute Systemfunktionen (siehe Tabelle unten)
   ========================================================================== */

#include "proglib.c"

#define SRC_BUF     0x00280000       /* Quelltext, bis 256 KB */
#define SRC_MAX     250000
#define OUT_BUF     0x00300000       /* erzeugter Code, bis 256 KB */
#define OUT_MAX     250000
#define STR_BUF     0x00380000       /* Zeichenketten des Programms */
#define INC_BUF     0x003A0000       /* Zwischenspeicher fuer #include */
#define LOAD_ADDR   0x00200000       /* dorthin laedt das OS das Ergebnis */
#define DATA_ADDR   0x00240000       /* globale Variablen des erzeugten Programms */

#define MAXGLOB     256
#define MAXLOC      64
#define MAXFUNC     192
#define MAXFIX      3000
#define MAXMACRO    256
#define NAMELEN     20

/* Typen */
#define TY_INT      0
#define TY_CHAR     1
#define TY_PINT     2
#define TY_PCHAR    3

/* Token */
#define TK_EOF      0
#define TK_NUM      1
#define TK_NAME     2
#define TK_STR      3
#define TK_OP       4

/* --- Quelltext und Token ------------------------------------------------- */
char* src;
int   sp_;                           /* Leseposition im Quelltext */
int   src_len;
int   zeile;

int   tk;                            /* Art des aktuellen Tokens */
int   tk_num;
char  tk_name[NAMELEN];
char  tk_op[4];
int   tk_str;                        /* Adresse der Zeichenkette */

/* --- Ausgabe ------------------------------------------------------------- */
int code_len;
int str_len;
int fehler;

/* --- Symbole ------------------------------------------------------------- */
char g_name[MAXGLOB * NAMELEN];
int  g_addr[MAXGLOB];
int  g_type[MAXGLOB];
int  g_isarr[MAXGLOB];
int  g_count;
int  data_ptr;                       /* Adresse fuer die naechste globale Variable */

char l_name[MAXLOC * NAMELEN];
int  l_off[MAXLOC];
int  l_type[MAXLOC];
int  l_isarr[MAXLOC];
int  l_count;
int  frame_size;

char f_name[MAXFUNC * NAMELEN];
int  f_addr[MAXFUNC];
int  f_count;

/* offene Aufrufe, deren Ziel beim Erzeugen noch unbekannt war */
int  fix_pos[MAXFIX];
int  fix_zeile[MAXFIX];              /* fuer die Fehlermeldung: wo stand der Aufruf? */
char fix_name[MAXFIX * NAMELEN];
int  fix_count;

/* Zeichenketten landen hinter dem Code -- ihre endgueltige Adresse steht
   erst fest, wenn der Code fertig ist. Also auch hier: nachtragen. */
int  sfix_pos[MAXFIX];
int  sfix_off[MAXFIX];
int  sfix_count;

/* --- Makros aus #define -------------------------------------------------- */
char m_name[MAXMACRO * NAMELEN];
int  m_val[MAXMACRO];
int  m_count;

/* --- Zustand des Ausdrucks ----------------------------------------------- */
int is_lval;                         /* r0 enthaelt eine Adresse, keinen Wert */
int cur_type;

/* --- Schleifen (fuer break / continue) ----------------------------------- */
int brk_pos[128];                    /* bis zu 8 break je Schleife */
int brk_count[16];
int cnt_target[16];
int loop_depth;

/* globale Variablen mit Startwert -- der Code dafuer laeuft vor main() */
int init_addr[128];
int init_val[128];
int init_count;
int init_call_pos;

char* g_at(int i) { return (char*)((int)g_name + i * NAMELEN); }
char* l_at(int i) { return (char*)((int)l_name + i * NAMELEN); }
char* f_at(int i) { return (char*)((int)f_name + i * NAMELEN); }
char* fix_at(int i) { return (char*)((int)fix_name + i * NAMELEN); }
char* m_at(int i)   { return (char*)((int)m_name + i * NAMELEN); }

int find_macro(char* name) {
    int i;
    for (i = 0; i < m_count; i++)
        if (strcmp(m_at(i), name) == 0) return i;
    return 0 - 1;
}

/* ==========================================================================
   Fehlerausgabe
   ========================================================================== */

void cc_error(char* text) {
    if (fehler > 8) return;
    fehler++;
    melde_ort(zeile);                /* rechnet auf die Zeile in DER Datei um */
    printc(": ", RED);
    printc(text, RED);
    nl();
}

/* ==========================================================================
   Praeprozessor

   Laedt den Quelltext und arbeitet dabei die Rautenzeilen ab:
     - define NAME wert    kommt in die Makrotabelle
     - include DATEI       wird an dieser Stelle eingefuegt
   Jede entfernte Zeile hinterlaesst eine leere Zeile, damit die
   Zeilennummern in Fehlermeldungen weiter stimmen.
   ========================================================================== */

int pp_out;                          /* Schreibposition im Quelltextpuffer */

/* --- Woher stammt eine Zeile? -------------------------------------------
   Der Vorverarbeiter klebt alle eingebundenen Dateien vor den eigenen
   Quelltext. Eine Fehlermeldung "line 146" bei einer acht Zeilen langen
   Datei hilft aber niemandem. Deshalb merken wir uns, welcher Bereich der
   zusammengeklebten Fassung aus welcher Datei stammt, und rechnen die
   Nummer beim Melden wieder zurueck. */
#define MAXINC   8
int inc_start[MAXINC];               /* erste Zeile im zusammengeklebten Text */
int inc_len[MAXINC];
char inc_name[MAXINC * 28];
int inc_count = 0;
int pp_zeile = 1;                    /* Zeilenzaehler waehrend des Klebens */

char* inc_at(int i) { return (char*)((int)inc_name + i * 28); }

/* Meldet die Zeile so, wie sie in der Datei steht, aus der sie stammt. */
void melde_ort(int z) {
    int i; int vorher;
    for (i = 0; i < inc_count; i++) {
        if (z >= inc_start[i] && z < inc_start[i] + inc_len[i]) {
            print("  ");
            print(inc_at(i));
            print(" line ");
            printn(z - inc_start[i] + 1);
            return;
        }
    }
    vorher = 0;
    for (i = 0; i < inc_count; i++)
        if (inc_start[i] < z) vorher = vorher + inc_len[i];
    print("  line ");
    printn(z - vorher);
}

void pp_emit(char* zeile) {
    int i;
    pp_zeile++;
    i = 0;
    while (zeile[i]) {
        byte_put(SRC_BUF + pp_out, zeile[i]);
        pp_out++;
        i++;
    }
    byte_put(SRC_BUF + pp_out, 10);
    pp_out++;
}

int zeilen_beginnt_mit(char* zeile, char* wort) {
    int i;
    i = 0;
    while (wort[i]) {
        if (zeile[i] != wort[i]) return 0;
        i++;
    }
    return 1;
}

int wert_aus_text(char* t) {
    int v; int i;
    v = 0;
    i = 0;
    while (t[i] == ' ') i++;
    if (t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) {
        i = i + 2;
        while (isalnum(t[i])) {
            v = v * 16;
            if (isdigit(t[i])) v = v + t[i] - '0';
            else v = v + (toupper(t[i]) - 'A' + 10);
            i++;
        }
        return v;
    }
    if (t[i] == '-') { i++; while (isdigit(t[i])) { v = v * 10 + t[i] - '0'; i++; } return 0 - v; }
    if (!isdigit(t[i])) return 0 - 2147483647;       /* kein Zahlenwert */
    while (isdigit(t[i])) { v = v * 10 + t[i] - '0'; i++; }
    return v;
}

/* Verfolgt zeilenweise, ob wir mitten in einem Blockkommentar stehen.
   Der Praeprozessor arbeitet Zeile fuer Zeile und wuerde sonst ein Doppelkreuz
   auch dann fuer eine Anweisung halten, wenn es nur in einem Kommentar steht --
   die Zeile fiele weg, und mit ihr womoeglich das schliessende Kommentarende.
   Genau dieser Fehler hat im Kernel einmal echten Quelltext verschluckt. */
int komm_folge(char* z, int drin) {
    int i; int c; int d; int q;
    i = 0;
    while (z[i]) {
        c = z[i];
        d = z[i + 1];
        if (drin) {
            if (c == '*' && d == '/') { drin = 0; i = i + 2; continue; }
            i++;
            continue;
        }
        if (c == '/' && d == '/') return 0;          /* Rest der Zeile egal */
        if (c == '/' && d == '*') { drin = 1; i = i + 2; continue; }
        if (c == 34 || c == 39) {                    /* Text- oder Zeichenwert */
            q = c;
            i++;
            while (z[i]) {
                if (z[i] == 92) { i = i + 2; continue; }
                if (z[i] == q) { i++; break; }
                i++;
            }
            continue;
        }
        i++;
    }
    return drin;
}

int load_source(char* datei, int tiefe) {
    int n; int i; int j; int k; int puffer; int wert;
    int komm; int war_komm;
    char zeile[256];
    char name[28];

    puffer = INC_BUF + tiefe * 70000;
    /* Die Hauptdatei liegt da, wo der Benutzer steht. Eingebundene Dateien
       duerfen zusaetzlich in \SOURCE gesucht werden -- dort liegen die
       Bibliotheken wie PROGLIB.C. */
    if (tiefe == 0) n = fileread(datei, puffer, 68000);
    else            n = fileread_lib(datei, puffer, 68000);
    if (n < 0) return 0 - 1;
    komm = 0;
    byte_put(puffer + n, 0);

    i = 0;
    while (i < n) {
        j = 0;
        while (i < n && byte_get(puffer + i) != 10) {
            if (j < 250) { zeile[j] = byte_get(puffer + i); j++; }
            i++;
        }
        i++;
        zeile[j] = 0;

        k = 0;
        while (zeile[k] == ' ' || zeile[k] == 9) k++;

        war_komm = komm;
        komm = komm_folge(zeile, komm);
        if (war_komm || zeile[k] != '#') { pp_emit(zeile); continue; }

        if (zeilen_beginnt_mit(zeile + k, "#define")) {
            k = k + 7;
            while (zeile[k] == ' ') k++;
            j = 0;
            while (isalnum(zeile[k])) {
                if (j < NAMELEN - 1) { name[j] = zeile[k]; j++; }
                k++;
            }
            name[j] = 0;
            wert = wert_aus_text(zeile + k);
            if (wert != 0 - 2147483647 && m_count < MAXMACRO) {
                strncpy(m_at(m_count), name, NAMELEN);
                m_val[m_count] = wert;
                m_count++;
            }
            pp_emit("");
            continue;
        }

        if (zeilen_beginnt_mit(zeile + k, "#include")) {
            k = k + 8;
            while (zeile[k] && zeile[k] != 34) k++;
            k++;
            j = 0;
            while (zeile[k] && zeile[k] != 34) {
                if (j < 26) { name[j] = zeile[k]; j++; }
                k++;
            }
            name[j] = 0;
            pp_emit("");
            if (tiefe < 2) {
                if (inc_count < MAXINC) {
                    inc_start[inc_count] = pp_zeile;
                    strncpy(inc_at(inc_count), name, 26);
                    inc_len[inc_count] = 0;
                    inc_count++;
                }
                if (load_source(name, tiefe + 1) < 0) {
                    printc("  cannot include: ", RED);
                    print(name);
                    print(" -- not in this folder and not in \\SOURCE");
                    nl();
                    fehler++;
                }
                if (inc_count > 0)
                    inc_len[inc_count - 1] = pp_zeile - inc_start[inc_count - 1];
            }
            continue;
        }

        pp_emit("");                                 /* andere #-Zeilen */
    }
    return pp_out;
}

/* ==========================================================================
   Lexer
   ========================================================================== */

int is_kw(char* w) { return strcmp(tk_name, w) == 0; }

void next_tok() {
    int c; int n; int j;

    /* Leerraum und Kommentare ueberspringen */
    while (sp_ < src_len) {
        c = src[sp_];
        if (c == 10) { zeile++; sp_++; continue; }
        if (c == ' ' || c == 9 || c == 13) { sp_++; continue; }
        if (c == '/' && src[sp_ + 1] == '/') {
            while (sp_ < src_len && src[sp_] != 10) sp_++;
            continue;
        }
        if (c == '/' && src[sp_ + 1] == '*') {
            sp_ = sp_ + 2;
            while (sp_ < src_len && !(src[sp_] == '*' && src[sp_ + 1] == '/')) {
                if (src[sp_] == 10) zeile++;
                sp_++;
            }
            sp_ = sp_ + 2;
            continue;
        }
        break;
    }

    if (sp_ >= src_len) { tk = TK_EOF; return; }
    c = src[sp_];

    if (isdigit(c)) {
        n = 0;
        if (c == '0' && (src[sp_ + 1] == 'x' || src[sp_ + 1] == 'X')) {
            sp_ = sp_ + 2;
            while (isalnum(src[sp_])) {
                n = n * 16;
                if (isdigit(src[sp_])) n = n + src[sp_] - '0';
                else n = n + (toupper(src[sp_]) - 'A' + 10);
                sp_++;
            }
        } else {
            while (isdigit(src[sp_])) { n = n * 10 + (src[sp_] - '0'); sp_++; }
        }
        tk = TK_NUM;
        tk_num = n;
        return;
    }

    if (isalpha(c)) {
        j = 0;
        while (isalnum(src[sp_])) {
            if (j < NAMELEN - 1) { tk_name[j] = src[sp_]; j++; }
            sp_++;
        }
        tk_name[j] = 0;
        j = find_macro(tk_name);
        if (j >= 0) {                                /* #define-Konstante */
            tk = TK_NUM;
            tk_num = m_val[j];
            return;
        }
        tk = TK_NAME;
        return;
    }

    if (c == 39) {                                   /* 'A' */
        sp_++;
        n = src[sp_];
        if (n == 92) {
            sp_++;
            n = src[sp_];
            if (n == 'n') n = 10;
            else if (n == 't') n = 9;
            else if (n == '0') n = 0;
            else if (n == 'r') n = 13;
        }
        sp_++;
        if (src[sp_] == 39) sp_++;
        tk = TK_NUM;
        tk_num = n;
        return;
    }

    if (c == 34) {                                   /* "Text" */
        sp_++;
        tk_str = STR_BUF + str_len;
        while (sp_ < src_len && src[sp_] != 34) {
            n = src[sp_];
            if (n == 92) {
                sp_++;
                n = src[sp_];
                if (n == 'n') n = 10;
                else if (n == 't') n = 9;
                else if (n == '0') n = 0;
                else if (n == 'r') n = 13;
            }
            byte_put(STR_BUF + str_len, n);
            str_len++;
            sp_++;
        }
        sp_++;
        byte_put(STR_BUF + str_len, 0);
        str_len++;
        tk = TK_STR;
        return;
    }

    /* Operatoren, ein- und zweistellig */
    tk_op[0] = c;
    tk_op[1] = 0;
    tk_op[2] = 0;
    n = src[sp_ + 1];
    if (n == '=') {
        if (c == '=' || c == '!' || c == '<' || c == '>' || c == '+' ||
            c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|') {
            tk_op[1] = '=';
            sp_++;
        }
    } else if ((c == '&' && n == '&') || (c == '|' && n == '|') ||
               (c == '+' && n == '+') || (c == '-' && n == '-') ||
               (c == '<' && n == '<') || (c == '>' && n == '>')) {
        tk_op[1] = n;
        sp_++;
    }
    sp_++;
    tk = TK_OP;
}

int at_op(char* s)  { return tk == TK_OP && strcmp(tk_op, s) == 0; }
int at_kw(char* s)  { return tk == TK_NAME && strcmp(tk_name, s) == 0; }

int accept_op(char* s) {
    if (at_op(s)) { next_tok(); return 1; }
    return 0;
}

void expect_op(char* s) {
    if (!accept_op(s)) {
        cc_error("syntax error, expected a different symbol");
        next_tok();
    }
}

int accept_kw(char* s) {
    if (at_kw(s)) { next_tok(); return 1; }
    return 0;
}

/* ==========================================================================
   Codeerzeugung
   ========================================================================== */

void emit(int word) {
    if (code_len + 4 >= OUT_MAX) { cc_error("program too large"); return; }
    mem_put(OUT_BUF + code_len, word);
    code_len = code_len + 4;
}

int here() { return LOAD_ADDR + code_len; }

int enc_r(int op, int rd, int ra, int rb) {
    return (op << 24) | ((rd & 15) << 20) | ((ra & 15) << 16) | ((rb & 15) << 12);
}

int enc_i(int op, int rd, int ra, int imm) {
    return (op << 24) | ((rd & 15) << 20) | ((ra & 15) << 16) | (imm & 65535);
}

void e_mov(int rd, int ra)        { emit(enc_r(0x10, rd, ra, 0)); }
void e_movi(int rd, int imm)      { emit(enc_i(0x11, rd, 0, imm)); }
void e_li(int rd, int v) {
    emit(enc_i(0x11, rd, 0, v & 65535));
    emit(enc_i(0x13, rd, 0, (v >> 16) & 65535));
}
void e_ldw(int rd, int ra, int o) { emit(enc_i(0x1B, rd, ra, o)); }
void e_ldb(int rd, int ra, int o) { emit(enc_i(0x18, rd, ra, o)); }
void e_stw(int rd, int ra, int o) { emit(enc_i(0x1E, rd, ra, o)); }
void e_stb(int rd, int ra, int o) { emit(enc_i(0x1C, rd, ra, o)); }
void e_alu(int op, int rd, int ra, int rb) { emit(enc_r(op, rd, ra, rb)); }
void e_addi(int rd, int ra, int i) { emit(enc_i(0x30, rd, ra, i)); }
void e_subi(int rd, int ra, int i) { emit(enc_i(0x31, rd, ra, i)); }
void e_muli(int rd, int ra, int i) { emit(enc_i(0x32, rd, ra, i)); }
void e_cmpi(int rd, int i)        { emit(enc_i(0x3D, rd, 0, i)); }
void e_cmp(int rd, int ra)        { emit(enc_r(0x2D, rd, ra, 0)); }
void e_push(int rd)               { emit(enc_r(0x40, rd, 0, 0)); }
void e_pop(int rd)                { emit(enc_r(0x41, rd, 0, 0)); }
void e_ret()                      { emit(enc_r(0x05, 0, 0, 0)); }
void e_int(int n)                 { emit(enc_i(0x64, 0, 0, n)); }
/* Hardware direkt: outr schreibt r2 an den Port in r1, inr holt von dort.
   Ports sind auf dem TB-32 nicht geschuetzt -- ein Programm darf sie selbst
   bedienen. Das spart bei jedem Malbefehl den Sprung in den Kernel. */
void e_outr()                     { emit(enc_r(0x63, 2, 1, 0)); }
void e_inr()                      { emit(enc_r(0x61, 0, 1, 0)); }

/* Sprung mit noch unbekanntem Ziel: Platz lassen, Position zurueckgeben */
int e_jump_open(int cond) {
    int p;
    p = code_len;
    emit((0x50 << 24) | ((cond & 15) << 20));
    return p;
}

void patch_jump(int p, int ziel) {
    int word; int off;
    off = (ziel - (LOAD_ADDR + p)) / 4;
    word = mem_get(OUT_BUF + p);
    word = (word & 0xFFF00000) | (off & 1048575);
    mem_put(OUT_BUF + p, word);
}

void e_jump_back(int cond, int ziel) {
    int off;
    off = (ziel - here()) / 4;
    emit((0x50 << 24) | ((cond & 15) << 20) | (off & 1048575));
}

/* Aufruf einer Funktion, deren Adresse evtl. noch nicht bekannt ist */
void e_call(char* name) {
    int i;
    for (i = 0; i < f_count; i++) {
        if (strcmp(f_at(i), name) == 0 && f_addr[i] >= 0) {
            emit((0x42 << 24) | (((f_addr[i] - here()) / 4) & 16777215));
            return;
        }
    }
    if (fix_count < MAXFIX) {
        fix_pos[fix_count] = code_len;
        fix_zeile[fix_count] = zeile;     /* Zeilennummer mitschreiben */
        strncpy(fix_at(fix_count), name, NAMELEN);
        fix_count++;
    }
    emit(0x42 << 24);                                /* wird spaeter gefuellt */
}

/* Jetzt steht fest, wo die Zeichenketten liegen: direkt hinter dem Code. */
void resolve_strings() {
    int i; int adr;
    for (i = 0; i < sfix_count; i++) {
        adr = LOAD_ADDR + code_len + sfix_off[i];
        mem_put(OUT_BUF + sfix_pos[i], enc_i(0x11, 0, 0, adr & 65535));
        mem_put(OUT_BUF + sfix_pos[i] + 4, enc_i(0x13, 0, 0, (adr >> 16) & 65535));
    }
}

void resolve_calls() {
    int i; int j; int gefunden; int off;
    for (i = 0; i < fix_count; i++) {
        gefunden = 0 - 1;
        for (j = 0; j < f_count; j++)
            if (strcmp(f_at(j), fix_at(i)) == 0) gefunden = j;
        if (gefunden < 0 || f_addr[gefunden] < 0) {
            /* Mit Zeilennummer -- ohne sie sucht man in einer langen Datei
               ewig, und genau diese Meldung kommt am haeufigsten. */
            melde_ort(fix_zeile[i]);
            printc(": unknown function ", RED);
            printc(fix_at(i), BRIGHT);
            nl();
            fehler++;
            continue;
        }
        off = (f_addr[gefunden] - (LOAD_ADDR + fix_pos[i])) / 4;
        mem_put(OUT_BUF + fix_pos[i], (0x42 << 24) | (off & 16777215));
    }
}

/* ==========================================================================
   Symbole
   ========================================================================== */

int type_size(int t) {
    if (t == TY_CHAR) return 1;
    return 4;
}

int elem_size(int t) {
    if (t == TY_PCHAR) return 1;
    return 4;
}

int find_local(char* name) {
    int i;
    for (i = l_count - 1; i >= 0; i--)
        if (strcmp(l_at(i), name) == 0) return i;
    return 0 - 1;
}

int find_global(char* name) {
    int i;
    for (i = 0; i < g_count; i++)
        if (strcmp(g_at(i), name) == 0) return i;
    return 0 - 1;
}

int find_func(char* name) {
    int i;
    for (i = 0; i < f_count; i++)
        if (strcmp(f_at(i), name) == 0) return i;
    return 0 - 1;
}

void add_local(char* name, int typ, int groesse, int istarray) {
    if (l_count >= MAXLOC) { cc_error("too many local variables"); return; }
    frame_size = frame_size + groesse;
    strncpy(l_at(l_count), name, NAMELEN);
    l_off[l_count] = 0 - frame_size;
    l_type[l_count] = typ;
    l_isarr[l_count] = istarray;
    l_count++;
}

void add_global(char* name, int typ, int bytes, int istarray) {
    if (g_count >= MAXGLOB) { cc_error("too many global variables"); return; }
    strncpy(g_at(g_count), name, NAMELEN);
    g_addr[g_count] = data_ptr;
    g_type[g_count] = typ;
    g_isarr[g_count] = istarray;
    g_count++;
    data_ptr = data_ptr + ((bytes + 3) / 4) * 4;
}

/* ==========================================================================
   Eingebaute Systemfunktionen
   ========================================================================== */

/* Gibt die Syscall-Nummer zurueck, oder -1 wenn es keine eingebaute ist.
   Die zweite Zahl (in builtin_fix) ist ein fester Wert fuer r2. */
int builtin_fix;

int builtin_no(char* name) {
    builtin_fix = 0 - 1;
    if (strcmp(name, "sc") == 0) return 99;          /* roher Systemaufruf */
    if (strcmp(name, "portout") == 0) return 98;     /* direkt an die Hardware */
    if (strcmp(name, "portin") == 0)  return 97;
    if (strcmp(name, "putch") == 0)     { builtin_fix = 7; return 0; }
    if (strcmp(name, "putcolor") == 0)  return 0;
    if (strcmp(name, "print") == 0)     { builtin_fix = 7; return 1; }
    if (strcmp(name, "printc") == 0)    return 1;
    if (strcmp(name, "getkey") == 0)    return 2;
    if (strcmp(name, "cls") == 0)       { builtin_fix = 7; return 3; }
    if (strcmp(name, "exit") == 0)      return 4;
    if (strcmp(name, "ticks") == 0)     return 5;
    if (strcmp(name, "printn") == 0)    { builtin_fix = 7; return 6; }
    if (strcmp(name, "printnc") == 0)   return 6;
    if (strcmp(name, "setcursor") == 0) return 7;
    if (strcmp(name, "putat") == 0)     return 8;
    if (strcmp(name, "haskey") == 0)    return 9;
    if (strcmp(name, "fileread") == 0)  return 10;
    if (strcmp(name, "filewrite") == 0) return 11;
    if (strcmp(name, "clock") == 0)     return 12;
    if (strcmp(name, "date") == 0)      return 13;
    if (strcmp(name, "sleep") == 0)     return 14;
    if (strcmp(name, "beep") == 0)      return 15;
    if (strcmp(name, "disksize") == 0)  return 16;
    if (strcmp(name, "setmode") == 0)   return 17;
    if (strcmp(name, "out") == 0)       return 18;
    if (strcmp(name, "in") == 0)        return 19;
    if (strcmp(name, "box") == 0)       return 20;
    if (strcmp(name, "hline") == 0)     return 21;
    if (strcmp(name, "memkb") == 0)     return 22;
    if (strcmp(name, "flushkeys") == 0) return 23;
    return 0 - 1;
}

/* ==========================================================================
   Ausdruecke
   ========================================================================== */

void expr();
void assign_expr();
void unary();
int parse_type();

/* Konstanter Ausdruck fuer Arraygroessen:  MAXGLOB * NAMELEN + 4  */
int const_expr() {
    int v; int w;
    v = 0;
    if (tk == TK_NUM) { v = tk_num; next_tok(); }
    while (at_op("*") || at_op("+") || at_op("-")) {
        if (accept_op("*")) {
            w = 0;
            if (tk == TK_NUM) { w = tk_num; next_tok(); }
            v = v * w;
        } else if (accept_op("+")) {
            w = 0;
            if (tk == TK_NUM) { w = tk_num; next_tok(); }
            v = v + w;
        } else {
            accept_op("-");
            w = 0;
            if (tk == TK_NUM) { w = tk_num; next_tok(); }
            v = v - w;
        }
    }
    return v;
}

/* Macht aus einer Adresse in r0 den Wert an dieser Adresse */
void rvalue() {
    if (!is_lval) return;
    if (cur_type == TY_CHAR) e_ldb(0, 0, 0);
    else e_ldw(0, 0, 0);
    is_lval = 0;
}

void primary() {
    int i; int n; int builtin; int argn; int typ;
    char name[NAMELEN];
    typ = 0;

    if (tk == TK_NUM) {
        e_li(0, tk_num);
        is_lval = 0;
        cur_type = TY_INT;
        next_tok();
        return;
    }

    if (tk == TK_STR) {
        if (sfix_count < MAXFIX) {
            sfix_pos[sfix_count] = code_len;
            sfix_off[sfix_count] = tk_str - STR_BUF;
            sfix_count++;
        }
        e_li(0, 0);                                  /* Adresse folgt spaeter */
        is_lval = 0;
        cur_type = TY_PCHAR;
        next_tok();
        return;
    }

    if (accept_op("(")) {
        /* Typumwandlung?  (char*)x  -- in einem Ausdruck kann (int) nichts
           anderes bedeuten, deshalb reicht ein Blick auf das naechste Wort. */
        if (at_kw("int") || at_kw("char") || at_kw("void")) {
            typ = parse_type();
            expect_op(")");
            unary();
            rvalue();
            cur_type = typ;
            is_lval = 0;
            return;
        }
        expr();
        expect_op(")");
        return;
    }

    if (tk == TK_NAME) {
        strncpy(name, tk_name, NAMELEN);
        next_tok();

        if (at_op("(")) {                            /* Funktionsaufruf */
            next_tok();
            argn = 0;
            if (!at_op(")")) {
                while (1) {
                    assign_expr();
                    rvalue();
                    e_push(0);
                    argn++;
                    if (!accept_op(",")) break;
                }
            }
            expect_op(")");
            for (i = argn; i >= 1; i--) e_pop(i);    /* r1..rn fuellen */

            builtin = builtin_no(name);
            if (builtin == 98) {                     /* portout(port, wert) */
                e_outr();
                is_lval = 0;
                cur_type = TY_INT;
                return;
            }
            if (builtin == 97) {                     /* portin(port) */
                e_inr();
                is_lval = 0;
                cur_type = TY_INT;
                return;
            }
            if (builtin == 99) {                     /* sc(nummer, a1..a4) */
                e_mov(10, 1);
                e_mov(1, 2);
                e_mov(2, 3);
                e_mov(3, 4);
                e_mov(4, 5);
                e_mov(0, 10);
                e_int(0x40);
                is_lval = 0;
                cur_type = TY_INT;
                return;
            }
            if (builtin >= 0) {
                if (builtin_fix >= 0) {
                    if (argn == 0) e_movi(1, builtin_fix);
                    else if (argn < 2) e_movi(2, builtin_fix);
                }
                e_movi(0, builtin);
                e_int(0x40);
            } else {
                e_call(name);
            }
            is_lval = 0;
            cur_type = TY_INT;
            return;
        }

        i = find_local(name);
        if (i >= 0) {
            e_addi(0, 14, l_off[i]);
            if (l_isarr[i]) {
                is_lval = 0;                         /* ein Array IST seine Adresse */
                if (l_type[i] == TY_CHAR) cur_type = TY_PCHAR;
                else cur_type = TY_PINT;
            } else {
                is_lval = 1;
                cur_type = l_type[i];
            }
            return;
        }
        i = find_global(name);
        if (i >= 0) {
            e_li(0, g_addr[i]);
            if (g_isarr[i]) {
                is_lval = 0;
                if (g_type[i] == TY_CHAR) cur_type = TY_PCHAR;
                else cur_type = TY_PINT;
            } else {
                is_lval = 1;
                cur_type = g_type[i];
            }
            return;
        }
        i = find_func(name);
        if (i >= 0) {
            e_li(0, f_addr[i]);
            is_lval = 0;
            cur_type = TY_INT;
            return;
        }
        cc_error("unknown identifier");
        e_movi(0, 0);
        is_lval = 0;
        cur_type = TY_INT;
        return;
    }

    cc_error("expression expected");
    e_movi(0, 0);
    is_lval = 0;
    cur_type = TY_INT;
    next_tok();
}

void postfix() {
    int esz; int typ;
    primary();
    while (1) {
        if (at_op("[")) {
            next_tok();
            rvalue();                                /* Basisadresse in r0 */
            typ = cur_type;
            esz = elem_size(typ);
            e_push(0);
            expr();
            rvalue();
            if (esz > 1) e_muli(0, 0, esz);
            e_pop(10);
            e_alu(0x20, 0, 10, 0);                   /* add r0, r10, r0 */
            expect_op("]");
            is_lval = 1;
            if (typ == TY_PCHAR) cur_type = TY_CHAR;
            else cur_type = TY_INT;
            continue;
        }
        if (at_op("++") || at_op("--")) {
            int minus;
            minus = at_op("--");
            next_tok();
            if (!is_lval) { cc_error("cannot increment this"); return; }
            e_push(0);                               /* Adresse merken */
            if (cur_type == TY_CHAR) e_ldb(0, 0, 0);
            else e_ldw(0, 0, 0);
            e_push(0);                               /* alter Wert */
            if (minus) e_addi(0, 0, 0 - 1);
            else e_addi(0, 0, 1);
            e_pop(11);                               /* alter Wert -> r11 */
            e_pop(10);                               /* Adresse -> r10 */
            if (cur_type == TY_CHAR) e_stb(0, 10, 0);
            else e_stw(0, 10, 0);
            e_mov(0, 11);                            /* Ergebnis = alter Wert */
            is_lval = 0;
            continue;
        }
        break;
    }
}

void unary() {
    int i; int typ;
    if (accept_op("-")) {
        unary();
        rvalue();
        emit(enc_r(0x2C, 0, 0, 0));                  /* neg r0, r0 */
        is_lval = 0;
        return;
    }
    if (accept_op("!")) {
        unary();
        rvalue();
        e_cmpi(0, 0);
        i = e_jump_open(1);                          /* jz -> 1 */
        e_movi(0, 0);
        typ = e_jump_open(0);
        patch_jump(i, here());
        e_movi(0, 1);
        patch_jump(typ, here());
        is_lval = 0;
        cur_type = TY_INT;
        return;
    }
    if (accept_op("~")) {
        unary();
        rvalue();
        emit(enc_r(0x2B, 0, 0, 0));                  /* not r0, r0 */
        is_lval = 0;
        return;
    }
    if (accept_op("*")) {
        unary();
        rvalue();
        is_lval = 1;
        if (cur_type == TY_PCHAR) cur_type = TY_CHAR;
        else cur_type = TY_INT;
        return;
    }
    if (accept_op("&")) {
        unary();
        if (!is_lval) { cc_error("cannot take address of this"); return; }
        is_lval = 0;
        if (cur_type == TY_CHAR) cur_type = TY_PCHAR;
        else cur_type = TY_PINT;
        return;
    }
    if (at_op("++") || at_op("--")) {
        int minus;
        minus = at_op("--");
        next_tok();
        unary();
        if (!is_lval) { cc_error("cannot increment this"); return; }
        e_push(0);
        if (cur_type == TY_CHAR) e_ldb(0, 0, 0);
        else e_ldw(0, 0, 0);
        if (minus) e_addi(0, 0, 0 - 1);
        else e_addi(0, 0, 1);
        e_pop(10);
        if (cur_type == TY_CHAR) e_stb(0, 10, 0);
        else e_stw(0, 10, 0);
        is_lval = 0;
        return;
    }
    postfix();
}

/* Hilfsfunktion: linke Seite steht in r0, rechte Seite auswerten,
   danach linke in r0 und rechte in r10 */
void binop_prepare() {
    e_push(0);
}

void binop_finish() {
    e_mov(10, 0);
    e_pop(0);
}

void mul_expr() {
    int typ;
    unary();
    while (at_op("*") || at_op("/") || at_op("%")) {
        int welche;
        rvalue();
        welche = 0;
        if (at_op("/")) welche = 1;
        if (at_op("%")) welche = 2;
        next_tok();
        binop_prepare();
        unary();
        rvalue();
        binop_finish();
        if (welche == 0) e_alu(0x22, 0, 0, 10);
        if (welche == 1) e_alu(0x23, 0, 0, 10);
        if (welche == 2) e_alu(0x24, 0, 0, 10);
        cur_type = TY_INT;
    }
}

void add_expr() {
    int links; int esz; int minus;
    mul_expr();
    while (at_op("+") || at_op("-")) {
        rvalue();
        links = cur_type;
        minus = at_op("-");
        next_tok();
        binop_prepare();
        mul_expr();
        rvalue();
        /* Zeigerarithmetik: p + n rueckt um sizeof(*p) weiter */
        esz = 1;
        if (links == TY_PINT) esz = 4;
        if (links == TY_PCHAR) esz = 1;
        if ((links == TY_PINT) && esz > 1) e_muli(0, 0, esz);
        binop_finish();
        if (minus) e_alu(0x21, 0, 0, 10);
        else e_alu(0x20, 0, 0, 10);
        cur_type = links;
        if (links != TY_PINT && links != TY_PCHAR) cur_type = TY_INT;
    }
}

void shift_expr() {
    add_expr();
    while (at_op("<<") || at_op(">>")) {
        int rechts;
        rvalue();
        rechts = at_op(">>");
        next_tok();
        binop_prepare();
        add_expr();
        rvalue();
        binop_finish();
        if (rechts) e_alu(0x29, 0, 0, 10);
        else e_alu(0x28, 0, 0, 10);
        cur_type = TY_INT;
    }
}

/* Vergleich: Ergebnis 0 oder 1 in r0 */
void cmp_to_bool(int cond) {
    int j1; int j2;
    e_cmp(0, 10);
    j1 = e_jump_open(cond);
    e_movi(0, 0);
    j2 = e_jump_open(0);
    patch_jump(j1, here());
    e_movi(0, 1);
    patch_jump(j2, here());
}

void rel_expr() {
    int cond;
    shift_expr();
    while (at_op("<") || at_op(">") || at_op("<=") || at_op(">=")) {
        rvalue();
        cond = 11;                                   /* jl  */
        if (at_op(">"))  cond = 14;                  /* jg  */
        if (at_op("<=")) cond = 13;                  /* jle */
        if (at_op(">=")) cond = 12;                  /* jge */
        next_tok();
        binop_prepare();
        shift_expr();
        rvalue();
        binop_finish();
        cmp_to_bool(cond);
        cur_type = TY_INT;
    }
}

void eq_expr() {
    int cond;
    rel_expr();
    while (at_op("==") || at_op("!=")) {
        rvalue();
        cond = 1;
        if (at_op("!=")) cond = 2;
        next_tok();
        binop_prepare();
        rel_expr();
        rvalue();
        binop_finish();
        cmp_to_bool(cond);
        cur_type = TY_INT;
    }
}

void band_expr() {
    eq_expr();
    while (at_op("&")) {
        rvalue();
        next_tok();
        binop_prepare();
        eq_expr();
        rvalue();
        binop_finish();
        e_alu(0x25, 0, 0, 10);
        cur_type = TY_INT;
    }
}

void bxor_expr() {
    band_expr();
    while (at_op("^")) {
        rvalue();
        next_tok();
        binop_prepare();
        band_expr();
        rvalue();
        binop_finish();
        e_alu(0x27, 0, 0, 10);
        cur_type = TY_INT;
    }
}

void bor_expr() {
    bxor_expr();
    while (at_op("|")) {
        rvalue();
        next_tok();
        binop_prepare();
        bxor_expr();
        rvalue();
        binop_finish();
        e_alu(0x26, 0, 0, 10);
        cur_type = TY_INT;
    }
}

/*  a && b  --  wird zu:  ist a falsch? -> 0.  sonst ist b falsch? -> 0. sonst 1. */
void and_expr() {
    int j1; int j2; int je;
    bor_expr();
    while (at_op("&&")) {
        rvalue();
        next_tok();
        e_cmpi(0, 0);
        j1 = e_jump_open(1);
        bor_expr();
        rvalue();
        e_cmpi(0, 0);
        j2 = e_jump_open(1);
        e_movi(0, 1);
        je = e_jump_open(0);
        patch_jump(j1, here());
        patch_jump(j2, here());
        e_movi(0, 0);
        patch_jump(je, here());
        cur_type = TY_INT;
    }
}

/*  a || b  --  ist a wahr? -> 1. sonst ist b wahr? -> 1. sonst 0. */
void or_expr() {
    int j1; int j2; int je;
    and_expr();
    while (at_op("||")) {
        rvalue();
        next_tok();
        e_cmpi(0, 0);
        j1 = e_jump_open(2);
        and_expr();
        rvalue();
        e_cmpi(0, 0);
        j2 = e_jump_open(2);
        e_movi(0, 0);
        je = e_jump_open(0);
        patch_jump(j1, here());
        patch_jump(j2, here());
        e_movi(0, 1);
        patch_jump(je, here());
        cur_type = TY_INT;
    }
}

void assign_expr() {
    int typ; int istchar; int op;
    or_expr();

    if (at_op("=") || at_op("+=") || at_op("-=") || at_op("*=") || at_op("/=")) {
        if (!is_lval) { cc_error("cannot assign to this"); return; }
        istchar = 0;
        if (cur_type == TY_CHAR) istchar = 1;
        op = 0;
        if (at_op("+=")) op = 1;
        if (at_op("-=")) op = 2;
        if (at_op("*=")) op = 3;
        if (at_op("/=")) op = 4;
        next_tok();

        e_push(0);                                   /* Zieladresse merken */
        if (op) {                                    /* alten Wert holen */
            if (istchar) e_ldb(0, 0, 0);
            else e_ldw(0, 0, 0);
            e_push(0);
        }
        assign_expr();
        rvalue();
        if (op) {
            e_mov(10, 0);
            e_pop(0);                                /* alter Wert */
            if (op == 1) e_alu(0x20, 0, 0, 10);
            if (op == 2) e_alu(0x21, 0, 0, 10);
            if (op == 3) e_alu(0x22, 0, 0, 10);
            if (op == 4) e_alu(0x23, 0, 0, 10);
        }
        e_pop(10);                                   /* Zieladresse */
        if (istchar) e_stb(0, 10, 0);
        else e_stw(0, 10, 0);
        is_lval = 0;
    }
}

void expr() {
    assign_expr();
}

/* ==========================================================================
   Anweisungen
   ========================================================================== */

void statement();

/* Bedingung auswerten und springen, wenn sie FALSCH ist */
int cond_jump_false() {
    expect_op("(");
    expr();
    rvalue();
    expect_op(")");
    e_cmpi(0, 0);
    return e_jump_open(1);                           /* jz */
}

int parse_type() {
    /* Rueckgabe: -1 wenn kein Typ folgt, sonst der Typcode */
    int t;
    if (at_kw("int")) t = TY_INT;
    else if (at_kw("char")) t = TY_CHAR;
    else if (at_kw("void")) t = TY_INT;
    else return 0 - 1;
    next_tok();
    if (accept_op("*")) {
        if (t == TY_CHAR) return TY_PCHAR;
        return TY_PINT;
    }
    return t;
}

void local_decl(int typ) {
    int n; int groesse; int istarray;
    while (1) {
        if (tk != TK_NAME) { cc_error("variable name expected"); return; }
        istarray = 0;
        groesse = 4;
        if (typ == TY_CHAR) groesse = 4;             /* einzelne chars bekommen ein Wort */
        {
            char name[NAMELEN];
            strncpy(name, tk_name, NAMELEN);
            next_tok();
            if (accept_op("[")) {
                istarray = 1;
                n = const_expr();
                if (n < 1) n = 1;
                expect_op("]");
                groesse = n * type_size(typ);
                groesse = ((groesse + 3) / 4) * 4;
            }
            add_local(name, typ, groesse, istarray);

            if (accept_op("=")) {                    /* Startwert */
                int idx;
                idx = find_local(name);
                assign_expr();
                rvalue();
                e_addi(10, 14, l_off[idx]);
                if (typ == TY_CHAR) e_stb(0, 10, 0);
                else e_stw(0, 10, 0);
            }
        }
        if (!accept_op(",")) break;
    }
    expect_op(";");
}

void block() {
    int alt_count;
    alt_count = l_count;
    expect_op("{");
    while (!at_op("}") && tk != TK_EOF && fehler < 8) {
        statement();
    }
    expect_op("}");
    l_count = alt_count;                             /* Sichtbarkeit beenden */
}

void statement() {
    int typ; int j1; int j2; int start; int i;

    if (at_op("{")) { block(); return; }
    if (accept_op(";")) return;

    typ = parse_type();
    if (typ >= 0) { local_decl(typ); return; }

    if (accept_kw("if")) {
        j1 = cond_jump_false();
        statement();
        if (accept_kw("else")) {
            j2 = e_jump_open(0);
            patch_jump(j1, here());
            statement();
            patch_jump(j2, here());
        } else {
            patch_jump(j1, here());
        }
        return;
    }

    if (accept_kw("while")) {
        start = here();
        j1 = cond_jump_false();
        loop_depth++;
        brk_count[loop_depth] = 0;
        cnt_target[loop_depth] = start;
        statement();
        e_jump_back(0, start);
        patch_jump(j1, here());
        for (i = 0; i < brk_count[loop_depth]; i++)
            patch_jump(brk_pos[loop_depth * 8 + i], here());
        loop_depth--;
        return;
    }

    if (accept_kw("for")) {
        int bedingung; int schritt_start; int koerper_start; int ende;
        expect_op("(");
        if (!at_op(";")) { expr(); }
        expect_op(";");
        start = here();
        j1 = 0 - 1;
        if (!at_op(";")) {
            expr();
            rvalue();
            e_cmpi(0, 0);
            j1 = e_jump_open(1);
        }
        expect_op(";");
        /* Der Schrittteil muss NACH dem Rumpf laufen: wir springen um ihn
           herum, merken uns seine Adresse und springen spaeter hin. */
        j2 = e_jump_open(0);
        schritt_start = here();
        if (!at_op(")")) { expr(); }
        e_jump_back(0, start);
        expect_op(")");
        patch_jump(j2, here());
        loop_depth++;
        brk_count[loop_depth] = 0;
        cnt_target[loop_depth] = schritt_start;
        statement();
        e_jump_back(0, schritt_start);
        if (j1 >= 0) patch_jump(j1, here());
        for (i = 0; i < brk_count[loop_depth]; i++)
            patch_jump(brk_pos[loop_depth * 8 + i], here());
        loop_depth--;
        return;
    }

    if (accept_kw("break")) {
        if (loop_depth > 0 && brk_count[loop_depth] < 8) {
            brk_pos[loop_depth * 8 + brk_count[loop_depth]] = e_jump_open(0);
            brk_count[loop_depth]++;
        } else {
            cc_error("break outside a loop");
        }
        expect_op(";");
        return;
    }

    if (accept_kw("continue")) {
        if (loop_depth > 0) e_jump_back(0, cnt_target[loop_depth]);
        else cc_error("continue outside a loop");
        expect_op(";");
        return;
    }

    if (accept_kw("return")) {
        if (!at_op(";")) { expr(); rvalue(); }
        else e_movi(0, 0);
        e_mov(15, 14);                               /* mov sp, fp */
        e_pop(14);                                   /* pop fp */
        e_ret();
        expect_op(";");
        return;
    }

    expr();
    expect_op(";");
}

/* ==========================================================================
   Funktionen und globale Deklarationen
   ========================================================================== */

void function(char* name, int typ) {
    int f; int frame_patch; int i; int pcount;
    char pname[MAXLOC];

    f = find_func(name);
    if (f < 0) {
        f = f_count;
        strncpy(f_at(f), name, NAMELEN);
        f_count++;
    }
    f_addr[f] = here();
    if (f_addr[f] < 0) f_addr[f] = here();

    l_count = 0;
    frame_size = 0;
    loop_depth = 0;

    /* Parameterliste */
    pcount = 0;
    if (!at_op(")")) {
        while (1) {
            int ptyp;
            ptyp = parse_type();
            if (ptyp < 0) { cc_error("parameter type expected"); break; }
            if (tk != TK_NAME) { cc_error("parameter name expected"); break; }
            add_local(tk_name, ptyp, 4, 0);
            pcount++;
            next_tok();
            if (!accept_op(",")) break;
        }
    }
    expect_op(")");

    if (at_op(";")) {                                /* nur eine Ankuendigung */
        next_tok();
        f_addr[f] = 0 - 1;
        return;
    }

    e_push(14);                                      /* push fp */
    e_mov(14, 15);                                   /* mov fp, sp */
    frame_patch = code_len;
    emit(enc_i(0x31, 15, 15, 0));                    /* subi sp, sp, ? */

    for (i = 0; i < pcount; i++) {                   /* Parameter sichern */
        if (i < 5) e_stw(i + 1, 14, l_off[i]);
    }

    if (!at_op("{")) { cc_error("function body expected"); return; }
    block();

    /* Standard-Rueckgabe, falls kein return kam */
    e_movi(0, 0);
    e_mov(15, 14);
    e_pop(14);
    e_ret();

    mem_put(OUT_BUF + frame_patch, enc_i(0x31, 15, 15, frame_size + 16));
}

void declaration() {
    int typ; int n; int bytes; int istarray;
    char name[NAMELEN];

    typ = parse_type();
    if (typ < 0) {
        cc_error("type expected (int, char, void)");
        next_tok();
        return;
    }
    if (tk != TK_NAME) { cc_error("name expected"); next_tok(); return; }
    strncpy(name, tk_name, NAMELEN);
    next_tok();

    if (accept_op("(")) { function(name, typ); return; }

    /* globale Variable */
    while (1) {
        istarray = 0;
        bytes = 4;
        if (accept_op("[")) {
            istarray = 1;
            n = const_expr();
            if (n < 1) n = 1;
            expect_op("]");
            bytes = n * type_size(typ);
        }
        add_global(name, typ, bytes, istarray);

        if (accept_op("=")) {                        /* Startwert */
            int wert; int neg;
            neg = 0;
            if (accept_op("-")) neg = 1;
            wert = const_expr();
            if (neg) wert = 0 - wert;
            if (init_count < 128) {
                init_addr[init_count] = g_addr[g_count - 1];
                init_val[init_count] = wert;
                init_count++;
            }
        }

        if (!accept_op(",")) break;
        if (tk != TK_NAME) break;
        strncpy(name, tk_name, NAMELEN);
        next_tok();
    }
    expect_op(";");
}

/* ==========================================================================
   Hauptprogramm
   ========================================================================== */

int main() {
    int n; int i; int haupt;
    char quelle[24];
    char ziel[24];
    char* args;

    print("\nTB-32 C Compiler 1.0\n\n");

    args = (char*)0x00008200;
    i = 0;
    while (args[i] == ' ') i++;
    n = 0;
    while (args[i] && args[i] != ' ') { quelle[n] = args[i]; n++; i++; }
    quelle[n] = 0;
    while (args[i] == ' ') i++;
    n = 0;
    while (args[i] && args[i] != ' ') { ziel[n] = args[i]; n++; i++; }
    ziel[n] = 0;

    if (quelle[0] == 0 || ziel[0] == 0) {
        print("Usage:  CC <source.C> <target.TBX>\n\n");
        print("Compiles C source into a runnable program.\n\n");
        print("Supported: int, char, pointers, arrays, functions, if/else,\n");
        print("while, for, break, continue, return, all common operators.\n");
        print("Built-in calls: print printn printc putch cls getkey haskey\n");
        print("  setcursor putat sleep beep ticks clock in out box hline\n");
        print("  fileread filewrite memkb flushkeys setmode exit\n\n");
        print("Example:\n  EDIT MYPROG.C\n  CC MYPROG.C MY.TBX\n  MY\n");
        return 1;
    }

    fehler = 0;
    m_count = 0;
    pp_out = 0;
    n = load_source(quelle, 0);
    if (n < 0) {
        printc("Cannot open source file: ", RED);
        print(quelle);
        nl();
        return 1;
    }
    byte_put(SRC_BUF + n, 0);

    print("Compiling ");
    printc(quelle, BRIGHT);
    print(" (");
    printn(n);
    print(" bytes after preprocessing, ");
    printn(m_count);
    print(" macros) ...\n");

    src = (char*)SRC_BUF;
    src_len = n;
    sp_ = 0;
    zeile = 1;
    code_len = 0;
    str_len = 0;
    fehler = 0;
    g_count = 0;
    f_count = 0;
    fix_count = 0;
    l_count = 0;
    loop_depth = 0;
    sfix_count = 0;
    data_ptr = DATA_ADDR;

    /* Der Kopf des Programms: erst globale Startwerte setzen, dann main
       aufrufen, danach sauber beenden. */
    init_count = 0;
    init_call_pos = code_len;
    emit(0x42 << 24);                                /* call __init, folgt */
    e_call("main");
    e_movi(0, 4);
    e_int(0x40);
    e_ret();

    /* Fortschritt und Zwischenstand melden -- eine grafische Oberflaeche
       macht daraus Ladebalken und Statuszeile, im Textmodus passiert
       einfach nichts. */
    sc(29, (int)"Reading the source and expanding macros ...", 0, 0, 0);
    next_tok();
    sc(29, (int)"Compiling functions ...", 0, 0, 0);
    while (tk != TK_EOF && fehler < 8) {
        declaration();
        sc(28, sp_ * 100 / src_len, 0, 0, 0);
    }
    sc(28, 100, 0, 0, 0);
    sc(29, (int)"Resolving names and writing the program ...", 0, 0, 0);

    /* Jetzt die Initialisierung der globalen Variablen erzeugen und den
       Aufruf im Programmkopf darauf zeigen lassen. */
    i = here();
    mem_put(OUT_BUF + init_call_pos,
            (0x42 << 24) | (((i - (LOAD_ADDR + init_call_pos)) / 4) & 16777215));
    {
        int k;
        for (k = 0; k < init_count; k++) {
            e_li(0, init_val[k]);
            e_li(10, init_addr[k]);
            e_stw(0, 10, 0);
        }
    }
    e_ret();

    resolve_strings();
    resolve_calls();

    if (fehler) {
        printc("\nCompilation failed with ", RED);
        printn(fehler);
        printc(" error(s). No output written.\n", RED);
        return 1;
    }

    /* Zeichenketten anhaengen */
    for (i = 0; i < str_len; i++)
        byte_put(OUT_BUF + code_len + i, byte_get(STR_BUF + i));

    print("  Code: ");
    printnc(code_len, BRIGHT);
    print(" bytes    Strings: ");
    printnc(str_len, BRIGHT);
    print(" bytes    Globals: ");
    printnc(g_count, BRIGHT);
    nl();

    if (filewrite(ziel, OUT_BUF, code_len + str_len) != 0) {
        printc("Could not write output file.\n", RED);
        return 1;
    }

    printc("\nCreated ", GREEN);
    printc(ziel, BRIGHT);
    print("   Run it with: ");
    print(ziel);
    nl();
    return 0;
}
