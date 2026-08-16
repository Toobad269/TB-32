/* ==========================================================================
   TOOBAD-OS  --  Kernel and command interpreter

   Loaded by the boot sector and takes over the machine. Written in the
   custom language TC and translated by the custom compiler (tools/tcc.py)
   into machine code for the custom CPU.
   ========================================================================== */

#include "lib.c"
#include "fs.c"
#include "edit.c"
#include "font8.c"
#include "diag.c"
#include "proc.c"
#include "net.c"
#include "browser.c"
#include "term.c"
#include "syscall.c"
#include "dialog.c"
#include "gui.c"

#define CMDMAX 128

char cmdline[CMDMAX];
char cmd[32];
char arg1[64];
char arg2[64];
char progname[24];
char argzeile[80];      /* command line for the Python interpreter */

/* --- Splitting the input line into words ---------------------------------- */

int copy_word(char* src, int pos, char* out, int max) {
    int n;
    n = 0;
    while (src[pos] == ' ') pos++;
    while (src[pos] && src[pos] != ' ' && n < max - 1) {
        out[n] = src[pos];
        n++;
        pos++;
    }
    out[n] = 0;
    return pos;
}

void parse(char* line) {
    int p;
    p = copy_word(line, 0, cmd, 32);
    p = copy_word(line, p, arg1, 64);
    p = copy_word(line, p, arg2, 64);
}

/* ==========================================================================
   Help
   ========================================================================== */

void cmd_help(char* topic) {
    if (topic[0] == 0) {
        printc("\nTOOBAD-OS command reference\n\n", CYAN);
        printc("File commands\n", BRIGHT);
        print("  DIR [/W]        List files on the disk\n");
        print("  TYPE <file>     Display the contents of a file\n");
        print("  MORE <file>     Display a file one screen at a time\n");
        print("  EDIT <file>     Full screen text editor\n");
        print("  COPY <a> <b>    Copy a file\n");
        print("  REN <a> <b>     Rename a file\n");
        print("  DEL <file>      Delete a file\n");
        printc("\nDirectory commands\n", BRIGHT);
        print("  CD <name>       Change directory (CD .. goes up, CD \\ to root)\n");
        print("  MD <name>       Create a directory\n");
        print("  RD <name>       Remove an empty directory\n");
        printc("\nDisk commands\n", BRIGHT);
        print("  CHKDSK          Check the file system for errors\n");
        print("  FORMAT          Erase and re-initialise the disk\n");
        print("  VOL             Show volume information\n");
        print("  FC <a> <b>      Compare two files byte by byte\n");
        printc("\nSystem commands\n", BRIGHT);
        print("  CLS  VER  MEM  TIME  DATE  ECHO  COLOR <hex>\n");
        print("  SYSTEMINFO      Detailed hardware and system report\n");
        print("  DUMP <addr>     Hexadecimal memory dump\n");
        print("  DISPTEST        Display adapter test pattern\n");
        print("  TEMP [mode]     Temperature, fan and throttling\n");
        print("  SHUTDOWN [/R]   Power off, /R restarts the machine\n");
        print("  PgUp key        Scroll back through earlier output\n");
        printc("\nProcess commands\n", BRIGHT);
        print("  START <file> [/B]  Run a program, /B runs it in background\n");
        print("  TASKLIST        Show running processes\n");
        print("  TASKKILL <id>   Terminate a process\n");
        printc("\nGraphical interface\n", BRIGHT);
        print("  WIN             Start the desktop environment\n");
        print("\nType HELP <command> for details.\n");
        return;
    }
    if (stricmp(topic, "dir") == 0) {
        print("DIR [/W]\n  Lists the files stored on drive A:.\n");
        print("  /W  Wide format, names only.\n");
    } else if (stricmp(topic, "start") == 0) {
        print("START <file> [/B]\n  Loads a .TBX program from disk and runs it.\n");
        print("  /B  Starts the program in the background; the prompt returns\n");
        print("      immediately and the program keeps running.\n");
    } else if (stricmp(topic, "chkdsk") == 0) {
        print("CHKDSK\n  Verifies the directory, checks every file for a valid\n");
        print("  sector range and reports overlapping or lost sectors.\n");
    } else if (stricmp(topic, "dump") == 0) {
        print("DUMP <address>\n  Shows 128 bytes of memory in hexadecimal and\n");
        print("  as text. The address may be decimal or 0x-prefixed.\n");
    } else if (stricmp(topic, "shutdown") == 0) {
        print("SHUTDOWN [/R]\n  Powers the machine off. /R restarts it instead.\n");
    } else if (stricmp(topic, "color") == 0) {
        print("COLOR <hex>\n  Sets text colour. First digit is the background,\n");
        print("  second the foreground, e.g. COLOR 1F = white on blue.\n");
    } else {
        print("No help available for that command.\n");
    }
}

/* ==========================================================================
   System information
   ========================================================================== */


/* --- NET: look at the network card and try it out -------------------------
   Three forms. "NET" alone shows the status, "NET SEND <text>" sends a
   broadcast to everyone on the network, "NET WATCH" waits for frames and
   shows them until a key is pressed. The first stage doesn't need more
   than that -- it lets you see whether the chain of card, driver and wire
   even works. */
#define NET_ART_TEXT 0x7742          /* our own type: plain text */

void net_rahmen_zeigen(int len) {
    int i; int art; int c;
    char von[8];
    char text[24];
    for (i = 0; i < 6; i++) von[i] = net_getb(NET_PUFFER + 6 + i);
    net_mac_text(von, text);
    print("  from ");
    print(text);
    art = (net_getb(NET_PUFFER + 12) << 8) | net_getb(NET_PUFFER + 13);
    print("  type ");
    printn(art);
    print("  ");
    printn(len);
    print(" bytes  ");
    if (art == NET_ART_TEXT) {
        for (i = NET_KOPF; i < len && i < NET_KOPF + 60; i++) {
            c = net_getb(NET_PUFFER + i);
            if (c == 0) break;
            if (c >= 32 && c < 127) putch(c);
        }
    }
    nl();
}

void cmd_net(char* option, char* rest) {
    int i; int n; int len; int taste;
    char mac[8];
    char text[24];
    char alle[8];

    if (net_da() == 0) {
        printc("No network card.\n", RED);
        return;
    }

    if (stricmp(option, "send") == 0) {
        net_alle(alle);
        n = net_kopf_bauen(NET_PUFFER, alle, NET_ART_TEXT);
        for (i = 0; rest[i] != 0 && i < 200; i++) net_putb(n + i, rest[i]);
        net_putb(n + i, 0);
        len = NET_KOPF + i + 1;
        if (len < 60) len = 60;              /* a frame must not be shorter than this */
        if (net_senden(NET_PUFFER, len) < 0) printc("Send failed.\n", RED);
        else { print("Sent "); printn(len); print(" bytes to everyone.\n"); }
        return;
    }

    if (stricmp(option, "ip") == 0) {
        if (rest[0] != 0) {
            n = ip_lesen(rest);
            if (n == 0) { printc("Syntax: NET IP 10.0.0.5\n", RED); return; }
            ip_meine = n;
        }
        ip_text(ip_meine, text);
        print("  IP address                 ");
        printc(text, BRIGHT);
        nl();
        return;
    }

    if (stricmp(option, "gw") == 0) {
        if (rest[0] != 0) {
            n = ip_lesen(rest);
            if (n == 0) { printc("Syntax: NET GW 10.0.0.254\n", RED); return; }
            ip_gateway = n;
        }
        ip_text(ip_gateway, text);
        print("  Gateway                    ");
        printc(text, BRIGHT);
        nl();
        return;
    }

    if (stricmp(option, "dns") == 0) {
        if (rest[0] != 0) {
            n = ip_lesen(rest);
            if (n == 0) { printc("Syntax: NET DNS 1.1.1.1\n", RED); return; }
            ip_dns = n;
        }
        ip_text(ip_dns, text);
        print("  Name server                ");
        printc(text, BRIGHT);
        nl();
        return;
    }

    if (stricmp(option, "proxy") == 0) {
        /* "0.0.0.0" would seem like the obvious way to turn it off, but it
           doesn't work: ip_lesen returns 0 for that -- the same as for
           "broken". So instead we use a word that is unambiguous. */
        if (stricmp(rest, "off") == 0) {
            br_proxy = 0;
            print("  Proxy                      ");
            printc("off\n", YELLOW);
            return;
        }
        if (rest[0] != 0) {
            /* "127.0.0.1:8080" -- the colon separates off the port. */
            i = 0;
            while (rest[i] != 0 && rest[i] != ':') i++;
            n = ip_lesen(rest);
            if (n == 0 && rest[i] == ':') {
                rest[i] = 0;
                n = ip_lesen(rest);
                rest[i] = ':';
            }
            if (n == 0) {
                printc("Syntax: NET PROXY 127.0.0.1:8080\n", RED);
                return;
            }
            br_proxy = n;
            if (rest[i] == ':') br_proxy_port = atoi(rest + i + 1);
            if (br_proxy_port <= 0) br_proxy_port = 8080;
        }
        if (br_proxy == 0) {
            print("  Proxy                      ");
            printc("off -- HTTPS not available\n", YELLOW);
            return;
        }
        ip_text(br_proxy, text);
        print("  Proxy                      ");
        printc(text, BRIGHT);
        print(":");
        printn(br_proxy_port);
        nl();
        return;
    }

    if (stricmp(option, "arp") == 0) {
        printc("\n  Address table\n", BRIGHT);
        n = 0;
        for (i = 0; i < ARP_MAX; i++) {
            if (arp_frei[i] == 0) continue;
            ip_text(arp_ip[i], text);
            print("  ");
            print(text);
            print("   ");
            net_mac_text(arp_mac + i * 6, mac);
            print(mac);
            nl();
            n++;
        }
        if (n == 0) print("  (empty -- nobody has answered yet)\n");
        return;
    }

    if (stricmp(option, "watch") == 0) {
        print("Listening. Any key stops.\n");
        while (1) {
            if (sys_haskey()) { taste = sys_getkey(); break; }
            len = net_empfangen(NET_PUFFER);
            if (len > 0) net_rahmen_zeigen(len);
        }
        return;
    }

    net_mac(mac);
    net_mac_text(mac, text);
    printc("\nNetwork\n", BRIGHT);
    print("  Card                       TB-NET\n");
    print("  Link                       ");
    printc("up\n", GREEN);
    print("  Hardware address           ");
    printc(text, BRIGHT);
    nl();
    print("  Frames received            ");
    printnc(net_zaehler(0), BRIGHT);
    nl();
    print("  Frames sent                ");
    printnc(net_zaehler(1), BRIGHT);
    nl();
    ip_text(ip_meine, text);
    print("  IP address                 ");
    printc(text, BRIGHT);
    nl();
    ip_text(ip_gateway, text);
    print("  Gateway                    ");
    print(text);
    nl();
    ip_text(ip_dns, text);
    print("  Name server                ");
    print(text);
    nl();
    print("\n  NET IP <addr>     our address       NET ARP    who is known\n");
    print("  NET GW <addr>     way out           NET DNS <addr>  name server\n");
    print("  NET PROXY <a:p>   the way to HTTPS  NET SEND <text>  to everyone\n");
    print("  NET WATCH         show arrivals\n");
    print("  PING <addr>       is somebody there\n");
    print("  HOST <name>       what is the address of a name\n");
    print("  FETCH <name> [/p] fetch a page over TCP\n");
}


/* --- PING: is anybody there? -----------------------------------------------
   Ask four times, timing each one. That's exactly what PING does on every
   other machine too -- it sends an ICMP echo and waits for the echo to
   come back. */
void cmd_ping(char* ziel) {
    int ip; int i; int t; int gut;
    char text[24];

    if (net_da() == 0) { printc("No network card.\n", RED); return; }
    if (ziel[0] == 0) { printc("Syntax: PING 10.0.0.5\n", RED); return; }
    ip = ip_lesen(ziel);
    if (ip == 0) { printc("That is not an address.\n", RED); return; }
    if (ip_meine == 0) { printc("No address of our own. Use NET IP.\n", RED); return; }

    ip_text(ip, text);
    print("\nPinging ");
    print(text);
    print("\n");
    gut = 0;
    for (i = 1; i <= 4; i++) {
        t = icmp_ping(ip, i);
        if (t == 0 - 2) { printc("  no answer to who-has\n", YELLOW); continue; }
        if (t < 0) { printc("  timed out\n", YELLOW); continue; }
        print("  reply from ");
        print(text);
        print("   time ");
        printn(t * 10);
        print(" ms\n");
        gut++;
        sleep(30);
    }
    print("\n  ");
    printn(gut);
    print(" of 4 answered\n");
}


/* --- HOST: turn a name into an address --------------------------------------
   This is DNS. One question to the name service, one answer back -- and
   the answer contains the address. Without this service you'd have to
   remember numbers instead of names. */
void cmd_host(char* name) {
    int ip;
    char text[24];

    if (net_da() == 0) { printc("No network card.\n", RED); return; }
    if (name[0] == 0) { printc("Syntax: HOST example.com\n", RED); return; }
    if (ip_meine == 0) { printc("No address of our own. Use NET IP.\n", RED); return; }

    print("\nAsking ");
    ip_text(ip_dns, text);
    print(text);
    print(" for ");
    print(name);
    print("\n");
    ip = dns_aufloesen(name);
    if (ip == 0) {
        printc("  no answer -- is the router running?\n", YELLOW);
        return;
    }
    ip_text(ip, text);
    print("  ");
    print(name);
    print("  is  ");
    printc(text, BRIGHT);
    nl();
}


/* --- FETCH: actually fetch a page -------------------------------------------
   The proof that TCP works. Look up the name, open a connection, send an
   HTTP request, read the response. That's exactly what a browser does too
   -- except it then renders it nicely afterwards.

   HTTP is a TEXT protocol: what goes out here can be read. That makes it
   the right starting point for a browser of our own. */
#define HTTP_BAU  0x00170000
#define HTTP_ANT  0x00171000
#define HTTP_MAX  40960

int str_nach(int addr, char* s) {
    int i;
    i = 0;
    while (s[i] != 0) {
        net_putb(addr + i, s[i]);
        i++;
    }
    return i;
}

void cmd_fetch(char* wirt, char* pfad) {
    int ip; int n; int gesamt; int i; int zeilen; int c; int port;
    char text[24];
    char name[64];

    if (net_da() == 0) { printc("No network card.\n", RED); return; }
    if (wirt[0] == 0) { printc("Syntax: FETCH example.com [/path]\n", RED); return; }
    if (ip_meine == 0) { printc("No address of our own. Use NET IP.\n", RED); return; }

    /* "example.com:8080" -- the colon separates off the port. Without it,
       it's the usual web port 80. */
    port = 80;
    n = 0;
    while (wirt[n] != 0 && wirt[n] != ':' && n < 60) { name[n] = wirt[n]; n++; }
    name[n] = 0;
    if (wirt[n] == ':') port = atoi(wirt + n + 1);
    if (port <= 0 || port > 65535) port = 80;
    wirt = name;

    /* If a proxy is set up, the connection goes to IT -- and the request
       then contains the full address. That's exactly what a browser does. */
    if (br_proxy != 0) {
        print("Through the proxy ... ");
        if (tcp_verbinden(br_proxy, br_proxy_port) == 0) {
            /* Nobody there: then go direct instead. The default proxy is
               the one that pc.py starts alongside it -- anyone using the
               kernel differently shouldn't be left out in the cold. */
            printc("none there, going direct\n", YELLOW);
            br_proxy = 0;
        } else printc("connected\n", GREEN);
    }
    if (br_proxy == 0) {
        ip = ip_lesen(wirt);
        if (ip == 0) {
            print("Looking up ");
            print(wirt);
            print(" ... ");
            ip = dns_aufloesen(wirt);
            if (ip == 0) { printc("unknown name\n", YELLOW); return; }
            ip_text(ip, text);
            printc(text, BRIGHT);
            nl();
        }

        print("Connecting to port ");
        printn(port);
        print(" ... ");
        if (tcp_verbinden(ip, port) == 0) {
            printc("no answer -- is the router running?\n", YELLOW);
            return;
        }
        printc("connected\n", GREEN);
    }

    n = str_nach(HTTP_BAU, "GET ");
    if (br_proxy != 0) {
        n = n + str_nach(HTTP_BAU + n, "http://");
        n = n + str_nach(HTTP_BAU + n, wirt);
        if (port != 80) {
            n = n + str_nach(HTTP_BAU + n, ":");
            n = n + zahl_nach(HTTP_BAU + n, port);
        }
    }
    if (pfad[0] == 0) n = n + str_nach(HTTP_BAU + n, "/");
    else n = n + str_nach(HTTP_BAU + n, pfad);
    n = n + str_nach(HTTP_BAU + n, " HTTP/1.0\r\nHost: ");
    n = n + str_nach(HTTP_BAU + n, wirt);
    n = n + str_nach(HTTP_BAU + n, "\r\nConnection: close\r\n\r\n");
    tcp_schreiben(HTTP_BAU, n);

    gesamt = 0;
    while (gesamt < HTTP_MAX) {
        n = tcp_lesen(HTTP_ANT + gesamt, HTTP_MAX - gesamt, 300);
        if (n <= 0) break;
        gesamt = gesamt + n;
    }
    tcp_schliessen();

    print("\n");
    printn(gesamt);
    print(" bytes received\n\n");

    /* Show the first lines -- that's the response header and the start of
       the page. The browser will handle more of it later. */
    zeilen = 0;
    for (i = 0; i < gesamt && zeilen < 16; i++) {
        c = net_getb(HTTP_ANT + i);
        if (c == 13) continue;
        if (c == 10) { nl(); zeilen++; continue; }
        if (c >= 32 && c < 127) putch(c);
    }
    nl();
}


/* Points to whatever comes after the first <n> words of the line.
   This used to be computed as: cmdline + 4 + strlen(arg1) + 1. For
   "net ip" (6 characters) that landed on position 7 -- ONE past the
   terminating null byte, i.e. into nowhere. Sometimes there happened to
   be a zero there and everything worked, sometimes there was garbage, and
   NET IP would complain about an address nobody had typed. */
char* nach_woertern(char* zeile, int n) {
    int i; int w;
    i = 0;
    w = 0;
    while (w < n) {
        while (zeile[i] == ' ') i++;
        if (zeile[i] == 0) return zeile + i;
        while (zeile[i] != 0 && zeile[i] != ' ') i++;
        w++;
    }
    while (zeile[i] == ' ') i++;
    return zeile + i;
}

void cmd_ver() {
    printc("TOOBAD-OS Version 1.0\n", CYAN);
    print("TOOBAD BIOS v2.5.2, TB-32 architecture\n");
}

void cmd_mem() {
    int kb; int sekt; int belegt;
    kb = mem_get(0x000004A0);
    sekt = mem_get(0x000004A4);
    belegt = fs_used_sectors();
    printc("\nMemory\n", BRIGHT);
    print("  Total physical memory      ");
    printnc(kb, BRIGHT);
    print(" KB\n");
    print("  Kernel and system area     ");
    printnc(640, BRIGHT);
    print(" KB\n");
    print("  Available to programs      ");
    printnc(kb - 640, BRIGHT);
    print(" KB\n");
    printc("\nDisk\n", BRIGHT);
    print("  Capacity                   ");
    printnc(sekt / 2048, BRIGHT);
    print(" MB (");
    printn(sekt);
    print(" sectors)\n");
    print("  Used by files              ");
    printnc(belegt / 2, BRIGHT);
    print(" KB in ");
    printn(fs_count());
    print(" file(s)\n");
    print("  Free                       ");
    printnc((sekt - FS_DATA - belegt) / 2, BRIGHT);
    print(" KB\n");
}

void cmd_systeminfo() {
    int t; int d;
    t = sys_clock();
    d = sys_date();
    printc("\nSystem Information\n\n", CYAN);
    print("  OS Name                    TOOBAD-OS\n");
    print("  OS Version                 1.0\n");
    print("  System Manufacturer        Toobad\n");
    print("  System Model               TB-32\n");
    print("  Processor                  TOOBAD TB-32, 32-bit, 16 registers\n");
    print("  Instruction Set            fixed 4-byte words, RISC style\n");
    print("  BIOS Version               TOOBAD BIOS v2.5.2\n");
    print("  Total Physical Memory      ");
    printn(mem_get(0x000004A0));
    print(" KB\n");
    print("  Address Space              16 MB RAM, ROM at 0x0F000000\n");
    print("  Display Adapter            TB-VGA, 80x25 text / 640x400 x 256\n");
    print("  Storage Controller         TB-IDE, 512-byte sectors, DMA\n");
    print("  File System                TBFS\n");
    print("  Interrupt Vectors          256 entries at 0x00000000\n");
    print("  System Time                ");
    print2((t >> 16) & 255);
    putch(':');
    print2((t >> 8) & 255);
    putch(':');
    print2(t & 255);
    print("\n  System Date                ");
    print2(d & 255);
    putch('.');
    print2((d >> 8) & 255);
    putch('.');
    printn((d >> 16) & 65535);
    print("\n  System Up Time             ");
    printn(sys_ticks() / 100);
    print(" seconds\n");
    print("  Running Processes          ");
    printn(proc_count());
    print("\n  CPU Temperature            ");
    printn(sys_in(P_TEMP) / 10);
    print(" C, fan at ");
    printn(sys_in(P_FAN));
    print(" %");
    if (sys_in(P_THROTTLE)) {
        printc("  (throttling)", RED);
    }
    nl();
}

void cmd_time() {
    int t;
    t = sys_clock();
    print("Current time: ");
    print2((t >> 16) & 255);
    putch(':');
    print2((t >> 8) & 255);
    putch(':');
    print2(t & 255);
    nl();
}

void cmd_date() {
    int d;
    d = sys_date();
    print("Current date: ");
    print2(d & 255);
    putch('.');
    print2((d >> 8) & 255);
    putch('.');
    printn((d >> 16) & 65535);
    nl();
}

/* ==========================================================================
   File commands
   ========================================================================== */

/* Print a number right-aligned within a field of the given width */
void print_right(int wert, int breite, int farbe) {
    int stellen; int t;
    stellen = 1;
    t = wert;
    while (t >= 10) { t = t / 10; stellen++; }
    while (stellen < breite) { putch(' '); stellen++; }
    printnc(wert, farbe);
}

void cmd_dir(char* option) {
    int i; int n; int t; int breit; int spalte;
    char pfad[40];
    breit = 0;
    if (option[0] == '/' && toupper(option[1]) == 'W') breit = 1;

    print("\n Volume in drive A is TOOBAD-OS\n");
    print(" Directory of ");
    fs_path(pfad);
    print(pfad);
    print("\n\n");
    spalte = 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) continue;
        if (ent_versteckt(i)) continue;      /* don't show system files */
        if (ent_parent(i) != cwd) continue;
        if (ent_type(i) == FT_DIR) {
            if (breit) {
                putch('[');
                printc(ent_name(i), CYAN);
                putch(']');
                n = strlen(ent_name(i)) + 2;
                while (n < 18) { putch(' '); n++; }
                spalte++;
                if (spalte == 4) { nl(); spalte = 0; }
            } else {
                t = ent_time(i);
                print(" ");
                print2((t >> 16) & 255);
                putch(':');
                print2((t >> 8) & 255);
                print("       <DIR>  ");
                printc(ent_name(i), CYAN);
                nl();
            }
            continue;
        }
        if (breit) {
            printc(ent_name(i), BRIGHT);
            n = strlen(ent_name(i));
            while (n < 18) { putch(' '); n++; }
            spalte++;
            if (spalte == 4) { nl(); spalte = 0; }
            continue;
        }
        t = ent_time(i);
        print(" ");
        print2((t >> 16) & 255);
        putch(':');
        print2((t >> 8) & 255);
        print_right(ent_size(i), 12, NORMAL);
        print("  ");
        printc(ent_name(i), BRIGHT);
        nl();
    }
    if (breit && spalte) nl();
    nl();
    n = 0;
    for (i = 0; i < FS_MAXFILES; i++)
        if (ent_type(i) == FT_DIR && ent_parent(i) == cwd) n++;
    if (n) {
        print_right(n, 10, BRIGHT);
        print(" Dir(s)\n");
    }
    n = 0;
    t = 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == FT_FILE && ent_parent(i) == cwd) { n++; t = t + ent_size(i); }
    }
    print_right(n, 10, BRIGHT);
    print(" File(s)");
    print_right(t, 12, BRIGHT);
    print(" bytes\n");
    print_right((sys_disksize() - FS_DATA - fs_used_sectors()) / 2, 10, BRIGHT);
    print(" KB free\n");
}

void cmd_type(char* name) {
    int n; int i; char* t;
    if (name[0] == 0) { printc("Syntax: TYPE <file>\n", RED); return; }
    n = fs_read(name, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("File not found\n", RED); return; }
    t = (char*)FILEBUF;
    for (i = 0; i < n; i++) putch(t[i]);
    nl();
}

void cmd_more(char* name) {
    int n; int i; int zeilen; int k; char* t;
    if (name[0] == 0) { printc("Syntax: MORE <file>\n", RED); return; }
    n = fs_read(name, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("File not found\n", RED); return; }
    t = (char*)FILEBUF;
    zeilen = 0;
    for (i = 0; i < n; i++) {
        putch(t[i]);
        if (t[i] == 10) {
            zeilen++;
            if (zeilen >= 22) {
                printc("-- More --", INVERS);
                k = getkey();
                print("\r          \r");
                if (keycode(k) == K_ESC) return;
                zeilen = 0;
            }
        }
    }
    nl();
}

/* SUDO: enter the password once, then five minutes of peace -- just like
   real sudo. This lets you delete files in \SYSTEM. */
void cmd_sudo() {
    char pw[32];
    if (konto_offen()) {
        print("This machine has no password -- nothing is locked.\n");
        return;
    }
    print("Password: ");
    passwort_lesen(pw, 30);
    if (benutzer_passt(pw) == 0) {
        printc("Wrong password.\n", RED);
        return;
    }
    sudo_bis = sys_ticks() + 30000;               /* five minutes */
    printc("Granted for five minutes.\n", GREEN);
}

void cmd_del(char* name) {
    int r;
    if (name[0] == 0) { printc("Syntax: DEL <file>\n", RED); return; }
    r = fs_delete(name);
    if (r == 0) { print("File deleted\n"); return; }
    if (r == 0 - 3) {
        printc("That belongs to the system.\n", YELLOW);
        print("Use SUDO first -- it asks for your password.\n");
        return;
    }
    printc("File not found\n", RED);
}

void cmd_ren(char* a, char* b) {
    int r;
    if (a[0] == 0 || b[0] == 0) { printc("Syntax: REN <old> <new>\n", RED); return; }
    r = fs_rename(a, b);
    if (r == 0) print("File renamed\n");
    else if (r == 0 - 2) printc("A file with that name already exists\n", RED);
    else printc("File not found\n", RED);
}

void cmd_copy(char* a, char* b) {
    int n;
    if (a[0] == 0 || b[0] == 0) { printc("Syntax: COPY <source> <target>\n", RED); return; }
    n = fs_read(a, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("Source file not found\n", RED); return; }
    if (fs_write(b, FILEBUF, n) == 0) {
        print("        1 file(s) copied\n");
    } else {
        printc("Insufficient disk space\n", RED);
    }
}

/* ==========================================================================
   Disk commands
   ========================================================================== */

void cmd_vol() {
    printc("\n Volume in drive A is TOOBAD-OS\n", NORMAL);
    print(" Volume Serial Number is TB32-0001\n");
    print(" File system is TBFS\n");
    print(" Total size ");
    printn(sys_disksize() / 2048);
    print(" MB, sector size 512 bytes\n");
}

void cmd_chkdsk() {
    int i; int j; int fehler; int belegt; int s1; int e1; int s2; int e2;
    printc("\nChecking file system on A:\n\n", CYAN);
    print("The type of the file system is TBFS.\n\n");

    fehler = 0;
    print("Stage 1: Examining directory entries ...\n");
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_used(i) != 1) continue;
        if (ent_start(i) < FS_DATA || ent_start(i) >= sys_disksize()) {
            printc("  Bad start sector in entry ", RED);
            printn(i);
            nl();
            fehler++;
        }
        if (ent_name(i)[0] == 0) {
            printc("  Entry with empty name found\n", RED);
            fehler++;
        }
    }

    print("Stage 2: Checking for overlapping files ...\n");
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_used(i) != 1) continue;
        s1 = ent_start(i);
        e1 = s1 + sectors_for(ent_size(i));
        for (j = i + 1; j < FS_MAXFILES; j++) {
            if (ent_used(j) != 1) continue;
            s2 = ent_start(j);
            e2 = s2 + sectors_for(ent_size(j));
            if (s1 < e2 && s2 < e1) {
                printc("  Cross-linked: ", RED);
                print(ent_name(i));
                print(" and ");
                print(ent_name(j));
                nl();
                fehler++;
            }
        }
    }

    print("Stage 3: Verifying free space ...\n\n");
    belegt = fs_used_sectors();
    printnc(sys_disksize() * 512, BRIGHT);
    print(" bytes total disk space\n");
    printnc(FS_DATA * 512, BRIGHT);
    print(" bytes in system area\n");
    printnc(belegt * 512, BRIGHT);
    print(" bytes in ");
    printn(fs_count());
    print(" file(s)\n");
    printnc((sys_disksize() - FS_DATA - belegt) * 512, BRIGHT);
    print(" bytes available on disk\n\n");

    if (fehler == 0) printc("File system is clean. No errors found.\n", GREEN);
    else {
        printc("Errors found: ", RED);
        printn(fehler);
        nl();
    }
}

void cmd_format() {
    int k;
    printc("WARNING: ALL DATA ON DRIVE A: WILL BE LOST!\n", YELLOW);
    print("Proceed with Format (Y/N)? ");
    k = toupper(keychar(getkey()));
    putch(k);
    nl();
    if (k == 'Y') {
        print("Formatting ...\n");
        fs_format();
        print("Format complete.\n\n");
        printn(sys_disksize() * 512);
        print(" bytes total disk space\n");
    } else {
        print("Format cancelled\n");
    }
}

/* --- Temperature and cooling ---------------------------------------------- */

#define P_TEMP        0xA0
#define P_FAN         0xA1
#define P_THROTTLE    0xA2
#define P_TEMP_LIMIT  0xA3
#define P_FANMODE     0xA4
#define P_TEMP_MAX    0xA5

void balken(int wert, int maximum, int breite, int farbe) {
    int i; int voll;
    voll = wert * breite / maximum;
    if (voll > breite) voll = breite;
    putch('[');
    for (i = 0; i < breite; i++) {
        if (i < voll) putcolor(219, farbe);
        else putcolor(176, 0x08);
    }
    putch(']');
}

void cmd_temp(char* arg) {
    int t; int fan; int thr; int lim; int hoch; int farbe;

    if (arg[0]) {                                /* TEMP AUTO|QUIET|FULL */
        if (stricmp(arg, "auto") == 0)  { sys_out(P_FANMODE, 0); print("Fan control: automatic\n"); return; }
        if (stricmp(arg, "quiet") == 0) { sys_out(P_FANMODE, 1); print("Fan control: quiet\n"); return; }
        if (stricmp(arg, "full") == 0)  { sys_out(P_FANMODE, 2); print("Fan control: full speed\n"); return; }
        if (stricmp(arg, "reset") == 0) { sys_out(P_TEMP_MAX, 0); print("Peak temperature cleared\n"); return; }
        printc("Syntax: TEMP [AUTO|QUIET|FULL|RESET]\n", RED);
        return;
    }

    t = sys_in(P_TEMP);
    fan = sys_in(P_FAN);
    thr = sys_in(P_THROTTLE);
    lim = sys_in(P_TEMP_LIMIT);
    hoch = sys_in(P_TEMP_MAX);

    farbe = GREEN;
    if (t > 700) farbe = YELLOW;
    if (t > lim * 10) farbe = RED;

    printc("\nThermal status\n\n", CYAN);
    print("  CPU temperature   ");
    printnc(t / 10, farbe);
    putch('.');
    printnc(t % 10, farbe);
    print(" C   ");
    balken(t / 10, 110, 30, farbe);
    nl();
    print("  Fan speed         ");
    printnc(fan, BRIGHT);
    print(" %       ");
    balken(fan, 100, 30, CYAN);
    nl();
    print("  Throttling        ");
    if (thr) {
        printnc(thr, RED);
        print(" %       ");
        balken(thr, 100, 30, RED);
        nl();
        printc("  The CPU is running slower to cool down.\n", YELLOW);
    } else {
        printc("none\n", GREEN);
    }
    print("  Throttle limit    ");
    printn(lim);
    print(" C\n  Peak since boot   ");
    printn(hoch / 10);
    putch('.');
    printn(hoch % 10);
    print(" C\n\n  Fan control: TEMP AUTO | TEMP QUIET | TEMP FULL\n");
}

/* --- Directories ------------------------------------------------------------ */

void cmd_md(char* name) {
    int r;
    if (name[0] == 0) { printc("Syntax: MD <name>\n", RED); return; }
    r = fs_mkdir(name);
    if (r == 0) print("Directory created\n");
    else if (r == 0 - 2) printc("A file or directory with that name exists\n", RED);
    else printc("Directory table is full\n", RED);
}

void cmd_cd(char* name) {
    int r;
    char pfad[40];
    if (name[0] == 0) {                      /* ohne Angabe: Pfad anzeigen */
        fs_path(pfad);
        print(pfad);
        nl();
        return;
    }
    r = fs_chdir(name);
    if (r == 0 - 1) printc("The system cannot find the path specified\n", RED);
    else if (r == 0 - 2) printc("Not a directory\n", RED);
}

void cmd_rd(char* name) {
    int r;
    if (name[0] == 0) { printc("Syntax: RD <name>\n", RED); return; }
    r = fs_rmdir(name);
    if (r == 0) print("Directory removed\n");
    else if (r == 0 - 3) printc("The directory is not empty\n", RED);
    else if (r == 0 - 2) printc("Not a directory\n", RED);
    else printc("Directory not found\n", RED);
}

/* --- FC: compare two files byte by byte ------------------------------------ */

#define FC_BUF1  0x00400000
#define FC_BUF2  0x00500000
#define FC_MAX   0x000F0000

void cmd_fc(char* a, char* b) {
    int n1; int n2; int i; int unterschiede; int erste;
    char* p1;
    char* p2;

    if (a[0] == 0 || b[0] == 0) { printc("Syntax: FC <file1> <file2>\n", RED); return; }
    n1 = fs_read(a, FC_BUF1, FC_MAX);
    if (n1 < 0) { printc("File not found: ", RED); print(a); nl(); return; }
    n2 = fs_read(b, FC_BUF2, FC_MAX);
    if (n2 < 0) { printc("File not found: ", RED); print(b); nl(); return; }

    print("\nComparing ");
    printc(a, BRIGHT);
    print(" (");
    printn(n1);
    print(" bytes) and ");
    printc(b, BRIGHT);
    print(" (");
    printn(n2);
    print(" bytes)\n");

    if (n1 != n2) {
        printc("Files are different: sizes do not match.\n", RED);
        return;
    }
    p1 = (char*)FC_BUF1;
    p2 = (char*)FC_BUF2;
    unterschiede = 0;
    erste = 0 - 1;
    for (i = 0; i < n1; i++) {
        if (p1[i] != p2[i]) {
            unterschiede++;
            if (erste < 0) erste = i;
        }
    }
    if (unterschiede == 0) {
        printc("FC: no differences encountered -- the files are identical.\n", GREEN);
    } else {
        printc("Files are different: ", RED);
        printn(unterschiede);
        print(" byte(s), first at offset ");
        printn(erste);
        nl();
    }
}

/* ==========================================================================
   Debug / hardware
   ========================================================================== */

int parse_addr(char* s) {
    int v; int i;
    if (s[0] == '0' && toupper(s[1]) == 'X') {
        v = 0;
        i = 2;
        while (s[i]) {
            v = v * 16;
            if (s[i] >= '0' && s[i] <= '9') v = v + s[i] - '0';
            else v = v + (toupper(s[i]) - 'A' + 10);
            i++;
        }
        return v;
    }
    return atoi(s);
}

void cmd_dump(char* a) {
    int addr; int zeile; int i; int b; char* p;
    if (a[0] == 0) { printc("Syntax: DUMP <address>\n", RED); return; }
    addr = parse_addr(a);
    nl();
    for (zeile = 0; zeile < 8; zeile++) {
        sys_puthex(addr, CYAN, 8);
        print("  ");
        p = (char*)addr;
        for (i = 0; i < 16; i++) {
            b = p[i] & 255;
            sys_puthex(b, BRIGHT, 2);
            putch(' ');
            if (i == 7) putch(' ');
        }
        print(" ");
        for (i = 0; i < 16; i++) {
            b = p[i] & 255;
            if (b < 32 || b > 126) b = '.';
            putch(b);
        }
        nl();
        addr = addr + 16;
    }
}

void cmd_color(char* a) {
    int c;
    if (a[0] == 0) { text_attr = NORMAL; print("Colour reset\n"); return; }
    c = parse_addr(a);
    if (c == 0) {
        c = 0;
        if (a[0] >= '0' && a[0] <= '9') c = (a[0] - '0') * 16;
        else c = (toupper(a[0]) - 'A' + 10) * 16;
        if (a[1] >= '0' && a[1] <= '9') c = c + a[1] - '0';
        else c = c + toupper(a[1]) - 'A' + 10;
    }
    text_attr = c;
    print("Colour set\n");
}

/* ==========================================================================
   Processes and programs
   ========================================================================== */

void cmd_tasklist() {
    int i; int z;
    printc("\nImage Name          PID   Status      CPU Time\n", CYAN);
    print("==========================================================\n");
    for (i = 0; i < MAXPROC; i++) {
        if (p_state[i] == PS_FREI) continue;
        print(proc_name(i));
        z = strlen(proc_name(i));
        while (z < 20) { putch(' '); z++; }
        printn(i);
        print("     ");
        z = p_state[i];
        if (z == PS_LAEUFT)   printc("Running     ", GREEN);
        if (z == PS_BEREIT)   printc("Ready       ", BRIGHT);
        if (z == PS_SCHLAEFT) printc("Sleeping    ", NORMAL);
        printn(p_ticks[i] * 10);
        print(" ms\n");
    }
    print("\nMultitasking: ");
    if (mt_active) printc("enabled", GREEN);
    else printc("disabled", NORMAL);
    print("     Context switches: ");
    printnc(p_switches, BRIGHT);
    nl();
}

void cmd_taskkill(char* a) {
    int n;
    n = atoi(a);
    if (a[0] == 0) { printc("Syntax: TASKKILL <pid>\n", RED); return; }
    if (n <= 0 || n >= MAXPROC || p_state[n] == PS_FREI) {
        printc("No process with that ID\n", RED);
        return;
    }
    p_state[n] = PS_FREI;
    print("SUCCESS: process ");
    printn(n);
    print(" has been terminated\n");
}

void cmd_start(char* name, char* option) {
    int r; int hg; int i; int j; int n;
    char args[80];
    if (name[0] == 0) { printc("Syntax: START <file.TBX> [/B] [arguments]\n", RED); return; }
    hg = 0;

    /* Everything after the program name is passed to the program as
       arguments. /B may appear at ANY position -- it used to be that only
       the first word after the name counted, so "START X.TBX COLORS /B"
       ran in the foreground and blocked the command line. */
    i = 0;
    while (cmdline[i] == ' ') i++;
    while (cmdline[i] && cmdline[i] != ' ') i++;      /* skip START */
    while (cmdline[i] == ' ') i++;
    while (cmdline[i] && cmdline[i] != ' ') i++;      /* program name */

    n = 0;
    while (cmdline[i] && n < 78) {
        while (cmdline[i] == ' ') i++;
        if (cmdline[i] == 0) break;
        j = i;
        while (cmdline[j] && cmdline[j] != ' ') j++;  /* one word */
        if ((cmdline[i] == '/' || cmdline[i] == '-')
            && toupper(cmdline[i + 1]) == 'B' && j == i + 2) {
            hg = 1;                                   /* the word was /B */
        } else if (cmdline[i] == '&' && j == i + 1) {
            hg = 1;
        } else {
            if (n > 0 && n < 78) { args[n] = ' '; n++; }
            while (i < j && n < 78) { args[n] = cmdline[i]; n++; i++; }
        }
        i = j;
    }
    args[n] = 0;
    prog_setargs(args);

    r = prog_run(name, hg);
    if (r == 0 - 2) {
        printc("Blocked by system policy.\n", RED);
        return;
    }
    if (r == 0 - 1) {
        printc("Program not found: ", RED);
        printc(name, BRIGHT);
        print("\nUse DIR to list the programs on this disk.\n");
        return;
    }
    if (hg) {
        print("Started in background, process ID ");
        printnc(r, BRIGHT);
        nl();
    }
}

/* ==========================================================================
   Shell
   ========================================================================== */

void boot_screen() {
    sys_cls(NORMAL);
    sys_hline(0, 0, 80, 32, 0x1F);
    sys_putsat(2, 0, "TOOBAD-OS 2.5.2", 0x1F);
    sys_putsat(62, 0, "TB-32 System", 0x1F);
    sys_setcursor(0, 2);
}

void shell() {
    int n; int i; int r;
    char pfad[40];
    while (1) {
        color(NORMAL);
        fs_path(pfad);
        printc(pfad, GREEN);
        printc("> ", GREEN);
        n = readline(cmdline, CMDMAX);
        nl();
        if (n <= 0) continue;
        parse(cmdline);

        if      (stricmp(cmd, "help") == 0)       cmd_help(arg1);
        else if (stricmp(cmd, "?") == 0)          cmd_help(arg1);
        else if (stricmp(cmd, "ver") == 0)        cmd_ver();
        else if (stricmp(cmd, "cls") == 0)        cls();
        else if (stricmp(cmd, "mem") == 0)        cmd_mem();
        else if (stricmp(cmd, "systeminfo") == 0) cmd_systeminfo();
        else if (stricmp(cmd, "time") == 0)       cmd_time();
        else if (stricmp(cmd, "date") == 0)       cmd_date();
        else if (stricmp(cmd, "echo") == 0)       { print(cmdline + 5); nl(); }
        else if (stricmp(cmd, "color") == 0)      cmd_color(arg1);
        else if (stricmp(cmd, "net") == 0)        cmd_net(arg1, nach_woertern(cmdline, 2));
        else if (stricmp(cmd, "ping") == 0)       cmd_ping(arg1);
        else if (stricmp(cmd, "host") == 0)       cmd_host(arg1);
        else if (stricmp(cmd, "fetch") == 0)      cmd_fetch(arg1, arg2);

        else if (stricmp(cmd, "dir") == 0)        cmd_dir(arg1);
        else if (stricmp(cmd, "type") == 0)       cmd_type(arg1);
        else if (stricmp(cmd, "more") == 0)       cmd_more(arg1);
        else if (stricmp(cmd, "del") == 0)        cmd_del(arg1);
        else if (stricmp(cmd, "sudo") == 0)       cmd_sudo();
        else if (stricmp(cmd, "erase") == 0)      cmd_del(arg1);
        else if (stricmp(cmd, "ren") == 0)        cmd_ren(arg1, arg2);
        else if (stricmp(cmd, "copy") == 0)       cmd_copy(arg1, arg2);
        else if (stricmp(cmd, "edit") == 0) {
            if (arg1[0] == 0) printc("Syntax: EDIT <file>\n", RED);
            else edit(arg1);
        }

        else if (stricmp(cmd, "chkdsk") == 0)     cmd_chkdsk();
        else if (stricmp(cmd, "format") == 0)     cmd_format();
        else if (stricmp(cmd, "vol") == 0)        cmd_vol();
        else if (stricmp(cmd, "temp") == 0)       cmd_temp(arg1);
        else if (stricmp(cmd, "md") == 0)         cmd_md(arg1);
        else if (stricmp(cmd, "mkdir") == 0)      cmd_md(arg1);
        else if (stricmp(cmd, "cd") == 0)         cmd_cd(arg1);
        else if (stricmp(cmd, "chdir") == 0)      cmd_cd(arg1);
        else if (stricmp(cmd, "rd") == 0)         cmd_rd(arg1);
        else if (stricmp(cmd, "rmdir") == 0)      cmd_rd(arg1);
        else if (stricmp(cmd, "fc") == 0)         cmd_fc(arg1, arg2);

        else if (stricmp(cmd, "dump") == 0)       cmd_dump(arg1);
        else if (stricmp(cmd, "disptest") == 0)   display_test();

        else if (stricmp(cmd, "start") == 0)      cmd_start(arg1, arg2);
        else if (stricmp(cmd, "tasklist") == 0)   cmd_tasklist();
        else if (stricmp(cmd, "taskkill") == 0)   cmd_taskkill(arg1);

        else if (stricmp(cmd, "win") == 0 || stricmp(cmd, "desktop") == 0) {
            /* The desktop runs in process 0. A second call from within the
               terminal window would paint two UIs onto the same screen --
               so it is caught here. */
            if (gui_running) {
                printc("The desktop is already running.\n", YELLOW);
                print("This window is part of it. Use EXIT to close the window.\n");
            } else {
                gui_main();
            }
        }

        else if (stricmp(cmd, "shutdown") == 0) {
            if (arg1[0] == '/' && toupper(arg1[1]) == 'R') {
                print("The system is restarting ...\n");
                sleep(40);
                sys_out(P_POWER, 2);
            } else {
                print("The system is shutting down ...\n");
                sleep(30);
                sys_out(P_POWER, 1);
            }
        }
        else if (stricmp(cmd, "exit") == 0) {
            if (term_aktiv) return;              /* close the terminal window */
            print("Not running in a window.\n");
        }
        else if (stricmp(cmd, "tbcmd") == 0) {
            /* From the window into the big full-screen console. Counterpart
               to WIN, which leads back to the desktop. */
            if (term_aktiv) {
                term_aktiv = 0;
                gui_running = 0;
            } else {
                print("You are already in the full-screen console.\n");
            }
        }
        else if (stricmp(cmd, "reboot") == 0) {
            print("The system is restarting ...\n");
            sleep(40);
            sys_out(P_POWER, 2);
        }
        else {
            /* Not a built-in command: so we look for a program of the same
               name on the disk -- exactly like DOS and Unix do. That's how
               ASM SOURCE.ASM TARGET.TBX becomes a program call. */
            n = strlen(cmd);
            i = 0;
            while (cmdline[i] == ' ') i++;
            while (cmdline[i] && cmdline[i] != ' ') i++;   /* past the name */

            if (endet_auf(cmd, ".PY")) {
                /* A Python script is not a machine program -- it needs the
                   interpreter in front of it. The user should be able to
                   type GUESS.PY without needing to know that PY.TBX exists.
                   That's exactly what a big system does with the #! line. */
                strcpy(progname, "PY.TBX");
                strncpy(argzeile, cmd, 20);
                strcat(argzeile, cmdline + i);
                prog_setargs(argzeile);
            } else {
                if (n > 4 && cmd[n - 4] == '.') strncpy(progname, cmd, 20);
                else {
                    strncpy(progname, cmd, 16);
                    strcat(progname, ".TBX");
                }
                prog_setargs(cmdline + i);
            }
            r = prog_run(progname, 0);
            if (r == 0 - 2) {
                printc("Blocked by system policy.\n", RED);
            } else if (r == 0 - 1) {
                printc("'", RED);
                printc(cmd, BRIGHT);
                printc("' is not recognized as a command or program.\n", RED);
                print("Type HELP for a list of commands, DIR for programs.\n");
            }
        }
    }
}

/* ==========================================================================
   Setup on first boot, login on every boot after that

   On the very first startup there is no user yet. The system then asks
   for a name and password and stores both in \SYSTEM\USER.DAT. Every
   later boot requires the password before the command line appears.

   WHAT THIS IS AND WHAT IT ISN'T: it keeps curious people out. It is NOT
   security. The disk is unencrypted -- anyone who gets hold of hd0.img can
   read everything with a hex editor. The password is stored only as a
   checksum, not with a proper algorithm; the TB-32 doesn't have one.
   Deleting the file gets you back in -- just like a BIOS password is gone
   once you pull the button cell. That fits the era we're recreating, and
   nobody should mistake it for more than that.
   ========================================================================== */

#define CM_BOOTMODE 0x1D             /* 0 = desktop, 1 = text console */

#define USER_BUF   0x000C0000
#define USER_NAME  0                     /* 20-byte name */
#define USER_HASH  20                    /* 4-byte password checksum */
#define USER_LEN   24

int pw_summe(char* s) {
    int h;
    h = 0x1234;
    while (*s) { h = h * 31 + *s; s++; }
    return h;
}

/* Reads a line, but shows stars instead of the characters. */
int passwort_lesen(char* buf, int max) {
    int n; int k; int c; int code;
    n = 0;
    while (1) {
        k = getkey();
        c = keychar(k);
        code = keycode(k);
        if (code == K_ENTER) { buf[n] = 0; nl(); return n; }
        if (code == K_BACKSPACE) {
            if (n > 0) { n--; putch(8); }
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) {
            buf[n] = c;
            n++;
            putcolor('*', BRIGHT);
        }
    }
}

/* The account ALWAYS lives in the root directory -- no matter which folder
   the user happens to be in. fs_write and fs_read otherwise work in the
   current folder, and then USER.DAT would end up wherever the file window
   happened to be. That's exactly what happened: on one disk a USER.DAT
   ended up in \SYSTEM -- invisible to the login (which looks in the root
   directory) and protected from being reset (which leaves \SYSTEM alone).
   The machine kept asking for first-time setup after every boot, and the
   account could no longer be gotten rid of. */
void benutzer_anlegen(char* name, char* pw) {
    int n; int alt;
    memset((char*)USER_BUF, 0, USER_LEN);
    strncpy((char*)(USER_BUF + USER_NAME), name, 19);
    mem_put(USER_BUF + USER_HASH, pw_summe(pw));
    alt = cwd; cwd = 0 - 1;
    fs_write("USER.DAT", USER_BUF, USER_LEN);
    n = fs_find("USER.DAT");
    if (n >= 0) { ent_verstecken(n); fs_save_dir(); }
    cwd = alt;
}

/* A machine without a password is open -- so the \SYSTEM protection doesn't
   ask either in that case. Called from fs.c, which is included earlier and
   so doesn't know USER_BUF yet. */
int konto_offen() {
    return pw_summe("") == mem_get(USER_BUF + USER_HASH);
}

int benutzer_passt(char* pw) {
    return pw_summe(pw) == mem_get(USER_BUF + USER_HASH);
}

int benutzer_vorhanden() {
    int alt; int n;
    alt = cwd; cwd = 0 - 1;
    n = fs_read("USER.DAT", USER_BUF, USER_LEN);
    cwd = alt;
    return n == USER_LEN;
}

char* benutzer_name() { return (char*)(USER_BUF + USER_NAME); }

void ersteinrichtung() {
    char name[24];
    char pw1[32];
    char pw2[32];

    nl();
    printc("Welcome to TOOBAD-OS
", CYAN);
    print("This is the first start of this machine.\n\n");

    while (1) {
        print("User name: ");
        if (readline(name, 20) > 0) { nl(); break; }
        nl();
        printc("The name must not be empty.\n", RED);
    }
    while (1) {
        print("Password:  ");
        passwort_lesen(pw1, 30);
        print("Repeat:    ");
        passwort_lesen(pw2, 30);
        if (strcmp(pw1, pw2) == 0) break;
        printc("The two entries differ. Try again.\n\n", RED);
    }

    benutzer_anlegen(name, pw1);        /* stores it in the root directory */
    if (benutzer_vorhanden() == 0)
        printc("\nCould not save the account.\n", RED);
    else {
        nl();
        printc("Account created.\n", GREEN);
        print("Keep it -- deleting USER.DAT is the only way back in.\n");
    }
    nl();
}

void anmelden() {
    char pw[32];
    int falsch;
    falsch = 0;
    nl();
    print("User: ");
    printc((char*)(USER_BUF + USER_NAME), BRIGHT);
    nl();
    while (1) {
        print("Password: ");
        passwort_lesen(pw, 30);
        if (pw_summe(pw) == mem_get(USER_BUF + USER_HASH)) {
            printc("Welcome back.\n\n", GREEN);
            return;
        }
        falsch++;
        printc("Wrong password.\n", RED);
        /* Wait a little longer after each failed attempt -- that makes
           brute-force guessing tedious without locking anyone out. */
        sleep(falsch * 50);
    }
}

void benutzer_pruefen() {
    if (benutzer_vorhanden() == 0) {
        ersteinrichtung();               /* fresh disk: run first-time setup */
        return;
    }
    /* An empty password means: this machine is not locked. That way a
       freshly built machine boots without being asked -- and so do the
       test tools, which have nobody to type for them. Anyone who sets a
       password will be asked. */
    if (mem_get(USER_BUF + USER_HASH) == pw_summe("")) return;
    anmelden();
}

int main() {
    int formatiert;

    boot_screen();
    printc("TOOBAD-OS 2.5.2\n", CYAN);
    print("Copyright (C) Toobad. All rights reserved.\n\n");

    syscall_init();
    mt_enable();                       /* multitasking on, shell becomes process 0 */
    print("Mounting file system on A: ... ");
    formatiert = fs_mount();
    if (formatiert) printc("OK\n", GREEN);
    else printc("new disk, initialised\n", YELLOW);

    net_start();                     /* network card and our own address */

    /* Boot target: desktop or text console. Stored in the CMOS next to
       Quick Boot, changeable in Setup and in the Control Panel. Default is
       the desktop -- the test tools get their own CMOS with the console
       selected, because they read out the TEXT screen. */
    if (cmos_get(CM_BOOTMODE) == 0) {
        /* The blitter needs the address of the character set, otherwise
           it paints from nothing -- the login screen was nearly blank.
           gui_main() would otherwise set it itself, but here it only runs
           afterwards. */
        sys_setmode(1 + 256);
        sys_out(P_BLT_SRC, (int)font8);
        sys_out(P_MCUR_ON, 0);
        sys_flushkeys();
        /* Logging out leads back to the login screen without powering the
           machine off -- hence the loop. After that it ALWAYS asks, even
           for an open account: whoever logs out wants exactly that. */
        formatiert = 0;
        while (1) {
            /* gui_main() switches back to text mode on exit. So graphics
               mode has to be turned back on before every pass, otherwise
               the login screen paints into nothing. */
            sys_setmode(1 + 256);
            sys_out(P_BLT_SRC, (int)font8);
            if (benutzer_vorhanden() == 0) gui_anmelden(1);
            else if (formatiert || mem_get(USER_BUF + USER_HASH) != pw_summe(""))
                gui_anmelden(0);
            gui_abmelden = 0;
            gui_main();
            if (gui_abmelden == 0) break;
            formatiert = 1;
        }
        sys_setmode(0);
        cls(NORMAL);
    } else {
        benutzer_pruefen();
    }

    print("Starting command interpreter ...\n\n");
    print("Type ");
    printc("HELP", BRIGHT);
    print(" for a list of commands, ");
    printc("WIN", BRIGHT);
    print(" for the desktop.\n\n");

    shell();
    return 0;
}
