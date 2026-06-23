/*
 * Jupiter SDK — Controller Pak note filesystem
 *
 * Implements Nintendo's official 32 KB note filesystem so saves are
 * interoperable with real N64 games. Layout:
 *
 *   Block 0      — ID area (header, with backups at 0x20/0x40/0x60/0x80
 *                  inside the ID block — Nintendo stores 4 redundant copies)
 *   Blocks 1-4   — IndexTable (FAT-like). Block 1 is primary; blocks 3-4
 *                  are the mirror. Block 2 is reserved.
 *   Blocks 5-7   — NoteTable: 16 entries × 32 bytes = 512 bytes total
 *   Blocks 8-127 — File data (123 blocks usable, ~3.94 KB)
 *
 * A "note" is a save file. Each note is identified by:
 *   - game_code (4 ASCII chars, e.g. "NSME" for SM64 USA)
 *   - publisher (2 ASCII chars, e.g. "01" for Nintendo)
 *   - filename  (16 chars, N64-encoded — see cpakfs_decode_name)
 *   - extension (4 chars)
 *
 * Notes are at most 16 in number; total data blocks across all notes
 * cannot exceed 123.
 *
 * All FS functions assume cpak_probe() returned CPAK_CONTROLLER.
 */
#ifndef JUPITER_CPAKFS_H
#define JUPITER_CPAKFS_H

#include <stdint.h>

#define CPAKFS_MAX_NOTES        16
#define CPAKFS_DATA_BLOCKS      123
#define CPAKFS_NAME_LEN         16
#define CPAKFS_EXT_LEN          4
#define CPAKFS_GAMECODE_LEN     4
#define CPAKFS_PUBLISHER_LEN    2

typedef struct {
    char     game_code[CPAKFS_GAMECODE_LEN + 1];   /* null-terminated */
    char     publisher[CPAKFS_PUBLISHER_LEN + 1];
    char     name[CPAKFS_NAME_LEN + 1];            /* ASCII-decoded */
    char     ext[CPAKFS_EXT_LEN + 1];
    uint16_t first_block;                          /* 5..127 */
    uint16_t blocks_used;
    uint32_t bytes_used;                           /* blocks_used * 32 */
    int      slot;                                 /* 0..15, FS slot index */
} cpakfs_note_t;

/* Mount the filesystem: read header, validate, pick the live IndexTable.
 * Returns 0 on success, -1 on I/O error, -2 on corrupt header (use
 * cpakfs_format() to recover, destroying all data). */
int cpakfs_mount(void);

/* Format a blank Controller Pak. ALL DATA IS LOST. Returns 0 on success. */
int cpakfs_format(void);

/* Return number of valid notes (0..16). */
int cpakfs_note_count(void);

/* Get info on the i-th valid note (0..count-1). Returns 0 on success. */
int cpakfs_get_note(int i, cpakfs_note_t *out);

/* Free space, in 32-byte blocks (0..123). */
int cpakfs_free_blocks(void);

/* Read a note's full data into buf. Caller must size buf to
 * note->bytes_used. Returns bytes read, or -1 on error. */
int cpakfs_read_note(const cpakfs_note_t *note, void *buf);

/* Create a new note. game_code/publisher/name/ext follow the same
 * conventions as cpakfs_note_t. blocks must be >= 1 and <=
 * cpakfs_free_blocks(). Returns the slot index (0..15) on success, or
 * -1 if no slot, -2 if no space, -3 on I/O error. */
int cpakfs_create_note(const char *game_code,
                       const char *publisher,
                       const char *name,
                       const char *ext,
                       uint16_t blocks,
                       const void *initial_data);

/* Delete the note at slot. Returns 0 on success. */
int cpakfs_delete_note(int slot);

/* Overwrite an existing note's data in place. data_len must equal the
 * note's allocated size in bytes. Returns 0 on success. */
int cpakfs_write_note(const cpakfs_note_t *note, const void *data);

#endif
