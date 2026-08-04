/* ===========================================================================
   Der Browser -- der Teil, der die Seite holt und lesbar macht.

   Das Zeichnen steht in gui.c, hier steht das Verstehen: eine Adresse
   zerlegen, die Seite ueber TCP holen, den HTML-Text in Zeilen verwandeln.

   HTML ist Text mit Marken darin: <h1>Ueberschrift</h1>. Wir tun das
   Einfachste, was schon nuetzlich ist -- Marken herausnehmen, ein paar davon
   ernst nehmen (Ueberschriften, Absaetze, Aufzaehlungen, Verweise) und den
   Rest an der Fensterbreite umbrechen. Kein CSS, kein JavaScript. Und kein
   HTTPS: das ist Verschluesselung mit Zertifikaten und waere ein eigenes
   Projekt. Dafuer ist spaeter ein Vermittler auf dem Pi vorgesehen.
   =========================================================================== */

#define BR_ROH      0x00180000       /* die Antwort, wie sie ankommt */
#define BR_ROHMAX   65536
#define BR_TEXT     0x00190000       /* daraus gemachte Zeilen */
#define BR_ZEILEMAX 100              /* Zeichen je Zeile */
#define BR_ZEILEN   400              /* so viele Zeilen merken wir uns */
#define BR_LINKS    0x00196000       /* Ziele der Verweise, je 160 Byte */
#define BR_LINKMAX  32
/* Platz zum Zwischenlegen: beim Umbrechen wandert der Rest der Zeile kurz
   hierher, und die HTTP-Anfrage wird hier gebaut. Vorher lag beides bei
   BR_ROH + BR_ROHMAX -- das ist aber genau BR_TEXT, also die erste Zeile der
   Seite. Deshalb stand oben im Fenster ein Wortfetzen aus der Mitte des
   Textes. */
#define BR_KRATZ    0x00198000

int  br_anzahl = 0;                  /* wie viele Zeilen die Seite hat */
int  br_top = 0;                     /* erste sichtbare Zeile */
int  br_stil[400];                   /* 0 = Text, 1 = Ueberschrift */
int  br_link[400];                   /* Verweis dieser Zeile, -1 = keiner */
int  br_linkanzahl = 0;
int  br_laeuft = 0;                  /* wird gerade geholt? */
int  br_breite = 70;                 /* Zeichen je Zeile, setzt das Fenster */
char br_url[160];
char br_wirt[80];
char br_pfad[160];
int  br_port = 80;
char br_status[80];
char br_titel[64];
char br_zurueck[160];                /* die Adresse davor */

int  br_zeile_addr(int n) { return BR_TEXT + n * BR_ZEILEMAX; }
int  br_link_addr(int n)  { return BR_LINKS + n * 160; }

void br_setz(char* ziel, char* quelle, int max) {
    int i;
    i = 0;
    while (quelle[i] != 0 && i < max - 1) { ziel[i] = quelle[i]; i++; }
    ziel[i] = 0;
}

/* --- Adresse zerlegen -----------------------------------------------------
   "http://example.com:8080/pfad" wird zu Wirt, Port und Pfad. Ohne "http://"
   davor geht es auch -- das tippt niemand gern. */
void br_url_zerlegen(char* url) {
    int i; int n; int p;
    i = 0;
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p') {
        i = 4;
        if (url[i] == 's') i++;              /* https:// -- geht nicht, gleich */
        if (url[i] == ':') i++;
        while (url[i] == '/') i++;
    }
    n = 0;
    br_port = 80;
    while (url[i] != 0 && url[i] != '/' && url[i] != ':' && n < 78) {
        br_wirt[n] = url[i];
        n++;
        i++;
    }
    br_wirt[n] = 0;
    if (url[i] == ':') {
        i++;
        p = 0;
        while (url[i] >= '0' && url[i] <= '9') {
            p = p * 10 + (url[i] - '0');
            i++;
        }
        if (p > 0 && p < 65536) br_port = p;
    }
    n = 0;
    if (url[i] != '/') { br_pfad[0] = '/'; n = 1; }
    while (url[i] != 0 && n < 158) { br_pfad[n] = url[i]; n++; i++; }
    br_pfad[n] = 0;
}

/* --- HTML in Zeilen verwandeln --------------------------------------------
   Ein Durchgang von vorn nach hinten. Ausserhalb einer Marke ist alles Text;
   sobald ein '<' kommt, wird der Name der Marke gelesen und entschieden, was
   sie bedeutet. Mehrere Leerzeichen hintereinander werden zu einem -- HTML
   zaehlt sie nicht, und ohne das saehe jede Seite zerrupft aus. */

int br_zeile_n = 0;                  /* Laenge der Zeile, an der wir bauen */
int br_letztes_leer = 1;
/* Welcher Verweis in der Zeile steht, an der wir gerade bauen.
   Das muss GEMERKT werden: eine Zeile endet meist erst beim naechsten
   Absatz, und da ist </a> laengst vorbei -- der Verweis waere dann wieder
   -1, und keine Zeile haette je einen. Genau das war der erste Versuch. */
int br_zeilenlink = 0 - 1;

void br_zeile_ende(int stil, int link) {
    if (br_anzahl >= BR_ZEILEN) return;
    net_putb(br_zeile_addr(br_anzahl) + br_zeile_n, 0);
    br_stil[br_anzahl] = stil;
    br_link[br_anzahl] = br_zeilenlink >= 0 ? br_zeilenlink : link;
    br_zeilenlink = 0 - 1;
    br_anzahl = br_anzahl + 1;
    br_zeile_n = 0;
    br_letztes_leer = 1;
}

/* Ein Zeichen anhaengen und, wenn die Zeile voll ist, am letzten Leerzeichen
   umbrechen. Genau das nennt man Wortumbruch: nicht mitten im Wort trennen. */
void br_zeichen(int c, int stil, int link) {
    int i; int trenn; int rest; int j;
    if (br_anzahl >= BR_ZEILEN) return;
    if (br_zeile_n < br_breite) {
        net_putb(br_zeile_addr(br_anzahl) + br_zeile_n, c);
        br_zeile_n = br_zeile_n + 1;
        return;
    }
    trenn = 0 - 1;
    for (i = br_zeile_n - 1; i > 0; i--) {
        if (net_getb(br_zeile_addr(br_anzahl) + i) == ' ') { trenn = i; break; }
    }
    if (trenn < 0) {                          /* ein sehr langes Wort */
        br_zeile_ende(stil, link);
        net_putb(br_zeile_addr(br_anzahl) + 0, c);
        br_zeile_n = 1;
        return;
    }
    rest = br_zeile_n - trenn - 1;
    for (j = 0; j < rest; j++)
        net_putb(BR_KRATZ + j,
                 net_getb(br_zeile_addr(br_anzahl) + trenn + 1 + j));
    br_zeile_n = trenn;
    br_zeile_ende(stil, link);
    for (j = 0; j < rest; j++)
        net_putb(br_zeile_addr(br_anzahl) + j, net_getb(BR_KRATZ + j));
    br_zeile_n = rest;
    net_putb(br_zeile_addr(br_anzahl) + br_zeile_n, c);
    br_zeile_n = br_zeile_n + 1;
}

void br_wort(char* s, int stil, int link) {
    int i;
    for (i = 0; s[i] != 0; i++) br_zeichen(s[i], stil, link);
}

/* &amp; wird zu &, &lt; zu < und so weiter. Ohne das steht auf jeder Seite
   kaufmaennisches Und in Langschrift. */
int br_entitaet(int p, int ende) {
    int i; int c;
    char name[10];
    i = 0;
    while (p + i < ende && i < 8) {
        c = net_getb(p + i);
        if (c == ';') break;
        name[i] = c;
        i++;
    }
    name[i] = 0;
    if (strcmp(name, "amp") == 0)  return '&';
    if (strcmp(name, "lt") == 0)   return '<';
    if (strcmp(name, "gt") == 0)   return '>';
    if (strcmp(name, "quot") == 0) return '"';
    if (strcmp(name, "apos") == 0) return 39;
    if (strcmp(name, "nbsp") == 0) return ' ';
    return 0;                                  /* unbekannt: unveraendert */
}

void br_html(int quelle, int len) {
    int p; int ende; int c; int i; int n;
    int stil; int link; int inschrift; int marke;
    char name[24];
    char* z;

    br_anzahl = 0;
    br_zeile_n = 0;
    br_zeilenlink = 0 - 1;
    br_linkanzahl = 0;
    br_letztes_leer = 1;
    stil = 0;
    link = 0 - 1;
    inschrift = 0;
    br_titel[0] = 0;
    p = quelle;
    ende = quelle + len;

    while (p < ende) {
        c = net_getb(p);

        if (c == '<') {
            /* Den Namen der Marke lesen. "/p" zaehlt als "p" mit Schraegstrich. */
            n = 0;
            marke = p;
            p++;
            if (p < ende && net_getb(p) == '/') { p++; n = 0; name[0] = '/'; n = 1; }
            while (p < ende && n < 22) {
                c = net_getb(p);
                if (c == ' ' || c == '>' || c == 10 || c == 13) break;
                if (c >= 'A' && c <= 'Z') c = c + 32;
                name[n] = c;
                n++;
                p++;
            }
            name[n] = 0;

            /* Bei <a href="..."> das Ziel einsammeln, solange wir noch in
               der Marke stehen. */
            if (strcmp(name, "a") == 0 && br_linkanzahl < BR_LINKMAX) {
                i = p;
                while (i < ende && net_getb(i) != '>') {
                    if (net_getb(i) == 'h' && net_getb(i + 1) == 'r'
                        && net_getb(i + 2) == 'e' && net_getb(i + 3) == 'f') {
                        i = i + 4;
                        while (i < ende && (net_getb(i) == ' '
                                            || net_getb(i) == '=')) i++;
                        if (net_getb(i) == '"' || net_getb(i) == 39) i++;
                        n = 0;
                        while (i < ende && n < 158) {
                            c = net_getb(i);
                            if (c == '"' || c == 39 || c == '>' || c == ' ') break;
                            net_putb(br_link_addr(br_linkanzahl) + n, c);
                            n++;
                            i++;
                        }
                        net_putb(br_link_addr(br_linkanzahl) + n, 0);
                        link = br_linkanzahl;
                        if (br_zeilenlink < 0) br_zeilenlink = br_linkanzahl;
                        br_linkanzahl = br_linkanzahl + 1;
                        break;
                    }
                    i++;
                }
            }

            while (p < ende && net_getb(p) != '>') p++;
            p++;

            if (strcmp(name, "script") == 0 || strcmp(name, "style") == 0) {
                inschrift = 1;
                continue;
            }
            if (strcmp(name, "/script") == 0 || strcmp(name, "/style") == 0) {
                inschrift = 0;
                continue;
            }
            if (strcmp(name, "title") == 0) {
                n = 0;
                while (p < ende && net_getb(p) != '<' && n < 62) {
                    br_titel[n] = net_getb(p);
                    n++;
                    p++;
                }
                br_titel[n] = 0;
                continue;
            }
            if (strcmp(name, "/a") == 0) { link = 0 - 1; continue; }
            if (strcmp(name, "br") == 0) { br_zeile_ende(stil, link); continue; }
            if (strcmp(name, "li") == 0) {
                br_zeile_ende(stil, link);
                br_wort("  * ", 0, link);
                continue;
            }
            if (strcmp(name, "h1") == 0 || strcmp(name, "h2") == 0
                || strcmp(name, "h3") == 0) {
                if (br_zeile_n > 0) br_zeile_ende(stil, link);
                br_zeile_ende(0, 0 - 1);          /* eine Zeile Luft */
                stil = 1;
                continue;
            }
            if (strcmp(name, "/h1") == 0 || strcmp(name, "/h2") == 0
                || strcmp(name, "/h3") == 0) {
                br_zeile_ende(1, link);
                stil = 0;
                continue;
            }
            if (strcmp(name, "p") == 0 || strcmp(name, "/p") == 0
                || strcmp(name, "div") == 0 || strcmp(name, "/div") == 0
                || strcmp(name, "/li") == 0 || strcmp(name, "tr") == 0
                || strcmp(name, "/tr") == 0 || strcmp(name, "/ul") == 0
                || strcmp(name, "/table") == 0 || strcmp(name, "hr") == 0) {
                if (br_zeile_n > 0) br_zeile_ende(stil, link);
                continue;
            }
            continue;                            /* alle anderen Marken weg */
        }

        p++;
        if (inschrift) continue;

        if (c == '&') {
            i = br_entitaet(p, ende);
            if (i > 0) {
                br_zeichen(i, stil, link);
                while (p < ende && net_getb(p) != ';' && net_getb(p) != ' ') p++;
                if (p < ende && net_getb(p) == ';') p++;
                br_letztes_leer = 0;
                continue;
            }
        }

        if (c == 10 || c == 13 || c == 9) c = ' ';
        if (c == ' ') {
            if (br_letztes_leer) continue;       /* mehrere zu einem */
            br_letztes_leer = 1;
            if (br_zeile_n > 0) br_zeichen(' ', stil, link);
            continue;
        }
        if (c < 32 || c > 126) continue;
        br_letztes_leer = 0;
        br_zeichen(c, stil, link);
    }
    if (br_zeile_n > 0) br_zeile_ende(stil, link);
    z = br_status;
}

/* --- Holen ----------------------------------------------------------------
   Die Kette aus den Stufen davor: Namen nachschlagen, Verbindung aufbauen,
   Anfrage schicken, Antwort lesen. Neu ist nur, was danach passiert. */
int br_bau_anfrage() {
    int n;
    n = str_nach(BR_KRATZ + 512, "GET ");
    n = n + str_nach(BR_KRATZ + 512 + n, br_pfad);
    n = n + str_nach(BR_KRATZ + 512 + n, " HTTP/1.0\r\nHost: ");
    n = n + str_nach(BR_KRATZ + 512 + n, br_wirt);
    n = n + str_nach(BR_KRATZ + 512 + n,
                     "\r\nUser-Agent: TOOBAD-OS/2.5.2\r\nConnection: close\r\n\r\n");
    return n;
}

/* Rueckgabe: 1 = Seite steht, 0 = nichts geworden (Grund in br_status). */
int br_holen(char* url) {
    int ip; int n; int gesamt; int i; int kopfende; int code;

    if (net_da() == 0) { br_setz(br_status, "No network card.", 78); return 0; }
    if (ip_meine == 0) { br_setz(br_status, "No address. Use NET IP.", 78); return 0; }

    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p'
        && url[4] == 's') {
        br_setz(br_status, "HTTPS is not supported -- try http://", 78);
        return 0;
    }

    br_url_zerlegen(url);
    if (br_wirt[0] == 0) { br_setz(br_status, "No address given.", 78); return 0; }

    br_setz(br_status, "Looking up the name ...", 78);
    ip = ip_lesen(br_wirt);
    if (ip == 0) ip = dns_aufloesen(br_wirt);
    if (ip == 0) { br_setz(br_status, "Unknown name.", 78); return 0; }

    br_setz(br_status, "Connecting ...", 78);
    if (tcp_verbinden(ip, br_port) == 0) {
        br_setz(br_status, "No connection -- is the router running?", 78);
        return 0;
    }

    n = br_bau_anfrage();
    tcp_schreiben(BR_KRATZ + 512, n);

    br_setz(br_status, "Loading ...", 78);
    gesamt = 0;
    while (gesamt < BR_ROHMAX) {
        n = tcp_lesen(BR_ROH + gesamt, BR_ROHMAX - gesamt, 500);
        if (n <= 0) break;
        gesamt = gesamt + n;
    }
    tcp_schliessen();
    if (gesamt == 0) { br_setz(br_status, "The server said nothing.", 78); return 0; }

    /* Vor der Seite steht der Kopf der Antwort, dann eine Leerzeile. */
    code = 0;
    if (gesamt > 12) code = (net_getb(BR_ROH + 9) - '0') * 100
                          + (net_getb(BR_ROH + 10) - '0') * 10
                          + (net_getb(BR_ROH + 11) - '0');
    kopfende = 0;
    for (i = 0; i + 3 < gesamt; i++) {
        if (net_getb(BR_ROH + i) == 10 && net_getb(BR_ROH + i + 1) == 10) {
            kopfende = i + 2;
            break;
        }
        if (net_getb(BR_ROH + i) == 13 && net_getb(BR_ROH + i + 1) == 10
            && net_getb(BR_ROH + i + 2) == 13 && net_getb(BR_ROH + i + 3) == 10) {
            kopfende = i + 4;
            break;
        }
    }

    br_html(BR_ROH + kopfende, gesamt - kopfende);
    br_top = 0;
    br_setz(br_zurueck, br_url, 158);
    br_setz(br_url, url, 158);
    if (code >= 400) br_setz(br_status, "The server reported an error.", 78);
    else if (code >= 300) br_setz(br_status, "The page has moved (redirect).", 78);
    else br_setz(br_status, br_titel[0] ? br_titel : "Done.", 78);
    return 1;
}

/* Einem Verweis folgen. Relative Ziele ("/impressum") werden mit dem
   aktuellen Wirt ergaenzt -- so steht es auf fast jeder Seite. */
int br_folgen(int nummer) {
    int i; int n;
    char ziel[176];
    if (nummer < 0 || nummer >= br_linkanzahl) return 0;
    n = 0;
    while (n < 158) {
        i = net_getb(br_link_addr(nummer) + n);
        if (i == 0) break;
        ziel[n] = i;
        n++;
    }
    ziel[n] = 0;
    if (ziel[0] == 0) return 0;
    if (ziel[0] == 'h' && ziel[1] == 't' && ziel[2] == 't' && ziel[3] == 'p')
        return br_holen(ziel);
    /* Relativ: der Wirt davor -- und der Port dazu, wenn es nicht der
       uebliche 80 ist. Ohne ihn landete jeder Verweis auf einer Seite, die
       auf einem anderen Port liegt, ins Leere. */
    n = 0;
    for (i = 0; br_wirt[i] != 0; i++) { br_pfad[n] = br_wirt[i]; n++; }
    if (br_port != 80) {
        br_pfad[n] = ':';
        n++;
        if (br_port >= 10000) { br_pfad[n] = '0' + (br_port / 10000) % 10; n++; }
        if (br_port >= 1000)  { br_pfad[n] = '0' + (br_port / 1000) % 10; n++; }
        if (br_port >= 100)   { br_pfad[n] = '0' + (br_port / 100) % 10; n++; }
        if (br_port >= 10)    { br_pfad[n] = '0' + (br_port / 10) % 10; n++; }
        br_pfad[n] = '0' + br_port % 10;
        n++;
    }
    if (ziel[0] != '/') { br_pfad[n] = '/'; n++; }
    for (i = 0; ziel[i] != 0 && n < 158; i++) { br_pfad[n] = ziel[i]; n++; }
    br_pfad[n] = 0;
    return br_holen(br_pfad);
}
