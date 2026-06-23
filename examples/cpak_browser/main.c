/*
 * Jupiter SDK — N64 Controller Pak browser.
 *
 * Reads the standard mempak filesystem note table (16 entries × 32 B at
 * pages 3-4 = blocks 24-39) and renders each entry on the LCD. Lets
 * you navigate slots with the D-pad and:
 *   A     - inspect the full 32-byte note (hex dump)
 *   B     - delete the highlighted note (zero it, free its index chain)
 *   START - format the pak (wipe pages 0-4 + write fresh index table)
 *
 * Build: make GAME=examples/cpak_browser/main.c
 *
 * Cpak prerequisites: input_init(INPUT_N64) + Controller Pak inserted
 * (cpak_probe must return CPAK_CONTROLLER on first attempt).
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "cpak.h"
#include <string.h>
#include <stddef.h>

/* ---------------- 5x7 column-encoded font (from examples/menu) ---------- */
#define FNT_W 5
#define FNT_H 7
typedef struct { char ch; uint8_t cols[FNT_W]; } glyph_t;
static const glyph_t g_font[] = {
    {' ', {0,0,0,0,0}},
    {'A', {0x7E,0x09,0x09,0x09,0x7E}},  {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},  {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},  {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},  {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},  {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},  {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},  {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},  {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},  {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},  {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},  {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}},  {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},  {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},  {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},  {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},  {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},  {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},  {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},  {':', {0x00,0x24,0x00,0x00,0x00}},
    {'.', {0x00,0x40,0x00,0x00,0x00}},  {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'+', {0x08,0x08,0x3E,0x08,0x08}},  {'_', {0x40,0x40,0x40,0x40,0x40}},
    {'(', {0x00,0x1C,0x22,0x41,0x00}},  {')', {0x00,0x41,0x22,0x1C,0x00}},
    {'[', {0x00,0x7F,0x41,0x41,0x00}},  {']', {0x00,0x41,0x41,0x7F,0x00}},
    {'?', {0x02,0x01,0x51,0x09,0x06}},  {'!', {0x00,0x00,0x5F,0x00,0x00}},
    {'<', {0x08,0x14,0x22,0x41,0x00}},  {'>', {0x00,0x41,0x22,0x14,0x08}},
    {'=', {0x14,0x14,0x14,0x14,0x14}},  {',', {0x80,0x60,0x00,0x00,0x00}},
    {'\'',{0x00,0x07,0x00,0x00,0x00}},  {'"', {0x07,0x00,0x07,0x00,0x00}},
    {'a', {0x20,0x54,0x54,0x54,0x78}},  {'b', {0x7F,0x44,0x44,0x44,0x38}},
    {'c', {0x38,0x44,0x44,0x44,0x44}},  {'d', {0x38,0x44,0x44,0x44,0x7F}},
    {'e', {0x38,0x54,0x54,0x54,0x18}},  {'f', {0x04,0x04,0x7E,0x05,0x05}},
    {'g', {0x18,0xA4,0xA4,0xA4,0x7C}},  {'h', {0x7F,0x08,0x04,0x04,0x78}},
    {'i', {0x00,0x44,0x7D,0x40,0x00}},  {'j', {0x40,0x80,0x84,0x7D,0x00}},
    {'k', {0x7F,0x10,0x28,0x44,0x00}},  {'l', {0x00,0x41,0x7F,0x40,0x00}},
    {'m', {0x7C,0x04,0x78,0x04,0x78}},  {'n', {0x7C,0x08,0x04,0x04,0x78}},
    {'o', {0x38,0x44,0x44,0x44,0x38}},  {'p', {0xFC,0x14,0x14,0x14,0x08}},
    {'q', {0x08,0x14,0x14,0x14,0xFC}},  {'r', {0x7C,0x08,0x04,0x04,0x08}},
    {'s', {0x48,0x54,0x54,0x54,0x24}},  {'t', {0x04,0x3F,0x44,0x40,0x20}},
    {'u', {0x3C,0x40,0x40,0x20,0x7C}},  {'v', {0x1C,0x20,0x40,0x20,0x1C}},
    {'w', {0x3C,0x40,0x30,0x40,0x3C}},  {'x', {0x44,0x28,0x10,0x28,0x44}},
    {'y', {0x0C,0x50,0x50,0x50,0x3C}},  {'z', {0x44,0x64,0x54,0x4C,0x44}},
};
#define FONT_N (sizeof(g_font)/sizeof(g_font[0]))

static const uint8_t *glyph_for(char c) {
    for (uint32_t i = 0; i < FONT_N; i++) if (g_font[i].ch == c) return g_font[i].cols;
    return g_font[0].cols;
}

static void draw_text(uint32_t *fb, const char *s, int x, int y, int size, uint32_t color) {
    while (*s) {
        const uint8_t *g = glyph_for(*s);
        for (int col = 0; col < FNT_W; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < FNT_H; row++) {
                if (!(bits & (1 << row))) continue;
                for (int dy = 0; dy < size; dy++) {
                    int py = y + row * size + dy;
                    if (py < 0 || py >= (int)LCD_H) continue;
                    uint32_t *line = fb + py * LCD_W;
                    for (int dx = 0; dx < size; dx++) {
                        int px = x + col * size + dx;
                        if (px < 0 || px >= (int)LCD_W) continue;
                        line[px] = color;
                    }
                }
            }
        }
        x += (FNT_W + 1) * size;
        s++;
    }
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h && py < (int)LCD_H; py++) {
        if (py < 0) continue;
        uint32_t *row = fb + py * LCD_W;
        for (int px = x; px < x + w && px < (int)LCD_W; px++) {
            if (px < 0) continue;
            row[px] = color;
        }
    }
}

/* ---------------- Mempak FS layout ----------------
 * 32 KB pak = 128 pages × 256 bytes = 1024 blocks × 32 bytes.
 *   Page 0  (blocks  0..7 ) - Header (4 ID block copies)
 *   Page 1  (blocks  8..15) - Index Table (128 × 2 bytes BE)
 *   Page 2  (blocks 16..23) - Index Table backup
 *   Pages 3-4 (blocks 24..39) - Note Table (16 entries × 32 B)
 *   Pages 5..127 (blocks 40..1023) - File data
 * Index Table entry encodings (2 bytes BE):
 *   0x0001 = stop-of-chain marker
 *   0x0003 = free
 *   N      = next page (5..127)
 */
#define PAGE_HEADER_BLOCKS_LO  0
#define PAGE_HEADER_BLOCKS_HI  7
#define PAGE_INDEX_BLOCKS_LO   8
#define PAGE_INDEX_BLOCKS_HI   15
#define PAGE_INDEX_BAK_LO      16
#define PAGE_INDEX_BAK_HI      23
#define PAGE_NOTES_LO          24
#define PAGE_NOTES_HI          39
#define PAGE_DATA_LO           40
#define PAGE_DATA_HI           1023
#define NOTE_COUNT             16
#define NOTE_SIZE              32
#define INDEX_ENTRIES          128
#define INDEX_FREE             0x0003u
#define INDEX_STOP             0x0001u

/* Decoded note for display. */
typedef struct {
    uint8_t raw[NOTE_SIZE];
    int     used;            /* heuristic: at least 1 non-zero byte in 0..3 (game code) */
    uint8_t game_code[4];
    uint8_t start_page;
    char    name[17];        /* bytes 16..31 mostly map to ASCII, NUL-pad */
} note_t;

static uint8_t        g_index[256];      /* page 1 */
static note_t         g_notes[NOTE_COUNT];

/* ---------------- I/O helpers ---------------- */
static int read_index_table(void) {
    for (int b = 0; b < 8; b++) {
        if (cpak_read_block(PAGE_INDEX_BLOCKS_LO + b, &g_index[b * CPAK_BLOCK_SIZE]) < 0)
            return -1;
    }
    return 0;
}

static int read_note_table(void) {
    uint8_t buf[NOTE_COUNT * NOTE_SIZE];   /* 512 B = 16 blocks */
    for (int b = 0; b < 16; b++) {
        if (cpak_read_block(PAGE_NOTES_LO + b, &buf[b * CPAK_BLOCK_SIZE]) < 0)
            return -1;
    }
    for (int i = 0; i < NOTE_COUNT; i++) {
        memcpy(g_notes[i].raw, &buf[i * NOTE_SIZE], NOTE_SIZE);
        g_notes[i].used = 0;
        for (int k = 0; k < 4; k++) {
            g_notes[i].game_code[k] = g_notes[i].raw[k];
            if (g_notes[i].raw[k]) g_notes[i].used = 1;
        }
        g_notes[i].start_page = g_notes[i].raw[7];
        for (int k = 0; k < 16; k++) {
            uint8_t c = g_notes[i].raw[16 + k];
            g_notes[i].name[k] = (c >= 0x20 && c < 0x7F) ? (char)c : (c ? '?' : ' ');
        }
        g_notes[i].name[16] = 0;
    }
    return 0;
}

/* Index table get/set (entries are 2 bytes BE). */
static uint16_t idx_get(int page) {
    return ((uint16_t)g_index[page * 2] << 8) | g_index[page * 2 + 1];
}
static void idx_set(int page, uint16_t v) {
    g_index[page * 2 + 0] = (uint8_t)(v >> 8);
    g_index[page * 2 + 1] = (uint8_t)v;
}

/* Walk the chain starting at `start_page`, marking each as free, until we
 * hit a stop or run off the end. Caps iteration at 128 to avoid looping. */
static void free_chain(int start_page) {
    int page = start_page;
    for (int step = 0; step < INDEX_ENTRIES && page >= PAGE_DATA_LO/8 && page < INDEX_ENTRIES; step++) {
        uint16_t next = idx_get(page);
        idx_set(page, INDEX_FREE);
        if (next == INDEX_STOP || next == INDEX_FREE) break;
        if (next < PAGE_DATA_LO/8 || next >= INDEX_ENTRIES) break;
        page = next;
    }
}

/* Write the in-memory index table back to both pages 1 and 2. */
static int write_index_table(void) {
    for (int b = 0; b < 8; b++) {
        if (cpak_write_block(PAGE_INDEX_BLOCKS_LO + b, &g_index[b * CPAK_BLOCK_SIZE]) < 0) return -1;
        if (cpak_write_block(PAGE_INDEX_BAK_LO    + b, &g_index[b * CPAK_BLOCK_SIZE]) < 0) return -1;
    }
    return 0;
}

/* Delete one note: zero its entry (writes back to pages 3-4) + free its
 * index-table chain. */
static int delete_note(int slot) {
    if (slot < 0 || slot >= NOTE_COUNT) return -1;
    int start_page = g_notes[slot].start_page;
    if (start_page >= PAGE_DATA_LO/8 && start_page < INDEX_ENTRIES) {
        free_chain(start_page);
        if (write_index_table() < 0) return -1;
    }
    /* Zero the note in our buffer + write only the affected block.
     * Each block holds 1 note (32 B), block index = PAGE_NOTES_LO + slot. */
    memset(g_notes[slot].raw, 0, NOTE_SIZE);
    if (cpak_write_block(PAGE_NOTES_LO + slot, g_notes[slot].raw) < 0) return -1;
    g_notes[slot].used = 0;
    g_notes[slot].name[0] = 0;
    return 0;
}

/* Format: zero pages 0..4, then write a fresh index table marking pages
 * 0..4 as system + 5..127 as free. After this the pak is empty + clean
 * from the browser's perspective. */
static int format_pak(void) {
    uint8_t zero[CPAK_BLOCK_SIZE] = {0};
    for (int b = PAGE_HEADER_BLOCKS_LO; b <= PAGE_NOTES_HI; b++) {
        if (cpak_write_block(b, zero) < 0) return -1;
    }
    /* Build a clean index table. */
    memset(g_index, 0, sizeof(g_index));
    for (int p = 0; p < INDEX_ENTRIES; p++) {
        uint16_t v;
        if (p < PAGE_DATA_LO / 8) v = INDEX_STOP;   /* system pages reserved */
        else                       v = INDEX_FREE;
        idx_set(p, v);
    }
    if (write_index_table() < 0) return -1;
    return 0;
}

/* ---------------- UI ---------------- */
#define COL_BG       0xFF0A0A1A
#define COL_TITLE    0xFF40C0FF
#define COL_NORMAL   0xFFCCCCCC
#define COL_USED     0xFFFFFFFF
#define COL_EMPTY    0xFF606060
#define COL_HILITE   0xFF1A3050
#define COL_FOOTER   0xFF808080
#define COL_OK       0xFF40FF80
#define COL_ERR      0xFFFF6060
#define COL_WARN     0xFFFFC040

#define LIST_X      6
#define LIST_Y      28
#define LINE_H      14

typedef enum { MODE_LIST, MODE_INSPECT, MODE_DELETE_CONFIRM, MODE_FORMAT_CONFIRM } ui_mode_t;

static void draw_list(uint32_t *fb, int sel, const char *status_msg, uint32_t status_color) {
    memset32_neon((uint32_t)fb, COL_BG, LCD_W * LCD_H * 4);
    draw_text(fb, "N64 CONTROLLER PAK BROWSER", 80, 6, 2, COL_TITLE);

    for (int i = 0; i < NOTE_COUNT; i++) {
        int y = LIST_Y + i * LINE_H;
        if (i == sel) fill_rect(fb, 0, y - 1, LCD_W, LINE_H, COL_HILITE);
        char line[64];
        if (g_notes[i].used) {
            /* "[NN] USED  XXXXXXXXXXXXXXXX  GAME=XXXX  PG=NN" */
            int p = 0;
            line[p++] = '['; line[p++] = (char)('0' + i / 10); line[p++] = (char)('0' + i % 10); line[p++] = ']';
            line[p++] = ' ';
            for (int k = 0; k < 16 && g_notes[i].name[k]; k++) line[p++] = g_notes[i].name[k];
            for (int k = 0; k < 4; k++) {
                uint8_t c = g_notes[i].game_code[k];
                if (k == 0) { line[p++] = ' '; line[p++] = '['; }
                line[p++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
            }
            line[p++] = ']';
            line[p] = 0;
            draw_text(fb, line, LIST_X, y, 1, COL_USED);
        } else {
            char tmp[24];
            int p = 0;
            tmp[p++] = '['; tmp[p++] = (char)('0' + i / 10); tmp[p++] = (char)('0' + i % 10); tmp[p++] = ']';
            tmp[p++] = ' '; tmp[p++] = 'E'; tmp[p++] = 'M'; tmp[p++] = 'P'; tmp[p++] = 'T'; tmp[p++] = 'Y';
            tmp[p] = 0;
            draw_text(fb, tmp, LIST_X, y, 1, COL_EMPTY);
        }
    }

    /* Status message + footer. */
    if (status_msg) draw_text(fb, status_msg, LIST_X, LCD_H - 26, 1, status_color);
    draw_text(fb, "[A] INSPECT  [B] DELETE  [START] FORMAT  UP/DOWN SELECT",
              LIST_X, LCD_H - 12, 1, COL_FOOTER);
    dcache_clean_fb((uint32_t)fb);
}

static void draw_inspect(uint32_t *fb, int sel) {
    memset32_neon((uint32_t)fb, COL_BG, LCD_W * LCD_H * 4);
    char title[32]; int p = 0;
    const char *prefix = "NOTE SLOT ";
    while (*prefix) title[p++] = *prefix++;
    title[p++] = (char)('0' + sel / 10);
    title[p++] = (char)('0' + sel % 10);
    title[p] = 0;
    draw_text(fb, title, 6, 6, 2, COL_TITLE);

    /* 32 bytes as 2 rows of 16 each. */
    char line[80];
    for (int row = 0; row < 2; row++) {
        int p2 = 0;
        for (int col = 0; col < 16; col++) {
            uint8_t b = g_notes[sel].raw[row * 16 + col];
            const char *hex = "0123456789ABCDEF";
            line[p2++] = hex[(b >> 4) & 0xF];
            line[p2++] = hex[b & 0xF];
            line[p2++] = ' ';
        }
        line[p2++] = ' '; line[p2++] = '|'; line[p2++] = ' ';
        for (int col = 0; col < 16; col++) {
            uint8_t b = g_notes[sel].raw[row * 16 + col];
            line[p2++] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        line[p2] = 0;
        draw_text(fb, line, 6, 40 + row * 16, 1, COL_NORMAL);
    }

    /* Decoded fields. */
    if (g_notes[sel].used) {
        char dec[80]; int p3 = 0;
        const char *pre = "NAME: ";
        while (*pre) dec[p3++] = *pre++;
        for (int k = 0; k < 16 && g_notes[sel].name[k]; k++) dec[p3++] = g_notes[sel].name[k];
        dec[p3] = 0;
        draw_text(fb, dec, 6, 90, 1, COL_USED);

        p3 = 0;
        const char *pre2 = "GAME CODE: ";
        while (*pre2) dec[p3++] = *pre2++;
        for (int k = 0; k < 4; k++) {
            uint8_t c = g_notes[sel].game_code[k];
            dec[p3++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
        }
        dec[p3] = 0;
        draw_text(fb, dec, 6, 106, 1, COL_USED);

        p3 = 0;
        const char *pre3 = "START PAGE: ";
        while (*pre3) dec[p3++] = *pre3++;
        uint8_t sp = g_notes[sel].start_page;
        dec[p3++] = (char)('0' + sp / 100);
        dec[p3++] = (char)('0' + (sp / 10) % 10);
        dec[p3++] = (char)('0' + sp % 10);
        dec[p3] = 0;
        draw_text(fb, dec, 6, 122, 1, COL_USED);
    } else {
        draw_text(fb, "(empty slot)", 6, 90, 1, COL_EMPTY);
    }

    draw_text(fb, "[B] BACK", LIST_X, LCD_H - 12, 1, COL_FOOTER);
    dcache_clean_fb((uint32_t)fb);
}

static void draw_confirm(uint32_t *fb, const char *prompt) {
    memset32_neon((uint32_t)fb, COL_BG, LCD_W * LCD_H * 4);
    draw_text(fb, prompt, 40, 100, 2, COL_WARN);
    draw_text(fb, "[A] CONFIRM   [B] CANCEL", 80, 140, 2, COL_NORMAL);
    dcache_clean_fb((uint32_t)fb);
}

/* ---------------- Main ---------------- */
void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();
    video_init();

    uart_puts("\n\n=== N64 Controller Pak browser ===\n");

    input_init(INPUT_N64);

    /* Clear all display layers. */
    uint32_t *fb = (uint32_t *)FB0_ADDR;
    memset32_neon(FB0_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    /* Probe the pak. */
    int probe = cpak_probe();
    const char *status = NULL;
    uint32_t status_col = COL_NORMAL;
    if (probe != CPAK_CONTROLLER) {
        status = "NO CONTROLLER PAK DETECTED";
        status_col = COL_ERR;
    } else if (read_index_table() < 0 || read_note_table() < 0) {
        status = "READ FAILURE - PRESS START TO FORMAT";
        status_col = COL_ERR;
    } else {
        status = "PAK READY";
        status_col = COL_OK;
    }

    int sel = 0;
    ui_mode_t mode = MODE_LIST;
    draw_list(fb, sel, status, status_col);

    while (1) {
        uint32_t t0 = timer_read();

        input_poll();
        uint16_t pressed = input_pressed();

        if (mode == MODE_LIST) {
            int dirty = 0;
            if (pressed & BTN_DOWN) { sel = (sel + 1) % NOTE_COUNT; dirty = 1; }
            if (pressed & BTN_UP)   { sel = (sel + NOTE_COUNT - 1) % NOTE_COUNT; dirty = 1; }
            if (pressed & BTN_A) {
                mode = MODE_INSPECT;
                draw_inspect(fb, sel);
                dirty = 0;
            }
            if (pressed & BTN_B) {
                if (g_notes[sel].used) {
                    mode = MODE_DELETE_CONFIRM;
                    draw_confirm(fb, "DELETE NOTE?");
                    dirty = 0;
                }
            }
            if (pressed & BTN_START) {
                mode = MODE_FORMAT_CONFIRM;
                draw_confirm(fb, "FORMAT WIPES PAK");
                dirty = 0;
            }
            if (dirty) draw_list(fb, sel, status, status_col);
        }
        else if (mode == MODE_INSPECT) {
            if (pressed & BTN_B) {
                mode = MODE_LIST;
                draw_list(fb, sel, status, status_col);
            }
        }
        else if (mode == MODE_DELETE_CONFIRM) {
            if (pressed & BTN_A) {
                int rc = delete_note(sel);
                status_col = (rc == 0) ? COL_OK : COL_ERR;
                status     = (rc == 0) ? "NOTE DELETED" : "DELETE FAILED";
                /* Re-read note table to reflect what's actually on the pak. */
                read_note_table();
                mode = MODE_LIST;
                draw_list(fb, sel, status, status_col);
            } else if (pressed & BTN_B) {
                mode = MODE_LIST;
                draw_list(fb, sel, status, status_col);
            }
        }
        else if (mode == MODE_FORMAT_CONFIRM) {
            if (pressed & BTN_A) {
                int rc = format_pak();
                if (rc == 0) {
                    read_index_table();
                    read_note_table();
                    status     = "PAK FORMATTED";
                    status_col = COL_OK;
                } else {
                    status     = "FORMAT FAILED";
                    status_col = COL_ERR;
                }
                mode = MODE_LIST;
                draw_list(fb, sel, status, status_col);
            } else if (pressed & BTN_B) {
                mode = MODE_LIST;
                draw_list(fb, sel, status, status_col);
            }
        }

        /* 60 fps cap. */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) { }
    }
}
