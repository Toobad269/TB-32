/* ==========================================================================
   CONTROL PANEL  --  the hardware settings, as its own program

   Clock speed, POST beeper, quick boot, messages, fan. All of it lives in
   the coin-cell battery (CMOS) or in a port -- a program reaches both
   directly, since ports on the TB-32 are not protected. That's why this
   move didn't need a single new system call.

   Compile on the device itself:  CC CONTROL.C
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

#define C_BLACK    0
#define C_WHITE   15
#define C_TEXT     0
#define C_WINDARK  8
#define C_ACCENT   9
#define C_WARN     4
#define C_GOOD     2
#define C_WINBG    7
#define C_TITLEBAR 1

#define P_CMOS_IDX  0x70
#define P_CMOS_DAT  0x71
#define P_TEMP      0xA0
#define P_FANMODE   0xA4

int ctrl_sel = 0;
int ctrl_gesichert = 0;

void gx_num(int x, int y, int n, int farbe, int bg) { gx_zahl(x, y, n, farbe); }

void p_knopf(int x, int y, int w, int h, char* text, int gedrueckt) {
    gx_panel(x, y, w, h, gedrueckt);
    if (text[0])
        gx_text_mitte(x + gedrueckt, y + (h - 8) / 2 + gedrueckt, w, text, C_TEXT);
}

int cmos_get(int reg) {
    portout(P_CMOS_IDX, reg);
    return portin(P_CMOS_DAT);
}

void cmos_set(int reg, int wert) {
    portout(P_CMOS_IDX, reg);
    portout(P_CMOS_DAT, wert);
}

char* speed_name(int i) {
    if (i == 0) return "0.4 MHz";
    if (i == 1) return "1 MHz";
    if (i == 2) return "2 MHz";
    if (i == 3) return "4 MHz";
    return "8 MHz";
}

void app_control(int w) {
    int x; int y; int attr; int i; int k;
    x = 8;
    y = 8;
    gx_fill(0, 0, fn_breite, fn_hoehe, C_WINBG);

    gx_text(x, y, "Hardware settings (stored in CMOS)", C_ACCENT, 256);

    for (i = 0; i < 5; i++) {
        attr = C_TEXT;
        if (i == ctrl_sel) {
            gx_fill(x - 4, y + 16 + i * 14, fn_breite - 12, 12, C_TITLEBAR);
            attr = C_WHITE;
        }
        if (i == 0) {
            gx_text(x, y + 18, "CPU clock speed", attr, 256);
            gx_text(x + 176, y + 18, speed_name(cmos_get(0x13)), attr, 256);
        }
        if (i == 1) {
            gx_text(x, y + 32, "POST beep", attr, 256);
            if (cmos_get(0x12)) gx_text(x + 176, y + 32, "Enabled", attr, 256);
            else                gx_text(x + 176, y + 32, "Disabled", attr, 256);
        }
        if (i == 2) {
            gx_text(x, y + 46, "Quick boot", attr, 256);
            if (cmos_get(0x11)) gx_text(x + 176, y + 46, "Enabled", attr, 256);
            else                gx_text(x + 176, y + 46, "Disabled", attr, 256);
        }
        if (i == 3) {
            gx_text(x, y + 60, "POST messages", attr, 256);
            if (cmos_get(0x15)) gx_text(x + 176, y + 60, "Verbose", attr, 256);
            else                gx_text(x + 176, y + 60, "Minimal", attr, 256);
        }
        if (i == 4) {
            gx_text(x, y + 74, "Fan control", attr, 256);
            k = portin(P_FANMODE);
            if (k == 0)      gx_text(x + 176, y + 74, "Automatic", attr, 256);
            else if (k == 1) gx_text(x + 176, y + 74, "Quiet", attr, 256);
            else if (k == 2) gx_text(x + 176, y + 74, "Full speed", attr, 256);
            else             gx_text(x + 176, y + 74, "Manual", attr, 256);
        }
    }

    gx_text(x, y + 92, "Click a row to change the value.", C_WINDARK, 256);
    p_knopf(x, y + 104, 96, 16, "Save to CMOS", 0);
    /* Without feedback nobody would know whether the click registered --
       the values look the same before and after. The message disappears
       on its own because the window refreshes every second. */
    if (ctrl_gesichert > 0) {
        if (ticks() - ctrl_gesichert < 300)
            gx_text(x + 104, y + 108, "Saved", C_GOOD, 256);
        else
            ctrl_gesichert = 0;
    }
    gx_text(x + 110, y + 108, "Temperature:", C_WINDARK, 256);
    gx_num(x + 210, y + 108, portin(P_TEMP) / 10, C_ACCENT, 256);
    gx_text(x + 234, y + 108, "C", C_WINDARK, 256);
}

void control_click(int w, int mx, int my) {
    int y; int zeile; int v;
    y = 8;

    /* Also check the width. Before, only the row was checked here, and a
       click anywhere else on that row -- for example on the temperature
       display to the right -- would write to the CMOS. */
    if (my >= y + 104 && my < y + 120) {
        if (mx >= 8 && mx < 8 + 96) {
            cmos_set(0x3F, 1);
            ctrl_gesichert = ticks();
        }
        return;
    }
    zeile = (my - (y + 16)) / 14;
    if (zeile < 0 || zeile > 4) return;
    ctrl_sel = zeile;
    if (zeile == 0) {
        v = cmos_get(0x13) + 1;
        if (v > 4) v = 0;
        cmos_set(0x13, v);
    }
    if (zeile == 1) cmos_set(0x12, 1 - cmos_get(0x12));
    if (zeile == 2) cmos_set(0x11, 1 - cmos_get(0x11));
    if (zeile == 3) cmos_set(0x15, 1 - cmos_get(0x15));
    if (zeile == 4) {                            /* Cycle fan mode */
        v = portin(P_FANMODE) + 1;
        if (v > 2) v = 0;
        portout(P_FANMODE, v);
    }
}

int main() {
    int e[4];
    int art; int laufen;

    if (fenster_neu("Control Panel", 340, 200) < 0) {
        print("Needs the desktop -- type WIN first.\n");
        return 1;
    }
    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);
        if (art == FE_SCHLIESS) laufen = 0;
        else if (art == FE_TASTE && e[2] == K_ESC) laufen = 0;
        else if (art == FE_KLICK) control_click(0, e[1], e[2]);
        fenster_malziel();
        app_control(0);
        fenster_fertig();
        if (art == FE_NICHTS) sleep(50);      /* Temperature keeps updating */
    }
    fenster_zu();
    return 0;
}
