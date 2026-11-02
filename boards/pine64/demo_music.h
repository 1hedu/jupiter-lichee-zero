#ifndef PINE64_DEMO_MUSIC_H
#define PINE64_DEMO_MUSIC_H
#define PINE64_DEMO_VGM_SIZE 6592u
extern const unsigned char pine64_demo_vgm[PINE64_DEMO_VGM_SIZE];
#define PINE64_MUSIC_TITLE "Jupiter FM+PSG exercise"
#undef vgm_fighting_back
#undef vgm_fighting_back_len
#define vgm_fighting_back pine64_demo_vgm
#define vgm_fighting_back_len PINE64_DEMO_VGM_SIZE
#undef vgm_sortie
#undef vgm_sortie_len
#define vgm_sortie pine64_demo_vgm
#define vgm_sortie_len PINE64_DEMO_VGM_SIZE
#undef vgm_step_on_beat
#undef vgm_step_on_beat_len
#define vgm_step_on_beat pine64_demo_vgm
#define vgm_step_on_beat_len PINE64_DEMO_VGM_SIZE
#endif
