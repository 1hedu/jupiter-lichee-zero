/* Jupiter port: Stratagus engine self-test.
 *
 * Prove the linked-in Stratagus engine actually runs on bare-metal V3s.
 * Goal: construct a minimal 40×20 all-passable CMap, fabricate a CUnit +
 * CUnitType, call Stratagus's AStarFindPath from (1,1)→(38,18), verify we
 * get a plausible path length.
 *
 * If this passes, the rest of the engine — actions, missiles, spells —
 * is callable the same way. */

#include "stratagus.h"
#include "map.h"
#include "tileset.h"
#include "tile.h"
#include "unit.h"
#include "unittype.h"
#include "player.h"
#include "pathfinder.h"
#include "vec2i.h"
#include "video.h"
#include "commands.h"
#include "actions.h"
#include "animation.h"
#include "animation/animation_frame.h"
#include "animation/animation_wait.h"
#include "script.h"  /* for Stratagus InitLua / lua_State *Lua */
#include "upgrade_structs.h"
#include "unit_manager.h"   /* CUnitManager / UnitManager global */
#include "fow.h"            /* CFogOfWar / FogOfWar global */
#include "widgets.h"        /* initGuichan / freeGuichan / Gui */
#include "ui.h"             /* CUserInterface global */
#include <new>         /* placement new for UnitTypeVar bootstrap */
#include <setjmp.h>

/* tolua's generated file defines this with C++ linkage despite the
 * extern "C" in tolua++.h's header block — the file-scope forward decl
 * it emits falls outside any extern "C" at bindings cpp line 14. Match
 * that mangled name. */
int tolua_stratagus_open(lua_State *);

/* ExitFatal → longjmp escape rig; see war1_link_stubs.cpp. */
extern "C" jmp_buf g_war1_fatal_jmp;
extern "C" int     g_war1_fatal_armed;
#include "war1_pc8.h"
#include <memory>
#include <cstdio>
#include <cstring>

/* Real Lua 5.3 — provided by third_party/lua/lua-5.3.6/src */
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

/* uart_puts / uart_putdec / dcache_clean_range come from war1_pc8.h's
 * extern "C"-wrapped include of jupiter.h. */
#define WAR1_DCACHE_CLEAN_FB(addr) dcache_clean_range((addr), LCD_FB_BYTES)

/* ---- Asset catalog (Stratagus path → embedded pc8 blob) ---- */
#include "war1_asset_catalog.h"
#include "war1_sound_catalog.h"

static const war1_asset_entry_t *war1_asset_lookup(const char *path) {
    if (!path) return nullptr;
    for (int i = 0; i < WAR1_ASSET_COUNT; i++) {
        if (strcmp(WAR1_ASSETS[i].path, path) == 0) return &WAR1_ASSETS[i];
    }
    /* Some scripts prefix "contrib/graphics/" on select paths (e.g.
     * ui.lua passes "contrib/graphics/ui/human/top_resource_bar.png").
     * Strip that prefix as a fallback so the filler still resolves. */
    const char *prefix = "contrib/graphics/";
    size_t plen = 17;
    if (strncmp(path, prefix, plen) == 0) {
        const char *rest = path + plen;
        for (int i = 0; i < WAR1_ASSET_COUNT; i++) {
            if (strcmp(WAR1_ASSETS[i].path, rest) == 0) return &WAR1_ASSETS[i];
        }
    }
    return nullptr;
}

/* C-linkage probe for Stratagus's CanAccessFile. Our bare-metal iolib
 * override routes through this so UI.Fillers / campaign screens etc.
 * see assets that exist in the embedded catalog even though fs::exists
 * returns false for everything. */
extern "C" int war1_asset_exists(const char *path) {
    return war1_asset_lookup(path) != nullptr ? 1 : 0;
}

/* Used by the guisan ImageLoader (third_party/guisan_jupiter/jupiter_imageloader.cpp)
 * to read the raw pc8 blob bytes for a catalog path. Returns nullptr on miss. */
extern "C" const unsigned char *war1_asset_blob(const char *path) {
    const war1_asset_entry_t *e = war1_asset_lookup(path);
    return e ? e->data : nullptr;
}

/* ---- Sound catalog lookup + Mix_* implementations ----
 * SDL_mixer is a stub — Stratagus's sound_server.cpp goes through
 * Mix_LoadWAV(name) → Mix_PlayChannel(-1, chunk, loops). We resolve
 * `name` against war1_sound_catalog (built by scripts/war1_sounds.py from
 * data.War1gus/sounds/*.wav.gz) and dispatch onto Jupiter's PCM mixer
 * (audio_pcm_play_rate, which resamples on the fly so we keep the WAV's
 * native 11025 Hz instead of pre-bloating to 48 kHz). */
static const war1_sound_entry_t *war1_sound_lookup(const char *path) {
    if (!path) return nullptr;
    /* Try exact match first. */
    for (int i = 0; i < WAR1_SOUND_COUNT; i++) {
        if (strcmp(WAR1_SOUNDS[i].path, path) == 0) return &WAR1_SOUNDS[i];
    }
    /* LibraryFileName hands us paths like "blacksmith.wav" but Lua sometimes
     * resolves through an alias. Try matching the basename as a fallback. */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    for (int i = 0; i < WAR1_SOUND_COUNT; i++) {
        const char *cb = strrchr(WAR1_SOUNDS[i].path, '/');
        cb = cb ? cb + 1 : WAR1_SOUNDS[i].path;
        if (strcmp(cb, base) == 0) return &WAR1_SOUNDS[i];
    }
    return nullptr;
}

extern "C" {
#include "SDL_mixer.h"
}

/* Slot-based Mix_Chunk allocator. We need stable storage for chunks we
 * hand back (sound_server.cpp wraps them in unique_ptrs and dereferences
 * abuf/alen later). The chunks themselves are tiny — 16 slots is plenty
 * for our SFX catalog because LoadSample re-uses on cache hit isn't
 * necessary at this stage; we can churn through unique chunks. */
#define WAR1_MIX_CHUNK_POOL 256
static Mix_Chunk g_mix_chunk_pool[WAR1_MIX_CHUNK_POOL];
static int      g_mix_chunk_used = 0;

static Mix_Chunk *war1_mix_alloc_chunk(const war1_sound_entry_t *e) {
    if (g_mix_chunk_used >= WAR1_MIX_CHUNK_POOL) return nullptr;
    Mix_Chunk *c = &g_mix_chunk_pool[g_mix_chunk_used++];
    c->allocated = 0;
    c->abuf      = (Uint8 *)e->data;
    c->alen      = (Uint32)(e->data_end - e->data);
    c->volume    = MIX_MAX_VOLUME;
    c->rate      = e->rate;
    return c;
}

/* Forward decl — implementation lives in the SD-streaming section below. */
extern "C" Mix_Chunk *war1_intro_load(const char *file);

extern "C" Mix_Chunk *Mix_LoadWAV(const char *file) {
    const war1_sound_entry_t *e = war1_sound_lookup(file);
    static int probe = 0;
    if (probe < 8) {
        probe++;
        uart_puts("[Mix_LoadWAV] '");
        uart_puts(file ? file : "(null)");
        uart_puts(e ? "' HIT rate=" : "' MISS\n");
        if (e) { uart_putdec(e->rate); uart_puts("\n"); }
    }
    if (e) {
        return war1_mix_alloc_chunk(e);
    }
    /* Long voiced narrations (campaigns/<race>/<NN>_intro.wav,
     * ending_1.wav, etc.) live in the wc1_intros SD pack, not in the
     * static catalog. Stream from raw blocks. */
    if (file && (strncmp(file, "campaigns/", 10) == 0)) {
        return war1_intro_load(file);
    }
    return nullptr;
}

/* Round-robin SFX channel picker — Stratagus passes channel=-1 to Mix_
 * PlayChannel meaning "any free". We have 4 hardware PCM channels and
 * reserve channel 3 for music streaming (see SD music section below),
 * so SFX rotates through 0..2 only. */
#define WAR1_MUSIC_CHANNEL 3
static int war1_pick_sfx_channel(void) {
    static int rr = 0;
    for (int i = 0; i < WAR1_MUSIC_CHANNEL; i++) {
        if (!audio_pcm_channel_busy(i)) return i;
    }
    int ch = rr;
    rr = (rr + 1) % WAR1_MUSIC_CHANNEL;
    return ch;
}

extern "C" int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops) {
    if (!chunk || !chunk->abuf || chunk->alen < 2) {
        static int bad = 0;
        if (bad < 4) { bad++; uart_puts("[Mix_PlayChannel] bad chunk\n"); }
        return -1;
    }
    int ch = (channel < 0) ? war1_pick_sfx_channel() : (channel & 3);
    static int play = 0;
    if (play < 8) {
        play++;
        uart_puts("[Mix_PlayChannel] ch="); uart_putdec(ch);
        uart_puts(" len="); uart_putdec(chunk->alen / 2);
        uart_puts(" rate="); uart_putdec(chunk->rate);
        uart_puts(" vol="); uart_putdec(chunk->volume); uart_puts("\n");
    }
    /* Mix's MIX_MAX_VOLUME == 128; our mixer wants 0..255. Naively
     * doubling (chunk->volume * 2) overflows the uint8_t parameter at
     * exactly volume=128 (256 → 0) → silent SFX. Saturate via int. */
    int v = chunk->volume * 2;
    if (v > 255) v = 255;
    /* DAC is set to 11025 in main.c, so for war1gus WAVs (native 11025)
     * this becomes a 1:1 native-rate playback through the resampler
     * (pos_step = 11025*65536/11025 = 65536). */
    audio_pcm_play_rate(ch,
                        (const int16_t *)chunk->abuf,
                        chunk->alen / 2,
                        (uint8_t)(v ? v : 200),
                        loops != 0,
                        chunk->rate ? chunk->rate : 11025);
    static int post = 0;
    if (post < 8) { post++; uart_puts("[Mix_PlayChannel] post-play return\n"); }
    return ch;
}
extern "C" int Mix_PlayChannelTimed(int ch, Mix_Chunk *c, int loops, int /*ticks*/) {
    return Mix_PlayChannel(ch, c, loops);
}
extern "C" int Mix_HaltChannel(int ch) {
    /* Music lives on WAR1_MUSIC_CHANNEL — Stratagus's "halt all SFX" calls
     * (Mix_HaltChannel(-1) on map exit etc.) shouldn't kill music. */
    if (ch < 0) { for (int i = 0; i < WAR1_MUSIC_CHANNEL; i++) audio_pcm_stop(i); }
    else if (ch < 4) audio_pcm_stop((uint32_t)ch);
    return 0;
}
extern "C" int Mix_Playing(int ch) {
    if (ch < 0) {
        int n = 0;
        for (int i = 0; i < 4; i++) n += audio_pcm_channel_busy(i);
        return n;
    }
    if (ch >= 4) return 0;
    return audio_pcm_channel_busy((uint32_t)ch);
}
extern "C" int Mix_VolumeChunk(Mix_Chunk *c, int v) {
    if (!c) return 0;
    Uint8 prev = c->volume;
    if (v >= 0) c->volume = (v > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : (Uint8)v;
    return prev;
}
extern "C" int Mix_Volume(int /*ch*/, int v) {
    /* Per-channel volume isn't tracked separately by our PCM mixer; the
     * volume is baked into audio_pcm_play_rate at start. Honor the API
     * by reporting the requested value back. */
    return (v < 0) ? MIX_MAX_VOLUME : (v > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : v);
}
extern "C" void Mix_FreeChunk(Mix_Chunk *) { /* slot pool — never freed */ }

/* ---- SD-streamed music (Mix_LoadMUS / Mix_PlayMusic / friends) ----
 *
 * Music lives in a single packed image dd'd onto raw SD blocks past the
 * boot partition (see scripts/sd_pack_music.py). Layout:
 *
 *   music_lba + 0..3 : pack header (4 sectors)
 *   music_lba + 4..  : track 0 PCM (sector-aligned), then track 1, ...
 *
 * The 4 GB SD has its 128 MiB FAT16 partition starting at LBA 2048 and
 * ending at LBA 264700, so we reserve LBA 524288 (the 256 MiB mark) as
 * the music pack base — well clear of FAT and any future filesystem
 * growth. Tracks are 16-bit mono PCM at the per-track sample rate
 * recorded in the header. Largest WC1 track is ~6.3 MB (5.5 minutes of
 * 11025 Hz mono); fits in one heap allocation per track.
 *
 * Strategy: load whole track into a malloc'd buffer on Mix_PlayMusic,
 * play with audio_pcm_play_rate(loop=requested) on the dedicated music
 * channel, free the buffer when the next track plays or when stopped.
 * No streaming ring buffer needed at this size, and only one music
 * track is live at any moment in WC1.
 */
#include "sdmmc.h"

#define WAR1_MUSIC_LBA      524288u
#define WAR1_MUSIC_HDR_SECT 4u
#define WAR1_MUSIC_MAGIC    0x4D314357u  /* 'WC1M' little-endian */
#define WAR1_MUSIC_MAX_TRACKS 64

struct war1_music_track {
    char     name[16];
    uint32_t lba_offset;     /* sectors past WAR1_MUSIC_LBA */
    uint32_t length_bytes;   /* PCM payload, exact */
    uint32_t sample_rate;
    uint32_t flags;          /* bit0 = mono */
};

struct war1_music_pack_hdr {
    char     magic[4];
    uint16_t version;
    uint16_t num_tracks;
    uint32_t hdr_sectors;
    uint32_t reserved;
    war1_music_track tracks[WAR1_MUSIC_MAX_TRACKS];
};

static war1_music_pack_hdr g_music_pack;
static int                 g_music_pack_loaded = 0;
static int                 g_music_pack_failed = 0;
/* Mix_Music opaque pointer slot pool. The handle just carries a track
 * index; the actual buffer is owned by the active-playback state. */
struct war1_music_handle { int32_t track_idx; };
static war1_music_handle g_music_handles[WAR1_MUSIC_MAX_TRACKS];
/* Active playback state. */
static int16_t  *g_music_buf = nullptr;     /* malloc'd, freed on next play */
static uint32_t  g_music_buf_samples = 0;
static int32_t   g_music_active_track = -1;
static int       g_music_loops = 0;          /* honored at start-of-play */
static int       g_music_volume = MIX_MAX_VOLUME;
static void    (*g_music_finished_cb)(void) = nullptr;
/* For pumping: did the music channel go idle? */
static int       g_music_was_busy = 0;

extern void *malloc(size_t);
extern void free(void *);

static int war1_music_load_pack(void) {
    if (g_music_pack_loaded) return 0;
    if (g_music_pack_failed) return -1;
    static uint8_t hdr_buf[WAR1_MUSIC_HDR_SECT * 512] __attribute__((aligned(4)));
    if (!sdmmc_card()->initialised) {
        if (sdmmc_init() != 0) {
            uart_puts("[music] sdmmc_init failed\n");
            g_music_pack_failed = 1;
            return -1;
        }
    }
    if (sdmmc_read_blocks(WAR1_MUSIC_LBA, WAR1_MUSIC_HDR_SECT, hdr_buf) != 0) {
        uart_puts("[music] pack header read failed\n");
        g_music_pack_failed = 1;
        return -1;
    }
    uint32_t magic = ((uint32_t)hdr_buf[0])
                   | ((uint32_t)hdr_buf[1] << 8)
                   | ((uint32_t)hdr_buf[2] << 16)
                   | ((uint32_t)hdr_buf[3] << 24);
    if (magic != WAR1_MUSIC_MAGIC) {
        uart_puts("[music] pack magic mismatch — no pack at LBA ");
        uart_putdec(WAR1_MUSIC_LBA);
        uart_puts("\n");
        g_music_pack_failed = 1;
        return -1;
    }
    /* Copy header into typed struct. */
    memcpy(&g_music_pack, hdr_buf, sizeof(g_music_pack));
    if (g_music_pack.num_tracks == 0 || g_music_pack.num_tracks > WAR1_MUSIC_MAX_TRACKS) {
        uart_puts("[music] bad num_tracks\n");
        g_music_pack_failed = 1;
        return -1;
    }
    uart_puts("[music] pack loaded: ");
    uart_putdec(g_music_pack.num_tracks);
    uart_puts(" tracks\n");
    g_music_pack_loaded = 1;
    return 0;
}

/* Stratagus passes paths like "music/00.mid"; our pack stores names like
 * "music/00" (without extension). Match by stripping any extension from
 * the request before comparing. */
static int war1_music_lookup(const char *requested) {
    if (war1_music_load_pack() != 0) return -1;
    char wanted[16];
    int n = 0;
    while (requested[n] && requested[n] != '.' && n < 15) {
        wanted[n] = requested[n];
        n++;
    }
    wanted[n] = 0;
    for (int i = 0; i < g_music_pack.num_tracks; i++) {
        if (strncmp(g_music_pack.tracks[i].name, wanted, 16) == 0) return i;
    }
    return -1;
}

static void war1_music_free_buf(void) {
    if (g_music_buf) {
        free(g_music_buf);
        g_music_buf = nullptr;
        g_music_buf_samples = 0;
    }
}

/* Mix_Music handle that wraps a sound-catalog entry (for paths like
 * "sounds/logo.wav" that ShowTitleScreens passes to PlayMusic). The
 * track_idx field uses the sentinel WAV_HANDLE_TAG so Mix_PlayMusic can
 * dispatch to the embedded-PCM-on-music-channel path. */
#define WAR1_MUSIC_WAV_TAG (-2)
static war1_music_handle g_music_wav_handle;
static const war1_sound_entry_t *g_music_wav_entry = nullptr;

extern "C" Mix_Music *Mix_LoadMUS(const char *file) {
    if (!file) return nullptr;
    int idx = war1_music_lookup(file);
    if (idx < 0) {
        /* Not in the music pack — fall back to the SFX sound catalog so
         * the boot splash (sounds/logo.wav) and any other PlayMusic-of-a-
         * wav path actually play. */
        const war1_sound_entry_t *e = war1_sound_lookup(file);
        if (e) {
            g_music_wav_entry = e;
            g_music_wav_handle.track_idx = WAR1_MUSIC_WAV_TAG;
            return (Mix_Music *)&g_music_wav_handle;
        }
        static int probe = 0;
        if (probe < 8) {
            probe++;
            uart_puts("[Mix_LoadMUS] miss: '");
            uart_puts(file);
            uart_puts("'\n");
        }
        return nullptr;
    }
    g_music_handles[idx].track_idx = idx;
    return (Mix_Music *)&g_music_handles[idx];
}

extern "C" Mix_Music *Mix_LoadMUS_RW(SDL_RWops *, int) { return nullptr; }
extern "C" void Mix_FreeMusic(Mix_Music *) { /* handles live in static pool */ }

extern "C" int Mix_PlayMusic(Mix_Music *m, int loops) {
    war1_music_handle *h = (war1_music_handle *)m;
    if (!h) return -1;
    /* WAV-on-music-channel path (boot splash logo.wav, etc.). The
     * sound-catalog entry's PCM is already in CODE memory (objcopy-
     * embedded), so just hand it to the music channel directly — no
     * SD read, no malloc. */
    if (h->track_idx == WAR1_MUSIC_WAV_TAG && g_music_wav_entry) {
        const war1_sound_entry_t *e = g_music_wav_entry;
        audio_pcm_stop(WAR1_MUSIC_CHANNEL);
        war1_music_free_buf();
        uart_puts("[Mix_PlayMusic] wav '");
        uart_puts("' rate="); uart_putdec(e->rate);
        uart_puts(" bytes="); uart_putdec((uint32_t)(e->data_end - e->data));
        uart_puts("\n");
        audio_pcm_play_rate(WAR1_MUSIC_CHANNEL,
                            (const int16_t *)e->data,
                            (uint32_t)((e->data_end - e->data) / 2),
                            (uint8_t)g_music_volume,
                            loops != 0,
                            e->rate);
        g_music_active_track = WAR1_MUSIC_WAV_TAG;
        g_music_was_busy = 1;
        return 0;
    }
    if (h->track_idx < 0 || h->track_idx >= g_music_pack.num_tracks) return -1;
    war1_music_track *t = &g_music_pack.tracks[h->track_idx];

    /* Stop previous track + free its buffer. */
    audio_pcm_stop(WAR1_MUSIC_CHANNEL);
    war1_music_free_buf();

    uint32_t pcm_bytes = t->length_bytes;
    /* sdmmc_read_blocks reads whole sectors — buffer must be a sector
     * multiple OR the tail of the last sector overflows past the malloc'd
     * region into the next dlmalloc chunk's metadata. Round buf_bytes up
     * to 512 to match the actual read size. (The previous +3 round was
     * the source of a heap corruption that surfaced as a dlmalloc abort
     * during the next malloc — typically Lua's CclLoad → fs::path
     * splitting in widgets.lua reload during briefing's LoadUI.) */
    uint32_t buf_bytes = (pcm_bytes + 511u) & ~511u;
    g_music_buf = (int16_t *)malloc(buf_bytes);
    if (!g_music_buf) {
        uart_puts("[Mix_PlayMusic] malloc fail (");
        uart_putdec(buf_bytes);
        uart_puts(" bytes)\n");
        return -1;
    }
    /* Read whole track payload from SD. lba_offset is in sectors past
     * WAR1_MUSIC_LBA. Round read up to a sector multiple. */
    uint32_t sectors = (pcm_bytes + 511u) / 512u;
    uint32_t start_lba = WAR1_MUSIC_LBA + t->lba_offset;
    uart_puts("[Mix_PlayMusic] track ");
    uart_putdec((uint32_t)h->track_idx);
    uart_puts(" lba=");
    uart_putdec(start_lba);
    uart_puts(" sectors=");
    uart_putdec(sectors);
    uart_puts("\n");
    if (sdmmc_read_blocks(start_lba, sectors, g_music_buf) != 0) {
        uart_puts("[Mix_PlayMusic] SD read failed\n");
        war1_music_free_buf();
        return -1;
    }
    g_music_buf_samples = pcm_bytes / 2;   /* 16-bit samples */
    g_music_active_track = h->track_idx;
    g_music_loops = loops;
    /* Stratagus uses loops=-1 for "loop forever". Our audio mixer's loop
     * flag is 0/1 (off/on). */
    int loop_flag = (loops != 0) ? 1 : 0;
    int v = (g_music_volume * 2);
    if (v > 255) v = 255;
    audio_pcm_play_rate(WAR1_MUSIC_CHANNEL,
                        g_music_buf, g_music_buf_samples,
                        (uint8_t)v, (uint8_t)loop_flag,
                        t->sample_rate ? t->sample_rate : 11025);
    g_music_was_busy = 1;
    return 0;
}

extern "C" int Mix_PlayingMusic(void) {
    return audio_pcm_channel_busy(WAR1_MUSIC_CHANNEL) ? 1 : 0;
}

extern "C" int Mix_HaltMusic(void) {
    audio_pcm_stop(WAR1_MUSIC_CHANNEL);
    war1_music_free_buf();
    g_music_active_track = -1;
    g_music_was_busy = 0;
    return 0;
}

extern "C" int Mix_PauseMusic(void)  { audio_pcm_stop(WAR1_MUSIC_CHANNEL); return 0; }
extern "C" int Mix_ResumeMusic(void) { return 0;  /* nothing to resume — pause is destructive here */ }

extern "C" int Mix_VolumeMusic(int v) {
    int prev = g_music_volume;
    if (v >= 0) {
        g_music_volume = (v > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : v;
        /* Volume change mid-track: our mixer bakes volume in at start.
         * Re-issue audio_pcm_play_rate at current sample position would
         * require pos accessor — skipping; volume takes effect on next
         * Mix_PlayMusic. */
    }
    return prev;
}

extern "C" int Mix_FadeOutMusic(int /*ms*/) {
    /* Real fade-out would need a per-sample volume ramp; cheaper: just halt. */
    return Mix_HaltMusic();
}

extern "C" void Mix_HookMusicFinished(void (*cb)(void)) {
    g_music_finished_cb = cb;
}

/* ---- FatFs-streamed campaign intros (Mix_LoadWAV fallback) ----
 *
 * Long voiced narrations live on the 3.5 GB FAT partition under
 * /intros/<stratagus-path>.pcm — the user drops the contents of
 * examples/war1/fatfs_payload/intros/ onto the partition. Each file
 * carries a 16-byte WC1P header (magic+version+rate+length); the rest
 * is raw signed-16 mono PCM at the recorded rate.
 *
 * One intro buffer is live at a time (briefings play sequentially);
 * the chunk slot + malloc'd buffer recycle on the next load. */
struct war1_intro_pcm_hdr {
    char     magic[4];      /* 'WC1P' */
    uint32_t version;       /* 1 */
    uint32_t sample_rate;
    uint32_t length_bytes;
};

static Mix_Chunk g_intro_chunk;
static int16_t  *g_intro_buf = nullptr;

extern "C" Mix_Chunk *war1_intro_load(const char *file) {
    if (!file) return nullptr;
    /* Translate "campaigns/<race>/<NN>_intro.wav" → "/intros/campaigns/
     * <race>/<NN>_intro.pcm". The leading "/" routes through the FatFs
     * branch in war1_link_stubs._open. */
    char fpath[160];
    int n = snprintf(fpath, sizeof(fpath), "/intros/%s", file);
    if (n <= 0 || n >= (int)sizeof(fpath)) return nullptr;
    /* Swap trailing ".wav" → ".pcm" — ".wav" is 4 chars, ".pcm" is 4. */
    if (n >= 4 && strcmp(&fpath[n - 4], ".wav") == 0) {
        memcpy(&fpath[n - 4], ".pcm", 4);
    }
    FILE *f = fopen(fpath, "rb");
    if (!f) {
        uart_puts("[intros] open miss '"); uart_puts(fpath); uart_puts("'\n");
        return nullptr;
    }
    war1_intro_pcm_hdr hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        memcmp(hdr.magic, "WC1P", 4) != 0) {
        uart_puts("[intros] bad header '"); uart_puts(fpath); uart_puts("'\n");
        fclose(f);
        return nullptr;
    }
    if (g_intro_buf) { free(g_intro_buf); g_intro_buf = nullptr; }
    g_intro_buf = (int16_t *)malloc(hdr.length_bytes);
    if (!g_intro_buf) {
        uart_puts("[intros] malloc failed\n");
        fclose(f);
        return nullptr;
    }
    size_t got = fread(g_intro_buf, 1, hdr.length_bytes, f);
    fclose(f);
    if (got != hdr.length_bytes) {
        uart_puts("[intros] short read '"); uart_puts(fpath); uart_puts("'\n");
        free(g_intro_buf); g_intro_buf = nullptr;
        return nullptr;
    }
    g_intro_chunk.allocated = 0;
    g_intro_chunk.abuf      = (Uint8 *)g_intro_buf;
    g_intro_chunk.alen      = hdr.length_bytes;
    g_intro_chunk.volume    = MIX_MAX_VOLUME;
    g_intro_chunk.rate      = hdr.sample_rate;
    uart_puts("[intros] loaded '"); uart_puts(fpath);
    uart_puts("' bytes="); uart_putdec(hdr.length_bytes);
    uart_puts(" rate="); uart_putdec(hdr.sample_rate); uart_puts("\n");
    return &g_intro_chunk;
}

/* Called every frame from WaitEventsOneFrame. Detects natural end of a
 * non-looping track and fires the registered callback so Stratagus can
 * advance to the next track. */
extern "C" void Jupiter_CheckMusicFinishedNow();
/* Set when the music pump detects natural end-of-track. Drained at the
 * top of WaitEventsOneFrame's NEXT frame (war1_music_pump_drain), NOT
 * synchronously inside the pump itself. Firing the Lua MusicStopped
 * callback from the pump re-enters Lua mid-frame, which raced with menu
 * setup if the user skipped the splash and the splash WAV finished mid-
 * menu-construction (manifested as a hang on the next click). */
static volatile int g_music_finish_pending = 0;

extern "C" void war1_music_pump(void) {
    /* -1 = no active track; any other value (real index >= 0 OR
     * WAR1_MUSIC_WAV_TAG = -2 for the embedded-wav-on-music path) means
     * the music channel was started and we should detect natural end. */
    if (g_music_active_track == -1) return;
    int busy = audio_pcm_channel_busy(WAR1_MUSIC_CHANNEL);
    if (g_music_was_busy && !busy) {
        g_music_was_busy = 0;
        int32_t finished_track = g_music_active_track;
        g_music_active_track = -1;
        war1_music_free_buf();
        uart_puts("[music] track ");
        uart_putdec((uint32_t)finished_track);
        uart_puts(" finished (deferred fire)\n");
        g_music_finish_pending = 1;
    }
}

/* Called once per frame from a safe place (top of WaitEventsOneFrame,
 * after frame state is settled). Fires the Lua music-finished callback
 * here so it runs in a clean call frame, not nested inside the pump. */
extern "C" void war1_music_pump_drain(void) {
    if (!g_music_finish_pending) return;
    g_music_finish_pending = 0;
    Jupiter_CheckMusicFinishedNow();
    if (g_music_finished_cb) g_music_finished_cb();
}

/* Defined in war1_link_stubs.cpp. Current back buffer — the address all
 * pc8 blits + CVideo::FillRectangleClip + TheScreen->pixels target. Flips
 * between OVL_ADDR/OVL1_ADDR each frame in Invalidate(). */
extern "C" volatile uint32_t g_war1_back_fb;

/* ---- CGraphic ↔ pc8 bridge ------------------------------------
 * Per-CGraphic registry. When CGraphic::New(path,w,h) is called,
 * war1_link_stubs.cpp looks up `path` in the asset catalog and — on a hit —
 * calls war1_register_pc8_from_blob to attach the blob to the new CGraphic.
 * Later, DrawFrameClip(frame, x, y) routes through war1_blit_via_pc8 which
 * uses the CGraphic's Width/Height as frame size for slicing. */
struct PC8Entry {
    const CGraphic *g;
    pc8_t pc;
    /* Lazily-malloc'd writable palette copy. pc8_t::palette starts as
     * a const pointer into the (flash-resident) blob; on the rare
     * SetPaletteColor call (briefing recolors palette[255] for the
     * shadow dither) we malloc 1KB, copy the blob palette, and re-point
     * pc.palette here. Avoids burning 2MB of bss for the 2048×256-entry
     * full-table case when nearly all graphics are read-only. */
    uint32_t *palette_writable;
    /* Bumped on every SetPaletteColor. JupiterImages baked from this pc8
     * remember the epoch at bake time and invalidate when it changes. */
    uint32_t  palette_epoch;
};
/* Upper bound: 212 catalog entries × ~8 re-registrations ≈ 1700. Use
 * 2048 for headroom. Live in .bss_low (reclaimed 0x40800000 DDR) so it
 * doesn't compete with the main CODE region's bss budget. */
static PC8Entry g_pc8_registry[2048] __attribute__((section(".bss_low")));
static int      g_pc8_count = 0;

extern "C" int war1_register_pc8_from_blob(const CGraphic *g, const uint8_t *blob) {
    if (g_pc8_count >= (int)(sizeof(g_pc8_registry) / sizeof(g_pc8_registry[0]))) return -1;
    pc8_t pc;
    if (pc8_open(&pc, blob) != 0) return -1;
    PC8Entry &e = g_pc8_registry[g_pc8_count++];
    e.g  = g;
    e.pc = pc;
    e.palette_writable = nullptr;
    e.palette_epoch = 1;  /* never zero, so cached_epoch==0 always invalidates */
    /* CIcon::Load (icons.cpp:110) clamps `Frame >= G->NumFrames` back to 0
     * — without this, EVERY icon renders frame 0 (footman portrait) because
     * upstream's CGraphic::Load (which we stub as no-op) is what normally
     * populates NumFrames from mSurface->w/h. We have the real sheet dims
     * here from the pc8 header; compute NumFrames from frame size + sheet
     * size and write through the const so CIcon's clamp lets the actual
     * frame index pass. */
    CGraphic *gw = const_cast<CGraphic *>(g);
    int fw = gw->Width  > 0 ? gw->Width  : (int)pc.image_w;
    int fh = gw->Height > 0 ? gw->Height : (int)pc.image_h;
    int cols = pc.image_w / fw;
    int rows = pc.image_h / fh;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    gw->NumFrames = cols * rows;
    return 0;
}

/* Resolve a path to a pc8 blob, register it with the given CGraphic.
 * Called from CGraphic::New in war1_link_stubs.cpp. */
extern "C" int war1_register_graphic_by_path(const CGraphic *g, const char *path) {
    const war1_asset_entry_t *e = war1_asset_lookup(path);
    if (!e) return -1;
    return war1_register_pc8_from_blob(g, e->data);
}

extern "C" void war1_blit_via_pc8(const CGraphic *g, unsigned frame, int x, int y) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            int fw = g->Width  ? g->Width  : 16;
            int fh = g->Height ? g->Height : 16;
            bool is_tile = (g == Map.TileGraphic.get());
            int skip_zero = is_tile ? 0 : 1;
            pc8_blit_frame(&g_pc8_registry[i].pc, fw, fh, frame, x, y, g_war1_back_fb, skip_zero);
            return;
        }
    }
}

/* Aspect-fit scale a pc8 sheet into a (dstW × dstH) target rect at
 * (dstX, dstY). Same treatment as the ImageWidget bg path in arg_to_image
 * — vertical fill, horizontal pillarbox. Used by CGraphic::DrawClip when
 * the script Resize'd the graphic past its native sheet (TitleScreen
 * splash, scripted backdrops). Returns 0 (caller falls through) when the
 * target rect matches native. */
extern "C" int war1_blit_via_pc8_scaled(const CGraphic *g, int dstX, int dstY, int dstW, int dstH) {
    if (dstW <= 0 || dstH <= 0) return 0;
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g != g) continue;
        const pc8_t *pc = &g_pc8_registry[i].pc;
        const int sheetW = (int)pc->image_w;
        const int sheetH = (int)pc->image_h;
        if (sheetW <= 0 || sheetH <= 0) return 0;
        if (dstW == sheetW && dstH == sheetH) return 0;
        const int targetH = dstH;
        const int targetW = (sheetW * dstH + sheetH / 2) / sheetH;
        const int finalW  = targetW < dstW ? targetW : dstW;
        const int finalH  = targetH;
        const int padX    = (dstW - finalW) / 2;
        const int x0      = dstX + padX;
        const int y0      = dstY;
        volatile uint32_t *fb = (volatile uint32_t *)g_war1_back_fb;
        for (int y = 0; y < finalH; y++) {
            const int dy = y0 + y;
            if (dy < 0 || dy >= (int)LCD_H) continue;
            const int srcY = y * sheetH / finalH;
            const uint8_t *s = pc->pixels + srcY * sheetW;
            volatile uint32_t *row = fb + dy * LCD_W + x0;
            for (int x = 0; x < finalW; x++) {
                const int dx = x0 + x;
                if (dx < 0 || dx >= (int)LCD_W) continue;
                const int srcX = x * sheetW / finalW;
                row[x] = pc->palette[s[srcX]];
            }
        }
        return 1;
    }
    return 0;
}

/* Sample a single pixel from a registered CGraphic's pc8 sheet, returning
 * the resolved ARGB color (palette[index]). Out-of-range coords or
 * unregistered g -> 0 (transparent). Used by minimap.cpp's UpdateTerrain
 * to build the terrain mini-bitmap pixel-by-pixel from the tileset, since
 * the SDL_Surface path that upstream uses doesn't apply to our pc8 store. */
extern "C" uint32_t war1_pc8_sample_argb(const CGraphic *g, int gx, int gy) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            const pc8_t *pc = &g_pc8_registry[i].pc;
            if (gx < 0 || gy < 0 || gx >= (int)pc->image_w || gy >= (int)pc->image_h) return 0;
            uint8_t idx = pc->pixels[(uint32_t)gy * pc->image_w + (uint32_t)gx];
            return pc->palette[idx];
        }
    }
    return 0;
}


/* Horizontally-flipped frame blit. Stratagus calls DrawFrameClipX /
 * DrawPlayerColorFrameClipX for west-facing unit animations (the sprite
 * sheets only carry east-facing frames; west, NW, SW are the same frames
 * mirrored). Routed via the same pc8 registry as the normal blit. */
extern "C" void war1_blit_via_pc8_flipx(const CGraphic *g, unsigned frame, int x, int y) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            int fw = g->Width  ? g->Width  : 16;
            int fh = g->Height ? g->Height : 16;
            bool is_tile = (g == Map.TileGraphic.get());
            int skip_zero = is_tile ? 0 : 1;
            pc8_blit_frame_flipx(&g_pc8_registry[i].pc, fw, fh, frame, x, y,
                                 g_war1_back_fb, skip_zero);
            return;
        }
    }
}

/* Sub-rectangle blit path. Stratagus's HUD fillers, button panel, and icon
 * renderer use DrawSubClip(gx, gy, w, h, x, y) — pull a (gx,gy,w,h) region
 * from the graphic sheet and paint it at screen (x,y). The pc8 sheet is
 * just a large sprite atlas, so sub-rect blit is a direct read of those
 * pixels with skip-on-transparent (HUD sheets have transparent padding). */
extern "C" void war1_blit_subrect_via_pc8(const CGraphic *g,
                                          int gx, int gy, int w, int h,
                                          int x, int y) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            /* If caller passed 0/0 dims, fall back to the pc8's own
             * image dims so DrawSubClip(0,0,0,0,X,Y) means "blit the
             * whole sheet at (X,Y)". This rescues filler panels whose
             * CGraphic was created via Lua's CGraphic:New(file) without
             * w/h args. */
            int rw = w > 0 ? w : (int)g_pc8_registry[i].pc.image_w;
            int rh = h > 0 ? h : (int)g_pc8_registry[i].pc.image_h;
            pc8_blit_subrect(&g_pc8_registry[i].pc, gx, gy, rw, rh, x, y,
                             g_war1_back_fb, 1);
            return;
        }
    }
    /* No pc8 match — one-shot diag so we know this HUD CGraphic was
     * never registered (asset path not in the catalog or created via a
     * path DrawFrameClip doesn't share identity with). */
    static int miss_probe = 0;
    if (!miss_probe) {
        miss_probe = 1;
        uart_puts("[HUD] sub MISS file='");
        uart_puts(g->File.c_str());
        uart_puts("' registry_size=");
        uart_putdec((unsigned)g_pc8_count);
        uart_puts("\n");
    }
}

/* JupiterGraphics::drawImage fallback for non-JupiterImage gcn::Image*
 * arguments — specifically, the CGraphic that war1gus's guichan.lua
 * passes to ImageWidget(...). The image void* IS the CGraphic*.
 * Routes to the existing pc8 sub-rect blit so the in-game menu's panel
 * background and any other Lua-loaded CGraphic ImageWidget actually
 * render instead of silently dropping. */
extern "C" int war1_drawimage_via_pc8(const void *image,
                                      int srcX, int srcY,
                                      int dstX, int dstY,
                                      int width, int height) {
    if (!image) return 0;
    const CGraphic *g = static_cast<const CGraphic *>(image);
    war1_blit_subrect_via_pc8(g, srcX, srcY, width, height, dstX, dstY);
    return 1;
}

/* Direct accessors used by the widget bindings to wrap a CGraphic's pc8
 * in a JupiterImage on the fly (see arg_to_image in war1_widget_bindings.cpp).
 * Returns the indexed pixel array + palette + dims for a registered CGraphic;
 * returns nullptr if the pointer isn't in the registry (e.g. asset path
 * wasn't in the catalog so war1_register_graphic_by_path returned -1). */
extern "C" const uint8_t *war1_pc8_pixels_for(const CGraphic *g, int *outW, int *outH) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            if (outW) *outW = g_pc8_registry[i].pc.image_w;
            if (outH) *outH = g_pc8_registry[i].pc.image_h;
            return g_pc8_registry[i].pc.pixels;
        }
    }
    if (outW) *outW = 0;
    if (outH) *outH = 0;
    return nullptr;
}
extern "C" const uint32_t *war1_pc8_palette_for(const CGraphic *g) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) return g_pc8_registry[i].pc.palette;
    }
    return nullptr;
}

/* Mutate a registered pc8's palette entry. Stratagus's CGraphic::
 * SetPaletteColor(idx, r, g, b) is called from the briefing Lua to recolor
 * specific palette indices (idx 255 is the dithered shadow tone — without
 * this, those pixels stay at the source PNG's palette[255] which leaves
 * scattered bright dots peppered through the briefing-room stone wall). */
extern "C" int war1_pc8_set_palette_argb(const CGraphic *g, int idx, uint32_t argb) {
    if ((unsigned)idx >= 256) return -1;
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g != g) continue;
        PC8Entry &e = g_pc8_registry[i];
        if (!e.palette_writable) {
            e.palette_writable = (uint32_t *)malloc(256 * sizeof(uint32_t));
            if (!e.palette_writable) return -1;
            for (int k = 0; k < 256; k++) e.palette_writable[k] = e.pc.palette[k];
            e.pc.palette = e.palette_writable;
        }
        e.palette_writable[idx] = argb;
        e.palette_epoch++;
        return 0;
    }
    return -1;
}

/* Public read: current palette epoch for a CGraphic. JupiterImages cache
 * this value at bake time; drawImage compares and re-bakes if stale.
 * Typed as void* so JupiterGraphics doesn't need a CGraphic include. */
extern "C" uint32_t war1_pc8_palette_epoch(const void *gv) {
    const CGraphic *g = (const CGraphic *)gv;
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) return g_pc8_registry[i].palette_epoch;
    }
    return 0;
}

/* Re-bake a JupiterImage's ARGB cache from the live pc8 palette. Keeps
 * the same width/height layout the cache was built with (set by
 * arg_to_image: native pc8 size or letterboxed-to-Resize-target). */
extern "C" int war1_pc8_rebake_argb(const void *gv, uint32_t *dst,
                                    int outW, int outH) {
    const CGraphic *g = (const CGraphic *)gv;
    if (!dst || outW <= 0 || outH <= 0) return -1;
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g != g) continue;
        const pc8_t *pc = &g_pc8_registry[i].pc;
        const int sheetW = (int)pc->image_w;
        const int sheetH = (int)pc->image_h;
        if (sheetW <= 0 || sheetH <= 0) return -1;
        if (outW == sheetW && outH == sheetH) {
            for (int k = 0, n = sheetW * sheetH; k < n; k++) dst[k] = pc->palette[pc->pixels[k]];
        } else {
            /* Stretch-to-fill, matching arg_to_image's bake math. */
            for (int y = 0; y < outH; y++) {
                const int srcY = y * sheetH / outH;
                const uint8_t *s = pc->pixels + srcY * sheetW;
                uint32_t *row = dst + y * outW;
                for (int x = 0; x < outW; x++) {
                    const int srcX = x * sheetW / outW;
                    row[x] = pc->palette[s[srcX]];
                }
            }
        }
        return g_pc8_registry[i].palette_epoch;
    }
    return -1;
}

/* Look up the source-sheet width (image_w) for a CGraphic's pc8. Used
 * by CFont::drawString to compute glyphs-per-row from sheet_w / G->Width. */
extern "C" int war1_pc8_image_w_for(const CGraphic *g) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) return (int)g_pc8_registry[i].pc.image_w;
    }
    return 0;
}
extern "C" int war1_pc8_image_h_for(const CGraphic *g) {
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) return (int)g_pc8_registry[i].pc.image_h;
    }
    return 0;
}

/* Glyph blit for bitmap-font text rendering. Treats the source pc8 as a
 * 1-bit mask: any source pixel whose palette entry has alpha > 0 paints
 * the supplied fg_argb at that destination pixel; transparent source
 * pixels are skipped. This is what CFont::drawString uses to render
 * each glyph cell — gx,gy,w,h is the glyph slot in the font sheet. */
extern "C" void war1_blit_glyph_via_pc8(const CGraphic *g,
                                        int gx, int gy, int w, int h,
                                        int x, int y, uint32_t fg_argb) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < g_pc8_count; i++) {
        if (g_pc8_registry[i].g == g) {
            const pc8_t *pc = &g_pc8_registry[i].pc;
            if (gx + w > (int)pc->image_w) w = (int)pc->image_w - gx;
            if (gy + h > (int)pc->image_h) h = (int)pc->image_h - gy;
            if (w <= 0 || h <= 0) return;
            uint32_t *fb = (uint32_t *)g_war1_back_fb;
            for (int yy = 0; yy < h; yy++) {
                int dy = y + yy;
                if (dy < 0 || dy >= (int)LCD_H) continue;
                uint32_t *row = fb + dy * LCD_W;
                const uint8_t *src = pc->pixels + (uint32_t)(gy + yy) * pc->image_w + gx;
                for (int xx = 0; xx < w; xx++) {
                    int dx = x + xx;
                    if (dx < 0 || dx >= (int)LCD_W) continue;
                    uint8_t idx = src[xx];
                    uint32_t srcArgb = pc->palette[idx];
                    if ((srcArgb >> 24) == 0) continue;  /* transparent */
                    row[dx] = fg_argb;
                }
            }
            return;
        }
    }
}

/* Legacy test-harness entry points — unused by new level-selector main.c
 * but kept so any stray caller still compiles. */
extern "C" void war1_register_pc8(const CGraphic *g, const uint8_t *blob) {
    war1_register_pc8_from_blob(g, blob);
}
extern "C" CGraphic *war1_graphic_from_pc8(const uint8_t *blob, int frame_w, int frame_h) {
    CGraphic *g = new CGraphic();
    g->Width = frame_w;
    g->Height = frame_h;
    war1_register_pc8_from_blob(g, blob);
    return g;
}

extern "C" CPlayerColorGraphic *war1_playercolor_graphic_from_pc8(const uint8_t *blob, int frame_w, int frame_h) {
    CPlayerColorGraphic *g = new CPlayerColorGraphic();
    g->Width = frame_w;
    g->Height = frame_h;
    war1_register_pc8_from_blob(g, blob);
    return g;
}

/* ---- FatFs-streamed cinematics (Movie::Load / war1_movie_play) ----
 *
 * Same pattern the briefing intros use (see war1_intro_load above): the
 * user copies build/wc1_vbins/*.vbin onto the FatFs partition under
 * /videos/, the C side fopens the file by name and fread's it into a
 * malloc'd buffer for the cedar playback loop.
 *
 * Stratagus's Lua passes paths like "videos/hmap01.ogv"; we map those
 * to "/videos/hmap01.vbin" — the leading "/" routes through the FatFs
 * branch in war1_link_stubs._open exactly like the intro PCMs do. */
#include "jupiter.h"   /* cedar_*, dcache_*, video_*, ticks_to_us, timer_* */
extern "C" {
#include "input.h"
}
#include <stdio.h>

/* vbin v3 header — must match scripts/wc1_video_pack.py (and the layout
 * examples/cedar_video_av/main.c uses verbatim). */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t version;
    uint16_t reserved1;
    uint16_t width;
    uint16_t height;
    uint16_t fps_num;
    uint16_t fps_den;
    uint32_t qp;
    uint32_t num_frames;
    uint32_t video_size;
    uint32_t audio_offset;
    uint32_t audio_size;
    uint16_t audio_rate;
    uint8_t  audio_channels;
    uint8_t  audio_bps;
    uint8_t  reserved2[8];
} war1_vbin_hdr_t;

/* Cedar working buffers — must match BUF_LUMA/BUF_CHROMA in lib/cedar.c.
 * Moved to the unused 0x43E00000-0x43F00000 VRAM gap so cedar's per-
 * frame writes don't corrupt war1's bss. NAL_STAGE stays at 0x43F00000
 * (the 1 MB above the cedar buffers, into the lower edge of which we
 * also stage the start-code prefix). */
#define WAR1_LUMA_ADDR    0x43E98000u
#define WAR1_CHROMA_ADDR  0x43ED8000u
#define WAR1_NAL_STAGE    0x43F00000u

extern uint32_t current_audio_rate;
extern "C" int16_t  mix_buf[];
extern "C" volatile uint32_t mix_wr;

static int war1_movie_play_blob(const uint8_t *blob, uint32_t blob_size) {
    if (!blob || blob_size < sizeof(war1_vbin_hdr_t)) return -1;
    const war1_vbin_hdr_t *hdr = (const war1_vbin_hdr_t *)blob;
    if (memcmp(hdr->magic, "VBIN", 4) != 0 || hdr->version != 3) {
        uart_puts("[movie] bad vbin magic/version\n");
        return -1;
    }
    uart_puts("[movie] vbin "); uart_putdec(hdr->width);
    uart_puts("x"); uart_putdec(hdr->height);
    uart_puts(" frames="); uart_putdec(hdr->num_frames);
    uart_puts(" audio="); uart_putdec(hdr->audio_rate); uart_puts("Hz\n");

    /* Cedar one-time init. */
    static int s_cedar_inited = 0;
    if (!s_cedar_inited) { cedar_init(); s_cedar_inited = 1; }

    /* Take over the music channel (same one Stratagus's PlayMusic uses)
     * — the music pack track that may have been streaming up to now is
     * stopped and its buffer freed; movie audio uses the same slot. */
    audio_pcm_stop(WAR1_MUSIC_CHANNEL);
    war1_music_free_buf();

    /* Don't re-rate the codec mid-stream. audio_set_rate stops the DMA
     * (lib/audio.c:1124) and the multi-stop/start cycle between back-
     * to-back cinematics drove an audible loop on the second movie.
     * The PCM mixer's resampler in audio_pcm_play_rate handles src!=
     * codec_rate via pos_step = (src_rate << 24) / current_q8, so a
     * 22050 Hz cinematic plays through the codec's existing 11025 Hz
     * output with a 2:1 step (aliased but intelligible — same trade-
     * off the briefing voice WAVs already use). */

    /* Index frame offsets + per-frame hdr_bits up front. Bounded loop
     * over num_frames. */
    const uint8_t *video_start = blob + sizeof(war1_vbin_hdr_t);
    uint32_t *frame_offsets  = (uint32_t *)malloc(hdr->num_frames * sizeof(uint32_t));
    uint16_t *frame_hdr_bits = (uint16_t *)malloc(hdr->num_frames * sizeof(uint16_t));
    if (!frame_offsets || !frame_hdr_bits) {
        if (frame_offsets)  free(frame_offsets);
        if (frame_hdr_bits) free(frame_hdr_bits);
        return -1;
    }
    {
        const uint8_t *q = video_start;
        for (uint32_t f = 0; f < hdr->num_frames; f++) {
            frame_offsets[f] = (uint32_t)(q - video_start);
            uint32_t nal_size; memcpy(&nal_size, q, 4);
            uint16_t hb;       memcpy(&hb, q + 4, 2);
            frame_hdr_bits[f] = hb;
            q += 8 + nal_size;
        }
    }

    /* Point the mixer directly at the vbin's audio region — no extra
     * copy. With movies in the 8–10 MB range the previous malloc+memcpy
     * for audio (~944 KB) was running the heap out on the second
     * cinematic and crashing on a NULL deref. The blob stays alive
     * (war1_movie_play frees it AFTER war1_movie_play_blob returns),
     * and the mixer reads pos_int up to length once and stops. */
    const uint8_t *audio_src = blob + sizeof(war1_vbin_hdr_t) + hdr->audio_offset;
    const int16_t *audio_buf = (const int16_t *)audio_src;
    uint32_t audio_samples = hdr->audio_size / 2 / (hdr->audio_channels ? hdr->audio_channels : 1);
    uint64_t audio_us = (uint64_t)audio_samples * 1000000u / hdr->audio_rate;

    /* Render target — center the movie inside the LCD on a black bg. */
    int dst_x = ((int)LCD_W - (int)hdr->width)  / 2; if (dst_x < 0) dst_x = 0;
    int dst_y = ((int)LCD_H - (int)hdr->height) / 2; if (dst_y < 0) dst_y = 0;

    /* Single-buffered render: write directly into g_war1_back_fb. After
     * the movie ends, war1's normal Invalidate flip resumes from this
     * buffer (the briefing menu repaints on exit so a single stale
     * frame is fine). */
    uint32_t target_fb = g_war1_back_fb;
    memset((void *)target_fb, 0, LCD_W * LCD_H * 4);

    /* Kick playback. With audio_pcm_play_rate's resampler running at
     * src_rate == codec rate, pos_step is exactly 1.0 in Q16. */
    audio_pcm_play_rate(WAR1_MUSIC_CHANNEL, audio_buf, audio_samples,
                        220, /*loop=*/0, hdr->audio_rate);

    uint32_t t_start = timer_read();
    int last_frame = -1;
    int decoded = 0, dropped = 0;

    while (1) {
        uint32_t elapsed_us = ticks_to_us(timer_elapsed(t_start, timer_read()));
        if ((uint64_t)elapsed_us >= audio_us) break;
        /* User skip on any of A / B / START edge. */
        (void)input_poll();
        if (input_pressed() & (BTN_A | BTN_B | BTN_START)) break;

        uint32_t target_frame = (uint32_t)(((uint64_t)elapsed_us * hdr->fps_num)
                                            / (1000000ull * hdr->fps_den));
        if (target_frame >= hdr->num_frames) target_frame = hdr->num_frames - 1;
        if ((int)target_frame == last_frame) continue;
        if (last_frame >= 0 && (int)target_frame > last_frame + 1) {
            dropped += (int)target_frame - last_frame - 1;
        }
        last_frame = (int)target_frame;

        /* Decode the target frame. Stage [00 00 00 01] + NAL into the
         * cedar input region, dcache flush, decode. */
        uint32_t off = frame_offsets[target_frame];
        const uint8_t *q = video_start + off;
        uint32_t nal_size; memcpy(&nal_size, q, 4);
        uint16_t hb = frame_hdr_bits[target_frame];
        const uint8_t *nal = q + 8;

        ((volatile uint8_t *)WAR1_NAL_STAGE)[0] = 0x00;
        ((volatile uint8_t *)WAR1_NAL_STAGE)[1] = 0x00;
        ((volatile uint8_t *)WAR1_NAL_STAGE)[2] = 0x00;
        ((volatile uint8_t *)WAR1_NAL_STAGE)[3] = 0x01;
        memcpy((void *)(WAR1_NAL_STAGE + 4), nal, nal_size);
        dcache_clean_range(WAR1_NAL_STAGE, nal_size + 4);

        cedar_h264_decode((const uint8_t *)WAR1_NAL_STAGE,
                          nal_size + 4,
                          hdr->width, hdr->height,
                          hb,
                          (int)hdr->qp,
                          0, 0, 0);

        uint32_t stride = (hdr->width + 15) & ~15;
        uint32_t mb_h   = ((hdr->height + 15) / 16) * 16;
        dcache_invalidate_range(WAR1_LUMA_ADDR,   stride * mb_h);
        dcache_invalidate_range(WAR1_CHROMA_ADDR, stride * mb_h / 2);

        uint32_t *fb = (uint32_t *)target_fb;
        cedar_nv12_to_argb(fb + dst_y * LCD_W + dst_x, LCD_W,
                           hdr->width, hdr->height);
        dcache_clean_range(target_fb, LCD_W * LCD_H * 4);

        video_set_overlay(target_fb);
        video_wait_vblank();

        decoded++;
    }

    uart_puts("[movie] done — decoded "); uart_putdec(decoded);
    uart_puts(" dropped ");                uart_putdec(dropped);
    uart_puts("\n");

    audio_pcm_stop(WAR1_MUSIC_CHANNEL);
    free(frame_offsets);
    free(frame_hdr_bits);
    /* audio_buf points into `blob` — owned by the caller (war1_movie_play),
     * freed there after we return. The Lua's widget + logic-callback flow
     * gives the briefing one more Invalidate after we return, which
     * repaints bg2 (map png) into the OVL the cinematic just clobbered. */
    return 0;
}

/* Movie::Load entry point — Lua hands us the asset path
 * (e.g. "videos/hmap01.ogv"); we open the matching vbin file under
 * /videos/<name>.vbin on the FatFs partition, fread the whole thing
 * into a heap buffer, and run the cedar playback loop synchronously. */
extern "C" int war1_movie_play(const char *path) {
    if (!path) return -1;
    /* Strip any leading "videos/" and trailing extension to get the bare
     * track name, then build "/videos/<name>.vbin". */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char name[24];
    int n = 0;
    while (base[n] && base[n] != '.' && n + 1 < (int)sizeof(name)) {
        name[n] = base[n]; n++;
    }
    name[n] = 0;
    char fpath[64];
    int  fn = snprintf(fpath, sizeof(fpath), "/videos/%s.vbin", name);
    if (fn <= 0 || fn >= (int)sizeof(fpath)) return -1;

    FILE *f = fopen(fpath, "rb");
    if (!f) {
        uart_puts("[movie] open miss '"); uart_puts(fpath); uart_puts("'\n");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return -1; }
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        uart_puts("[movie] malloc fail ("); uart_putdec((uint32_t)len);
        uart_puts(" bytes)\n");
        fclose(f);
        return -1;
    }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) {
        uart_puts("[movie] short read\n");
        free(buf);
        return -1;
    }
    uart_puts("[movie] loaded "); uart_puts(fpath);
    uart_puts(" ("); uart_putdec((uint32_t)len); uart_puts(" bytes)\n");
    int rc = war1_movie_play_blob(buf, (uint32_t)len);
    free(buf);
    return rc;
}

/* Single, out-of-line definition of Movie::Load (declared in
 * include/movie.h). Routes through war1_movie_play; returning true if
 * a vbin was found and played. */
#include "movie.h"
bool Movie::Load(const std::string &filename, int /*w*/, int /*h*/) {
    return war1_movie_play(filename.c_str()) == 0;
}

/* We need at least one CPlayer with IsEnemy returning false. Stratagus has
 * a Players[] array of MAX_PLAYERS; point our fake unit at index 0. */
extern CPlayer Players[PlayerMax];

/* Placement-new the engine globals whose classes use in-class member
 * initializers (int X = 0; etc.). Bare-metal start doesn't run .init_array,
 * so non-POD globals stay zero-initialized — UnitTypeVar's CBoolKeys /
 * CVariableKeys never populate, UI.MapArea fields stay as stack-garbage,
 * Video.Width/Height/Depth start at 0 instead of "set by InitVideo". Run
 * the ctors explicitly here. Idempotent — safe to call multiple times. */
extern "C" void war1_init_globals(void) {
    /* v3s.h #defines UI as a hardware register base; Stratagus's UI global
     * gets macro-mangled. Undef for the placement-new line. */
    #undef UI
    extern CUserInterface UI;
    new (&UnitTypeVar) CUnitTypeVar();
    new (&UI) CUserInterface();
    new (&Video) CVideo();
}


/* Canonical entry point. Replaces what stratagus_main.cpp's `main` does on
 * desktop: bring up the engine globals, then call stratagusMain. The only
 * non-canonical bit is war1_init_globals — bare-metal start doesn't run
 * .init_array, so non-POD globals (UnitTypeVar, UI, Video) need their
 * member ctors run by hand before stratagusMain touches them.
 *
 * After this, control flows: stratagusMain → InitLua → tolua_stratagus_open
 * → InitAiModule → InitSound → LoadCcl(scripts/stratagus.lua) → InitVideo
 * → ShowTitleScreens → PreMenuSetup → MenuLoop (loads scripts/guichan.lua,
 * which calls RunProgramStartMenu → guichan event loop). */
extern int stratagusMain(int argc, char **argv);
extern "C" void war1_run(void) {
    uart_puts("[BRIDGE] war1_run: ENTER\n");
    war1_init_globals();
    /* -u /wc1 : POSIX absolute path; libstdc++'s fs::create_directories
     *           understands '/'-rooted paths but NOT drive-letter prefixes
     *           ("0:" is treated as a relative-path component under POSIX).
     *           Our syscall stubs translate any '/'-rooted path into the
     *           "0:" FatFs volume internally.
     *
     * Previously we passed -l to disable command-log writes (V3s had no
     * writable FS pre-FatFs), but that left CurrentReplay uninitialized
     * — SaveFullLog dereferences it during save and hangs. Removing -l
     * lets CommandLog initialize CurrentReplay on first unit command.
     */
    static char arg0[] = "war1";
    static char arg1[] = "-u";
    static char arg2[] = "/wc1";
    static char *argv[] = { arg0, arg1, arg2, nullptr };
    stratagusMain(3, argv);
    uart_puts("[BRIDGE] war1_run: stratagusMain returned\n");
}
