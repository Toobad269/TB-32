/* ==========================================================================
   Bus, Geraete und Maschine -- die C-Fassung

   Hier stehen dieselben Bausteine wie in hardware/devices.py: Grafikkarte
   mit Blitter, Tastatur, Platte, Timer, CMOS, Blockkopierer, Maus, Waerme.
   Der Bus verteilt Speicherzugriffe und Portbefehle -- genau wie das echte
   Mainboard, das entscheidet, welcher Chip gemeint ist.
   ========================================================================== */

#include "tb32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Taktstufen aus dem BIOS-Setup (muss zu hardware/machine.py passen) */
static const double CPU_SPEEDS[5] = {400000, 1000000, 2000000, 4000000, 8000000};

#define CMOS_SEC       0x00
#define CMOS_MIN       0x02
#define CMOS_HOUR      0x04
#define CMOS_WDAY      0x06
#define CMOS_DAY       0x07
#define CMOS_MONTH     0x08
#define CMOS_YEAR      0x09
#define CMOS_CPUSPEED  0x13
#define CMOS_MAGIC     0x2F

/* --- Speicher ------------------------------------------------------------ */

uint8_t bus_read8(Machine *m, uint32_t addr) {
    if (addr < RAM_SIZE) return m->ram[addr];
    if (addr >= VRAM_TEXT && addr < VRAM_TEXT + VRAM_TEXT_SIZE)
        return m->text[addr - VRAM_TEXT];
    if (addr >= VRAM_GFX && addr < VRAM_GFX + VRAM_GFX_SIZE)
        return m->gfx[addr - VRAM_GFX];
    if (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE)
        return m->rom[addr - ROM_BASE];
    return 0xFF;
}

void bus_write8(Machine *m, uint32_t addr, uint8_t val) {
    if (addr < RAM_SIZE) { m->ram[addr] = val; return; }
    if (addr >= VRAM_TEXT && addr < VRAM_TEXT + VRAM_TEXT_SIZE) {
        m->text[addr - VRAM_TEXT] = val;
        m->dirty = 1;
        return;
    }
    if (addr >= VRAM_GFX && addr < VRAM_GFX + VRAM_GFX_SIZE) {
        m->gfx[addr - VRAM_GFX] = val;
        m->dirty = 1;
        return;
    }
    /* ROM ist nur lesbar -- Schreibversuche werden still verworfen, genau
       wie bei einem echten Flash-Baustein ohne Freigabe. */
}

uint32_t bus_read32(Machine *m, uint32_t addr) {
    if (addr + 3 < RAM_SIZE) {
        const uint8_t *p = m->ram + addr;
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
             | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return (uint32_t)bus_read8(m, addr)
         | ((uint32_t)bus_read8(m, addr + 1) << 8)
         | ((uint32_t)bus_read8(m, addr + 2) << 16)
         | ((uint32_t)bus_read8(m, addr + 3) << 24);
}

void bus_write32(Machine *m, uint32_t addr, uint32_t val) {
    if (addr + 3 < RAM_SIZE) {
        uint8_t *p = m->ram + addr;
        p[0] = (uint8_t)val;
        p[1] = (uint8_t)(val >> 8);
        p[2] = (uint8_t)(val >> 16);
        p[3] = (uint8_t)(val >> 24);
        return;
    }
    bus_write8(m, addr,     (uint8_t)val);
    bus_write8(m, addr + 1, (uint8_t)(val >> 8));
    bus_write8(m, addr + 2, (uint8_t)(val >> 16));
    bus_write8(m, addr + 3, (uint8_t)(val >> 24));
}

static void block_read(Machine *m, uint32_t addr, uint8_t *ziel, uint32_t n) {
    uint32_t i;
    if (addr + n <= RAM_SIZE) { memcpy(ziel, m->ram + addr, n); return; }
    for (i = 0; i < n; i++) ziel[i] = bus_read8(m, addr + i);
}

static void block_write(Machine *m, uint32_t addr, const uint8_t *quelle, uint32_t n) {
    uint32_t i;
    if (addr + n <= RAM_SIZE) { memcpy(m->ram + addr, quelle, n); return; }
    for (i = 0; i < n; i++) bus_write8(m, addr + i, quelle[i]);
}

/* --- Grafikkarte ---------------------------------------------------------- */

static void vga_clear_text(Machine *m, uint8_t attr) {
    uint32_t i;
    for (i = 0; i < VRAM_TEXT_SIZE; i += 2) {
        m->text[i] = 0x20;
        m->text[i + 1] = attr;
    }
    m->dirty = 1;
}

static void palette_init(Machine *m) {
    static const uint32_t basis[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA,
        0xAA5500, 0xAAAAAA, 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    int i, r, g, b, n = 0;
    for (i = 0; i < 16; i++) m->palette[n++] = basis[i];
    for (r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++)
                m->palette[n++] = ((uint32_t)(r * 51) << 16)
                                | ((uint32_t)(g * 51) << 8) | (uint32_t)(b * 51);
    while (n < 256) {
        int v = (n - 232) * 10 + 8;
        if (v > 255) v = 255;
        m->palette[n] = ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v;
        n++;
    }
}

/* Fuer jedes Bitmuster einer Zeichensatzzeile: welche Spalten sind gesetzt? */
static uint8_t gesetzt[256][9];

static void gesetzt_init(void) {
    int bits, c, n;
    for (bits = 0; bits < 256; bits++) {
        n = 0;
        for (c = 0; c < 8; c++)
            if (bits & (0x80 >> c)) gesetzt[bits][n++] = (uint8_t)c;
        gesetzt[bits][8] = (uint8_t)n;
    }
}

static void blit(Machine *m, int cmd) {
    int x = m->blt_x, y = m->blt_y, w = m->blt_w, h = m->blt_h;
    uint8_t col = (uint8_t)(m->blt_col & 0xFF);
    uint8_t *fb;
    int GFX_W_L, GFX_H_L;
    /* Ziel: Bildschirm oder ein Puffer im Arbeitsspeicher. Der Rest rechnet
       mit GFX_W_L/GFX_H_L -- die werden hier zu den Groessen des Ziels, und
       nichts weiter unten muss davon wissen. Siehe hardware/devices.py, das
       muss dieselbe Rechnung machen. */
    if (m->blt_ziel) {
        GFX_W_L = (int)m->blt_zielb;
        GFX_H_L = (int)m->blt_zielh;
        if (GFX_W_L <= 0 || GFX_H_L <= 0) return;
        fb = m->ram + m->blt_ziel;
    } else {
        GFX_W_L = GFX_W;
        GFX_H_L = GFX_H;
        fb = m->gfx;
        m->dirty = 1;
    }

    if (cmd == 1) {                               /* gefuellte Flaeche */
        int x0, y0, x1, y1, breit, yy;
        if (w <= 0 || h <= 0) return;
        x0 = x < 0 ? 0 : x;  y0 = y < 0 ? 0 : y;
        x1 = x + w > GFX_W_L ? GFX_W_L : x + w;
        y1 = y + h > GFX_H_L ? GFX_H_L : y + h;
        if (x1 <= x0 || y1 <= y0) return;
        breit = x1 - x0;
        for (yy = y0; yy < y1; yy++) memset(fb + yy * GFX_W_L + x0, col, (size_t)breit);
        return;
    }
    if (cmd == 2) {                               /* Rahmen */
        int xx, yy;
        for (xx = (x < 0 ? 0 : x); xx < x + w && xx < GFX_W_L; xx++) {
            if (y >= 0 && y < GFX_H_L)             fb[y * GFX_W_L + xx] = col;
            if (y + h - 1 >= 0 && y + h - 1 < GFX_H_L) fb[(y + h - 1) * GFX_W_L + xx] = col;
        }
        for (yy = (y < 0 ? 0 : y); yy < y + h && yy < GFX_H_L; yy++) {
            if (x >= 0 && x < GFX_W_L)             fb[yy * GFX_W_L + x] = col;
            if (x + w - 1 >= 0 && x + w - 1 < GFX_W_L) fb[yy * GFX_W_L + x + w - 1] = col;
        }
        return;
    }
    if (cmd == 3) {                               /* Zeichen */
        int code = (int)(m->blt_chr & 0xFF);
        uint32_t bg = m->blt_bg;
        int zoom = (int)m->blt_zoom;
        uint8_t muster[8];
        int r, c, br;
        if (code < 32 || code > 127) code = 32;
        if (zoom < 1) zoom = 1;
        br = 8 * zoom;
        block_read(m, m->blt_src + (uint32_t)(code - 32) * 8, muster, 8);
        if (x < 0 || x > GFX_W_L - br || y < 0 || y > GFX_H_L - br) return;
        for (r = 0; r < 8; r++) {
            int zr;
            for (zr = 0; zr < zoom; zr++) {
                uint8_t *zeile = fb + (y + r * zoom + zr) * GFX_W_L + x;
                for (c = 0; c < br; c++) {
                    if (muster[r] & (0x80 >> (c / zoom))) zeile[c] = col;
                    else if (bg < 256) zeile[c] = (uint8_t)bg;
                }
            }
        }
        return;
    }
    if (cmd == 4) {                               /* Bild aus dem RAM */
        int r, c;
        uint8_t *zeile;
        if (w <= 0 || h <= 0) return;
        zeile = (uint8_t *)malloc((size_t)w);
        if (!zeile) return;
        for (r = 0; r < h; r++) {
            int yy = y + r;
            if (yy < 0 || yy >= GFX_H_L) continue;
            block_read(m, m->blt_src + (uint32_t)(r * w), zeile, (uint32_t)w);
            for (c = 0; c < w; c++) {
                int xx = x + c;
                if (xx < 0 || xx >= GFX_W_L) continue;
                if (zeile[c] != 255) fb[yy * GFX_W_L + xx] = zeile[c];
            }
        }
        free(zeile);
        return;
    }
    if (cmd == 5) {                               /* Bereich kopieren */
        int sx = (int)m->blt_chr, sy = (int)m->blt_src, r;
        for (r = 0; r < h; r++) {
            if (sy + r < 0 || sy + r >= GFX_H_L) continue;
            if (y + r < 0 || y + r >= GFX_H_L) continue;
            memmove(fb + (y + r) * GFX_W_L + x, fb + (sy + r) * GFX_W_L + sx, (size_t)w);
        }
        return;
    }
    if (cmd == 6) {                               /* Zeichenkette aus dem RAM */
        int i, r, c, zoom = (int)m->blt_zoom, br;
        uint32_t bg = m->blt_bg;
        uint8_t *txt;
        if (w <= 0) return;
        if (zoom < 1) zoom = 1;
        br = 8 * zoom;
        if (w > 256) w = 256;
        if (y < 0 || y > GFX_H_L - br) return;
        txt = (uint8_t *)malloc((size_t)w);
        if (!txt) return;
        block_read(m, m->blt_chr, txt, (uint32_t)w);
        for (i = 0; i < w; i++) {
            int zx = x + i * br, code = txt[i];
            uint8_t muster[8];
            if (zx < 0 || zx > GFX_W_L - br) continue;
            if (code < 32 || code > 127) code = 32;
            block_read(m, m->blt_src + (uint32_t)(code - 32) * 8, muster, 8);
            for (r = 0; r < 8; r++) {
                int zr;
                for (zr = 0; zr < zoom; zr++) {
                    uint8_t *zeile = fb + (y + r * zoom + zr) * GFX_W_L + zx;
                    for (c = 0; c < br; c++) {
                        if (muster[r] & (0x80 >> (c / zoom))) zeile[c] = col;
                        else if (bg < 256) zeile[c] = (uint8_t)bg;
                    }
                }
            }
        }
        free(txt);
        return;
    }
    if (cmd == 7) {                               /* Bild skaliert */
        int qb = (int)(m->blt_chr & 0xFFFF);
        int qh = (int)((m->blt_chr >> 16) & 0xFFFF);
        int zy, zx;
        uint8_t *quelle;
        if (w <= 0 || h <= 0 || qb <= 0 || qh <= 0) return;
        quelle = (uint8_t *)malloc((size_t)qb * (size_t)qh);
        if (!quelle) return;
        block_read(m, m->blt_src, quelle, (uint32_t)(qb * qh));
        for (zy = 0; zy < h; zy++) {
            int yy = y + zy, qz;
            uint8_t *qz_zeile;
            if (yy < 0 || yy >= GFX_H_L) continue;
            qz = (zy * qh) / h;
            qz_zeile = quelle + qz * qb;
            for (zx = 0; zx < w; zx++) {
                int xx = x + zx;
                uint8_t v;
                if (xx < 0 || xx >= GFX_W_L) continue;
                v = qz_zeile[(zx * qb) / w];
                if (v != 255) fb[yy * GFX_W_L + xx] = v;
            }
        }
        free(quelle);
        return;
    }
}

static void doppel_setzen(Machine *m, int an) {
    if (an && !m->doppel) {
        uint8_t *andere = (m->gfx == m->seite_a) ? m->seite_b : m->seite_a;
        memcpy(andere, m->gfx, VRAM_GFX_SIZE);
        m->gfx_sicht = m->gfx;
        m->gfx = andere;
        m->doppel = 1;
    } else if (!an && m->doppel) {
        memcpy(m->gfx_sicht, m->gfx, VRAM_GFX_SIZE);
        m->gfx = m->gfx_sicht;
        m->doppel = 0;
    }
    m->dirty = 1;
}

/* --- Blockkopierer -------------------------------------------------------- */

static void dma_los(Machine *m, int cmd) {
    uint32_t n;
    uint8_t *puf;
    if (m->dma_len == 0) return;
    n = m->dma_len;
    if (n > (1u << 24)) n = 1u << 24;
    if (cmd == 1) {
        puf = (uint8_t *)malloc(n);
        if (!puf) return;
        block_read(m, m->dma_src, puf, n);
        block_write(m, m->dma_dst, puf, n);
        free(puf);
        m->dirty = 1;
        return;
    }
    if (cmd == 2) {
        puf = (uint8_t *)malloc(n);
        if (!puf) return;
        memset(puf, (int)(m->dma_val & 0xFF), n);
        block_write(m, m->dma_dst, puf, n);
        free(puf);
        m->dirty = 1;
        return;
    }
    /* Suchbefehle -- das Gegenstueck zu den Zeichenkettenbefehlen echter
       Prozessoren. Ergebnis landet im Laengenregister. */
    puf = (uint8_t *)malloc(n);
    if (!puf) return;
    if (cmd == 3) {                       /* wie viele Bytes ab SRC == VAL */
        uint32_t i = 0;
        block_read(m, m->dma_src, puf, n);
        while (i < n && puf[i] == (uint8_t)m->dma_val) i++;
        m->dma_len = i;
    } else if (cmd == 4) {                /* erste Stelle ab SRC mit == VAL */
        uint32_t i = 0;
        block_read(m, m->dma_src, puf, n);
        while (i < n && puf[i] != (uint8_t)m->dma_val) i++;
        m->dma_len = (i >= n) ? 0xFFFFFFFFu : i;
    } else if (cmd == 5) {                /* wie viele Bytes VOR SRC == VAL */
        uint32_t i = 0;
        block_read(m, m->dma_src - n + 1, puf, n);
        while (i < n && puf[n - 1 - i] == (uint8_t)m->dma_val) i++;
        m->dma_len = i;
    }
    free(puf);
}

/* --- Platte --------------------------------------------------------------- */

static void disk_cmd(Machine *m, uint32_t cmd) {
    uint32_t n = m->disk_count ? m->disk_count : 1;
    uint32_t bytes = n * DISK_SECT;
    uint32_t off = m->disk_lba * DISK_SECT;
    if (!m->disk || off + bytes > m->disk_sektoren * DISK_SECT) {
        m->disk_status = 1;
        return;
    }
    if (cmd == 1) {
        block_write(m, m->disk_addr, m->disk + off, bytes);
        m->disk_status = 0;
    } else if (cmd == 2) {
        block_read(m, m->disk_addr, m->disk + off, bytes);
        m->disk_dirty = 1;
        m->disk_status = 0;
    } else {
        m->disk_status = 2;
    }
}

/* --- Ports ---------------------------------------------------------------- */

uint32_t port_in(Machine *m, uint32_t port) {
    switch (port) {
    case PORT_TIMER_TICKS: return m->timer_ticks;
    case PORT_KBD_DATA: {
        uint32_t v;
        if (m->kbd_kopf == m->kbd_ende) return 0;
        v = m->kbd[m->kbd_kopf];
        m->kbd_kopf = (m->kbd_kopf + 1) % KBD_BUF;
        return v;
    }
    case PORT_KBD_STATUS: return m->kbd_kopf != m->kbd_ende;
    case PORT_DISK_STATUS: return m->disk_status;
    case PORT_DISK_SIZE:   return m->disk_sektoren;
    case PORT_VGA_MODE:    return (uint32_t)m->mode;
    case PORT_VGA_CURSOR:  return m->cursor;
    /* Lesbar fuer den Prozesswechsel: der Blitter gehoert dem Programm,
       das gerade malt. Siehe hardware/devices.py -- dieselbe Rechnung. */
    case PORT_BLT_ZIEL:  return m->blt_ziel;
    case PORT_BLT_ZIELB: return m->blt_zielb;
    case PORT_BLT_ZIELH: return m->blt_zielh;
    case PORT_BLT_SRC:   return m->blt_src;
    case PORT_DMA_LEN:     return m->dma_len;
    case PORT_MOUSE_X:     return (uint32_t)m->maus_x;
    case PORT_MOUSE_Y:     return (uint32_t)m->maus_y;
    case PORT_MOUSE_BTN:   return (uint32_t)m->maus_btn;
    case PORT_MOUSE_WHEEL: { int w = m->maus_rad; m->maus_rad = 0; return (uint32_t)w; }
    case PORT_CMOS_IDX:    return (uint32_t)m->cmos_idx;
    case PORT_CMOS_DATA: {
        if (m->cmos_idx <= CMOS_YEAR) {
            time_t t = time(NULL);
            struct tm *lt = localtime(&t);
            m->cmos[CMOS_SEC]   = (uint8_t)lt->tm_sec;
            m->cmos[CMOS_MIN]   = (uint8_t)lt->tm_min;
            m->cmos[CMOS_HOUR]  = (uint8_t)lt->tm_hour;
            m->cmos[CMOS_WDAY]  = (uint8_t)lt->tm_wday;
            m->cmos[CMOS_DAY]   = (uint8_t)lt->tm_mday;
            m->cmos[CMOS_MONTH] = (uint8_t)(lt->tm_mon + 1);
            m->cmos[CMOS_YEAR]  = (uint8_t)((lt->tm_year + 1900) % 100);
        }
        return m->cmos[m->cmos_idx & 63];
    }
    case PORT_TEMP:        return (uint32_t)(m->temp * 10);
    case PORT_FAN:         return (uint32_t)m->fan;
    case PORT_THROTTLE:    return (uint32_t)m->throttle;
    case PORT_TEMP_LIMIT:  return (uint32_t)m->limit;
    case PORT_FANMODE:     return (uint32_t)m->fan_mode;
    case PORT_TEMP_MAX:    return (uint32_t)(m->temp_max * 10);
    /* 0, nicht 2: dieser Port beantwortet auch die Frage "liegt ein
       Flashwunsch an?" (Befehl 9). Eine 2 hiesse dort JA, und die Firmware
       zeigte beim Start die rote Flash-Rueckfrage -- ohne dass jemand etwas
       angemeldet haette. Dass kein Abbild kommt, merkt sie ohnehin an der
       Puffergroesse 0. */
    case PORT_FLASH_CMD:   return 0;
    case PORT_FLASH_SIZE:  return 0;      /* keine Datei im Puffer */
    default: return 0;
    }
}

void port_out(Machine *m, uint32_t port, uint32_t val) {
    switch (port) {
    case PORT_PIC_ACK: case PORT_PIC_MASK: break;

    case PORT_TIMER_HZ: m->timer_hz = (double)val; break;

    case PORT_DISK_LBA:   m->disk_lba = val; break;
    case PORT_DISK_COUNT: m->disk_count = val & 0xFFFF; break;
    case PORT_DISK_ADDR:  m->disk_addr = val; break;
    case PORT_DISK_CMD:   disk_cmd(m, val); break;

    case PORT_VGA_MODE:
        m->blt_zoom = 1;
    m->blt_ziel = 0;                  /* Moduswechsel setzt den Blitter zurueck */
        m->mode = (int)(val & 1);
        if (val & 0x100) {
            if (m->mode == 0) vga_clear_text(m, 0x07);
            else {
                memset(m->gfx, 0, VRAM_GFX_SIZE);
                if (m->doppel) memset(m->gfx_sicht, 0, VRAM_GFX_SIZE);
            }
        }
        m->dirty = 1;
        break;
    case PORT_VGA_CURSOR: m->cursor = val & 0xFFFF; m->dirty = 1; break;
    case PORT_VGA_PALIDX: m->pal_index = (int)(val & 0xFF); break;
    case PORT_VGA_PALVAL: m->palette[m->pal_index] = val & 0xFFFFFF; m->dirty = 1; break;

    case PORT_BLT_X:   m->blt_x = (val < 0x8000) ? (int)val : (int)val - 0x10000; break;
    case PORT_BLT_Y:   m->blt_y = (val < 0x8000) ? (int)val : (int)val - 0x10000; break;
    case PORT_BLT_W:   m->blt_w = (int)val; break;
    case PORT_BLT_H:   m->blt_h = (int)val; break;
    case PORT_BLT_COL: m->blt_col = val; break;
    case PORT_BLT_CHR: m->blt_chr = val; break;
    case PORT_BLT_SRC: m->blt_src = val; break;
    case PORT_BLT_BG:  m->blt_bg = val; break;
    case PORT_BLT_ZOOM: m->blt_zoom = (val < 1) ? 1 : (val > 16 ? 16 : val); break;
    case PORT_BLT_ZIEL:  m->blt_ziel = val; break;
    case PORT_BLT_ZIELB: m->blt_zielb = val; break;
    case PORT_BLT_ZIELH: m->blt_zielh = val; break;
    case PORT_BLT_CMD: blit(m, (int)val); break;

    case PORT_GFX_DOPPEL: doppel_setzen(m, (int)val); break;
    case PORT_GFX_TAUSCH:
        /* 1 = Seiten wechseln (schnell, fuer Spiele die alles neu malen)
           2 = Rueckseite auf die Vorderseite kopieren (die Rueckseite
               bleibt stehen -- das braucht der Schreibtisch, der immer nur
               ein Fenster neu malt) */
        if (m->doppel) {
            if (val == 2) {
                memcpy(m->gfx_sicht, m->gfx, VRAM_GFX_SIZE);
            } else {
                uint8_t *t = m->gfx; m->gfx = m->gfx_sicht; m->gfx_sicht = t;
            }
            m->dirty = 1;
        }
        break;

    case PORT_MCUR_X:  m->mcur_x = (int)val; m->dirty = 1; break;
    case PORT_MCUR_Y:  m->mcur_y = (int)val; m->dirty = 1; break;
    case PORT_MCUR_ON: m->mcur_on = (int)val; m->dirty = 1; break;

    case PORT_DMA_SRC: m->dma_src = val; break;
    case PORT_DMA_DST: m->dma_dst = val; break;
    case PORT_DMA_LEN: m->dma_len = val; break;
    case PORT_DMA_VAL: m->dma_val = val & 0xFF; break;
    case PORT_DMA_CMD: dma_los(m, (int)val); break;

    case PORT_SPK_FREQ: m->spk_freq = (int)val; break;
    case PORT_SPK_ON:   m->spk_on = (int)val; break;

    case PORT_CMOS_IDX:  m->cmos_idx = (int)(val & 63); break;
    case PORT_CMOS_DATA: m->cmos[m->cmos_idx & 63] = (uint8_t)val; break;

    case PORT_DEBUG_OUT: fputc((int)(val & 0xFF), stderr); break;

    case PORT_POWER:
        if (val == 1) { m->powered = 0; m->halted = 1; }
        break;

    case PORT_FAN:        m->fan = (int)val; break;
    case PORT_TEMP_LIMIT: m->limit = (int)val; break;
    case PORT_FANMODE:    m->fan_mode = (int)val; break;
    default: break;
    }
}

/* --- Eingabe -------------------------------------------------------------- */

void m_key_push(Machine *m, int ascii, int scancode) {
    int naechste = (m->kbd_ende + 1) % KBD_BUF;
    if (naechste == m->kbd_kopf) return;              /* Puffer voll */
    m->kbd[m->kbd_ende] = (uint32_t)((scancode << 8) | (ascii & 0xFF));
    m->kbd_ende = naechste;
    m->irq_pending |= 1u << IRQ_KBD;
}

void m_mouse(Machine *m, int x, int y, int buttons) {
    int geaendert = (x != m->maus_x || y != m->maus_y || buttons != m->maus_btn);
    m->maus_x = x; m->maus_y = y; m->maus_btn = buttons;
    if (geaendert && m->maus_an) m->irq_pending |= 1u << IRQ_MOUSE;
}

/* --- Waerme --------------------------------------------------------------- */

static void thermal_advance(Machine *m, double dt, double auslastung, double takt_mhz) {
    double heizung, kuehlung, ziel;
    if (dt <= 0) return;
    if (m->fan_mode == 0) {
        ziel = 25 + (m->temp - 40) * 2.5;
        if (ziel < 20) ziel = 20;
        if (ziel > 100) ziel = 100;
        m->fan = (int)ziel;
    }
    heizung = 2.5 * takt_mhz * auslastung + 0.4;
    kuehlung = (m->temp - 22.0) * (0.05 + m->fan / 100.0 * 0.25);
    m->temp += (heizung - kuehlung) * dt;
    if (m->temp < 22.0) m->temp = 22.0;
    if (m->temp > m->temp_max) m->temp_max = m->temp;
    if (m->temp > m->limit) {
        int z = (int)((m->temp - m->limit) * 12);
        if (z > 80) z = 80;
        m->throttle = z;
    } else if (m->throttle > 0) {
        m->throttle -= 2;
        if (m->throttle < 0) m->throttle = 0;
    }
}

/* --- Maschine ------------------------------------------------------------- */

static uint8_t *datei_lesen(const char *pfad, size_t *laenge) {
    FILE *f = fopen(pfad, "rb");
    uint8_t *puf;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    puf = (uint8_t *)malloc((size_t)n);
    if (!puf) { fclose(f); return NULL; }
    if (fread(puf, 1, (size_t)n, f) != (size_t)n) { free(puf); fclose(f); return NULL; }
    fclose(f);
    *laenge = (size_t)n;
    return puf;
}

Machine *m_new(const char *rom_pfad, const char *disk_pfad, const char *cmos_pfad) {
    Machine *m = (Machine *)calloc(1, sizeof(Machine));
    size_t n = 0;
    uint8_t *d;
    if (!m) return NULL;

    gesetzt_init();

    m->ram = (uint8_t *)calloc(RAM_SIZE, 1);
    m->rom = (uint8_t *)calloc(ROM_SIZE, 1);
    m->text = (uint8_t *)calloc(VRAM_TEXT_SIZE, 1);
    m->seite_a = (uint8_t *)calloc(VRAM_GFX_SIZE, 1);
    m->seite_b = (uint8_t *)calloc(VRAM_GFX_SIZE, 1);
    if (!m->ram || !m->rom || !m->text || !m->seite_a || !m->seite_b) return NULL;
    m->gfx = m->seite_a;
    m->gfx_sicht = m->seite_a;
    palette_init(m);
    m->blt_bg = 256;
    m->blt_zoom = 1;
    m->blt_col = 15;
    m->blt_chr = 32;
    m->maus_x = GFX_W / 2;
    m->maus_y = GFX_H / 2;
    m->maus_an = 1;
    m->temp = 22.0;
    m->temp_max = 22.0;
    m->fan = 40;
    m->limit = 85;

    d = datei_lesen(rom_pfad, &n);
    if (!d) { fprintf(stderr, "Kein BIOS gefunden: %s\n", rom_pfad); return NULL; }
    memcpy(m->rom, d, n < ROM_SIZE ? n : ROM_SIZE);
    free(d);

    d = datei_lesen(disk_pfad, &n);
    if (d) {
        m->disk = d;
        m->disk_sektoren = (uint32_t)(n / DISK_SECT);
    }

    /* CMOS: die Knopfzelle. Fehlt sie, legen wir Standardwerte an. */
    d = datei_lesen(cmos_pfad, &n);
    if (d) {
        memcpy(m->cmos, d, n < 64 ? n : 64);
        free(d);
    }
    if (m->cmos[CMOS_MAGIC] != 0x5A) {
        memset(m->cmos, 0, 64);
        m->cmos[0x12] = 1;                 /* Signalton */
        m->cmos[CMOS_CPUSPEED] = 2;
        m->cmos[0x14] = 16;                /* Speichergroesse */
        m->cmos[0x15] = 1;                 /* ausfuehrliche Meldungen */
        m->cmos[CMOS_MAGIC] = 0x5A;
    }
    m->ips = CPU_SPEEDS[m->cmos[CMOS_CPUSPEED] < 5 ? m->cmos[CMOS_CPUSPEED] : 2];
    return m;
}

void m_free(Machine *m) {
    if (!m) return;
    free(m->ram); free(m->rom); free(m->text);
    free(m->seite_a); free(m->seite_b); free(m->disk);
    free(m);
}

void m_power_on(Machine *m) {
    memset(m->ram, 0, RAM_SIZE);
    vga_clear_text(m, 0x07);
    m->mode = 0;
    memset(m->r, 0, sizeof m->r);
    m->pc = RESET_VECTOR;
    m->flags = 0;
    m->halted = 0;
    m->powered = 1;
    m->irq_pending = 0;
    m->timer_hz = 0;
    m->timer_ticks = 0;
    m->timer_rest = 0;
    m->fault[0] = 0;
}

void m_run_slice(Machine *m, double dt) {
    double takt, auslastung;
    int budget, n;

    if (!m->powered) return;

    /* Timer: erzeugt Ticks und meldet sie als Interrupt */
    if (m->timer_hz > 0) {
        m->timer_rest += dt * m->timer_hz;
        while (m->timer_rest >= 1.0) {
            m->timer_rest -= 1.0;
            m->timer_ticks++;
            m->irq_pending |= 1u << IRQ_TIMER;
        }
    }

    takt = m->ips * (100 - m->throttle) / 100.0;
    if (takt < 50000) takt = 50000;
    budget = (int)(takt * (dt < 0.1 ? dt : 0.1));
    if (budget < 1) budget = 1;

    n = cpu_run(m, budget);
    m->befehle_gesamt += (uint64_t)n;

    auslastung = (double)n / (double)budget;
    if (auslastung > 1.0) auslastung = 1.0;
    thermal_advance(m, dt, auslastung, takt / 1000000.0);
}
