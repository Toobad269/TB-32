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

/* ===========================================================================
   Stufe 2: ARP und IP.

   Die Karte kennt nur Hardware-Adressen (02:54:42:...). Das Internet kennt
   nur IP-Adressen (10.0.0.5). Beides muss zusammenfinden, und genau das ist
   ARP: "Wer hat 10.0.0.5? Bitte an mich antworten." Wer sie hat, antwortet
   mit seiner Hardware-Adresse, und die merken wir uns eine Weile.

   Darueber liegt IP: ein Kopf mit Absender, Ziel, Lebenszeit und einer
   Pruefsumme. Und darin steckt ICMP -- das ist das, was PING benutzt.
   =========================================================================== */

#define ART_IP        0x0800
#define ART_ARP       0x0806

#define PROTO_ICMP    1

#define NET_SENDE     0x00161000     /* Rahmen zum Senden */
#define ARP_MAX       8

/* Zahlen stehen im Netz mit dem hoechsten Byte zuerst -- "Netzwerk-Ordnung".
   Der TB-32 legt sie andersherum ab. Deshalb Byte fuer Byte, nie mit einem
   Wortzugriff: das gaebe die Bytes verdreht. */
int  net_get16(int a) { return (net_getb(a) << 8) | net_getb(a + 1); }
void net_put16(int a, int v) {
    net_putb(a, (v >> 8) & 0xFF);
    net_putb(a + 1, v & 0xFF);
}
int net_get32(int a) {
    return (net_getb(a) << 24) | (net_getb(a + 1) << 16) |
           (net_getb(a + 2) << 8) | net_getb(a + 3);
}
void net_put32(int a, int v) {
    net_putb(a, (v >> 24) & 0xFF);
    net_putb(a + 1, (v >> 16) & 0xFF);
    net_putb(a + 2, (v >> 8) & 0xFF);
    net_putb(a + 3, v & 0xFF);
}

int  ip_meine = 0;                   /* eigene Adresse, 0 = keine */
int  ip_maske = 0xFFFFFF00;          /* /24 -- alles im selben Netz */
int  ip_gateway = 0x0A0000FE;        /* 10.0.0.254 -- der Weg nach draussen */
int  ip_dns     = 0x01010101;        /* 1.1.1.1 -- wer die Namen kennt */
int  arp_ip[ARP_MAX];
char arp_mac[48];                    /* ARP_MAX * 6 */
int  arp_frei[ARP_MAX];              /* 0 = Platz leer, 1 = belegt */
int  arp_naechster = 0;
int  ping_von = 0;                   /* Antwort auf unser PING: von wem */
int  ping_folge = 0;                 /* ... und welche Nummer */
int  ip_kennung = 1;                 /* laufende Nummer im IP-Kopf */

/* --- Pruefsumme -----------------------------------------------------------
   Die eine Rechnung, die im ganzen Internet steckt: alle Zahlen zu 16 Bit
   addieren, den Ueberlauf wieder unten drauf, und das Ergebnis umdrehen.
   Wer sie nachrechnet und 0 bekommt, weiss: unterwegs ist nichts kaputt
   gegangen. */
int net_pruefsumme(int addr, int len) {
    int summe; int i;
    summe = 0;
    i = 0;
    while (i + 1 < len) {
        summe = summe + net_get16(addr + i);
        i = i + 2;
    }
    if (i < len) summe = summe + (net_getb(addr + i) << 8);
    while (summe >> 16) summe = (summe & 0xFFFF) + (summe >> 16);
    return (~summe) & 0xFFFF;
}

/* --- ARP-Tabelle ---------------------------------------------------------- */

void arp_merken(int ip, int macaddr) {
    int i; int platz;
    platz = 0 - 1;
    for (i = 0; i < ARP_MAX; i++) {
        if (arp_frei[i] && arp_ip[i] == ip) { platz = i; break; }
        if (arp_frei[i] == 0 && platz < 0) platz = i;
    }
    if (platz < 0) {                 /* alles voll: den aeltesten ueberschreiben */
        platz = arp_naechster;
        arp_naechster = (arp_naechster + 1) % ARP_MAX;
    }
    arp_ip[platz] = ip;
    for (i = 0; i < 6; i++) arp_mac[platz * 6 + i] = net_getb(macaddr + i);
    arp_frei[platz] = 1;
}

int arp_finden(int ip, char* out) {
    int i; int j;
    for (i = 0; i < ARP_MAX; i++) {
        if (arp_frei[i] == 0 || arp_ip[i] != ip) continue;
        for (j = 0; j < 6; j++) out[j] = arp_mac[i * 6 + j];
        return 1;
    }
    return 0;
}

/* "Wer hat diese IP?" -- an alle im Netz. */
void arp_anfragen(int ip) {
    int p; int i;
    char alle[8];
    char selbst[8];
    net_alle(alle);
    net_mac(selbst);
    p = net_kopf_bauen(NET_SENDE, alle, ART_ARP);
    net_put16(p, 1);                 /* Hardware: Ethernet */
    net_put16(p + 2, ART_IP);        /* danach wird gefragt: eine IP */
    net_putb(p + 4, 6);
    net_putb(p + 5, 4);
    net_put16(p + 6, 1);             /* 1 = Frage */
    for (i = 0; i < 6; i++) net_putb(p + 8 + i, selbst[i]);
    net_put32(p + 14, ip_meine);
    for (i = 0; i < 6; i++) net_putb(p + 18 + i, 0);
    net_put32(p + 24, ip);
    net_senden(NET_SENDE, NET_KOPF + 28);
}

/* Auf eine Frage antworten, die uns gilt. */
void arp_antworten(int rx) {
    int p; int q; int i;
    char an[8];
    char selbst[8];
    p = rx + NET_KOPF;
    for (i = 0; i < 6; i++) an[i] = net_getb(p + 8 + i);
    net_mac(selbst);
    q = net_kopf_bauen(NET_SENDE, an, ART_ARP);
    net_put16(q, 1);
    net_put16(q + 2, ART_IP);
    net_putb(q + 4, 6);
    net_putb(q + 5, 4);
    net_put16(q + 6, 2);             /* 2 = Antwort */
    for (i = 0; i < 6; i++) net_putb(q + 8 + i, selbst[i]);
    net_put32(q + 14, ip_meine);
    for (i = 0; i < 6; i++) net_putb(q + 18 + i, an[i]);
    net_put32(q + 24, net_get32(p + 14));
    net_senden(NET_SENDE, NET_KOPF + 28);
}

/* --- IP -------------------------------------------------------------------
   Baut Ethernet-Kopf, IP-Kopf und haengt <len> Byte ab <daten> an. Wen wir
   nicht kennen, nach dem fragen wir erst -- und melden Fehlschlag, wenn
   niemand antwortet. */
int ip_senden(int zielip, int proto, int daten, int len) {
    int p; int i; int gesamt; int frist; int naechster;
    char mac[8];

    if (ip_meine == 0) return 0 - 2;             /* keine eigene Adresse */

    /* Wer nicht im eigenen Netz wohnt, ist nur ueber den Gateway zu
       erreichen -- den Router. Nach IHM fragen wir dann, nicht nach dem
       eigentlichen Ziel. Genau das macht jeder Rechner im Internet. */
    naechster = zielip;
    if ((zielip & ip_maske) != (ip_meine & ip_maske)) naechster = ip_gateway;

    if (arp_finden(naechster, mac) == 0) {
        arp_anfragen(naechster);
        frist = sys_ticks() + 100;               /* eine Sekunde warten */
        while (sys_ticks() < frist) {
            net_bearbeiten();
            if (arp_finden(naechster, mac)) break;
        }
        if (arp_finden(naechster, mac) == 0) return 0 - 1;   /* niemand da */
    }

    p = net_kopf_bauen(NET_SENDE, mac, ART_IP);
    gesamt = 20 + len;
    net_putb(p, 0x45);                           /* Fassung 4, Kopf 20 Byte */
    net_putb(p + 1, 0);
    net_put16(p + 2, gesamt);
    net_put16(p + 4, ip_kennung);
    ip_kennung = (ip_kennung + 1) & 0xFFFF;
    net_put16(p + 6, 0);                         /* nicht zerlegt */
    net_putb(p + 8, 64);                         /* Lebenszeit */
    net_putb(p + 9, proto);
    net_put16(p + 10, 0);                        /* Pruefsumme, gleich */
    net_put32(p + 12, ip_meine);
    net_put32(p + 16, zielip);
    net_put16(p + 10, net_pruefsumme(p, 20));
    for (i = 0; i < len; i++) net_putb(p + 20 + i, net_getb(daten + i));
    gesamt = NET_KOPF + 20 + len;
    if (gesamt < 60) gesamt = 60;
    return net_senden(NET_SENDE, gesamt);
}

/* --- ICMP: das Protokoll hinter PING -------------------------------------- */

void icmp_antworten(int rx, int kopf, int nutz, int len) {
    int i; int p;
    char mac[8];
    for (i = 0; i < 6; i++) mac[i] = net_getb(rx + 6 + i);
    arp_merken(net_get32(kopf + 12), rx + 6);
    /* Die Antwort ist die Frage mit Art 0 statt 8 -- Inhalt bleibt gleich,
       damit der Fragende seine eigenen Daten wiedererkennt. */
    for (i = 0; i < len; i++) net_putb(NET_PUFFER + 1200 + i, net_getb(nutz + i));
    net_putb(NET_PUFFER + 1200, 0);
    net_putb(NET_PUFFER + 1202, 0);
    net_putb(NET_PUFFER + 1203, 0);
    net_put16(NET_PUFFER + 1202, net_pruefsumme(NET_PUFFER + 1200, len));
    ip_senden(net_get32(kopf + 12), PROTO_ICMP, NET_PUFFER + 1200, len);
}

/* --- Die Poststelle -------------------------------------------------------
   Holt einen Rahmen ab und tut das Naheliegende damit. Wird ueberall dort
   aufgerufen, wo der Rechner sonst nur warten wuerde -- an der Eingabezeile
   und in der Schleife des Schreibtischs. Deshalb antwortet die Maschine auf
   PING, auch wenn niemand davor sitzt.
   Rueckgabe: 1 = es lag etwas an. */
int net_bearbeiten() {
    int len; int p; int art; int kopf; int nutz; int nlen; int i2;
    len = net_empfangen(NET_PUFFER);
    if (len <= 0) return 0;
    art = (net_getb(NET_PUFFER + 12) << 8) | net_getb(NET_PUFFER + 13);

    if (art == ART_ARP) {
        p = NET_PUFFER + NET_KOPF;
        if (net_get16(p + 6) == 1 && net_get32(p + 24) == ip_meine
            && ip_meine != 0) {
            arp_merken(net_get32(p + 14), p + 8);
            arp_antworten(NET_PUFFER);
        } else if (net_get16(p + 6) == 2) {
            arp_merken(net_get32(p + 14), p + 8);
        }
        return 1;
    }

    if (art == ART_IP) {
        kopf = NET_PUFFER + NET_KOPF;
        if (net_get32(kopf + 16) != ip_meine || ip_meine == 0) return 1;
        nutz = kopf + (net_getb(kopf) & 0x0F) * 4;
        nlen = net_get16(kopf + 2) - (net_getb(kopf) & 0x0F) * 4;
        if (net_getb(kopf + 9) == PROTO_UDP && nlen >= 8) {
            /* Ein UDP-Paket: Absender und Zielport merken, Inhalt ablegen.
               Ein Platz reicht -- gefragt wird, dann wird gewartet. */
            udp_von = net_get32(kopf + 12);
            udp_port = net_get16(nutz + 2);
            udp_len = net_get16(nutz + 4) - 8;
            if (udp_len > 512) udp_len = 512;
            for (i2 = 0; i2 < udp_len; i2++)
                net_putb(UDP_POST + i2, net_getb(nutz + 8 + i2));
            return 1;
        }
        if (net_getb(kopf + 9) == PROTO_ICMP && nlen > 0) {
            if (net_getb(nutz) == 8) {                  /* Frage: bist du da? */
                icmp_antworten(NET_PUFFER, kopf, nutz, nlen);
            } else if (net_getb(nutz) == 0) {           /* Antwort auf unsere */
                ping_von = net_get32(kopf + 12);
                ping_folge = net_get16(nutz + 6);
            }
        }
        return 1;
    }
    return 1;
}

/* Einmal anpingen. Rueckgabe: Zeit in Hundertstelsekunden, -1 = keine
   Antwort, -2 = niemand mit dieser Adresse gefunden. */
int icmp_ping(int zielip, int folge) {
    int i; int frist; int start; int r;
    net_putb(NET_PUFFER + 1100, 8);              /* Art 8 = Frage */
    net_putb(NET_PUFFER + 1101, 0);
    net_put16(NET_PUFFER + 1102, 0);
    net_put16(NET_PUFFER + 1104, 0x5442);        /* Kennung: "TB" */
    net_put16(NET_PUFFER + 1106, folge);
    for (i = 0; i < 24; i++) net_putb(NET_PUFFER + 1108 + i, 'a' + (i % 26));
    net_put16(NET_PUFFER + 1102, net_pruefsumme(NET_PUFFER + 1100, 32));

    ping_von = 0;
    ping_folge = 0 - 1;
    start = sys_ticks();
    r = ip_senden(zielip, PROTO_ICMP, NET_PUFFER + 1100, 32);
    if (r == 0 - 1) return 0 - 2;
    if (r < 0) return 0 - 1;
    frist = sys_ticks() + 100;
    while (sys_ticks() < frist) {
        net_bearbeiten();
        if (ping_von == zielip && ping_folge == folge)
            return sys_ticks() - start;
    }
    return 0 - 1;
}

/* --- Adressen als Text ---------------------------------------------------- */

void ip_text(int ip, char* out) {
    int i; int n; int teil; int z; int stelle;
    n = 0;
    for (i = 3; i >= 0; i--) {
        teil = (ip >> (i * 8)) & 0xFF;
        stelle = 100;
        z = 0;
        while (stelle > 0) {
            if (teil >= stelle || z || stelle == 1) {
                out[n] = '0' + (teil / stelle);
                n++;
                z = 1;
            }
            teil = teil % stelle;
            stelle = stelle / 10;
        }
        if (i > 0) { out[n] = '.'; n++; }
    }
    out[n] = 0;
}

/* "10.0.0.5" einlesen. Rueckgabe: die Adresse, 0 = unbrauchbar. */
int ip_lesen(char* s) {
    int teil[4];
    int i; int n; int wert; int ziffern;
    n = 0;
    i = 0;
    while (n < 4) {
        wert = 0;
        ziffern = 0;
        while (s[i] >= '0' && s[i] <= '9') {
            wert = wert * 10 + (s[i] - '0');
            i++;
            ziffern++;
        }
        if (ziffern == 0 || wert > 255) return 0;
        teil[n] = wert;
        n++;
        if (n < 4) {
            if (s[i] != '.') return 0;
            i++;
        }
    }
    if (s[i] != 0 && s[i] != ' ') return 0;
    return (teil[0] << 24) | (teil[1] << 16) | (teil[2] << 8) | teil[3];
}

/* Beim Start: eine Adresse aus der eigenen Hardware-Adresse ableiten, damit
   ohne Zutun schon etwas geht. 10.0.0.<letztes Byte> -- zwei Rechner auf
   demselben Mac bekommen so verschiedene Adressen. */
void net_start() {
    int i;
    char mac[8];
    for (i = 0; i < ARP_MAX; i++) arp_frei[i] = 0;
    if (net_da() == 0) return;
    net_mac(mac);
    ip_meine = 0x0A000000 | (mac[5] & 0xFF);
}

/* ===========================================================================
   Stufe 3: UDP und DNS.

   IP bringt ein Paket zum richtigen Rechner. Aber auf einem Rechner laufen
   viele Programme -- welches ist gemeint? Dafuer gibt es Portnummern, und
   das einfachste Protokoll mit Ports ist UDP: acht Byte Kopf, fertig. Kein
   Verbindungsaufbau, keine Bestaetigung. Ein Paket geht raus, vielleicht
   kommt eins zurueck.

   Genau so arbeitet DNS -- die Stelle, die aus "example.com" eine Adresse
   macht. Eine Frage hin, eine Antwort zurueck.
   =========================================================================== */

#define PROTO_UDP     17
#define UDP_BAU       0x00162000     /* hier wird ein UDP-Paket gebaut */
#define UDP_POST      0x00163000     /* ... und hier liegt, was ankam */

int udp_port_frei = 40000;
int udp_von = 0;                     /* von wem kam das letzte Paket */
int udp_port = 0;                    /* ... an welchen unserer Ports */
int udp_len = 0;                     /* ... und wie lang war es */

/* Die Pruefsumme von UDP rechnet Absender und Ziel mit, obwohl die im
   IP-Kopf stehen -- der "Pseudokopf". Damit faellt auf, wenn ein Paket beim
   richtigen Rechner, aber im falschen Zusammenhang landet. */
int udp_pruefsumme(int quellip, int zielip, int addr, int len) {
    int summe; int i;
    summe = 0;
    summe = summe + ((quellip >> 16) & 0xFFFF) + (quellip & 0xFFFF);
    summe = summe + ((zielip >> 16) & 0xFFFF) + (zielip & 0xFFFF);
    summe = summe + PROTO_UDP + len;
    i = 0;
    while (i + 1 < len) {
        summe = summe + net_get16(addr + i);
        i = i + 2;
    }
    if (i < len) summe = summe + (net_getb(addr + i) << 8);
    while (summe >> 16) summe = (summe & 0xFFFF) + (summe >> 16);
    summe = (~summe) & 0xFFFF;
    if (summe == 0) summe = 0xFFFF;  /* 0 hiesse "nicht gerechnet" */
    return summe;
}

/* <daten> und <len> sind die Nutzdaten. Rueckgabe wie bei ip_senden. */
int udp_senden(int zielip, int quellport, int zielport, int daten, int len) {
    int i;
    net_put16(UDP_BAU, quellport);
    net_put16(UDP_BAU + 2, zielport);
    net_put16(UDP_BAU + 4, 8 + len);
    net_put16(UDP_BAU + 6, 0);
    for (i = 0; i < len; i++) net_putb(UDP_BAU + 8 + i, net_getb(daten + i));
    net_put16(UDP_BAU + 6, udp_pruefsumme(ip_meine, zielip, UDP_BAU, 8 + len));
    return ip_senden(zielip, PROTO_UDP, UDP_BAU, 8 + len);
}

/* --- DNS ------------------------------------------------------------------
   Ein Name wird in Stuecke zerlegt: "example.com" wird zu
   7 e x a m p l e 3 c o m 0. Vor jedem Stueck steht seine Laenge, am Ende
   eine Null. Rueckgabe: wie viele Byte geschrieben wurden. */
int dns_name_schreiben(int addr, char* name) {
    int n; int anfang; int laenge; int i;
    n = 0;
    i = 0;
    while (1) {
        anfang = n;                  /* hier kommt gleich die Laenge hin */
        n++;
        laenge = 0;
        while (name[i] != 0 && name[i] != '.') {
            net_putb(addr + n, name[i]);
            n++;
            i++;
            laenge++;
        }
        net_putb(addr + anfang, laenge);
        if (name[i] == 0) break;
        i++;                         /* den Punkt ueberspringen */
    }
    net_putb(addr + n, 0);
    n++;
    return n;
}

/* Ueber einen Namen in einer Antwort hinweggehen. Namen duerfen abgekuerzt
   sein: zwei Byte, die mit 0xC0 anfangen, zeigen auf eine Stelle weiter
   vorn im selben Paket. Wer das nicht beachtet, laeuft ins Leere. */
int dns_name_ueberspringen(int addr, int ende) {
    int n;
    n = addr;
    while (n < ende) {
        if ((net_getb(n) & 0xC0) == 0xC0) return n + 2;
        if (net_getb(n) == 0) return n + 1;
        n = n + net_getb(n) + 1;
    }
    return ende;
}

/* Fragt den Namensdienst nach <name>. Rueckgabe: Adresse, 0 = nichts. */
int dns_aufloesen(char* name) {
    int len; int port; int frist; int i;
    int p; int ende; int anzahl; int typ; int rlen;

    if (net_da() == 0 || ip_meine == 0) return 0;

    net_put16(UDP_POST + 512, 0x7742);          /* Kennung */
    net_put16(UDP_POST + 514, 0x0100);          /* bitte nachschlagen */
    net_put16(UDP_POST + 516, 1);               /* eine Frage */
    net_put16(UDP_POST + 518, 0);
    net_put16(UDP_POST + 520, 0);
    net_put16(UDP_POST + 522, 0);
    len = 12 + dns_name_schreiben(UDP_POST + 524, name);
    net_put16(UDP_POST + 512 + len, 1);         /* Art A: eine IPv4-Adresse */
    net_put16(UDP_POST + 514 + len, 1);         /* Klasse: Internet */
    len = len + 4;

    port = udp_port_frei;
    udp_port_frei = udp_port_frei + 1;
    if (udp_port_frei > 45000) udp_port_frei = 40000;

    udp_len = 0;
    udp_port = 0;
    if (udp_senden(ip_dns, port, 53, UDP_POST + 512, len) < 0) return 0;

    frist = sys_ticks() + 300;                  /* drei Sekunden */
    while (sys_ticks() < frist) {
        net_bearbeiten();
        if (udp_len > 0 && udp_port == port) break;
    }
    if (udp_len <= 12 || udp_port != port) return 0;

    ende = UDP_POST + udp_len;
    anzahl = net_get16(UDP_POST + 6);           /* wie viele Antworten */
    p = dns_name_ueberspringen(UDP_POST + 12, ende) + 4;   /* Frage weg */
    for (i = 0; i < anzahl; i++) {
        p = dns_name_ueberspringen(p, ende);
        if (p + 10 > ende) return 0;
        typ = net_get16(p);
        rlen = net_get16(p + 8);
        p = p + 10;
        if (typ == 1 && rlen == 4) return net_get32(p);
        p = p + rlen;                           /* etwas anderes: weiter */
    }
    return 0;
}
