/* ==========================================================================
   PY  --  ein Python-Interpreter fuer den TB-32

   Das echte CPython laesst sich hier nicht installieren: es ist Maschinencode
   fuer x86/ARM und braucht ein Betriebssystem mit Speicherverwaltung und
   C-Bibliothek. Der Weg, den auch MicroPython geht, ist ein anderer: die
   SPRACHE neu umsetzen, fuer diese Maschine. Genau das ist diese Datei.

   Unterstuetzt:
       Zahlen, Zeichenketten, Listen
       Variablen, Zuweisung, auch  +=  -=  *=  /=
       print(...)  input(...)  len  str  int  abs  min  max  chr  ord  range
       + - * / % , Vergleiche, and / or / not
       if / elif / else, while, for x in range(...), break, continue
       def mit Parametern und return (auch rekursiv)
       Kommentare mit #, Bloecke ueber Einrueckung -- wie in echtem Python

   Aufruf:   PY PROGRAMM.PY
   ========================================================================== */

#include "proglib.c"

#define SRC_BUF     0x00280000       /* Quelltext */
#define SRC_MAX     60000
#define HEAP_BASE   0x00300000       /* Werte und Zeichenketten */
#define HEAP_MAX    0x00100000

#define MAXLINES    1200
#define MAXVARS     256
#define MAXFUNCS    64
#define MAXTOK      128
#define NAMELEN     16

/* Werttypen */
#define T_INT   0
#define T_STR   1
#define T_LIST  2

/* Tokenarten */
#define K_END   0
#define K_NUM   1
#define K_NAME  2
#define K_STR   3
#define K_OP    4

/* --- Quelltext, zeilenweise ---------------------------------------------- */
int line_pos[MAXLINES];              /* Startadresse der Zeile */
int line_ind[MAXLINES];              /* Einrueckungstiefe */
int line_count;

/* --- Variablen ----------------------------------------------------------- */
char var_name[MAXVARS * NAMELEN];
int  var_val[MAXVARS];               /* Zeiger auf ein Wertobjekt */
int  var_count;
int  scope_base;                     /* ab hier sind die Variablen lokal */

/* --- Funktionen ---------------------------------------------------------- */
char fn_name[MAXFUNCS * NAMELEN];
int  fn_line[MAXFUNCS];
int  fn_nparam[MAXFUNCS];
char fn_param[MAXFUNCS * 4 * NAMELEN];   /* bis zu 4 Parameter */
int  fn_count;

/* --- Zustand ------------------------------------------------------------- */
int heap_ptr;
int fehler;
int flow;                            /* 0 = normal, 1 = break, 2 = continue, 3 = return */
int ret_val;
int akt_zeile;

/* --- Token der aktuellen Zeile ------------------------------------------- */
int  tok_kind[MAXTOK];
int  tok_num[MAXTOK];
char tok_txt[MAXTOK * NAMELEN];
int  tok_count;
int  tp;                             /* Leseposition im Token-Strom */

char* tok_text(int i) { return (char*)((int)tok_txt + i * NAMELEN); }
char* var_at(int i)   { return (char*)((int)var_name + i * NAMELEN); }
char* fn_at(int i)    { return (char*)((int)fn_name + i * NAMELEN); }
char* param_at(int f, int p) {
    return (char*)((int)fn_param + (f * 4 + p) * NAMELEN);
}

/* ==========================================================================
   Speicher und Werte
   ========================================================================== */

int alloc(int bytes) {
    int a;
    a = heap_ptr;
    heap_ptr = heap_ptr + ((bytes + 3) / 4) * 4;
    return a;
}

int value_new(int typ, int wert) {
    int a;
    a = alloc(8);
    mem_put(a, typ);
    mem_put(a + 4, wert);
    return a;
}

int v_type(int v) { return mem_get(v); }
int v_num(int v)  { return mem_get(v + 4); }

int make_int(int n) { return value_new(T_INT, n); }

/* Zeichenkette in den Heap kopieren und als Wert verpacken */
int make_str(char* s) {
    int a; int n;
    n = strlen(s);
    a = alloc(n + 1);
    strcpy((char*)a, s);
    return value_new(T_STR, a);
}

char* str_of(int v) { return (char*)v_num(v); }

/* Liste: [Anzahl][Kapazitaet][Wert0][Wert1]... */
int make_list(int kapazitaet) {
    int a; int i;
    if (kapazitaet < 4) kapazitaet = 4;
    a = alloc(8 + kapazitaet * 4);
    mem_put(a, 0);
    mem_put(a + 4, kapazitaet);
    return value_new(T_LIST, a);
}

int list_len(int v)          { return mem_get(v_num(v)); }
int list_get(int v, int i)   { return mem_get(v_num(v) + 8 + i * 4); }
void list_set(int v, int i, int w) { mem_put(v_num(v) + 8 + i * 4, w); }

void list_add(int v, int w) {
    int L; int n; int kap; int neu; int i;
    L = v_num(v);
    n = mem_get(L);
    kap = mem_get(L + 4);
    if (n >= kap) {                                  /* Liste vergroessern */
        neu = alloc(8 + kap * 2 * 4);
        mem_put(neu, n);
        mem_put(neu + 4, kap * 2);
        for (i = 0; i < n; i++) mem_put(neu + 8 + i * 4, mem_get(L + 8 + i * 4));
        mem_put(v + 4, neu);
        L = neu;
    }
    mem_put(L + 8 + n * 4, w);
    mem_put(L, n + 1);
}

/* Wahrheitswert eines Wertes */
int is_true(int v) {
    if (v_type(v) == T_INT)  return v_num(v) != 0;
    if (v_type(v) == T_STR)  return strlen(str_of(v)) > 0;
    if (v_type(v) == T_LIST) return list_len(v) > 0;
    return 0;
}

/* --- Fehler -------------------------------------------------------------- */

void py_error(char* text) {
    if (fehler) return;
    fehler = 1;
    nl();
    printc("Traceback (most recent call last):", RED);
    nl();
    print("  line ");
    printn(akt_zeile + 1);
    nl();
    printc(text, RED);
    nl();
}

/* --- Wert ausgeben ------------------------------------------------------- */

void print_value(int v) {
    int i;
    if (v_type(v) == T_INT) { printn(v_num(v)); return; }
    if (v_type(v) == T_STR) { print(str_of(v)); return; }
    putch('[');
    for (i = 0; i < list_len(v); i++) {
        if (i > 0) print(", ");
        print_value(list_get(v, i));
    }
    putch(']');
}

/* ==========================================================================
   Tokenizer -- zerlegt eine Zeile in Bausteine
   ========================================================================== */

void tokenize(int addr) {
    char* s;
    int i; int n; int j; int c;
    s = (char*)addr;
    i = 0;
    tok_count = 0;

    while (s[i] && s[i] != 10) {
        if (s[i] == ' ' || s[i] == 9 || s[i] == 13) { i++; continue; }
        if (s[i] == '#') break;                       /* Kommentar */
        if (tok_count >= MAXTOK - 1) break;

        if (isdigit(s[i])) {
            n = 0;
            while (isdigit(s[i])) { n = n * 10 + (s[i] - '0'); i++; }
            tok_kind[tok_count] = K_NUM;
            tok_num[tok_count] = n;
            tok_count++;
            continue;
        }

        if (isalpha(s[i])) {
            j = 0;
            while (isalnum(s[i])) {
                if (j < NAMELEN - 1) { tok_text(tok_count)[j] = s[i]; j++; }
                i++;
            }
            tok_text(tok_count)[j] = 0;
            tok_kind[tok_count] = K_NAME;
            tok_count++;
            continue;
        }

        if (s[i] == 34 || s[i] == 39) {               /* "Text" oder 'Text' */
            c = s[i];
            i++;
            j = 0;
            tok_num[tok_count] = alloc(200);
            while (s[i] && s[i] != c) {
                if (s[i] == 92) {                     /* \n \t \\ */
                    i++;
                    if (s[i] == 'n') byte_put(tok_num[tok_count] + j, 10);
                    else if (s[i] == 't') byte_put(tok_num[tok_count] + j, 9);
                    else byte_put(tok_num[tok_count] + j, s[i]);
                } else {
                    byte_put(tok_num[tok_count] + j, s[i]);
                }
                j++;
                i++;
            }
            if (s[i] == c) i++;
            byte_put(tok_num[tok_count] + j, 0);
            tok_kind[tok_count] = K_STR;
            tok_count++;
            continue;
        }

        /* Operatoren, auch zweistellige */
        j = 0;
        tok_text(tok_count)[0] = s[i];
        tok_text(tok_count)[1] = 0;
        if (s[i + 1] == '=') {
            if (s[i] == '=' || s[i] == '!' || s[i] == '<' || s[i] == '>' ||
                s[i] == '+' || s[i] == '-' || s[i] == '*' || s[i] == '/') {
                tok_text(tok_count)[1] = '=';
                tok_text(tok_count)[2] = 0;
                i++;
            }
        }
        i++;
        tok_kind[tok_count] = K_OP;
        tok_count++;
    }
    tok_kind[tok_count] = K_END;
    tok_text(tok_count)[0] = 0;
}

int tok_is(int i, char* s) {
    if (tok_kind[i] == K_END) return 0;
    return strcmp(tok_text(i), s) == 0;
}

int at_op(char* s)   { return tok_kind[tp] == K_OP && tok_is(tp, s); }
int at_name(char* s) { return tok_kind[tp] == K_NAME && tok_is(tp, s); }

int accept_op(char* s) {
    if (at_op(s)) { tp++; return 1; }
    return 0;
}

/* ==========================================================================
   Variablen
   ========================================================================== */

int find_var(char* name) {
    int i;
    for (i = var_count - 1; i >= 0; i--) {          /* lokale zuerst */
        if (i < scope_base && scope_base > 0) {
            /* globale Variablen bleiben sichtbar */
        }
        if (strcmp(var_at(i), name) == 0) return i;
    }
    return 0 - 1;
}

void set_var(char* name, int wert) {
    int i;
    i = find_var(name);
    if (i >= 0) { var_val[i] = wert; return; }
    if (var_count >= MAXVARS) { py_error("too many variables"); return; }
    strncpy(var_at(var_count), name, NAMELEN);
    var_val[var_count] = wert;
    var_count++;
}

int get_var(char* name) {
    int i;
    i = find_var(name);
    if (i < 0) {
        py_error("NameError: name is not defined");
        return make_int(0);
    }
    return var_val[i];
}

int find_fn(char* name) {
    int i;
    for (i = 0; i < fn_count; i++)
        if (strcmp(fn_at(i), name) == 0) return i;
    return 0 - 1;
}

/* ==========================================================================
   Ausdruecke  (nach Vorrang gestaffelt, wie in Python)
   ========================================================================== */

int expr_or();
int exec_block(int von, int bis, int einr);

int call_function(int f, int a1, int a2, int a3, int a4, int n);

/* Klammerausdruck oder Grundwert */
int expr_atom() {
    int v; int w; int i; int n; int a1; int a2; int a3; int a4;
    char name[NAMELEN];

    if (tok_kind[tp] == K_NUM) { v = make_int(tok_num[tp]); tp++; return v; }

    if (tok_kind[tp] == K_STR) {
        v = value_new(T_STR, tok_num[tp]);
        tp++;
        return v;
    }

    if (accept_op("(")) {
        v = expr_or();
        accept_op(")");
        return v;
    }

    if (accept_op("[")) {                            /* Listenliteral */
        v = make_list(8);
        if (!at_op("]")) {
            while (1) {
                list_add(v, expr_or());
                if (!accept_op(",")) break;
            }
        }
        accept_op("]");
        return v;
    }

    if (accept_op("-")) {
        v = expr_atom();
        return make_int(0 - v_num(v));
    }

    if (tok_kind[tp] == K_NAME) {
        strncpy(name, tok_text(tp), NAMELEN);
        tp++;

        /* die drei eingebauten Werte */
        if (strcmp(name, "True") == 0)  return make_int(1);
        if (strcmp(name, "False") == 0) return make_int(0);
        if (strcmp(name, "None") == 0)  return make_int(0);

        if (at_op("(")) {                            /* Funktionsaufruf */
            tp++;
            n = 0;
            a1 = 0; a2 = 0; a3 = 0; a4 = 0;
            if (!at_op(")")) {
                while (1) {
                    w = expr_or();
                    if (n == 0) a1 = w;
                    if (n == 1) a2 = w;
                    if (n == 2) a3 = w;
                    if (n == 3) a4 = w;
                    n++;
                    if (!accept_op(",")) break;
                }
            }
            accept_op(")");

            /* eingebaute Funktionen */
            if (strcmp(name, "len") == 0) {
                if (v_type(a1) == T_STR) return make_int(strlen(str_of(a1)));
                if (v_type(a1) == T_LIST) return make_int(list_len(a1));
                return make_int(0);
            }
            if (strcmp(name, "str") == 0) {
                char buf[16];
                if (v_type(a1) == T_STR) return a1;
                itoa(v_num(a1), buf);
                return make_str(buf);
            }
            if (strcmp(name, "int") == 0) {
                if (v_type(a1) == T_STR) return make_int(atoi(str_of(a1)));
                return a1;
            }
            if (strcmp(name, "abs") == 0) {
                i = v_num(a1);
                if (i < 0) i = 0 - i;
                return make_int(i);
            }
            if (strcmp(name, "min") == 0) {
                if (v_num(a1) < v_num(a2)) return a1;
                return a2;
            }
            if (strcmp(name, "max") == 0) {
                if (v_num(a1) > v_num(a2)) return a1;
                return a2;
            }
            if (strcmp(name, "chr") == 0) {
                char buf[4];
                buf[0] = v_num(a1);
                buf[1] = 0;
                return make_str(buf);
            }
            if (strcmp(name, "ord") == 0) {
                return make_int(str_of(a1)[0]);
            }
            if (strcmp(name, "input") == 0) {
                char eingabe[128];
                if (n > 0) print(str_of(a1));
                i = 0;
                while (1) {
                    w = getkey();
                    if (keycode(w) == K_ENTER) break;
                    if (keycode(w) == 14) {              /* Backspace */
                        if (i > 0) { i--; putch(8); }
                        continue;
                    }
                    if (keychar(w) >= 32 && i < 126) {
                        eingabe[i] = keychar(w);
                        putch(eingabe[i]);
                        i++;
                    }
                }
                eingabe[i] = 0;
                nl();
                return make_str(eingabe);
            }
            if (strcmp(name, "sleep") == 0) { sleep(v_num(a1)); return make_int(0); }
            if (strcmp(name, "cls") == 0)   { cls(); return make_int(0); }
            if (strcmp(name, "beep") == 0)  { beep(v_num(a1), 8); return make_int(0); }
            if (strcmp(name, "ticks") == 0) { return make_int(ticks()); }
            if (strcmp(name, "list") == 0)  { return make_list(v_num(a1) + 4); }

            i = find_fn(name);
            if (i < 0) { py_error("NameError: function is not defined"); return make_int(0); }
            return call_function(i, a1, a2, a3, a4, n);
        }

        v = get_var(name);

        while (1) {
            if (at_op("[")) {                        /* Index */
                tp++;
                w = expr_or();
                accept_op("]");
                i = v_num(w);
                if (v_type(v) == T_LIST) {
                    if (i < 0 || i >= list_len(v)) {
                        py_error("IndexError: list index out of range");
                        return make_int(0);
                    }
                    v = list_get(v, i);
                } else if (v_type(v) == T_STR) {
                    char buf[4];
                    if (i < 0 || i >= strlen(str_of(v))) {
                        py_error("IndexError: string index out of range");
                        return make_int(0);
                    }
                    buf[0] = str_of(v)[i];
                    buf[1] = 0;
                    v = make_str(buf);
                }
                continue;
            }
            if (at_op(".")) {                        /* nur .append( */
                tp++;
                if (at_name("append")) {
                    tp++;
                    accept_op("(");
                    w = expr_or();
                    accept_op(")");
                    list_add(v, w);
                    return make_int(0);
                }
                py_error("unknown attribute");
                return make_int(0);
            }
            break;
        }
        return v;
    }

    py_error("SyntaxError: invalid expression");
    tp++;
    return make_int(0);
}

int expr_mul() {
    int a; int b; int r;
    a = expr_atom();
    while (at_op("*") || at_op("/") || at_op("%")) {
        if (accept_op("*")) {
            b = expr_atom();
            if (v_type(a) == T_STR) {                /* "ab" * 3 */
                char buf[128];
                int k; int j;
                buf[0] = 0;
                for (k = 0; k < v_num(b); k++) {
                    if (strlen(buf) + strlen(str_of(a)) < 120) strcat(buf, str_of(a));
                }
                a = make_str(buf);
            } else {
                a = make_int(v_num(a) * v_num(b));
            }
        } else if (accept_op("/")) {
            b = expr_atom();
            if (v_num(b) == 0) { py_error("ZeroDivisionError: division by zero"); return make_int(0); }
            a = make_int(v_num(a) / v_num(b));
        } else {
            accept_op("%");
            b = expr_atom();
            if (v_num(b) == 0) { py_error("ZeroDivisionError: modulo by zero"); return make_int(0); }
            a = make_int(v_num(a) % v_num(b));
        }
    }
    return a;
}

int expr_add() {
    int a; int b;
    a = expr_mul();
    while (at_op("+") || at_op("-")) {
        if (accept_op("+")) {
            b = expr_mul();
            if (v_type(a) == T_STR || v_type(b) == T_STR) {
                char buf[200];
                char zahl[16];
                buf[0] = 0;
                if (v_type(a) == T_STR) strcpy(buf, str_of(a));
                else { itoa(v_num(a), zahl); strcpy(buf, zahl); }
                if (v_type(b) == T_STR) strcat(buf, str_of(b));
                else { itoa(v_num(b), zahl); strcat(buf, zahl); }
                a = make_str(buf);
            } else if (v_type(a) == T_LIST && v_type(b) == T_LIST) {
                int neu; int i;
                neu = make_list(list_len(a) + list_len(b) + 4);
                for (i = 0; i < list_len(a); i++) list_add(neu, list_get(a, i));
                for (i = 0; i < list_len(b); i++) list_add(neu, list_get(b, i));
                a = neu;
            } else {
                a = make_int(v_num(a) + v_num(b));
            }
        } else {
            accept_op("-");
            b = expr_mul();
            a = make_int(v_num(a) - v_num(b));
        }
    }
    return a;
}

int vergleich(int a, int b, char* op) {
    int r;
    if (v_type(a) == T_STR && v_type(b) == T_STR) {
        r = strcmp(str_of(a), str_of(b));
    } else {
        r = v_num(a) - v_num(b);
    }
    if (strcmp(op, "==") == 0) return r == 0;
    if (strcmp(op, "!=") == 0) return r != 0;
    if (strcmp(op, "<") == 0)  return r < 0;
    if (strcmp(op, ">") == 0)  return r > 0;
    if (strcmp(op, "<=") == 0) return r <= 0;
    if (strcmp(op, ">=") == 0) return r >= 0;
    return 0;
}

int expr_cmp() {
    int a; int b;
    char op[4];
    a = expr_add();
    while (at_op("==") || at_op("!=") || at_op("<") || at_op(">") ||
           at_op("<=") || at_op(">=")) {
        strncpy(op, tok_text(tp), 4);
        tp++;
        b = expr_add();
        a = make_int(vergleich(a, b, op));
    }
    return a;
}

int expr_not() {
    int v;
    if (at_name("not")) {
        tp++;
        v = expr_not();
        return make_int(!is_true(v));
    }
    return expr_cmp();
}

int expr_and() {
    int a; int b;
    a = expr_not();
    while (at_name("and")) {
        tp++;
        b = expr_not();
        a = make_int(is_true(a) && is_true(b));
    }
    return a;
}

int expr_or() {
    int a; int b;
    a = expr_and();
    while (at_name("or")) {
        tp++;
        b = expr_and();
        a = make_int(is_true(a) || is_true(b));
    }
    return a;
}

/* ==========================================================================
   Anweisungen
   ========================================================================== */

/* Findet das Ende eines eingerueckten Blocks ab Zeile <von> */
int block_end(int von, int einr) {
    int i;
    i = von;
    while (i < line_count) {
        if (line_ind[i] <= einr && line_ind[i] >= 0) return i;
        i++;
    }
    return line_count;
}

int call_function(int f, int a1, int a2, int a3, int a4, int n) {
    int alt_base; int alt_count; int i; int ende; int ergebnis; int alt_zeile;

    alt_base = scope_base;
    alt_count = var_count;
    scope_base = var_count;

    if (n > fn_nparam[f]) n = fn_nparam[f];
    for (i = 0; i < n; i++) {
        if (i == 0) set_var(param_at(f, 0), a1);
        if (i == 1) set_var(param_at(f, 1), a2);
        if (i == 2) set_var(param_at(f, 2), a3);
        if (i == 3) set_var(param_at(f, 3), a4);
    }

    alt_zeile = akt_zeile;
    ende = block_end(fn_line[f] + 1, line_ind[fn_line[f]]);
    ret_val = make_int(0);
    exec_block(fn_line[f] + 1, ende, line_ind[fn_line[f]]);
    if (flow == 3) flow = 0;
    ergebnis = ret_val;
    akt_zeile = alt_zeile;

    var_count = alt_count;                          /* lokale Variablen weg */
    scope_base = alt_base;
    return ergebnis;
}

/* Eine einzelne Anweisung ausfuehren (Token liegen schon bereit) */
void exec_stmt(int zeile) {
    int v; int i; int idx;
    char name[NAMELEN];

    if (tok_kind[0] == K_END) return;

    /* print(...) */
    if (tok_kind[tp] == K_NAME && tok_is(tp, "print")) {
        tp++;
        accept_op("(");
        if (at_op(")")) { nl(); return; }
        while (1) {
            v = expr_or();
            if (fehler) return;
            print_value(v);
            if (!accept_op(",")) break;
            putch(' ');
        }
        accept_op(")");
        nl();
        return;
    }

    if (at_name("return")) {
        tp++;
        if (tok_kind[tp] == K_END) ret_val = make_int(0);
        else ret_val = expr_or();
        flow = 3;
        return;
    }
    if (at_name("break"))    { flow = 1; return; }
    if (at_name("continue")) { flow = 2; return; }
    if (at_name("pass"))     { return; }

    /* Zuweisung an eine Variable oder ein Listenelement */
    if (tok_kind[tp] == K_NAME) {
        strncpy(name, tok_text(tp), NAMELEN);

        if (tok_kind[tp + 1] == K_OP && tok_is(tp + 1, "=")) {
            tp = tp + 2;
            v = expr_or();
            if (!fehler) set_var(name, v);
            return;
        }
        if (tok_kind[tp + 1] == K_OP &&
            (tok_is(tp + 1, "+=") || tok_is(tp + 1, "-=") ||
             tok_is(tp + 1, "*=") || tok_is(tp + 1, "/="))) {
            int alt; int neu;
            char op[4];
            strncpy(op, tok_text(tp + 1), 4);
            tp = tp + 2;
            neu = expr_or();
            alt = get_var(name);
            if (fehler) return;
            if (op[0] == '+') {
                if (v_type(alt) == T_STR || v_type(neu) == T_STR) {
                    char buf[200];
                    char zahl[16];
                    buf[0] = 0;
                    if (v_type(alt) == T_STR) strcpy(buf, str_of(alt));
                    else { itoa(v_num(alt), zahl); strcpy(buf, zahl); }
                    if (v_type(neu) == T_STR) strcat(buf, str_of(neu));
                    else { itoa(v_num(neu), zahl); strcat(buf, zahl); }
                    set_var(name, make_str(buf));
                    return;
                }
                set_var(name, make_int(v_num(alt) + v_num(neu)));
            }
            if (op[0] == '-') set_var(name, make_int(v_num(alt) - v_num(neu)));
            if (op[0] == '*') set_var(name, make_int(v_num(alt) * v_num(neu)));
            if (op[0] == '/') {
                if (v_num(neu) == 0) { py_error("ZeroDivisionError: division by zero"); return; }
                set_var(name, make_int(v_num(alt) / v_num(neu)));
            }
            return;
        }
        /* Zuweisung an Listenelement:  a[i] = wert */
        if (tok_kind[tp + 1] == K_OP && tok_is(tp + 1, "[")) {
            int liste; int index; int merke;
            merke = tp;
            liste = get_var(name);
            if (fehler) return;
            if (v_type(liste) == T_LIST) {
                tp = tp + 2;
                index = expr_or();
                accept_op("]");
                if (at_op("=")) {
                    tp++;
                    v = expr_or();
                    i = v_num(index);
                    if (i < 0 || i >= list_len(liste)) {
                        py_error("IndexError: list assignment out of range");
                        return;
                    }
                    list_set(liste, i, v);
                    return;
                }
            }
            tp = merke;
        }
    }

    /* sonst: einfach auswerten (z.B. ein Funktionsaufruf) */
    expr_or();
}

/* ==========================================================================
   Bloecke ausfuehren
   ========================================================================== */

int exec_block(int von, int bis, int einr) {
    int i; int ende; int bedingung; int start; int stop; int schritt;
    int wert; int j; int liste; int genommen;
    char varname[NAMELEN];

    i = von;
    while (i < bis) {
        if (fehler) return 0;
        if (flow) return 0;

        akt_zeile = i;
        tokenize(line_pos[i]);
        tp = 0;

        if (tok_kind[0] == K_END) { i++; continue; }

        /* --- def --- */
        if (at_name("def")) {
            i = block_end(i + 1, line_ind[i]);
            continue;
        }

        /* --- if / elif / else --- */
        if (at_name("if")) {
            tp++;
            wert = expr_or();
            genommen = is_true(wert);
            ende = block_end(i + 1, line_ind[i]);
            if (genommen) exec_block(i + 1, ende, line_ind[i]);
            i = ende;

            /* elif- und else-Zweige derselben Ebene abarbeiten */
            while (i < bis && !fehler && !flow) {
                akt_zeile = i;
                tokenize(line_pos[i]);
                tp = 0;
                if (at_name("elif") && line_ind[i] == line_ind[akt_zeile]) {
                    tp++;
                    ende = block_end(i + 1, line_ind[i]);
                    if (!genommen) {
                        wert = expr_or();
                        if (is_true(wert)) {
                            genommen = 1;
                            exec_block(i + 1, ende, line_ind[i]);
                        }
                    }
                    i = ende;
                    continue;
                }
                if (at_name("else")) {
                    ende = block_end(i + 1, line_ind[i]);
                    if (!genommen) exec_block(i + 1, ende, line_ind[i]);
                    i = ende;
                }
                break;
            }
            continue;
        }

        /* --- while --- */
        if (at_name("while")) {
            ende = block_end(i + 1, line_ind[i]);
            while (1) {
                akt_zeile = i;
                tokenize(line_pos[i]);
                tp = 1;
                wert = expr_or();
                if (fehler) return 0;
                if (!is_true(wert)) break;
                exec_block(i + 1, ende, line_ind[i]);
                if (fehler) return 0;
                if (flow == 1) { flow = 0; break; }
                if (flow == 2) flow = 0;
                if (flow == 3) return 0;
            }
            i = ende;
            continue;
        }

        /* --- for x in range(...) / for x in liste --- */
        if (at_name("for")) {
            tp++;
            strncpy(varname, tok_text(tp), NAMELEN);
            tp++;
            if (!at_name("in")) { py_error("SyntaxError: expected 'in'"); return 0; }
            tp++;
            ende = block_end(i + 1, line_ind[i]);

            if (at_name("range")) {
                tp++;
                accept_op("(");
                start = 0;
                schritt = 1;
                wert = expr_or();
                if (accept_op(",")) {
                    start = v_num(wert);
                    wert = expr_or();
                    stop = v_num(wert);
                    if (accept_op(",")) {
                        wert = expr_or();
                        schritt = v_num(wert);
                    }
                } else {
                    stop = v_num(wert);
                }
                accept_op(")");
                j = start;
                while (1) {
                    if (schritt > 0 && j >= stop) break;
                    if (schritt < 0 && j <= stop) break;
                    set_var(varname, make_int(j));
                    exec_block(i + 1, ende, line_ind[i]);
                    if (fehler) return 0;
                    if (flow == 1) { flow = 0; break; }
                    if (flow == 2) flow = 0;
                    if (flow == 3) return 0;
                    j = j + schritt;
                }
            } else {
                liste = expr_or();
                if (v_type(liste) == T_LIST) {
                    for (j = 0; j < list_len(liste); j++) {
                        set_var(varname, list_get(liste, j));
                        exec_block(i + 1, ende, line_ind[i]);
                        if (fehler) return 0;
                        if (flow == 1) { flow = 0; break; }
                        if (flow == 2) flow = 0;
                        if (flow == 3) return 0;
                    }
                } else if (v_type(liste) == T_STR) {
                    char buf[4];
                    for (j = 0; j < strlen(str_of(liste)); j++) {
                        buf[0] = str_of(liste)[j];
                        buf[1] = 0;
                        set_var(varname, make_str(buf));
                        exec_block(i + 1, ende, line_ind[i]);
                        if (fehler) return 0;
                        if (flow == 1) { flow = 0; break; }
                        if (flow == 2) flow = 0;
                        if (flow == 3) return 0;
                    }
                }
            }
            i = ende;
            continue;
        }

        exec_stmt(i);
        i++;
    }
    return 0;
}

/* ==========================================================================
   Vorbereitung: Zeilen einlesen, Einrueckung messen, Funktionen finden
   ========================================================================== */

void prepare(int addr, int len) {
    int i; int j; int spalten; int f;
    char* s;
    s = (char*)addr;
    line_count = 0;
    fn_count = 0;

    i = 0;
    while (i < len && line_count < MAXLINES) {
        spalten = 0;
        while (i < len && (s[i] == ' ' || s[i] == 9)) {
            if (s[i] == 9) spalten = spalten + 4;
            else spalten++;
            i++;
        }
        line_pos[line_count] = addr + i;
        line_ind[line_count] = spalten;
        /* leere Zeilen und reine Kommentare bekommen eine sehr grosse
           Einrueckung, damit sie Bloecke nicht vorzeitig beenden */
        if (s[i] == 10 || s[i] == 0 || s[i] == '#') line_ind[line_count] = 9999;
        line_count++;
        while (i < len && s[i] != 10) i++;
        i++;
    }

    /* Funktionsdefinitionen einsammeln */
    for (i = 0; i < line_count; i++) {
        tokenize(line_pos[i]);
        if (tok_kind[0] == K_NAME && strcmp(tok_text(0), "def") == 0) {
            if (fn_count >= MAXFUNCS) break;
            f = fn_count;
            strncpy(fn_at(f), tok_text(1), NAMELEN);
            fn_line[f] = i;
            fn_nparam[f] = 0;
            j = 3;                                   /* def name ( p1 , p2 ) : */
            while (tok_kind[j] == K_NAME && fn_nparam[f] < 4) {
                strncpy(param_at(f, fn_nparam[f]), tok_text(j), NAMELEN);
                fn_nparam[f]++;
                j = j + 2;                           /* Komma ueberspringen */
            }
            fn_count++;
        }
    }
}

/* ========================================================================== */

int main() {
    int n; int i;
    char datei[24];
    char* args;

    args = (char*)0x00008200;
    i = 0;
    while (args[i] == ' ') i++;
    n = 0;
    while (args[i] && args[i] != ' ') { datei[n] = args[i]; n++; i++; }
    datei[n] = 0;

    if (datei[0] == 0) {
        print("\nTOOBAD Python 1.0  for TB-32\n\n");
        print("Usage:  PY <file.PY>\n\n");
        print("A Python interpreter written for this machine.\n");
        print("Supported: variables, strings, lists, if/elif/else, while,\n");
        print("for ... in range(), def/return, and the usual operators.\n");
        print("Built-ins: print input len str int abs min max chr ord\n");
        print("           range sleep cls beep ticks list\n");
        return 1;
    }

    n = fileread(datei, SRC_BUF, SRC_MAX);
    if (n < 0) {
        printc("\nCannot open file: ", RED);
        print(datei);
        nl();
        return 1;
    }
    byte_put(SRC_BUF + n, 0);

    heap_ptr = HEAP_BASE;
    var_count = 0;
    scope_base = 0;
    fehler = 0;
    flow = 0;
    akt_zeile = 0;

    prepare(SRC_BUF, n);
    exec_block(0, line_count, 0 - 1);

    if (fehler) return 1;
    return 0;
}
