/* SDL_mixer stub — Jupiter port.
 *
 * Upstream Stratagus sound_server.cpp and music.cpp use SDL_mixer for
 * WAV/MUS playback + mixing. On bare-metal we route to the Jupiter
 * audio_pcm_play layer via war1_sound.cpp; these stubs just satisfy the
 * link. Stratagus calls them but most paths early-return when no sound
 * is enabled. */
#ifndef JUPITER_SDL_MIXER_STUB_H
#define JUPITER_SDL_MIXER_STUB_H
#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Jupiter port: extra `rate` field carries the WAV's native sample rate
 * so the audio_pcm_play_rate mixer can resample on the fly without us
 * pre-resampling every WAV to 48kHz at conversion time. */
typedef struct Mix_Chunk { Uint32 allocated; Uint8 *abuf; Uint32 alen; Uint8 volume; Uint32 rate; } Mix_Chunk;
typedef struct _Mix_Music Mix_Music;

enum { MIX_INIT_MP3 = 0x8, MIX_INIT_OGG = 0x10, MIX_INIT_MOD = 0x4,
       MIX_INIT_FLAC = 0x1, MIX_INIT_MID = 0x20, MIX_INIT_OPUS = 0x40 };

#define MIX_DEFAULT_FREQUENCY 44100
#define MIX_DEFAULT_FORMAT    0x8010  /* AUDIO_S16LSB */
#define MIX_DEFAULT_CHANNELS  2
#define MIX_MAX_VOLUME        128

int  Mix_Init(int flags);
void Mix_Quit(void);
int  Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int  Mix_AllocateChannels(int numchans);

Mix_Chunk *Mix_LoadWAV(const char *file);
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *rw, int freesrc);
void Mix_FreeChunk(Mix_Chunk *chunk);

Mix_Music *Mix_LoadMUS(const char *file);
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *rw, int freesrc);
void Mix_FreeMusic(Mix_Music *music);

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks);
int Mix_HaltChannel(int channel);
int Mix_Playing(int channel);
int Mix_Volume(int channel, int volume);
int Mix_VolumeChunk(Mix_Chunk *chunk, int volume);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);
Mix_Chunk *Mix_GetChunk(int channel);

int  Mix_PlayMusic(Mix_Music *music, int loops);
int  Mix_PlayingMusic(void);
int  Mix_HaltMusic(void);
int  Mix_ResumeMusic(void);
int  Mix_Resume(int channel);
int  Mix_PauseMusic(void);
int  Mix_Pause(int channel);
int  Mix_VolumeMusic(int volume);
int  Mix_FadeInMusic(Mix_Music *music, int loops, int ms);
int  Mix_FadeOutMusic(int ms);

const char *Mix_GetError(void);
int  Mix_GetNumChunkDecoders(void);
int  Mix_GetNumMusicDecoders(void);
const char *Mix_GetChunkDecoder(int index);
const char *Mix_GetMusicDecoder(int index);

void (*Mix_GetMusicHookData(void))(void *, Uint8 *, int);
void Mix_HookMusicFinished(void (*cb)(void));
typedef void (*Mix_EffectFunc_t)(int, void *, int, void *);
typedef void (*Mix_EffectDone_t)(int, void *);
int  Mix_RegisterEffect(int channel, Mix_EffectFunc_t f, Mix_EffectDone_t d, void *arg);
int  Mix_UnregisterAllEffects(int channel);
int  Mix_ChannelFinished(void (*cb)(int));

#ifdef __cplusplus
}
#endif
#endif
