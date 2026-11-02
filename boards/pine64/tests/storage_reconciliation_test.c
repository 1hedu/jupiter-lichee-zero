/* Actual CPak raw and filesystem code with a byte-level Joybus card model. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpak.h"
#include "cpakfs.h"
#define N64_JOYBUS_H
#define PMU_H
static uint8_t pak[CPAK_TOTAL_BYTES], tx[35];
static unsigned tx_size, delay_calls, reads, writes;
static uint32_t selected_block;
static uint8_t returned_crc;
static uint32_t n64_joybus_lock(void) { tx_size = 0; return 0x80; }
static void n64_joybus_unlock(uint32_t saved) { assert(saved == 0x80); }
static void delay_us(uint32_t us) { assert(us == 100); ++delay_calls; }
static void n64_send_byte(uint8_t byte) { assert(tx_size < sizeof(tx)); tx[tx_size++] = byte; }
static uint8_t wire_crc(const uint8_t *data)
{
    uint8_t crc = 0;
    for (unsigned bit = 0; bit < 264; ++bit) {
        unsigned outgoing = crc >> 7;
        unsigned incoming = bit < 256 ? (data[bit / 8] >> (7 - bit % 8)) & 1u : 0;
        crc = (uint8_t)((crc << 1) | incoming);
        if (outgoing) crc ^= 0x85;
    }
    return crc;
}
static void n64_send_stop(void)
{
    assert(tx_size == 3 || tx_size == 35);
    selected_block = (((unsigned)tx[1] << 8) | tx[2]) >> 5;
    assert(selected_block < CPAK_NUM_BLOCKS);
    if (tx[0] == 3) {
        assert(tx_size == 35);
        memcpy(pak + selected_block * 32, tx + 3, 32);
        ++writes;
    } else { assert(tx[0] == 2 && tx_size == 3); ++reads; }
    returned_crc = wire_crc(pak + selected_block * 32);
}
static int n64_recv_buf(uint8_t *out, int bytes)
{
    assert(bytes == 32 && tx[0] == 2);
    memcpy(out, pak + selected_block * 32, 32);
    return 0;
}
static int n64_recv_byte(uint8_t *out) { *out = returned_crc; return 0; }
void uart_puts(const char *s) { (void)s; }
void uart_puthex(uint32_t n) { (void)n; }
void uart_putdec(uint32_t n) { (void)n; }
uint32_t timer_read(void) { return 0xabc12345u; }
#include "../../../lib/cpak.c"

int main(void)
{
    uint8_t buffer[512];
    assert(cpak_read(UINT32_MAX - 3u, buffer, 8u) == -1);
    assert(cpak_write(UINT32_MAX - 3u, buffer, 8u) == -1);
    assert(cpak_read(CPAK_TOTAL_BYTES - 4u, buffer, 8u) == -1);
    assert(cpak_write(CPAK_TOTAL_BYTES - 4u, buffer, 8u) == -1);
    assert(cpak_read_block(2048, buffer) == -1 && cpak_write_block(2048, buffer) == -1);
    assert(!cpak_read(CPAK_TOTAL_BYTES, NULL, 0) && !cpak_write(CPAK_TOTAL_BYTES, NULL, 0));
    assert(!reads && !writes && !delay_calls);
    memset(buffer, 0xfe, sizeof(buffer));
    assert(!cpak_write(0, buffer, 64) && delay_calls == 2 && writes == 2);
    memset(buffer, 0, sizeof(buffer));
    assert(!cpak_read(0, buffer, 64) && delay_calls == 4 && reads == 2);
    for (unsigned i = 0; i < 64; ++i) assert(buffer[i] == 0xfe);
    assert(!cpakfs_format() && !cpakfs_mount());
    assert(cpakfs_note_count() == 0 && cpakfs_free_blocks() == 984);
    char *short_name = malloc(2), *empty = malloc(1);
    assert(short_name && empty);
    short_name[0] = 'A'; short_name[1] = 0; empty[0] = 0;
    int slot = cpakfs_create_note(short_name, empty, short_name, empty, 8, NULL);
    assert(slot >= 0 && cpakfs_free_blocks() == 976);
    cpakfs_note_t note;
    assert(!cpakfs_get_note(0, &note));
    assert(!strcmp(note.game_code, "A   ") && !strcmp(note.publisher, "  "));
    assert(!strcmp(note.name, "A") && !strcmp(note.ext, ""));
    assert(note.blocks_used == 8 && note.bytes_used == 256);
    assert(cpakfs_read_note(&note, buffer) == 256);
    for (unsigned i = 0; i < 256; ++i) assert(buffer[i] == 0);
    memset(buffer, 0x53, 256);
    assert(!cpakfs_write_note(&note, buffer));
    memset(buffer, 0, 256);
    assert(cpakfs_read_note(&note, buffer) == 256);
    for (unsigned i = 0; i < 256; ++i) assert(buffer[i] == 0x53);
    assert(!cpakfs_delete_note(slot) && cpakfs_free_blocks() == 984);
    assert(cpakfs_create_note(NULL, NULL, NULL, NULL, UINT16_MAX, NULL) == -2);
    free(short_name); free(empty);
    puts("CPak PASS: overflow rejection, one gap per transaction, short-string bounds, format/mount and real note round-trip");
    return 0;
}
