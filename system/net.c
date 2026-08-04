/* ===========================================================================
   Treiber fuer die Netzwerkkarte TB-NET.

   Die Karte kennt nur Rahmen. Sechs Byte Ziel, sechs Byte Absender, zwei
   Byte Art, dann die Nutzdaten -- genau wie bei Ethernet. Was in den
   Nutzdaten steht, ist ihr gleich; darum kuemmert sich, was hier drueber
   liegt. Dieser Treiber macht deshalb auch nicht mehr als: hinlegen und
   losschicken, abholen und hinlegen.

   Die Adresse kommt von der Karte selbst, nicht von uns. Beim Senden traegt
   sie den Absender ein -- man kann sich hier also nicht als jemand anderes
   ausgeben. Auf einer echten Karte geht das auch nicht ohne Weiteres.
   =========================================================================== */

#define P_NET_STATUS   0xC0
#define P_NET_ADDR     0xC1
#define P_NET_LEN      0xC2
#define P_NET_CMD      0xC3
#define P_NET_MAC_HI   0xC4
#define P_NET_MAC_LO   0xC5
#define P_NET_ZAEHLER  0xC6
#define P_NET_ZINDEX   0xC7

#define NET_KOPF       14        /* Ziel 6 + Absender 6 + Art 2 */
#define NET_MAX      1518

/* Ein Rahmen, an dem wir arbeiten. Einer reicht: der Treiber holt ihn ab,
   die Schicht darueber verarbeitet ihn, dann ist der Platz wieder frei. */
#define NET_PUFFER  0x00160000

/* Einzelne Byte lesen und schreiben -- ein Rahmen ist byteweise aufgebaut,
   nicht wortweise. */
int  net_getb(int addr)          { char* p; p = (char*)addr; return *p & 0xFF; }
void net_putb(int addr, int v)   { char* p; p = (char*)addr; *p = v; }

int net_da() { return sys_in(P_NET_STATUS) & 1; }

/* Liegt Post bereit? */
int net_wartet() { return (sys_in(P_NET_STATUS) >> 1) & 1; }

/* Die eigene Adresse in sechs Byte ab <out>. */
void net_mac(char* out) {
    int hi; int lo;
    hi = sys_in(P_NET_MAC_HI);
    lo = sys_in(P_NET_MAC_LO);
    out[0] = (hi >> 8) & 0xFF;
    out[1] = hi & 0xFF;
    out[2] = (lo >> 24) & 0xFF;
    out[3] = (lo >> 16) & 0xFF;
    out[4] = (lo >> 8) & 0xFF;
    out[5] = lo & 0xFF;
}

/* <welcher>: 0 = empfangene Rahmen, 1 = gesendete. */
int net_zaehler(int welcher) {
    sys_out(P_NET_ZINDEX, welcher);
    return sys_in(P_NET_ZAEHLER);
}

/* Schickt <len> Byte ab <addr> auf den Draht. Der Absender wird von der
   Karte eingetragen, das Ziel muss dastehen. */
int net_senden(int addr, int len) {
    if (net_da() == 0) return 0 - 1;
    if (len < NET_KOPF) return 0 - 1;
    if (len > NET_MAX) len = NET_MAX;
    sys_out(P_NET_ADDR, addr);
    sys_out(P_NET_LEN, len);
    sys_out(P_NET_CMD, 1);
    return len;
}

/* Holt den naechsten Rahmen nach <addr>. Rueckgabe: Laenge, 0 = nichts da. */
int net_empfangen(int addr) {
    if (net_da() == 0) return 0;
    sys_out(P_NET_ADDR, addr);
    sys_out(P_NET_CMD, 2);
    return sys_in(P_NET_LEN);
}

void net_leeren() { sys_out(P_NET_CMD, 3); }

/* Baut den Kopf eines Rahmens an <addr>: Ziel, Art. Der Absender bleibt
   frei, den setzt die Karte. Rueckgabe: wo die Nutzdaten anfangen. */
int net_kopf_bauen(int addr, char* ziel, int art) {
    int i;
    for (i = 0; i < 6; i++) net_putb(addr + i, ziel[i]);
    for (i = 6; i < 12; i++) net_putb(addr + i, 0);
    net_putb(addr + 12, (art >> 8) & 0xFF);
    net_putb(addr + 13, art & 0xFF);
    return addr + NET_KOPF;
}

/* Die Rundruf-Adresse: sechsmal 0xFF heisst "an alle im Netz". */
void net_alle(char* out) {
    int i;
    for (i = 0; i < 6; i++) out[i] = 0xFF;
}

/* Eine Adresse als 02:54:42:00:77:D8 nach <out> schreiben (18 Byte). */
void net_mac_text(char* mac, char* out) {
    int i; int b; int n;
    char* hex;
    hex = "0123456789ABCDEF";
    n = 0;
    for (i = 0; i < 6; i++) {
        if (i > 0) { out[n] = ':'; n++; }
        b = mac[i] & 0xFF;
        out[n] = hex[(b >> 4) & 0x0F]; n++;
        out[n] = hex[b & 0x0F]; n++;
    }
    out[n] = 0;
}
