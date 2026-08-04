#include "proglib.c"
#include "gfxlib.c"

int spur[64];
int n = 0;

int main() {
    int kopf; int x; int y;
    /* Frueher schrieb dieses Programm in die Textkonsole -- im
       Schreibtisch sah man davon nichts. Jetzt bekommt es ein
       Fenster mit Textausgabe. */
    if (tf_neu("KELLERTEST") < 0) return 1;

    spur[0] = 0; spur[1] = 0;
    kopf = 2;
    while (kopf > 0) {
        kopf = kopf - 2;
        x = spur[kopf];
        y = spur[kopf + 1];
        if (x > 10) continue;
        n++;
        if (kopf + 2 < 60) {
            spur[kopf] = x + 1;
            spur[kopf + 1] = y;
            kopf = kopf + 2;
        }
    }
    tf_text("Durchlaeufe: "); tf_zahl(n); tf_text("\n");
    tf_warten();
    return 0;
}
