/* ==========================================================================
   TB-32 in C -- derselbe Rechner, nur nicht mehr in Python

   Warum es das gibt: Python schafft rund 3 Millionen TB-32-Befehle je
   Sekunde. Derselbe Emulator in C liegt zwei Groessenordnungen darueber --
   und er ist die Grundlage dafuer, den Rechner spaeter ohne Betriebssystem
   direkt auf einem Raspberry Pi zu starten.

   Wichtig: **TOOBAD-OS aendert sich dabei um kein einziges Byte.** Der TB-32
   bleibt der Prozessor, der Blitter bleibt der Blitter. Getauscht wird nur
   das, was die Chips nachbaut.

   Die Wahrheit ueber die Architektur steht weiterhin in hardware/isa.py --
   diese Datei muss dazu passen. Wer dort etwas aendert, aendert es hier mit.
   ========================================================================== */

#ifndef TB32_H
#define TB32_H

#include <stdint.h>
#include <stddef.h>

/* --- Speicherkarte (muss zu hardware/isa.py passen) ---------------------- */

#define RAM_SIZE        (16u * 1024u * 1024u)

#define VRAM_TEXT       0x02000000u
#define VRAM_TEXT_SIZE  (80u * 25u * 2u)
#define VRAM_GFX        0x02100000u
#define GFX_W           640
#define GFX_H           400
#define VRAM_GFX_SIZE   ((unsigned)(GFX_W * GFX_H))

#define ROM_BASE        0x0F000000u
#define ROM_SIZE        (64u * 1024u)

#define RESET_VECTOR    ROM_BASE
#define IVT_BASE        0x00000000u

/* --- Flags --------------------------------------------------------------- */

#define FLAG_Z  (1u << 0)
#define FLAG_N  (1u << 1)
#define FLAG_C  (1u << 2)
#define FLAG_V  (1u << 3)
#define FLAG_I  (1u << 9)

#define SIGN    0x80000000u

/* --- Interruptleitungen -------------------------------------------------- */

/* Das sind bereits die Interrupt-Vektoren, nicht Leitungsnummern -- genau
   wie in hardware/isa.py. Wer hier 0/1/2 einsetzt, laesst den Timer auf
   Vektor 0 springen, und der heisst "Division durch null". */
#define IRQ_TIMER  0x08
#define IRQ_KBD    0x09
#define IRQ_MOUSE  0x0C

/* --- Ports (Auszug -- die vollstaendige Liste steht in isa.py) ----------- */

#define PORT_PIC_ACK       0x0000
#define PORT_PIC_MASK      0x0001
#define PORT_TIMER_HZ      0x0010
#define PORT_TIMER_TICKS   0x0011
#define PORT_KBD_DATA      0x0020
#define PORT_KBD_STATUS    0x0021
#define PORT_DISK_LBA      0x0030
#define PORT_DISK_COUNT    0x0031
#define PORT_DISK_ADDR     0x0032
#define PORT_DISK_CMD      0x0033
#define PORT_DISK_STATUS   0x0034
#define PORT_DISK_SIZE     0x0035
#define PORT_VGA_MODE      0x0040
#define PORT_VGA_CURSOR    0x0041
#define PORT_VGA_PALIDX    0x0042
#define PORT_VGA_PALVAL    0x0043
#define PORT_BLT_X         0x0044
#define PORT_BLT_Y         0x0045
#define PORT_BLT_W         0x0046
#define PORT_BLT_H         0x0047
#define PORT_BLT_COL       0x0048
#define PORT_BLT_CMD       0x0049
#define PORT_BLT_CHR       0x004A
#define PORT_BLT_SRC       0x004B
#define PORT_BLT_BG        0x004C
#define PORT_MCUR_X        0x004D
#define PORT_MCUR_Y        0x004E
#define PORT_MCUR_ON       0x004F
#define PORT_SPK_FREQ      0x0050
#define PORT_SPK_ON        0x0051
#define PORT_GFX_DOPPEL    0x0052
#define PORT_GFX_TAUSCH    0x0053
#define PORT_BLT_ZOOM      0x0054
#define PORT_BLT_ZIEL      0x005B   /* Blitter malt in den Speicher */
#define PORT_BLT_ZIELB     0x005C
#define PORT_BLT_ZIELH     0x005D
#define PORT_DMA_SRC       0x0056
#define PORT_DMA_DST       0x0057
#define PORT_DMA_LEN       0x0058
#define PORT_DMA_VAL       0x0059
#define PORT_DMA_CMD       0x005A
#define PORT_MOUSE_X       0x0060
#define PORT_MOUSE_Y       0x0061
#define PORT_MOUSE_BTN     0x0062
#define PORT_MOUSE_WHEEL   0x0063
#define PORT_CMOS_IDX      0x0070
#define PORT_CMOS_DATA     0x0071
#define PORT_DEBUG_OUT     0x0080
#define PORT_POWER         0x0090
#define PORT_TEMP          0x00A0
#define PORT_FAN           0x00A1
#define PORT_THROTTLE      0x00A2
#define PORT_TEMP_LIMIT    0x00A3
#define PORT_FANMODE       0x00A4
#define PORT_TEMP_MAX      0x00A5
/* ROM-Baustein neu beschreiben. Die kopflose C-Fassung kann keinen
   Dateidialog oeffnen -- Befehl 1 liefert deshalb immer "keine Datei", und
   damit lehnt die Firmware das Flashen sauber ab. Die Ports muessen aber
   dieselben sein wie in hardware/isa.py, sonst laufen die beiden Emulatoren
   auseinander. */
#define PORT_FLASH_CMD     0x00B0
#define PORT_FLASH_SIZE    0x00B1
#define PORT_FLASH_ADDR    0x00B2

/* --- Die Maschine -------------------------------------------------------- */

#define KBD_BUF   64
#define DISK_SECT 512

typedef struct {
    /* Prozessor */
    uint32_t r[16];
    uint32_t pc;
    uint32_t flags;
    int      halted;
    int      powered;
    uint64_t cycles;
    uint32_t irq_pending;        /* Bitmaske der anliegenden Leitungen */
    uint32_t irq_mask;
    char     fault[128];

    /* Speicher */
    uint8_t *ram;                /* RAM_SIZE Bytes */
    uint8_t *rom;                /* ROM_SIZE Bytes */

    /* Grafikkarte */
    uint8_t *text;               /* VRAM_TEXT_SIZE */
    uint8_t *seite_a;            /* VRAM_GFX_SIZE */
    uint8_t *seite_b;
    uint8_t *gfx;                /* hierhin wird gemalt */
    uint8_t *gfx_sicht;          /* diese wird angezeigt */
    int      doppel;
    int      mode;               /* 0 = Text, 1 = Grafik */
    uint32_t cursor;
    uint32_t palette[256];
    int      pal_index;
    int      dirty;
    int      mcur_x, mcur_y, mcur_on;

    /* Blitter */
    int      blt_x, blt_y, blt_w, blt_h;
    uint32_t blt_col, blt_chr, blt_src, blt_bg, blt_zoom;
    uint32_t blt_ziel, blt_zielb, blt_zielh;   /* Zielpuffer statt Bildschirm */

    /* Blockkopierer */
    uint32_t dma_src, dma_dst, dma_len, dma_val;

    /* Tastatur */
    uint32_t kbd[KBD_BUF];
    int      kbd_kopf, kbd_ende;

    /* Maus */
    int      maus_x, maus_y, maus_btn, maus_rad;
    int      maus_an;

    /* Platte */
    uint32_t disk_lba, disk_count, disk_addr, disk_status;
    uint32_t disk_sektoren;
    uint8_t *disk;               /* das ganze Abbild im Speicher */
    int      disk_dirty;

    /* Timer */
    double   timer_hz;
    uint32_t timer_ticks;
    double   timer_rest;

    /* CMOS */
    uint8_t  cmos[64];
    int      cmos_idx;

    /* Waerme */
    double   temp, temp_max;
    int      fan, fan_mode, limit, throttle;

    /* Lautsprecher */
    int      spk_freq, spk_on;

    /* Takt */
    double   ips;                /* Befehle je Sekunde */
    uint64_t befehle_gesamt;
    int      zeitmangel;
} Machine;

/* --- Schnittstelle ------------------------------------------------------- */

Machine *m_new(const char *rom_pfad, const char *disk_pfad, const char *cmos_pfad);
void     m_free(Machine *m);
void     m_power_on(Machine *m);
void     m_run_slice(Machine *m, double dt);
int      cpu_run(Machine *m, int budget);

void     m_key_push(Machine *m, int ascii, int scancode);
void     m_mouse(Machine *m, int x, int y, int buttons);

uint32_t bus_read32(Machine *m, uint32_t addr);
void     bus_write32(Machine *m, uint32_t addr, uint32_t val);
uint8_t  bus_read8(Machine *m, uint32_t addr);
void     bus_write8(Machine *m, uint32_t addr, uint8_t val);
uint32_t port_in(Machine *m, uint32_t port);
void     port_out(Machine *m, uint32_t port, uint32_t val);

#endif
