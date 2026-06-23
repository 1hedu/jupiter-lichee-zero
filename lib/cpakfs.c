/*
 * Jupiter SDK — Controller Pak note filesystem
 *
 * Reference: n64brew wiki "Controller Pak/Filesystem", cross-checked
 * against libdragon's mempak implementation.
 *
 * Layout (32 KiB total, 256-byte pages, 128 pages):
 *
 *   Page 0 : ID sector
 *     0x00..0x1F   Label area (32B, unused — leave zero)
 *     0x20..0x3F   ID block copy 0 (32B)
 *     0x60..0x7F   ID block copy 1 (32B)
 *     0x80..0x9F   ID block copy 2 (32B)
 *     0xC0..0xDF   ID block copy 3 (32B)
 *     (other regions 0x40..0x5F, 0xA0..0xBF, 0xE0..0xFF unused)
 *
 *   Page 1 : Index Table primary
 *   Page 2 : Index Table backup
 *     128 × uint16_t (BE) entries; entry[i] is the IndexTable I-Node
 *     for page i. The 5 reserved I-Nodes (i=0..4) are zero except for
 *     the LOW BYTE of I-Node 0, which is the 8-bit checksum (sum of
 *     all bytes from offset 0x0A to end of page).
 *     Entry values:
 *       0x0001 = end of chain
 *       0x0003 = free
 *       5..127 = next page index in chain
 *
 *   Page 3 + Page 4 : Note Table (16 × 32-byte entries)
 *   Page 5..127     : Data (123 pages = ~31.5 KiB usable)
 *
 * ID block format (32 bytes), all big-endian:
 *   0x00..0x17   Serial number (24 bytes — random / unique)
 *   0x18..0x19   Device ID  (bit 0 = 1 for cpak)
 *   0x1A..0x1B   Bank size  (always 0x0001 for 32 KiB single-bank)
 *   0x1C..0x1D   Checksum 1 = sum of words 0..13 (mod 0x10000)
 *   0x1E..0x1F   Checksum 2 = (0xFFF2 - checksum1) mod 0x10000
 */
#include "jupiter.h"
#include "cpak.h"
#include "cpakfs.h"

#define PAGE_SIZE        256
#define PAGE_BLOCKS      8         /* 256 / 32 */
#define NUM_PAGES        128
#define NUM_DATA_PAGES   123
#define DATA_PAGE_FIRST  5
#define INODE_END        0x0001
#define INODE_FREE       0x0003

#define PAGE_ID          0
#define PAGE_INDEX_PRI   1
#define PAGE_INDEX_BAK   2
#define PAGE_NOTE_LO     3
#define PAGE_NOTE_HI     4

/* ID block copy offsets within page 0 */
static const int id_copy_offsets[4] = { 0x20, 0x60, 0x80, 0xC0 };

/* ================================================================== */
/* Endianness helpers                                                   */
/* ================================================================== */
static inline uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}
static inline void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v & 0xFF;
}

/* ================================================================== */
/* N64 character encoding for note names/extensions                     */
/* ================================================================== */
static char cpakfs_decode_char(uint8_t c)
{
    if (c == 0x00) return 0;
    if (c == 0x0F) return ' ';
    if (c >= 0x10 && c <= 0x19) return '0' + (c - 0x10);
    if (c >= 0x1A && c <= 0x33) return 'A' + (c - 0x1A);
    switch (c) {
        case 0x34: return '!';
        case 0x35: return '"';
        case 0x36: return '#';
        case 0x37: return '`';
        case 0x38: return '*';
        case 0x39: return '+';
        case 0x3A: return ',';
        case 0x3B: return '-';
        case 0x3C: return '.';
        case 0x3D: return '/';
        case 0x3E: return ':';
        case 0x3F: return '=';
        default:   return '?';   /* katakana / unknown */
    }
}

static uint8_t cpakfs_encode_char(char c)
{
    if (c == 0)   return 0x00;
    if (c == ' ') return 0x0F;
    if (c >= '0' && c <= '9') return 0x10 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 0x1A + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x1A + (c - 'a');   /* fold to upper */
    switch (c) {
        case '!': return 0x34;
        case '"': return 0x35;
        case '#': return 0x36;
        case '`': return 0x37;
        case '*': return 0x38;
        case '+': return 0x39;
        case ',': return 0x3A;
        case '-': return 0x3B;
        case '.': return 0x3C;
        case '/': return 0x3D;
        case ':': return 0x3E;
        case '=': return 0x3F;
    }
    return 0x0F;
}

/* ================================================================== */
/* Page-level I/O                                                       */
/* ================================================================== */
static int read_page(int page, uint8_t *out)
{
    if (page < 0 || page >= NUM_PAGES) return -1;
    for (int i = 0; i < PAGE_BLOCKS; i++) {
        int rc = cpak_read_block(page * PAGE_BLOCKS + i,
                                 out + i * CPAK_BLOCK_SIZE);
        if (rc < 0) return rc;
    }
    return 0;
}

static int write_page(int page, const uint8_t *in)
{
    if (page < 0 || page >= NUM_PAGES) return -1;
    for (int i = 0; i < PAGE_BLOCKS; i++) {
        int rc = cpak_write_block(page * PAGE_BLOCKS + i,
                                  in + i * CPAK_BLOCK_SIZE);
        if (rc < 0) return rc;
    }
    return 0;
}

/* ================================================================== */
/* In-memory FS state                                                   */
/* ================================================================== */
static int     fs_mounted;
static uint8_t idx_table[PAGE_SIZE];          /* Index Table primary */
static uint8_t note_table[PAGE_SIZE * 2];     /* Note Table (2 pages) */

/* ================================================================== */
/* Index Table checksum / validation                                    */
/* ================================================================== */
/*
 * 8-bit checksum stored in the LOW byte of the first I-Node (offset 1).
 * Computed by summing all bytes from offset 0x0A through end of page.
 * Bytes at offsets 0..9 are not included (those are the 5 reserved
 * I-Nodes plus the checksum itself).
 */
static uint8_t idx_checksum(const uint8_t *page)
{
    uint32_t sum = 0;
    for (int i = 0x0A; i < PAGE_SIZE; i++) sum += page[i];
    return (uint8_t)(sum & 0xFF);
}

static int idx_valid(const uint8_t *page)
{
    return page[1] == idx_checksum(page);
}

/* ================================================================== */
/* ID block validation                                                  */
/* ================================================================== */
/*
 * Sum first 14 BE words; checksum1 = sum (mod 0x10000).
 * Checksum2 = (0xFFF2 - checksum1) (mod 0x10000).
 */
static int id_block_valid(const uint8_t *blk)
{
    uint32_t s = 0;
    for (int i = 0; i < 14; i++) s += be16(blk + i * 2);
    uint16_t c1 = (uint16_t)(s & 0xFFFF);
    uint16_t c2 = (uint16_t)((0xFFF2 - c1) & 0xFFFF);
    return be16(blk + 0x1C) == c1 && be16(blk + 0x1E) == c2;
}

static void id_block_finalize(uint8_t *blk)
{
    uint32_t s = 0;
    for (int i = 0; i < 14; i++) s += be16(blk + i * 2);
    uint16_t c1 = (uint16_t)(s & 0xFFFF);
    uint16_t c2 = (uint16_t)((0xFFF2 - c1) & 0xFFFF);
    put_be16(blk + 0x1C, c1);
    put_be16(blk + 0x1E, c2);
}

static int id_page_valid(const uint8_t *page)
{
    for (int i = 0; i < 4; i++) {
        if (id_block_valid(page + id_copy_offsets[i])) return 1;
    }
    return 0;
}

/* ================================================================== */
/* Mount                                                                */
/* ================================================================== */
int cpakfs_mount(void)
{
    fs_mounted = 0;

    uint8_t pg[PAGE_SIZE];
    if (read_page(PAGE_ID, pg) < 0) return -1;
    if (!id_page_valid(pg))         return -2;

    if (read_page(PAGE_INDEX_PRI, idx_table) < 0) return -1;
    if (!idx_valid(idx_table)) {
        if (read_page(PAGE_INDEX_BAK, idx_table) < 0) return -1;
        if (!idx_valid(idx_table)) return -2;
    }

    if (read_page(PAGE_NOTE_LO, note_table) < 0) return -1;
    if (read_page(PAGE_NOTE_HI, note_table + PAGE_SIZE) < 0) return -1;

    fs_mounted = 1;
    return 0;
}

/* ================================================================== */
/* Format                                                               */
/* ================================================================== */
/*
 * Lays down a valid empty Pak: ID page with 4 valid ID block copies,
 * empty (all-free) Index Table primary + backup, zero Note Table.
 *
 * Serial number is left as zeros — fine for an unbranded fresh format.
 * Real packs use random bytes; we don't have an RTC seed handy. If
 * Jupiter picks up a hardware RNG later, fill blk[0..0x17] with it.
 */
int cpakfs_format(void)
{
    uint8_t pg[PAGE_SIZE];

    /* --- Page 0: ID sector --- */
    for (int i = 0; i < PAGE_SIZE; i++) pg[i] = 0;

    /* Build the canonical ID block once, then replicate to 4 slots */
    uint8_t blk[32];
    for (int i = 0; i < 32; i++) blk[i] = 0;
    /* serial: zeros (placeholder; replace with RNG when available) */
    /* device id: bit 0 = 1 (controller pak) */
    blk[0x18] = 0x00;
    blk[0x19] = 0x01;
    /* bank size: 1 (single 32 KiB bank) */
    blk[0x1A] = 0x00;
    blk[0x1B] = 0x01;
    id_block_finalize(blk);

    for (int c = 0; c < 4; c++) {
        for (int i = 0; i < 32; i++) pg[id_copy_offsets[c] + i] = blk[i];
    }
    if (write_page(PAGE_ID, pg) < 0) return -1;

    /* --- Pages 1, 2: Index Table primary + backup --- */
    for (int i = 0; i < PAGE_SIZE; i++) pg[i] = 0;
    /* Reserved I-Nodes 0..4 stay zero; data I-Nodes 5..127 = FREE */
    for (int i = 5; i < 128; i++) put_be16(pg + i * 2, INODE_FREE);
    pg[1] = idx_checksum(pg);
    if (write_page(PAGE_INDEX_PRI, pg) < 0) return -1;
    if (write_page(PAGE_INDEX_BAK, pg) < 0) return -1;
    for (int i = 0; i < PAGE_SIZE; i++) idx_table[i] = pg[i];

    /* --- Pages 3, 4: Note Table (zeroed) --- */
    for (int i = 0; i < PAGE_SIZE; i++) pg[i] = 0;
    if (write_page(PAGE_NOTE_LO, pg) < 0) return -1;
    if (write_page(PAGE_NOTE_HI, pg) < 0) return -1;
    for (int i = 0; i < (int)sizeof(note_table); i++) note_table[i] = 0;

    fs_mounted = 1;
    return 0;
}

/* ================================================================== */
/* Note Table accessors                                                 */
/* ================================================================== */
/*
 * Note entry (32 bytes), big-endian fields:
 *   0x00..0x03   Game code (4 ASCII chars)
 *   0x04..0x05   Publisher (2 ASCII chars)
 *   0x06..0x07   Start page (BE u16)
 *   0x08..0x09   Status flags (bit 0x02 = used)
 *   0x0A..0x0B   Reserved
 *   0x0C..0x0F   Extension (4 chars, N64-encoded)
 *   0x10..0x1F   Name (16 chars, N64-encoded)
 */
static int note_slot_used(int slot)
{
    const uint8_t *e = note_table + slot * 32;
    /* "used" = status bit 0x02 set, AND start page in valid data range */
    if ((e[9] & 0x02) == 0) return 0;
    uint16_t start = be16(e + 6);
    return (start >= DATA_PAGE_FIRST && start < NUM_PAGES);
}

static void parse_note(int slot, cpakfs_note_t *out)
{
    const uint8_t *e = note_table + slot * 32;

    for (int i = 0; i < 4; i++) out->game_code[i] = e[i];
    out->game_code[4] = 0;
    for (int i = 0; i < 2; i++) out->publisher[i] = e[4 + i];
    out->publisher[2] = 0;

    for (int i = 0; i < 4; i++)  out->ext[i]  = cpakfs_decode_char(e[0x0C + i]);
    out->ext[4] = 0;
    for (int i = 0; i < 16; i++) out->name[i] = cpakfs_decode_char(e[0x10 + i]);
    out->name[16] = 0;

    out->first_block = be16(e + 6);
    out->slot = slot;

    /* Walk the chain to count pages */
    uint16_t pages = 0;
    uint16_t p = out->first_block;
    int safety = NUM_PAGES;
    while (p >= DATA_PAGE_FIRST && p < NUM_PAGES && safety-- > 0) {
        pages++;
        uint16_t next = be16(idx_table + p * 2);
        if (next == INODE_END) break;
        p = next;
    }
    out->blocks_used = pages * PAGE_BLOCKS;
    out->bytes_used  = pages * PAGE_SIZE;
}

/* ================================================================== */
/* Public queries                                                       */
/* ================================================================== */
int cpakfs_note_count(void)
{
    if (!fs_mounted) return -1;
    int n = 0;
    for (int s = 0; s < CPAKFS_MAX_NOTES; s++)
        if (note_slot_used(s)) n++;
    return n;
}

int cpakfs_get_note(int i, cpakfs_note_t *out)
{
    if (!fs_mounted) return -1;
    int seen = 0;
    for (int s = 0; s < CPAKFS_MAX_NOTES; s++) {
        if (note_slot_used(s)) {
            if (seen == i) { parse_note(s, out); return 0; }
            seen++;
        }
    }
    return -1;
}

int cpakfs_free_blocks(void)
{
    if (!fs_mounted) return -1;
    int free_pages = 0;
    for (int p = DATA_PAGE_FIRST; p < NUM_PAGES; p++) {
        if (be16(idx_table + p * 2) == INODE_FREE) free_pages++;
    }
    return free_pages * PAGE_BLOCKS;
}

/* ================================================================== */
/* Read note                                                            */
/* ================================================================== */
int cpakfs_read_note(const cpakfs_note_t *note, void *buf)
{
    if (!fs_mounted) return -1;
    uint8_t *dst = (uint8_t *)buf;
    uint16_t p = note->first_block;
    int safety = NUM_PAGES;
    int total = 0;

    while (p >= DATA_PAGE_FIRST && p < NUM_PAGES && safety-- > 0) {
        if (read_page(p, dst + total) < 0) return -1;
        total += PAGE_SIZE;
        uint16_t next = be16(idx_table + p * 2);
        if (next == INODE_END) break;
        p = next;
    }
    return total;
}

/* ================================================================== */
/* Write existing note (in place)                                       */
/* ================================================================== */
int cpakfs_write_note(const cpakfs_note_t *note, const void *data)
{
    if (!fs_mounted) return -1;
    const uint8_t *src = (const uint8_t *)data;
    uint16_t p = note->first_block;
    int safety = NUM_PAGES;
    int written = 0;

    while (p >= DATA_PAGE_FIRST && p < NUM_PAGES && safety-- > 0) {
        if (write_page(p, src + written) < 0) return -1;
        written += PAGE_SIZE;
        uint16_t next = be16(idx_table + p * 2);
        if (next == INODE_END) break;
        p = next;
    }
    return 0;
}

/* ================================================================== */
/* Create note                                                          */
/* ================================================================== */
int cpakfs_create_note(const char *game_code,
                       const char *publisher,
                       const char *name,
                       const char *ext,
                       uint16_t blocks,
                       const void *initial_data)
{
    if (!fs_mounted) return -3;
    if (blocks < 1)  return -2;

    /* Round up to whole pages — chain links are page-granular */
    uint16_t pages = (blocks + PAGE_BLOCKS - 1) / PAGE_BLOCKS;

    /* Find free Note Table slot */
    int slot = -1;
    for (int s = 0; s < CPAKFS_MAX_NOTES; s++) {
        if (!note_slot_used(s)) { slot = s; break; }
    }
    if (slot < 0) return -1;

    /* Find free pages */
    uint16_t alloc[NUM_PAGES];
    int n_alloc = 0;
    for (int p = DATA_PAGE_FIRST; p < NUM_PAGES && n_alloc < pages; p++) {
        if (be16(idx_table + p * 2) == INODE_FREE) alloc[n_alloc++] = p;
    }
    if (n_alloc < pages) return -2;

    /* Build the chain */
    for (int i = 0; i < pages - 1; i++)
        put_be16(idx_table + alloc[i] * 2, alloc[i + 1]);
    put_be16(idx_table + alloc[pages - 1] * 2, INODE_END);
    idx_table[1] = idx_checksum(idx_table);

    /* Fill the Note Table entry */
    uint8_t *e = note_table + slot * 32;
    for (int i = 0; i < 32; i++) e[i] = 0;
    for (int i = 0; i < 4; i++)
        e[i]     = (game_code && game_code[i]) ? game_code[i] : ' ';
    for (int i = 0; i < 2; i++)
        e[4 + i] = (publisher && publisher[i]) ? publisher[i] : ' ';
    put_be16(e + 6, alloc[0]);
    e[8] = 0x00;
    e[9] = 0x02;   /* status flag: used */
    for (int i = 0; i < 4; i++)
        e[0x0C + i] = (ext && ext[i]) ? cpakfs_encode_char(ext[i]) : 0;
    for (int i = 0; i < 16; i++)
        e[0x10 + i] = (name && name[i]) ? cpakfs_encode_char(name[i]) : 0;

    /* Commit metadata (Index Table primary + backup, Note Table) */
    if (write_page(PAGE_INDEX_PRI, idx_table)             < 0) return -3;
    if (write_page(PAGE_INDEX_BAK, idx_table)             < 0) return -3;
    if (write_page(PAGE_NOTE_LO,   note_table)            < 0) return -3;
    if (write_page(PAGE_NOTE_HI,   note_table + PAGE_SIZE) < 0) return -3;

    /* Write data pages (or zeros if no initial data given) */
    uint8_t zero[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++) zero[i] = 0;
    const uint8_t *src = (const uint8_t *)initial_data;
    for (int i = 0; i < pages; i++) {
        const uint8_t *pg = initial_data ? (src + i * PAGE_SIZE) : zero;
        if (write_page(alloc[i], pg) < 0) return -3;
    }

    return slot;
}

/* ================================================================== */
/* Delete note                                                          */
/* ================================================================== */
int cpakfs_delete_note(int slot)
{
    if (!fs_mounted) return -1;
    if (slot < 0 || slot >= CPAKFS_MAX_NOTES) return -1;
    if (!note_slot_used(slot)) return -1;

    /* Walk chain, freeing pages */
    uint8_t *e = note_table + slot * 32;
    uint16_t p = be16(e + 6);
    int safety = NUM_PAGES;
    while (p >= DATA_PAGE_FIRST && p < NUM_PAGES && safety-- > 0) {
        uint16_t next = be16(idx_table + p * 2);
        put_be16(idx_table + p * 2, INODE_FREE);
        if (next == INODE_END) break;
        p = next;
    }

    idx_table[1] = idx_checksum(idx_table);
    for (int i = 0; i < 32; i++) e[i] = 0;

    if (write_page(PAGE_INDEX_PRI, idx_table)             < 0) return -1;
    if (write_page(PAGE_INDEX_BAK, idx_table)             < 0) return -1;
    if (write_page(PAGE_NOTE_LO,   note_table)            < 0) return -1;
    if (write_page(PAGE_NOTE_HI,   note_table + PAGE_SIZE) < 0) return -1;
    return 0;
}
