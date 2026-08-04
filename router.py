#!/usr/bin/env python3
"""
Der Router am virtuellen Draht.

    python3 router.py            an -- laeuft, bis man Strg+C drueckt

Ohne ihn reden die TB-32 nur untereinander. Mit ihm kommen sie hinaus: er
haengt an derselben Multicast-Gruppe wie die Netzwerkkarten, hat eine eigene
Adresse (10.0.0.254) und macht genau das, was der Kasten an deiner Wand auch
macht -- er nimmt Pakete an, schickt sie ins echte Netz und traegt die
Antworten zurueck.

Warum das nicht schummelt: der TB-32 baut seine Pakete komplett selbst --
ARP, IP-Kopf, Pruefsumme, UDP, DNS-Frage. Der Router liest sie nur und gibt
sie weiter, so wie ein echter Router auch. Auf dem Pi faellt er ersatzlos
weg; dort steht ein richtiger Router im Netz.

Was er kann:
  * ARP beantworten (wer nach 10.0.0.254 fragt, bekommt seine Adresse)
  * PING auf ihn selbst beantworten
  * UDP nach draussen weiterleiten und die Antworten zurueckbringen (NAT)

  * TCP: er ist die Gegenseite fuer den TB-32 (Handschlag, Nummern,
    Bestaetigungen) und traegt den Byte-Strom auf einer normalen Verbindung
    ins echte Netz

Was er nicht kann: PING nach draussen (dafuer braeuchte er rohe Sockets und
damit Administratorrechte).
"""

import select
import socket
import struct
import sys
import time

GRUPPE = "239.32.32.32"
PORT = 32032
MAC = bytes([0x02, 0x54, 0x42, 0xFF, 0xFF, 0xFE])
IP = "10.0.0.254"
ALLE = b"\xff" * 6

ART_IP = 0x0800
ART_ARP = 0x0806
PROTO_ICMP = 1
PROTO_UDP = 17
PROTO_TCP = 6

FIN, SYN, RST, PSH, ACK = 1, 2, 4, 8, 16

# Wie lange eine Weiterleitung offen bleibt, wenn nichts mehr kommt
FRIST = 60.0


def ip2i(s):
    return struct.unpack(">I", socket.inet_aton(s))[0]


def i2ip(i):
    return socket.inet_ntoa(struct.pack(">I", i))


def summe16(daten, start=0):
    """Die Pruefsumme des Internets: zu 16 Bit addieren, Ueberlauf drauf."""
    s = start
    for i in range(0, len(daten) - 1, 2):
        s += (daten[i] << 8) | daten[i + 1]
    if len(daten) % 2:
        s += daten[-1] << 8
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF


class Router:
    def __init__(self, ip=IP, dns=None):
        self.ip = ip2i(ip)
        # Wohin Namensfragen wirklich gehen. Normal an den, den der TB-32
        # nennt; mit --dns an einen anderen (eigener Server, oder im
        # Selbsttest an einen Prueflings-Dienst auf einem hohen Port).
        self.dns = dns
        self.draht = self._draht_oeffnen()
        # (Klienten-IP, Klienten-Port, Ziel-IP, Ziel-Port) -> [Socket, Zeit, MAC]
        self.wege = {}
        self.gesehen = {}                 # IP -> MAC, damit wir zurueckfinden
        self.tcp = {}                     # (IP, Port, ZielIP, ZielPort) -> Verbindung

    def _draht_oeffnen(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, "SO_REUSEPORT"):
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        s.bind(("", PORT))
        lo = socket.inet_aton("127.0.0.1")
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                     struct.pack("4s4s", socket.inet_aton(GRUPPE), lo))
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, lo)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        s.setblocking(False)
        return s

    # -- hinaus auf den Draht ----------------------------------------------
    def rahmen_senden(self, ziel_mac, art, nutz):
        rahmen = ziel_mac + MAC + struct.pack(">H", art) + nutz
        if len(rahmen) < 60:
            rahmen = rahmen + b"\x00" * (60 - len(rahmen))
        self.draht.sendto(rahmen, (GRUPPE, PORT))

    def ip_senden(self, ziel_mac, zielip, proto, nutz, quellip=None):
        """quellip: mit wessen Adresse das Paket dasteht.

        Fuer Antworten des Routers selbst (ARP, PING) ist das seine eigene.
        Fuer alles Weitergeleitete muss dort die Adresse des ECHTEN Servers
        stehen -- der TB-32 hat ja diesen angesprochen und wirft alles weg,
        was von jemand anderem kommt. Genau so verhaelt sich jeder Router,
        der Adressen umsetzt."""
        kopf = bytearray(20)
        kopf[0] = 0x45
        struct.pack_into(">H", kopf, 2, 20 + len(nutz))
        struct.pack_into(">H", kopf, 4, 0)
        kopf[8] = 64
        kopf[9] = proto
        struct.pack_into(">I", kopf, 12, self.ip if quellip is None else quellip)
        struct.pack_into(">I", kopf, 16, zielip)
        struct.pack_into(">H", kopf, 10, summe16(bytes(kopf)))
        self.rahmen_senden(ziel_mac, ART_IP, bytes(kopf) + nutz)

    # -- was hereinkommt ----------------------------------------------------
    def rahmen(self, daten):
        if len(daten) < 14:
            return
        ziel, quelle = daten[:6], daten[6:12]
        art = struct.unpack(">H", daten[12:14])[0]
        if quelle == MAC:
            return                            # unser eigener Rahmen
        if ziel != MAC and ziel != ALLE:
            return
        if art == ART_ARP:
            self.arp(daten[14:], quelle)
        elif art == ART_IP:
            self.ip_paket(daten[14:], quelle)

    def arp(self, p, von_mac):
        if len(p) < 28 or struct.unpack(">H", p[6:8])[0] != 1:
            return                            # keine Frage
        gesucht = struct.unpack(">I", p[24:28])[0]
        if gesucht != self.ip:
            return                            # nicht nach uns gefragt
        frager_ip = struct.unpack(">I", p[14:18])[0]
        self.gesehen[frager_ip] = von_mac
        antwort = (struct.pack(">HHBBH", 1, ART_IP, 6, 4, 2) + MAC +
                   struct.pack(">I", self.ip) + p[8:14] + p[14:18])
        self.rahmen_senden(von_mac, ART_ARP, antwort)
        print(f"  ARP  {i2ip(frager_ip)} fragt nach uns -> beantwortet")

    def ip_paket(self, p, von_mac):
        if len(p) < 20:
            return
        ihl = (p[0] & 0x0F) * 4
        proto = p[9]
        quellip = struct.unpack(">I", p[12:16])[0]
        zielip = struct.unpack(">I", p[16:20])[0]
        nutz = p[ihl:struct.unpack(">H", p[2:4])[0]]
        self.gesehen[quellip] = von_mac

        if proto == PROTO_ICMP and zielip == self.ip and nutz and nutz[0] == 8:
            antwort = bytearray(nutz)
            antwort[0] = 0
            struct.pack_into(">H", antwort, 2, 0)
            struct.pack_into(">H", antwort, 2, summe16(bytes(antwort)))
            self.ip_senden(von_mac, quellip, PROTO_ICMP, bytes(antwort))
            print(f"  PING von {i2ip(quellip)} -> beantwortet")
            return

        if proto == PROTO_TCP and len(nutz) >= 20:
            self.tcp_paket(quellip, zielip, nutz, von_mac)
            return

        if proto == PROTO_UDP and len(nutz) >= 8:
            qport, zport, laenge = struct.unpack(">HHH", nutz[:6])
            self.udp_hinaus(quellip, qport, zielip, zport,
                            nutz[8:laenge], von_mac)


    # -- TCP ----------------------------------------------------------------
    # Der Router ist hier die Gegenseite: er spricht mit dem TB-32 echtes TCP
    # (Handschlag, Nummern, Bestaetigungen) und traegt den Byte-Strom auf
    # einer normalen Verbindung ins echte Netz. Genau so macht es die
    # Benutzer-Netzwerkfunktion von QEMU auch. Der TB-32 merkt keinen
    # Unterschied zu einem Webserver -- und muss sein TCP wirklich koennen.

    def tcp_segment(self, v, flaggen, nutz=b""):
        kopf = bytearray(20)
        struct.pack_into(">HH", kopf, 0, v["zport"], v["qport"])
        struct.pack_into(">I", kopf, 4, v["seq"] & 0xFFFFFFFF)
        struct.pack_into(">I", kopf, 8, v["ack"] & 0xFFFFFFFF)
        kopf[12] = 5 << 4
        kopf[13] = flaggen
        struct.pack_into(">H", kopf, 14, 8192)
        pseudo = struct.pack(">IIBBH", v["zielip"], v["quellip"], 0,
                             PROTO_TCP, 20 + len(nutz))
        struct.pack_into(">H", kopf, 16,
                         summe16(bytes(kopf) + nutz, summe16(pseudo) ^ 0xFFFF))
        self.ip_senden(v["mac"], v["quellip"], PROTO_TCP, bytes(kopf) + nutz,
                       quellip=v["zielip"])

    def tcp_paket(self, quellip, zielip, p, von_mac):
        qport, zport = struct.unpack(">HH", p[:4])
        seq, ack = struct.unpack(">II", p[4:12])
        koplen = (p[12] >> 4) * 4
        flaggen = p[13]
        daten = p[koplen:]
        schluessel = (quellip, qport, zielip, zport)
        v = self.tcp.get(schluessel)

        if flaggen & SYN and v is None:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(5.0)
            try:
                s.connect((i2ip(zielip), zport))
                s.setblocking(False)
            except OSError as e:
                print(f"  TCP  {i2ip(zielip)}:{zport} nicht erreichbar: {e}")
                v = {"quellip": quellip, "qport": qport, "zport": zport,
                     "zielip": zielip, "seq": 0, "ack": seq + 1,
                     "mac": von_mac}
                self.tcp_segment(v, RST | ACK)
                return
            v = {"quellip": quellip, "qport": qport, "zport": zport,
                 "zielip": zielip, "seq": 1000, "ack": seq + 1, "sock": s,
                 "mac": von_mac, "zeit": time.time(), "zu": False}
            self.tcp[schluessel] = v
            self.tcp_segment(v, SYN | ACK)
            v["seq"] += 1                       # das SYN zaehlt ein Byte
            print(f"  TCP  {i2ip(quellip)}:{qport} -> {i2ip(zielip)}:{zport} steht")
            return

        if v is None:
            return
        v["mac"] = von_mac
        v["zeit"] = time.time()

        if flaggen & RST:
            self.tcp_zu(schluessel)
            return

        if daten and seq == v["ack"]:
            v["ack"] += len(daten)
            try:
                v["sock"].sendall(daten)
            except OSError:
                pass
            self.tcp_segment(v, ACK)
        elif daten:
            self.tcp_segment(v, ACK)            # sagen, wo wir stehen

        if flaggen & FIN:
            v["ack"] += 1
            self.tcp_segment(v, ACK)
            try:
                v["sock"].shutdown(socket.SHUT_WR)
            except OSError:
                pass
            v["zu"] = True

    def tcp_zu(self, schluessel):
        v = self.tcp.pop(schluessel, None)
        if v and v.get("sock"):
            try:
                v["sock"].close()
            except OSError:
                pass

    def tcp_holen(self):
        """Was der echte Server schickt, geht als TCP-Segmente an den TB-32."""
        offen = [v["sock"] for v in self.tcp.values() if v.get("sock")]
        if not offen:
            return
        bereit, _, _ = select.select(offen, [], [], 0)
        for s in bereit:
            for schluessel, v in list(self.tcp.items()):
                if v.get("sock") is not s:
                    continue
                try:
                    daten = s.recv(1024)
                except (BlockingIOError, OSError):
                    continue
                if not daten:                   # Server ist fertig
                    self.tcp_segment(v, FIN | ACK)
                    v["seq"] += 1
                    self.tcp_zu(schluessel)
                    print("       Server hat geschlossen")
                    continue
                # In Haeppchen, die durch einen Rahmen passen
                for i in range(0, len(daten), 512):
                    stueck = daten[i:i + 512]
                    self.tcp_segment(v, PSH | ACK, stueck)
                    v["seq"] += len(stueck)
                    time.sleep(0.01)            # dem TB-32 Zeit zum Lesen
                print(f"       {len(daten)} Byte an {i2ip(v['quellip'])}:{v['qport']}")

    def udp_hinaus(self, quellip, qport, zielip, zport, daten, von_mac):
        schluessel = (quellip, qport, zielip, zport)
        wohin = (i2ip(zielip), zport)
        if zport == 53 and self.dns:
            wohin = self.dns
        weg = self.wege.get(schluessel)
        if weg is None:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.setblocking(False)
            weg = [s, 0.0, von_mac]
            self.wege[schluessel] = weg
            print(f"  UDP  {i2ip(quellip)}:{qport} -> {i2ip(zielip)}:{zport}")
        weg[1] = time.time()
        weg[2] = von_mac
        try:
            weg[0].sendto(daten, wohin)
        except OSError as e:
            print(f"       ging nicht: {e}")

    def antworten_holen(self):
        """Was von draussen zurueckkommt, geht an den richtigen TB-32."""
        offen = [w[0] for w in self.wege.values()]
        if not offen:
            return
        bereit, _, _ = select.select(offen, [], [], 0)
        for s in bereit:
            for schluessel, weg in list(self.wege.items()):
                if weg[0] is not s:
                    continue
                quellip, qport, zielip, zport = schluessel
                try:
                    daten, _ = s.recvfrom(2048)
                except OSError:
                    continue
                kopf = struct.pack(">HHHH", zport, qport, 8 + len(daten), 0)
                pseudo = (struct.pack(">IIBBH", zielip, quellip, 0,
                                      PROTO_UDP, 8 + len(daten)))
                pruef = summe16(kopf + daten, summe16(pseudo) ^ 0xFFFF)
                kopf = struct.pack(">HHHH", zport, qport, 8 + len(daten),
                                   pruef or 0xFFFF)
                ziel_mac = self.gesehen.get(quellip, weg[2])
                self.ip_senden(ziel_mac, quellip, PROTO_UDP, kopf + daten,
                               quellip=zielip)
                weg[1] = time.time()
                print(f"       {len(daten)} Byte zurueck an {i2ip(quellip)}:{qport}")

    def aufraeumen(self):
        jetzt = time.time()
        for schluessel, weg in list(self.wege.items()):
            if jetzt - weg[1] > FRIST:
                weg[0].close()
                del self.wege[schluessel]

    def laufen(self):
        mac = ":".join(f"{b:02X}" for b in MAC)
        print(f"Router laeuft.  {i2ip(self.ip)}   {mac}")
        print("Im TB-32:  NET GW zeigt den Weg nach draussen, "
              "HOST <name> fragt nach einer Adresse.")
        print("Strg+C beendet.\n")
        while True:
            bereit, _, _ = select.select([self.draht], [], [], 0.05)
            if bereit:
                while True:
                    try:
                        daten, _ = self.draht.recvfrom(2048)
                    except (BlockingIOError, OSError):
                        break
                    self.rahmen(daten)
            self.antworten_holen()
            self.tcp_holen()
            self.aufraeumen()


def main():
    """Aufruf:  python3 router.py [eigene-IP] [--dns adresse[:port]]"""
    ip = IP
    dns = None
    argumente = sys.argv[1:]
    i = 0
    while i < len(argumente):
        a = argumente[i]
        if a == "--dns" and i + 1 < len(argumente):
            ziel = argumente[i + 1]
            if ":" in ziel:
                wirt, port = ziel.split(":")
                dns = (wirt, int(port))
            else:
                dns = (ziel, 53)
            i += 2
            continue
        if a.count(".") == 3:
            ip = a
        i += 1
    r = Router(ip, dns)
    try:
        r.laufen()
    except KeyboardInterrupt:
        print("\nRouter aus.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
