/* ==========================================================================
   HackOS 1.0 for the TOOBAD TB-32 "toobad-defender" branch

   A graphical cyber-lab OS. All scanning/targets are simulated training
   targets inside this OS; no external attack functionality is implemented.
   ========================================================================== */

#include "font8.c"

int sys_setmode(int mode);
int sys_getkey();
int sys_haskey();
int sys_flushkeys();
int sys_ticks();
int sys_clock();
int sys_in(int port);
int sys_out(int port, int value);
int sys_halt();

/* GPU / mouse / power ports */
#define P_BLT_X       0x44
#define P_BLT_Y       0x45
#define P_BLT_W       0x46
#define P_BLT_H       0x47
#define P_BLT_COL     0x48
#define P_BLT_CMD     0x49
#define P_BLT_CHR     0x4A
#define P_BLT_SRC     0x4B
#define P_BLT_BG      0x4C
#define P_MCUR_X      0x4D
#define P_MCUR_Y      0x4E
#define P_MCUR_ON     0x4F
#define P_DBL         0x52
#define P_SWAP        0x53
#define P_ZOOM        0x54
#define P_MOUSE_X     0x60
#define P_MOUSE_Y     0x61
#define P_MOUSE_BTN   0x62
#define P_POWER       0x90
#define P_TEMP        0xA0
#define P_THROTTLE    0xA2

#define BLT_FILL      1
#define BLT_FRAME     2
#define BLT_CHAR      3

/* classic palette */
#define BLACK         0
#define BLUE          1
#define GREEN         2
#define CYAN          3
#define RED           4
#define MAGENTA       5
#define BROWN         6
#define LIGHTGRAY     7
#define DARKGRAY      8
#define LIGHTBLUE     9
#define LIGHTGREEN   10
#define LIGHTCYAN    11
#define LIGHTRED     12
#define YELLOW       14
#define WHITE        15

#define K_BACKSPACE 14
#define K_ENTER     28
#define K_ESC        1

#define APP_TERM      0
#define APP_SCAN      1
#define APP_MON       2
#define APP_MISSION   3
#define APP_ABOUT     4
#define APP_POWER     5

char input[52];
char lastcmd[52];
int input_len = 0;
int app = APP_TERM;
int prev_btn = 0;
int mx = 0;
int my = 0;

int terminal_result = 0;
int scan_active = 0;
int scan_progress = 0;
int scan_done = 0;
int scan_last_tick = 0;

int mission1 = 0;
int mission2 = 0;
int mission3 = 0;
int score = 0;
int power_confirm = 0;

/* --------------------------------------------------------------------------
   Small helpers
   -------------------------------------------------------------------------- */

int slen(char* s) {
    int n;
    n = 0;
    while (s[n]) n++;
    return n;
}

int upper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

int equal_ci(char* a, char* b) {
    int i;
    i = 0;
    while (a[i] && b[i]) {
        if (upper(a[i]) != upper(b[i])) return 0;
        i++;
    }
    if (a[i] == 0 && b[i] == 0) return 1;
    return 0;
}

void copystr(char* dst, char* src, int max) {
    int i;
    i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

/* --------------------------------------------------------------------------
   Graphics
   -------------------------------------------------------------------------- */

void fill(int x, int y, int w, int h, int color) {
    sys_out(P_BLT_X, x);
    sys_out(P_BLT_Y, y);
    sys_out(P_BLT_W, w);
    sys_out(P_BLT_H, h);
    sys_out(P_BLT_COL, color);
    sys_out(P_BLT_CMD, BLT_FILL);
}

void frame(int x, int y, int w, int h, int color) {
    sys_out(P_BLT_X, x);
    sys_out(P_BLT_Y, y);
    sys_out(P_BLT_W, w);
    sys_out(P_BLT_H, h);
    sys_out(P_BLT_COL, color);
    sys_out(P_BLT_CMD, BLT_FRAME);
}

void draw_char(int x, int y, int c, int color) {
    if (c < 32 || c > 127) c = '?';
    sys_out(P_ZOOM, 1);
    sys_out(P_BLT_SRC, (int)font8);
    sys_out(P_BLT_BG, 256);
    sys_out(P_BLT_X, x);
    sys_out(P_BLT_Y, y);
    sys_out(P_BLT_COL, color);
    sys_out(P_BLT_CHR, c);
    sys_out(P_BLT_CMD, BLT_CHAR);
}

void text(int x, int y, char* s, int color) {
    while (*s) {
        draw_char(x, y, *s, color);
        x = x + 8;
        s++;
    }
}

void text_center(int x, int y, int w, char* s, int color) {
    text(x + (w - slen(s) * 8) / 2, y, s, color);
}

char numbuf[16];

void draw_num(int x, int y, int n, int color) {
    int i;
    int p;
    int neg;
    neg = 0;
    if (n < 0) {
        neg = 1;
        n = 0 - n;
    }
    i = 0;
    if (n == 0) {
        numbuf[i] = '0';
        i++;
    } else {
        while (n > 0 && i < 12) {
            numbuf[i] = '0' + (n % 10);
            n = n / 10;
            i++;
        }
    }
    if (neg) {
        numbuf[i] = '-';
        i++;
    }
    numbuf[i] = 0;
    p = 0;
    while (p < i / 2) {
        int t;
        t = numbuf[p];
        numbuf[p] = numbuf[i - 1 - p];
        numbuf[i - 1 - p] = t;
        p++;
    }
    text(x, y, numbuf, color);
}

void button(int x, int y, int w, char* label, int active) {
    int c;
    c = DARKGRAY;
    if (active) c = GREEN;
    fill(x, y, w, 30, c);
    frame(x, y, w, 30, active ? LIGHTGREEN : LIGHTGRAY);
    text_center(x, y + 11, w, label, active ? BLACK : WHITE);
}

int hit(int x, int y, int w, int h) {
    if (mx < x) return 0;
    if (my < y) return 0;
    if (mx >= x + w) return 0;
    if (my >= y + h) return 0;
    return 1;
}

/* --------------------------------------------------------------------------
   Shared shell
   -------------------------------------------------------------------------- */

void chrome() {
    int t;
    int h;
    int m;

    fill(0, 0, 640, 400, BLACK);
    fill(0, 0, 640, 42, DARKGRAY);
    fill(0, 42, 185, 336, 0);
    fill(0, 378, 640, 22, DARKGRAY);

    text(14, 10, "HACK", LIGHTGREEN);
    text(46, 10, "OS", WHITE);
    text(72, 10, " // TB-32 CYBER LAB", LIGHTGRAY);
    text(502, 10, "SAFE LAB", LIGHTGREEN);

    button(12, 62, 160, "TERMINAL", app == APP_TERM);
    button(12, 102, 160, "LAB SCANNER", app == APP_SCAN);
    button(12, 142, 160, "SYSTEM MON", app == APP_MON);
    button(12, 182, 160, "MISSIONS", app == APP_MISSION);
    button(12, 222, 160, "ABOUT", app == APP_ABOUT);
    button(12, 330, 160, "POWER", app == APP_POWER);

    frame(190, 52, 438, 316, GREEN);

    text(12, 385, "TB-32 32-bit | 16 MiB RAM", LIGHTGRAY);

    t = sys_clock();
    h = (t >> 16) & 255;
    m = (t >> 8) & 255;
    text(548, 385, "TIME ", LIGHTGRAY);
    if (h < 10) text(588, 385, "0", WHITE);
    draw_num(h < 10 ? 596 : 588, 385, h, WHITE);
    text(604, 385, ":", WHITE);
    if (m < 10) text(612, 385, "0", WHITE);
    draw_num(m < 10 ? 620 : 612, 385, m, WHITE);
}

void draw_terminal() {
    text(206, 66, "ROOT TERMINAL", LIGHTGREEN);
    text(206, 86, "Commands: help scan targets status missions about clear", DARKGRAY);
    text(206, 112, "hackos@tb32:~$ ", LIGHTGREEN);
    text(334, 112, input, WHITE);
    text(206 + (16 + input_len) * 8, 112, "_", LIGHTGREEN);

    frame(204, 136, 408, 210, DARKGRAY);

    if (lastcmd[0]) {
        text(216, 148, "last> ", DARKGRAY);
        text(264, 148, lastcmd, LIGHTCYAN);
    }

    if (terminal_result == 0) {
        text(216, 174, "HackOS ready. This is an isolated cyber range.", LIGHTGREEN);
        text(216, 190, "No real targets are scanned or attacked.", LIGHTGRAY);
    } else if (terminal_result == 1) {
        text(216, 174, "help     show commands", WHITE);
        text(216, 190, "scan     map simulated training subnet", WHITE);
        text(216, 206, "targets  show lab target list", WHITE);
        text(216, 222, "status   system status", WHITE);
        text(216, 238, "missions open training missions", WHITE);
        text(216, 254, "about    information about HackOS", WHITE);
        text(216, 270, "clear    clear terminal result", WHITE);
    } else if (terminal_result == 2) {
        text(216, 174, "Lab scan queued. Opening LAB SCANNER...", YELLOW);
    } else if (terminal_result == 3) {
        text(216, 174, "NODE-05  10.10.0.5   training SSH node", LIGHTGREEN);
        text(216, 190, "NODE-08  10.10.0.8   training WEB node", LIGHTGREEN);
        text(216, 206, "NODE-12  10.10.0.12  protected DB node", YELLOW);
    } else if (terminal_result == 4) {
        text(216, 174, "CPU: TB-32 native 32-bit", WHITE);
        text(216, 190, "RAM: 16 MiB", WHITE);
        text(216, 206, "Graphics: 640x400x256", WHITE);
        text(216, 222, "Mode: isolated cyber laboratory", LIGHTGREEN);
    } else if (terminal_result == 5) {
        text(216, 174, "Mission console opened.", LIGHTGREEN);
    } else if (terminal_result == 6) {
        text(216, 174, "Unknown command. Type help.", LIGHTRED);
    } else if (terminal_result == 7) {
        text(216, 174, "HackOS 1.0 - graphical TB-32 cyber-lab OS.", LIGHTGREEN);
        text(216, 190, "Built as native TB-32 machine code.", WHITE);
    }
}

void draw_scanner() {
    text(206, 66, "LAB SCANNER", LIGHTGREEN);
    text(206, 86, "Only simulated hosts in 10.10.0.0/24 are shown.", LIGHTGRAY);

    button(206, 112, 150, scan_active ? "SCANNING..." : "START SCAN", scan_active);

    frame(206, 154, 392, 18, DARKGRAY);
    fill(208, 156, (scan_progress * 388) / 100, 14, LIGHTGREEN);
    draw_num(606, 158, scan_progress, WHITE);
    text(622, 158, "%", WHITE);

    if (!scan_done) {
        text(206, 196, "Waiting for an authorized lab scan.", DARKGRAY);
    } else {
        text(206, 196, "HOST              SERVICE       STATE", DARKGRAY);
        text(206, 218, "10.10.0.5 NODE-05 SSH :22       OPEN", LIGHTGREEN);
        text(206, 238, "10.10.0.8 NODE-08 WEB :8080     OPEN", LIGHTGREEN);
        text(206, 258, "10.10.0.12 NODE-12 DB :3306     FILTERED", YELLOW);
        text(206, 292, "Mission 1 complete: training subnet mapped.", LIGHTCYAN);
    }
}

void draw_monitor() {
    int temp;
    int throttle;

    temp = sys_in(P_TEMP) / 10;
    throttle = sys_in(P_THROTTLE);

    text(206, 66, "SYSTEM MONITOR", LIGHTGREEN);

    text(206, 102, "CPU", DARKGRAY);
    text(330, 102, "TB-32 / 16 registers", WHITE);

    text(206, 126, "RAM", DARKGRAY);
    text(330, 126, "16384 KiB", WHITE);

    text(206, 150, "TICKS", DARKGRAY);
    draw_num(330, 150, sys_ticks(), LIGHTCYAN);

    text(206, 174, "TEMP", DARKGRAY);
    draw_num(330, 174, temp, temp > 70 ? LIGHTRED : LIGHTGREEN);
    text(362, 174, " C", WHITE);

    text(206, 198, "THROTTLE", DARKGRAY);
    draw_num(330, 198, throttle, WHITE);
    text(362, 198, "%", WHITE);

    text(206, 222, "MOUSE", DARKGRAY);
    draw_num(330, 222, mx, WHITE);
    text(370, 222, "x", DARKGRAY);
    draw_num(386, 222, my, WHITE);

    text(206, 266, "DEFENSIVE MODE", LIGHTGREEN);
    text(206, 284, "External offensive actions: disabled", LIGHTGRAY);
    text(206, 300, "Training range: local simulation", LIGHTGRAY);
}

void mission_box(int y, char* title, char* detail, int done) {
    frame(206, y, 392, 62, done ? GREEN : DARKGRAY);
    text(218, y + 10, title, done ? LIGHTGREEN : WHITE);
    text(218, y + 30, detail, LIGHTGRAY);
    text(530, y + 10, done ? "[DONE]" : "[TODO]", done ? LIGHTGREEN : YELLOW);
}

void draw_missions() {
    text(206, 66, "CYBER RANGE MISSIONS", LIGHTGREEN);
    text(484, 66, "SCORE:", DARKGRAY);
    draw_num(540, 66, score, LIGHTGREEN);

    mission_box(96, "01 MAP THE RANGE", "Run the simulated Lab Scanner.", mission1);
    mission_box(166, "02 INSPECT NODE-08", "Read its safe training banner.", mission2);
    mission_box(236, "03 HARDEN NODE-05", "Apply the simulated security patch.", mission3);

    if (!mission2) button(444, 184, 132, "INSPECT", 0);
    if (!mission3) button(444, 254, 132, "PATCH", 0);

    if (mission1 && mission2 && mission3) {
        text(206, 318, "RANGE COMPLETE // 300 POINTS", LIGHTGREEN);
    }
}

void draw_about() {
    text(206, 66, "ABOUT HACKOS", LIGHTGREEN);
    text(206, 104, "HackOS 1.0", WHITE);
    text(206, 128, "Native operating system for the custom TB-32 CPU.", LIGHTGRAY);
    text(206, 152, "GUI: 640x400 / 256 colors / hardware blitter.", LIGHTGRAY);
    text(206, 176, "Input: TB-32 mouse + keyboard hardware ports.", LIGHTGRAY);
    text(206, 216, "Cyber tools are intentionally an isolated simulation.", LIGHTGREEN);
    text(206, 240, "Use it for learning, demos and CTF-style exercises.", LIGHTGRAY);
    text(206, 282, "TOOBAD-OS and Defender stay installed separately.", LIGHTCYAN);
}

void draw_power() {
    text(206, 66, "POWER", LIGHTGREEN);
    text(206, 102, "These controls affect only the virtual TB-32 machine.", LIGHTGRAY);

    button(206, 142, 160, "REBOOT", 0);
    button(390, 142, 160, "SHUT DOWN", 0);

    if (power_confirm == 1) {
        text(206, 204, "Click REBOOT again to confirm.", YELLOW);
    } else if (power_confirm == 2) {
        text(206, 204, "Click SHUT DOWN again to confirm.", YELLOW);
    }
}

void render() {
    chrome();
    if (app == APP_TERM) draw_terminal();
    else if (app == APP_SCAN) draw_scanner();
    else if (app == APP_MON) draw_monitor();
    else if (app == APP_MISSION) draw_missions();
    else if (app == APP_ABOUT) draw_about();
    else draw_power();

    sys_out(P_SWAP, 1);
}

/* --------------------------------------------------------------------------
   Training logic
   -------------------------------------------------------------------------- */

void start_scan() {
    scan_active = 1;
    scan_done = 0;
    scan_progress = 0;
    scan_last_tick = sys_ticks();
}

void update_scan() {
    int now;
    if (!scan_active) return;

    now = sys_ticks();
    if (now - scan_last_tick >= 4) {
        scan_last_tick = now;
        scan_progress = scan_progress + 2;
        if (scan_progress >= 100) {
            scan_progress = 100;
            scan_active = 0;
            scan_done = 1;
            if (!mission1) {
                mission1 = 1;
                score = score + 100;
            }
        }
    }
}

void run_command() {
    copystr(lastcmd, input, 52);

    if (equal_ci(input, "help")) {
        terminal_result = 1;
    } else if (equal_ci(input, "scan")) {
        terminal_result = 2;
        start_scan();
        app = APP_SCAN;
    } else if (equal_ci(input, "targets")) {
        terminal_result = 3;
    } else if (equal_ci(input, "status")) {
        terminal_result = 4;
    } else if (equal_ci(input, "missions")) {
        terminal_result = 5;
        app = APP_MISSION;
    } else if (equal_ci(input, "about")) {
        terminal_result = 7;
    } else if (equal_ci(input, "clear")) {
        terminal_result = 0;
        lastcmd[0] = 0;
    } else if (input[0] != 0) {
        terminal_result = 6;
    }

    input_len = 0;
    input[0] = 0;
}

void keyboard() {
    int k;
    int c;
    int sc;

    if (!sys_haskey()) return;
    k = sys_getkey();
    c = k & 255;
    sc = (k >> 8) & 255;

    if (sc == K_ESC) {
        app = APP_TERM;
        return;
    }

    if (app != APP_TERM) return;

    if (sc == K_ENTER) {
        run_command();
        return;
    }

    if (sc == K_BACKSPACE) {
        if (input_len > 0) {
            input_len--;
            input[input_len] = 0;
        }
        return;
    }

    if (c >= 32 && c < 127 && input_len < 50) {
        input[input_len] = c;
        input_len++;
        input[input_len] = 0;
    }
}

void mouse() {
    int btn;
    int click;

    mx = sys_in(P_MOUSE_X);
    my = sys_in(P_MOUSE_Y);
    btn = sys_in(P_MOUSE_BTN);

    sys_out(P_MCUR_X, mx);
    sys_out(P_MCUR_Y, my);

    click = (btn & 1) && ((prev_btn & 1) == 0);
    prev_btn = btn;
    if (!click) return;

    if (hit(12, 62, 160, 30)) { app = APP_TERM; power_confirm = 0; return; }
    if (hit(12, 102, 160, 30)) { app = APP_SCAN; power_confirm = 0; return; }
    if (hit(12, 142, 160, 30)) { app = APP_MON; power_confirm = 0; return; }
    if (hit(12, 182, 160, 30)) { app = APP_MISSION; power_confirm = 0; return; }
    if (hit(12, 222, 160, 30)) { app = APP_ABOUT; power_confirm = 0; return; }
    if (hit(12, 330, 160, 30)) { app = APP_POWER; power_confirm = 0; return; }

    if (app == APP_SCAN && hit(206, 112, 150, 30) && !scan_active) {
        start_scan();
        return;
    }

    if (app == APP_MISSION) {
        if (!mission2 && hit(444, 184, 132, 30)) {
            if (mission1) {
                mission2 = 1;
                score = score + 100;
            }
            return;
        }
        if (!mission3 && hit(444, 254, 132, 30)) {
            if (mission2) {
                mission3 = 1;
                score = score + 100;
            }
            return;
        }
    }

    if (app == APP_POWER) {
        if (hit(206, 142, 160, 30)) {
            if (power_confirm == 1) {
                sys_out(P_POWER, 2);
                while (1) sys_halt();
            }
            power_confirm = 1;
            return;
        }
        if (hit(390, 142, 160, 30)) {
            if (power_confirm == 2) {
                sys_out(P_POWER, 1);
                while (1) sys_halt();
            }
            power_confirm = 2;
            return;
        }
    }
}

/* --------------------------------------------------------------------------
   Kernel
   -------------------------------------------------------------------------- */

int main() {
    int last_render;
    int now;

    input[0] = 0;
    lastcmd[0] = 0;

    sys_flushkeys();
    sys_setmode(1 + 256);
    sys_out(P_BLT_SRC, (int)font8);
    sys_out(P_DBL, 1);
    sys_out(P_MCUR_ON, 1);

    last_render = 0;
    render();

    while (1) {
        mouse();
        keyboard();
        update_scan();

        now = sys_ticks();
        if (now - last_render >= 5) {
            last_render = now;
            render();
        }

        sys_halt();
    }

    return 0;
}
