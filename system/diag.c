/* ==========================================================================
   Diagnose: Testbild fuer die Grafikkarte

   Ersetzt das frueher hier liegende Spiel. Ein Anzeigetest gehoert zu jedem
   ernsthaften System -- er prueft Aufloesung, Palette und Bildaufbau.
   ========================================================================== */

#define VRAM_GFX  0x02100000
#define GFX_W     640
#define GFX_H     400

void px(int x, int y, int farbe) {
    char* fb;
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return;
    fb = (char*)VRAM_GFX;
    fb[y * GFX_W + x] = farbe;
}

/* Flaeche ueber den 2D-Beschleuniger fuellen -- hunderte Male schneller,
   als jeden Punkt einzeln ueber den Bus zu schreiben. */
void gfx_rect(int x, int y, int w, int h, int farbe) {
    sys_out(0x44, x);
    sys_out(0x45, y);
    sys_out(0x46, w);
    sys_out(0x47, h);
    sys_out(0x48, farbe);
    sys_out(0x49, 1);
}

/* Zeichnet ein Zeichen aus dem 8x8-Zeichensatz ueber den Beschleuniger */
void dg_char(int x, int y, int c, int col) {
    sys_out(0x44, x);
    sys_out(0x45, y);
    sys_out(0x48, col);
    sys_out(0x4C, 256);
    sys_out(0x4A, c);
    sys_out(0x49, 3);
}

void dg_text(int x, int y, char* s, int col) {
    while (*s) {
        dg_char(x, y, *s, col);
        x = x + 8;
        s++;
    }
}

void display_test() {
    int i; int x; int y; int k;

    sys_setmode(1 + 256);
    sys_out(0x4B, (int)font8);

    dg_text(8, 8, "TB-VGA DISPLAY ADAPTER TEST", 15);
    dg_text(8, 20, "640 x 400 pixels, 256 colours", 7);

    /* 16 Grundfarben als Balken */
    dg_text(8, 40, "COLOUR BARS", 15);
    for (i = 0; i < 16; i++)
        gfx_rect(8 + i * 39, 52, 38, 40, i);

    /* Graustufen */
    dg_text(8, 100, "GREYSCALE RAMP", 15);
    for (i = 0; i < 64; i++)
        gfx_rect(8 + i * 9, 112, 9, 30, 232 + (i / 3));

    /* Farbwuerfel */
    dg_text(8, 150, "COLOUR CUBE", 15);
    for (i = 0; i < 216; i++)
        gfx_rect(8 + (i % 36) * 17, 162 + (i / 36) * 14, 16, 13, 16 + i);

    /* Gitter zur Pruefung der Geometrie */
    dg_text(8, 258, "GEOMETRY GRID", 15);
    for (x = 8; x < 632; x = x + 16) gfx_rect(x, 270, 1, 110, 8);
    for (y = 270; y < 380; y = y + 16) gfx_rect(8, y, 624, 1, 8);
    gfx_rect(8, 270, 624, 1, 15);
    gfx_rect(8, 379, 624, 1, 15);
    gfx_rect(8, 270, 1, 110, 15);
    gfx_rect(631, 270, 1, 110, 15);
    for (i = 0; i < 110; i++) {          /* Diagonalen */
        px(8 + i * 5, 270 + i, 15);
        px(631 - i * 5, 270 + i, 15);
    }

    dg_text(8, 386, "Press any key to return", 7);
    sys_flushkeys();
    getkey();
    sys_setmode(0 + 256);
    sys_cls(NORMAL);
    sys_setcursor(0, 0);
}
