/* ==========================================================================
   Prozesse und Multitasking

   Der Timer meldet sich 100-mal pro Sekunde per Interrupt. Bei jedem dieser
   Interrupts sichert der Kernel die Register des laufenden Programms auf
   dessen eigenen Stack, sucht den naechsten rechenbereiten Prozess und macht
   dort weiter. Genau das nennt man praeemptives Multitasking -- der gleiche
   Trick, mit dem jeder echte PC hunderte Programme "gleichzeitig" laufen
   laesst, obwohl es nur einen Prozessor gibt.

   Ein Prozess besteht bei uns aus:
       Zustand (0 = frei, 1 = bereit, 2 = laeuft, 3 = schlaeft, 4 = beendet)
       gesicherter Stackpointer
       eigener Stackbereich
       Startadresse
   ========================================================================== */

#define MAXPROC     8
#define PSTACK_BASE 0x000A0000       /* je Prozess 8 KB Stack, ueber dem Kernel-Stack */
#define PSTACK_SIZE 8192

#define P_BLT_ZIEL   0x5B
#define P_BLT_ZIELB  0x5C
#define P_BLT_ZIELH  0x5D
#define P_BLT_SRC_P  0x4B

#define PS_FREI     0
#define PS_BEREIT   1
#define PS_LAEUFT   2
#define PS_SCHLAEFT 3

int p_state[MAXPROC];
int p_sp[MAXPROC];
/* Der Blitter gehoert dem Prozess, der gerade malt.
   Ohne das hier war der Bildschirm schwarz, sobald ein Programm ein eigenes
   Fenster hatte: es stellte den Blitter auf seinen Bildpuffer, wurde mitten
   im Malen unterbrochen, und der Schreibtisch malte danach SEIN Bild in den
   fremden Puffer statt auf den Schirm. Der Zustand der Grafikhardware muss
   beim Wechsel gesichert werden -- genau wie die Register. */
int p_blt_ziel[MAXPROC];
int p_blt_b[MAXPROC];
int p_blt_h[MAXPROC];
int p_blt_src[MAXPROC];
int p_wake[MAXPROC];                 /* Weckzeit fuer schlafende Prozesse */
int p_ticks[MAXPROC];                /* wie viel Rechenzeit verbraucht wurde */
int p_bg[MAXPROC];                   /* 1 = im Hintergrund gestartet (/B) */
char p_name[MAXPROC * 16];
int p_current = 0;
int p_switches = 0;
int mt_active = 0;                   /* Multitasking eingeschaltet? */

char* proc_name(int i) { return (char*)((int)p_name + i * 16); }

void proc_init() {
    int i;
    for (i = 0; i < MAXPROC; i++) {
        p_state[i] = PS_FREI;
        p_ticks[i] = 0;
    }
    p_state[0] = PS_LAEUFT;              /* Prozess 0 ist die Shell selbst */
    strncpy(proc_name(0), "shell", 15);
    p_current = 0;
}

/* Legt einen neuen Prozess an. Sein Stack wird so vorbereitet, als waere er
   gerade von einem Timer-Interrupt unterbrochen worden -- dann kann ihn der
   Umschalter genauso fortsetzen wie jeden anderen. */
int proc_start(char* name, int einsprung) {
    int i; int sp; int j;
    for (i = 1; i < MAXPROC; i++) {
        if (p_state[i] != PS_FREI) continue;

        sp = PSTACK_BASE + i * PSTACK_SIZE + PSTACK_SIZE - 16;

        /* Der Umschalter erwartet auf dem Stack (von oben nach unten):
           FLAGS, Ruecksprungadresse, danach die 15 gesicherten Register
           r0 bis r14. */
        sp = sp - 4;  mem_put(sp, 512);             /* FLAGS mit Interrupt-Bit */
        sp = sp - 4;  mem_put(sp, einsprung);       /* dorthin kehrt iret zurueck */
        for (j = 0; j <= 14; j++) { sp = sp - 4; mem_put(sp, 0); }

        p_sp[i] = sp;
        p_wake[i] = 0;
        p_ticks[i] = 0;
        /* Der Platz wird wiederverwendet -- die Marke des Vormieters muss
           weg. Stand hier noch die 1 eines Hintergrundprogramms, dann galt
           auch der Nachfolger als Hintergrund und bekam nie eine Taste:
           nach dem Uebersetzen fiel der Vogel in Flappy sofort herunter,
           weil die Kommandozeile den frei gewordenen Platz des Compilers
           geerbt hatte. */
        p_bg[i] = 0;
        p_blt_ziel[i] = 0;               /* ein neuer Prozess malt auf den Schirm */
        p_blt_b[i] = 0;
        p_blt_h[i] = 0;
        p_blt_src[i] = 0;
        memset(proc_name(i), 0, 16);
        strncpy(proc_name(i), name, 15);
        p_state[i] = PS_BEREIT;
        return i;
    }
    return 0 - 1;
}

/* Wird vom Umschalter in start.asm gerufen: alten Stackpointer merken,
   naechsten Prozess auswaehlen, dessen Stackpointer zurueckgeben. */
int proc_schedule(int alter_sp) {
    p_sp[p_current] = alter_sp;
    p_blt_ziel[p_current] = sys_in(P_BLT_ZIEL);
    p_blt_b[p_current] = sys_in(P_BLT_ZIELB);
    p_blt_h[p_current] = sys_in(P_BLT_ZIELH);
    p_blt_src[p_current] = sys_in(P_BLT_SRC_P);
    p_ticks[p_current]++;
    if (p_state[p_current] == PS_LAEUFT) p_state[p_current] = PS_BEREIT;
    p_current = proc_next();
    p_state[p_current] = PS_LAEUFT;
    sys_out(P_BLT_ZIEL, p_blt_ziel[p_current]);
    sys_out(P_BLT_ZIELB, p_blt_b[p_current]);
    sys_out(P_BLT_ZIELH, p_blt_h[p_current]);
    sys_out(P_BLT_SRC_P, p_blt_src[p_current]);
    p_switches++;
    return p_sp[p_current];
}

/* Sucht den naechsten rechenbereiten Prozess (Reihum-Verfahren). */
int proc_next() {
    int i; int k; int jetzt;
    jetzt = sys_ticks();
    for (i = 1; i <= MAXPROC; i++) {
        k = (p_current + i) % MAXPROC;
        if (p_state[k] == PS_SCHLAEFT && jetzt >= p_wake[k])
            p_state[k] = PS_BEREIT;
        if (p_state[k] == PS_BEREIT || p_state[k] == PS_LAEUFT)
            return k;
    }
    return p_current;
}

void proc_exit() {
    if (screen_owner == p_current + 1) {     /* Sperre nicht mit ins Grab nehmen */
        screen_owner = 0;
        screen_depth = 0;
    }
    if (p_current == 0) return;
    p_state[p_current] = PS_FREI;
    /* Wir stecken hier in einem Systemaufruf, und dort sind die Interrupts
       gesperrt -- normalerweise gibt das iret sie wieder frei. Das kommt
       aber nie, weil wir nicht zurueckkehren. Also von Hand freigeben,
       sonst bekaeme der Timer nie wieder das Wort und der Scheduler
       (und damit das ganze System) stuende still. */
    asm("sti");
    while (1) asm("hlt");                /* bis der Scheduler umschaltet */
}

void proc_sleep(int ticks) {
    int ziel;
    if (mt_active == 0) { sleep(ticks); return; }
    ziel = sys_ticks() + ticks;
    p_wake[p_current] = ziel;
    p_state[p_current] = PS_SCHLAEFT;
    asm("int 0x41");                     /* freiwillig abgeben */

    /* Wichtig: proc_next() gibt den eigenen Prozess sofort wieder zurueck,
       wenn sonst niemand rechnen will -- sonst haette der Umschalter gar
       keinen, den er nehmen koennte. Damit war das Schlafen aber wirkungslos:
       wir kamen unmittelbar zurueck, schliefen wieder, kamen zurueck ... und
       der Prozessor lief die ganze Zeit auf Volllast. Ein Spiel mit Bildtakt
       hat den TB-32 so auf 65 Grad geheizt und auf 40 Prozent Takt gedrosselt.

       Also warten wir den Rest hier selbst ab -- mit angehaltenem Prozessor.
       Das hlt weckt der Timer; wird waehrenddessen ein anderer Prozess
       rechenbereit, schaltet der Umschalter im selben Interrupt zu ihm.
       Das sti braucht es, weil wir in einem Systemaufruf stecken und die
       Interrupts dort gesperrt sind -- ohne kaeme der Timer nie. */
    asm("sti");
    while (sys_ticks() < ziel) asm("hlt");
}

int proc_count() {
    int i; int n;
    n = 0;
    for (i = 0; i < MAXPROC; i++) if (p_state[i] != PS_FREI) n++;
    return n;
}

/* --- Multitasking ein- und ausschalten ----------------------------------- */

int sched_irq_asm();                     /* steht in start.asm */
int alt_timer_handler = 0;

void mt_enable() {
    if (mt_active) return;
    proc_init();
    alt_timer_handler = mem_get(0x08 * 4);
    asm("cli");
    mem_put(0x08 * 4, (int)sched_irq_asm);      /* Timer-Interrupt umbiegen */
    mem_put(0x41 * 4, (int)sched_irq_asm);      /* freiwillige Abgabe */
    mt_active = 1;
    asm("sti");
}

void mt_disable() {
    if (mt_active == 0) return;
    asm("cli");
    mem_put(0x08 * 4, alt_timer_handler);
    mt_active = 0;
    asm("sti");
}

/* Beim Systemstart wird das Multitasking sofort aktiviert -- ab dann ist die
   Kommandozeile Prozess 0 und jedes mit START /B gestartete Programm bekommt
   seine eigene Zeitscheibe. */
