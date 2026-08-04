#!/usr/bin/env python3
"""
Der Vermittler -- damit der TB-32 auch verschluesselte Seiten sieht.

    python3 proxy.py                laeuft auf Port 8080
    python3 proxy.py 9000           ... oder auf einem anderen

Das Problem: fast das ganze Web laeuft heute ueber **HTTPS**. Das ist
Verschluesselung mit Zertifikaten -- ein eigenes grosses Projekt, und im
TB-32 waere es Jahre Arbeit fuer etwas, das man am Ende nicht sieht.

Die Loesung ist so alt wie das Netz selbst: ein Vermittler. Der TB-32 sagt
ihm in schlichtem HTTP "hol mir bitte diese Seite", der Vermittler holt sie
-- verschluesselt, wenn noetig -- und reicht sie unverschluesselt weiter.
Genau so arbeiten die Proxys in Firmen und Schulen auch.

Der TB-32 bleibt dabei ehrlich: er spricht HTTP, und nur HTTP. Was er nicht
kann, gibt er ab, statt so zu tun als ob. Auf dem Pi laeuft dasselbe
Programm -- am besten dort, dann steht der Vermittler im eigenen Netz.

Im TB-32 einschalten:   NET PROXY 127.0.0.1:8080
Wieder aus:             NET PROXY OFF
"""

import socket
import socketserver
import ssl
import sys
import urllib.error
import urllib.request

MAXGROESSE = 512 * 1024          # mehr passt im TB-32 ohnehin nicht in den Puffer
FRIST = 15


def fehlerseite(code, titel, text):
    seite = (f"<html><head><title>{titel}</title></head><body>"
             f"<h1>{titel}</h1><p>{text}</p></body></html>").encode()
    return (f"HTTP/1.0 {code} {titel}\r\n"
            f"Content-Type: text/html\r\n"
            f"Content-Length: {len(seite)}\r\n"
            f"Connection: close\r\n\r\n").encode() + seite


class Vermittler(socketserver.StreamRequestHandler):
    timeout = FRIST

    def handle(self):
        try:
            zeile = self.rfile.readline(4096).decode("latin1").strip()
        except OSError:
            return
        if not zeile:
            return
        teile = zeile.split()
        if len(teile) < 2 or teile[0].upper() not in ("GET", "HEAD"):
            self.wfile.write(fehlerseite(501, "Not supported",
                                         "Only GET is supported."))
            return
        ziel = teile[1]

        # Kopfzeilen lesen (und bis auf den Host verwerfen -- der TB-32
        # schickt ohnehin kaum welche).
        wirt = ""
        while True:
            try:
                k = self.rfile.readline(4096).decode("latin1").strip()
            except OSError:
                break
            if not k:
                break
            if k.lower().startswith("host:"):
                wirt = k.split(":", 1)[1].strip()

        if ziel.startswith("/"):
            # Kein vollstaendiges Ziel: dann muss der Host-Kopf herhalten.
            if not wirt:
                self.wfile.write(fehlerseite(400, "No address",
                                             "The request had no address."))
                return
            ziel = "http://" + wirt + ziel
        if not ziel.startswith("http"):
            ziel = "http://" + ziel

        print(f"  {ziel}")
        try:
            anfrage = urllib.request.Request(
                ziel, headers={"User-Agent": "TOOBAD-OS/2.5.2",
                               "Accept": "text/html, text/plain"})
            with urllib.request.urlopen(anfrage, timeout=FRIST) as antwort:
                inhalt = antwort.read(MAXGROESSE)
                art = antwort.headers.get("Content-Type", "text/html")
                code = antwort.status
        except urllib.error.HTTPError as e:
            inhalt = e.read(MAXGROESSE)
            art = e.headers.get("Content-Type", "text/html")
            code = e.code
        except (urllib.error.URLError, ssl.SSLError, socket.timeout, OSError) as e:
            grund = str(getattr(e, "reason", e))
            print(f"       ging nicht: {grund}")
            self.wfile.write(fehlerseite(502, "Could not fetch", grund))
            return

        kopf = (f"HTTP/1.0 {code} OK\r\n"
                f"Content-Type: {art}\r\n"
                f"Content-Length: {len(inhalt)}\r\n"
                f"Connection: close\r\n\r\n").encode()
        try:
            self.wfile.write(kopf)
            if teile[0].upper() == "GET":
                self.wfile.write(inhalt)
        except OSError:
            pass
        print(f"       {len(inhalt)} Byte weitergereicht")


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    port = 8080
    for a in sys.argv[1:]:
        if a.isdigit():
            port = int(a)
    with Server(("", port), Vermittler) as s:
        print(f"Vermittler laeuft auf Port {port}.")
        print(f"Im TB-32:  NET PROXY 127.0.0.1:{port}")
        print("Strg+C beendet.\n")
        try:
            s.serve_forever()
        except KeyboardInterrupt:
            print("\nVermittler aus.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
