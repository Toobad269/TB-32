/* ==========================================================================
   Kopfloser Start der C-Fassung -- zum Vergleichen mit Python

       ./tb32 [Sekunden] [Tastenfolge]

   Bootet den Rechner, laesst ihn die angegebene Zeit laufen und schreibt
   danach den Textbildschirm auf die Ausgabe. Damit lassen sich beide
   Fassungen Zeile fuer Zeile gegeneinander pruefen: derselbe Bootvorgang,
   dasselbe Bild.
   ========================================================================== */

#include "tb32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void schirm_zeigen(Machine *m) {
    int y, x;
    for (y = 0; y < 25; y++) {
        char zeile[81];
        int len = 0;
        for (x = 0; x < 80; x++) {
            int c = m->text[(y * 80 + x) * 2];
            zeile[x] = (c >= 32 && c < 127) ? (char)c : ' ';
            if (zeile[x] != ' ') len = x + 1;
        }
        zeile[len] = 0;
        printf("%s\n", zeile);
    }
}

int main(int argc, char **argv) {
    double sekunden = (argc > 1) ? atof(argv[1]) : 4.0;
    /* Leerer String heisst "nichts tippen" -- so kann man das CMOS als
       vierten Parameter mitgeben, ohne eine Taste zu senden. */
    const char *tasten = (argc > 2 && argv[2][0]) ? argv[2] : NULL;
    const double dt = 1.0 / 60.0;
    int schritte, i;
    clock_t t0;
    double echt;
    Machine *m;

    m = m_new("firmware/bios.bin", "disk/hd0.img",
                          argc > 3 ? argv[3] : "disk/cmos.bin");
    if (!m) {
        fprintf(stderr, "Maschine liess sich nicht bauen. "
                        "Erst 'python3 build.py' laufen lassen.\n");
        return 1;
    }
    m_power_on(m);

    schritte = (int)(sekunden / dt);
    t0 = clock();
    for (i = 0; i < schritte; i++) {
        m_run_slice(m, dt);
        if (!m->powered) break;
    }

    /* Tastenfolge nachreichen und weiterlaufen lassen */
    if (tasten) {
        size_t k;
        for (k = 0; k < strlen(tasten); k++) {
            m_key_push(m, tasten[k], 0);
            for (i = 0; i < 6; i++) m_run_slice(m, dt);
        }
        m_key_push(m, 13, 0x1C);                    /* Eingabetaste */
        for (i = 0; i < 120; i++) m_run_slice(m, dt);
    }
    echt = (double)(clock() - t0) / CLOCKS_PER_SEC;

    schirm_zeigen(m);
    fprintf(stderr, "\n%llu Befehle in %.3f s echter Zeit = %.1f Millionen/s\n",
            (unsigned long long)m->befehle_gesamt, echt,
            (double)m->befehle_gesamt / echt / 1e6);
    if (m->fault[0]) fprintf(stderr, "Fehler: %s\n", m->fault);
    m_free(m);
    return 0;
}
