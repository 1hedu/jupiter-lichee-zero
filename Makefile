# Jupiter SDK — Makefile
# Bare-metal game kit for Lichee Pi Zero (Allwinner V3s)
#
# Usage:
#   make                              Build the game template
#   make GAME=examples/parallax/main.c   Build an example
#   make boot                         Build + generate boot.scr
#   make sdcard MNT=/mnt              Copy to mounted SD card

CROSS   = arm-none-eabi-
CC      = $(CROSS)gcc
CXX     = $(CROSS)g++
OBJCOPY = $(CROSS)objcopy
SIZE    = $(CROSS)size

CFLAGS  = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
          -nostdlib -ffreestanding -Ofast -mthumb \
          -Wall -Wextra -Werror \
          -I include -I libvgm -I libvgm/emu -I libvgm/emu/cores \
          -I third_party/fatfs \
          -DHAVE_STDINT_H

LDSCRIPT = scripts/jupiter.ld
LDFLAGS  = -T $(LDSCRIPT)

# Library sources (always linked)
LIB_SRCS = lib/uart.c lib/midi.c lib/timer.c lib/mem.c lib/video.c lib/tiles.c lib/mmu.c \
           lib/sprite.c lib/snes.c lib/genesis.c lib/audio.c lib/genesis_asm.S \
           lib/nes.c lib/gb.c lib/hstimer.c lib/input.c \
           lib/libc_shim.c lib/irq.c lib/sram.c lib/cedar.c lib/cedar_enc.c lib/si5351.c lib/ym3438_hw.c \
           lib/sdmmc.c lib/cpak.c lib/n64_joybus.c \
           third_party/dlmalloc/dlmalloc_jupiter.c

# FatFs (ChaN R0.15a, BSD-2-clause). Always linked — provides f_mount /
# f_open / f_read / f_write / f_lseek / f_opendir / f_readdir on top of
# our SDMMC0 raw-block driver. FF_MULTI_PARTITION = 1 so logical drive
# 0: maps to the user's 2nd primary partition (the 3.57 GB FAT16 made in
# Disk Management). The 128 MB boot partition is never touched.
FATFS_SRCS = third_party/fatfs/ff.c third_party/fatfs/diskio.c third_party/fatfs/ffunicode.c
FATFS_OBJS = $(patsubst %.c,build/fatfs/%.o,$(FATFS_SRCS))

# Menu bundles mt32_rt + sc55_warcraft, so force those backends on.
IS_MENU := $(findstring menu,$(GAME))

# ---- Optional: mt32emu (Munt) for MT-32 LA synthesis ----
# Pulled in automatically when the game path contains "mt32" or is the menu.
MT32_ENABLE := $(findstring mt32,$(GAME))$(IS_MENU)
ifneq ($(MT32_ENABLE),)
  MT32_SRC_DIR = third_party/munt/mt32emu/src
  MT32_CPP_SRCS := $(wildcard $(MT32_SRC_DIR)/*.cpp) \
                   $(MT32_SRC_DIR)/c_interface/c_interface.cpp \
                   $(MT32_SRC_DIR)/sha1/sha1.cpp \
                   $(MT32_SRC_DIR)/srchelper/InternalResampler.cpp \
                   $(wildcard $(MT32_SRC_DIR)/srchelper/srctools/src/*.cpp)
  # math-neon: NEON-accelerated sinf/cosf/expf/logf/powf for LA synthesis
  MT32_MATH_NEON_SRCS := $(wildcard third_party/math_neon/math_sinf.c \
                           third_party/math_neon/math_cosf.c \
                           third_party/math_neon/math_expf.c \
                           third_party/math_neon/math_logf.c \
                           third_party/math_neon/math_log10f.c \
                           third_party/math_neon/math_powf.c \
                           third_party/math_neon/math_sqrtf.c \
                           third_party/math_neon/math_sincosf.c \
                           third_party/math_neon/math_floorf.c \
                           third_party/math_neon/math_fabsf.c)
  MT32_MATH_NEON_OBJS := $(patsubst third_party/math_neon/%.c,build/mt32/math_neon/%.o,$(MT32_MATH_NEON_SRCS))
  MT32_OBJS := $(patsubst $(MT32_SRC_DIR)/%.cpp,build/mt32/%.o,$(MT32_CPP_SRCS)) \
               build/mt32/cpp_runtime.o \
               build/mt32/mt32_math_shim.o \
               build/mt32/mt32_lut_dump.o $(MT32_MATH_NEON_OBJS)
  MT32_CXX_FLAGS = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
                   -nostdlib -Ofast -std=c++11 \
                   -ffp-contract=off \
                   -fno-exceptions -fno-rtti -fno-threadsafe-statics \
                   -fno-use-cxa-atexit -ftree-vectorize \
                   -Wno-unused-parameter -Wno-sign-compare -Wno-unused-but-set-variable \
                   -DMT32EMU_WITH_INTERNAL_RESAMPLER=1 \
                   -I $(MT32_SRC_DIR) \
                   -I $(MT32_SRC_DIR)/srchelper/srctools/include \
                   -I third_party/math_neon \
                   -I include
  CFLAGS += -I $(MT32_SRC_DIR) -I examples/mt32_poc
endif

# ---- Optional: Nuked-SC55 (Roland Sound Canvas SC-55 mkII emulator) ----
# Pulled in automatically when the game path contains "sc55".
SC55_ENABLE := $(findstring sc55,$(GAME))$(IS_MENU)
ifneq ($(SC55_ENABLE),)
  SC55_SRC_DIR  = third_party/nuked_sc55/src
  SC55_STUB_DIR = third_party/nuked_sc55_stubs
  SC55_CPP_SRCS := $(SC55_SRC_DIR)/mcu.cpp \
                   $(SC55_SRC_DIR)/mcu_opcodes.cpp \
                   $(SC55_SRC_DIR)/mcu_interrupt.cpp \
                   $(SC55_SRC_DIR)/mcu_timer.cpp \
                   $(SC55_SRC_DIR)/pcm.cpp \
                   $(SC55_SRC_DIR)/submcu.cpp
  SC55_OBJS := $(patsubst $(SC55_SRC_DIR)/%.cpp,build/sc55/%.o,$(SC55_CPP_SRCS)) \
               build/sc55/nuked_sc55_driver.o \
               build/sc55/nuked_sc55_lcd_stub.o
  # Avoid duplicate operator new/delete symbols when MT32 also links its copy.
  ifeq ($(MT32_ENABLE),)
    SC55_OBJS += build/sc55/cpp_runtime.o
  endif
  SC55_CXX_FLAGS = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
                   -nostdlib -O3 -funroll-loops -std=c++11 \
                   -fno-exceptions -fno-rtti -fno-threadsafe-statics \
                   -fno-use-cxa-atexit \
                   -ffunction-sections -fdata-sections \
                   -Wno-unused-parameter -Wno-sign-compare \
                   -Wno-unused-but-set-variable -Wno-unused-variable \
                   -Wno-unused-function -Wno-return-type \
                   -I $(SC55_SRC_DIR) -I $(SC55_STUB_DIR) -I include
  CFLAGS += -I $(SC55_SRC_DIR) -I include
  LDFLAGS += -Wl,--gc-sections
endif

# ---- Optional: Warcraft 1 port (Stratagus engine + War1gus data) ----
# Pulled in automatically when the game path contains "war1".
comma := ,
WAR1_ENABLE := $(findstring war1,$(GAME))
ifneq ($(WAR1_ENABLE),)
  WAR1_SRC_DIR  = third_party/stratagus/src
  WAR1_PORT_DIR = third_party/stratagus_port
  WAR1_STUBS    = $(WAR1_PORT_DIR)/stubs
  LUA_SRC_DIR   = third_party/lua/lua-5.3.6/src
  LUA_CORE_SRCS = $(addprefix $(LUA_SRC_DIR)/, \
      lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
      lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c \
      ltm.c lundump.c lvm.c lzio.c)
  LUA_LIB_SRCS = $(addprefix $(LUA_SRC_DIR)/, \
      lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c lmathlib.c \
      lstrlib.c ltablib.c lutf8lib.c)
  LUA_SRCS = $(LUA_CORE_SRCS) $(LUA_LIB_SRCS)
  LUA_OBJS = $(patsubst %.c,build/war1/%.o,$(LUA_SRCS))

  # Files we don't compile yet — they have working stubs in
  # war1_link_stubs.cpp and switching to upstream wholesale risks
  # regressions in known-working subsystems. Re-enable one at a time
  # as we wire each menu/widget/font feature properly.
  #   linedraw.cpp — SDL_LockSurface / SDLputPixel
  #   font.cpp     — CFont::Height does G->Height with no null guard;
  #                  CharWidth array filled by MeasureWidths after font load
  #   widgets.cpp  — Stratagus's guisan integration; needs gcn::Gui +
  #                  JupiterGraphics wired before we drop our shim
  WAR1_CPP_SRCS := $(filter-out \
                       %/video/linedraw.cpp \
                       %/video/font.cpp \
                       %/ui/widgets.cpp, \
                       $(shell find $(WAR1_SRC_DIR) -name "*.cpp"))
  WAR1_PORT_SRCS := $(WAR1_PORT_DIR)/war1_stub_bodies.cpp \
                    $(WAR1_PORT_DIR)/war1_link_stubs.cpp \
                    $(WAR1_PORT_DIR)/war1_tolua_bindings.cpp \
                    $(WAR1_PORT_DIR)/war1_widgets.cpp \
                    $(WAR1_PORT_DIR)/war1_widget_bindings.cpp
  # Guisan widget toolkit: the in-game menu, save/load, options, dialogs.
  # Skip src/sdl/ and src/opengl/ — replaced by our Jupiter backend in
  # third_party/guisan_jupiter/. Compile core + widget impls only.
  GUISAN_DIR = third_party/guisan
  GUISAN_JUP_DIR = third_party/guisan_jupiter
  GUISAN_CORE_SRCS := $(wildcard $(GUISAN_DIR)/src/*.cpp)
  GUISAN_WIDGET_SRCS := $(wildcard $(GUISAN_DIR)/src/widgets/*.cpp)
  GUISAN_JUP_SRCS := $(wildcard $(GUISAN_JUP_DIR)/*.cpp)
  GUISAN_SRCS = $(GUISAN_CORE_SRCS) $(GUISAN_WIDGET_SRCS) $(GUISAN_JUP_SRCS)
  TOLUA_DIR = third_party/toluapp
  TOLUA_SRCS = $(TOLUA_DIR)/tolua_event.c $(TOLUA_DIR)/tolua_is.c \
               $(TOLUA_DIR)/tolua_map.c $(TOLUA_DIR)/tolua_push.c \
               $(TOLUA_DIR)/tolua_to.c
  TOLUA_OBJS = $(patsubst %.c,build/war1/%.o,$(TOLUA_SRCS))
  WAR1_EXAMPLE_CPP := $(wildcard examples/war1/*.cpp)
  WAR1_ASSET_SRCS := $(shell find examples/war1/assets -name "*.pc8" 2>/dev/null)
  WAR1_PCM_SRCS := $(shell find examples/war1/assets/sounds -name "*.pcm" 2>/dev/null)
  WAR1_LUA_SCRIPT_SRCS := $(shell find examples/war1/scripts -name "*.lua" 2>/dev/null)
  WAR1_SMS_SCRIPT_SRCS := $(shell find examples/war1/scripts -name "*.sms" 2>/dev/null)
  WAR1_SMP_SCRIPT_SRCS := $(shell find examples/war1/scripts -name "*.smp" 2>/dev/null)
  WAR1_TXT_SCRIPT_SRCS := $(shell find examples/war1/scripts -name "*.txt" 2>/dev/null)
  WAR1_OBJS := $(patsubst %.cpp,build/war1/%.o,$(WAR1_CPP_SRCS)) \
               $(patsubst %.cpp,build/war1/%.o,$(WAR1_PORT_SRCS)) \
               $(patsubst %.cpp,build/war1/%.o,$(WAR1_EXAMPLE_CPP)) \
               $(patsubst %.cpp,build/war1/%.o,$(GUISAN_SRCS)) \
               $(patsubst %.pc8,build/war1/%.o,$(WAR1_ASSET_SRCS)) \
               $(patsubst %.pcm,build/war1/%.pcm.o,$(WAR1_PCM_SRCS)) \
               $(patsubst %.lua,build/war1/%.o,$(WAR1_LUA_SCRIPT_SRCS)) \
               $(patsubst %.sms,build/war1/%.sms.o,$(WAR1_SMS_SCRIPT_SRCS)) \
               $(patsubst %.smp,build/war1/%.smp.o,$(WAR1_SMP_SCRIPT_SRCS)) \
               $(patsubst %.txt,build/war1/%.txt.o,$(WAR1_TXT_SCRIPT_SRCS)) \
               $(LUA_OBJS) \
               $(TOLUA_OBJS)
  WAR1_CXX_FLAGS = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
                   -mthumb -nostdlib -O3 -std=c++17 \
                   -fexceptions -fpermissive -fno-threadsafe-statics \
                   -fno-asynchronous-unwind-tables \
                   -DUSE_ZLIB -DUSE_BZ2LIB -DDYNAMIC_LOAD -DLUA_USE_C89 -DLUA_COMPAT_5_1 -DLUA_COMPAT_5_2 \
                   -Wno-deprecated-declarations -Wno-unused-parameter \
                   -Wno-sign-compare -Wno-unused-variable \
                   -Wno-unused-but-set-variable -Wno-format \
                   -ffunction-sections -fdata-sections \
                   -I $(LUA_SRC_DIR) \
                   -I $(WAR1_SRC_DIR)/include \
                   -I $(WAR1_STUBS) \
                   -I $(GUISAN_DIR)/include \
                   -I $(GUISAN_JUP_DIR) \
                   -I third_party/fatfs \
                   -I include
  WAR1_LUA_CFLAGS = -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
                    -mthumb -nostdlib -Os -DLUA_USE_C89 -DLUA_COMPAT_5_1 -DLUA_COMPAT_5_2 \
                    -Wno-unused-parameter -Wno-unused-but-set-variable \
                    -Wno-maybe-uninitialized -Wno-implicit-fallthrough \
                    -ffunction-sections -fdata-sections \
                    -I $(LUA_SRC_DIR) -I $(TOLUA_DIR) -I include
  # tolua++ C files compile with the same flags; tolua++.h is in $(TOLUA_DIR).
  CFLAGS += -I $(LUA_SRC_DIR) -I $(WAR1_SRC_DIR)/include -I $(WAR1_STUBS) -I $(TOLUA_DIR)
  WAR1_CXX_FLAGS += -I $(TOLUA_DIR)
  LDFLAGS += -Wl,--gc-sections
endif

# libvgm chip emulators + VGM player. Default uses the Gens core for the
# YM2612 (fast, real-time-friendly). When the GAME path contains "opn2",
# we swap to the Nuked-OPN2 core (cycle-accurate, much slower) plus a
# tiny shim that exports the same YM2612_* symbols vgm_player.c expects.
OPN2_ENABLE := $(findstring opn2,$(GAME))
ifneq ($(OPN2_ENABLE),)
  YM2612_CORE = libvgm/emu/cores/ym3438.c lib/nuked_opn2_shim.c
else
  YM2612_CORE = libvgm/emu/cores/ym2612.c
endif

VGM_SRCS = libvgm/vgm_player.c libvgm/emu/cores/np_nes_apu.c \
           libvgm/emu/cores/np_nes_dmc.c libvgm/emu/cores/gb.c \
           libvgm/emu/cores/sn76489.c $(YM2612_CORE)

# Game source (override with GAME=path/to/main.c)
GAME ?= template/game.c

# Menu build: auto-include wrapper files for combined example binary
ifneq (,$(findstring menu,$(GAME)))
  MENU_SRCS := $(wildcard examples/menu/ex_*.c)
endif

# Assembly startup + NEON inner loops
ASM_SRCS = scripts/start.S lib/sprite_neon.S lib/tiles_neon.S lib/mode7_neon.S

# ARM optimized libc routines (MIT/Apache-2.0 with LLVM-exception)
# From: https://github.com/ARM-software/optimized-routines
ARM_OPT_SRCS = lib/arm_opt/memcpy.S lib/arm_opt/memset.S \
               lib/arm_opt/strcmp.S lib/arm_opt/strlen.S \
               lib/arm_opt/memchr.S

# Ne10 NEON FFT (BSD 3-Clause) — always linked, small footprint
# From: https://github.com/projectNe10/Ne10
NE10_SRCS = third_party/ne10/NE10_fft.c \
            third_party/ne10/NE10_fft_float32.c \
            third_party/ne10/NE10_fft_float32.neonintrinsic.c \
            third_party/ne10/NE10_fft_generic_float32.c

# ---- Optional: embedded binary assets (objcopy) ----
CEDAR_GENESIS := $(findstring cedar_genesis,$(GAME))
ifneq ($(CEDAR_GENESIS),)
  ASSET_OBJS = build/pulseman_argb.o
endif
# cedar_nes: no external assets, CHR + thumbnail embedded as C headers
CEDAR_SNES := $(findstring cedar_snes,$(GAME))
ifneq ($(CEDAR_SNES),)
  ASSET_OBJS = build/ff6soldier_argb.o
endif
TRILAYER := $(findstring trilayer,$(GAME))
ifneq ($(TRILAYER),)
  ASSET_OBJS = build/pulseman_argb.o
endif
INPUT_TEST := $(findstring input_test,$(GAME))
ifneq ($(INPUT_TEST),)
  ASSET_OBJS = build/pulseman_argb.o
endif
INPUT_MT32 := $(findstring input_mt32,$(GAME))
ifneq ($(INPUT_MT32),)
  ASSET_OBJS = build/pulseman_argb.o
endif
OPN2_INPUT := $(findstring opn2_input,$(GAME))
ifneq ($(OPN2_INPUT),)
  ASSET_OBJS = build/pulseman_argb.o
endif
OPN2_HW := $(findstring opn2_hw,$(GAME))
ifneq ($(OPN2_HW),)
  ASSET_OBJS = build/pulseman_argb.o
endif
CEDAR_VIDEO := $(findstring cedar_video,$(GAME))
ifneq ($(CEDAR_VIDEO),)
  ASSET_OBJS = build/title_vbin.o
endif
# waveterm needs no embedded asset — UI is pure phosphor-CRT text mode.
CEDAR_VIDEO_AV := $(findstring cedar_video_av,$(GAME))
ifneq ($(CEDAR_VIDEO_AV),)
  ASSET_OBJS = build/title_vbin.o
endif
MENU_BUILD := $(findstring menu,$(GAME))
MT32_RT := $(findstring mt32_rt,$(GAME))$(MENU_BUILD)
ifneq ($(MT32_RT),)
  MT32_MIDI_SRCS := $(wildcard examples/mt32_rt/midi/*.mid)
  MT32_MIDI_OBJS := $(patsubst examples/mt32_rt/midi/%.mid,build/mt32_rt/midi/%.o,$(MT32_MIDI_SRCS))
  ASSET_OBJS = build/mt32_rt/mt32_control_rom.o \
               build/mt32_rt/mt32_pcm_rom.o \
               $(MT32_MIDI_OBJS)
  ifneq ($(MENU_BUILD),)
    # Menu needs pulseman.argb too (for other demos that expect it)
    ASSET_OBJS += build/pulseman_argb.o
  endif
else ifneq ($(MENU_BUILD),)
  ASSET_OBJS = build/pulseman_argb.o
endif

TARGET = build/jupiter

.PHONY: all clean size boot sdcard

all: $(TARGET).bin

# FatFs compile rule. Upstream code uses old-style declarations and
# triggers our -Wextra warnings; relax just enough to build clean.
build/fatfs/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
	      -nostdlib -ffreestanding -Os -mthumb \
	      -Wno-unused-parameter -Wno-unused-but-set-variable \
	      -Wno-unused-variable -Wno-sign-compare -Wno-unused-function \
	      -Wno-old-style-declaration \
	      -I third_party/fatfs -I include \
	      -c $< -o $@

# mt32emu C++ compile rules
build/mt32/%.o: $(MT32_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(MT32_CXX_FLAGS) -c $< -o $@

build/mt32/cpp_runtime.o: lib/cpp_runtime.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(MT32_CXX_FLAGS) -c $< -o $@

build/mt32/mt32_math_shim.o: lib/mt32_math_shim.c
	@mkdir -p $(dir $@)
	$(CC) -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -Ofast -c $< -o $@

build/mt32/mt32_lut_dump.o: lib/mt32_lut_dump.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(MT32_CXX_FLAGS) -c $< -o $@

# math-neon C files for MT-32 NEON acceleration
build/mt32/math_neon/%.o: third_party/math_neon/%.c
	@mkdir -p $(dir $@)
	$(CC) -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -Ofast -I third_party/math_neon -c $< -o $@

# Nuked-SC55 C++ compile rules
build/sc55/%.o: $(SC55_SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(SC55_CXX_FLAGS) -c $< -o $@

build/sc55/nuked_sc55_driver.o: lib/nuked_sc55_driver.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(SC55_CXX_FLAGS) -c $< -o $@

build/sc55/nuked_sc55_lcd_stub.o: lib/nuked_sc55_lcd_stub.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(SC55_CXX_FLAGS) -c $< -o $@

build/sc55/cpp_runtime.o: lib/cpp_runtime.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(SC55_CXX_FLAGS) -c $< -o $@

# Embed binary assets via objcopy
build/ff6soldier_argb.o: tools/ff6soldier.argb
	@mkdir -p build
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

build/pulseman_argb.o: tools/pulseman.argb
	@mkdir -p build
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@


build/pulseman_h264.o: tools/pulseman.h264
	@mkdir -p build
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

build/title_vbin.o: build/title.vbin
	@mkdir -p build
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# mt32_rt asset embedding
build/mt32_rt/mt32_control_rom.o: examples/mt32_rt/roms/MT32_CONTROL.ROM
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

build/mt32_rt/mt32_pcm_rom.o: examples/mt32_rt/roms/MT32_PCM.ROM
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

build/mt32_rt/midi/%.o: examples/mt32_rt/midi/%.mid
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# Regenerate catalog whenever the midi set changes
examples/mt32_rt/midi_catalog.h: scripts/gen_midi_catalog.py $(wildcard examples/mt32_rt/midi/*.mid)
	python scripts/gen_midi_catalog.py

MT32_EXTRA_DEPS :=
ifneq ($(MT32_RT),)
  MT32_EXTRA_DEPS := examples/mt32_rt/midi_catalog.h
endif
WAR1_EXTRA_DEPS :=
WAR1_LINK_WRAP :=
ifneq ($(WAR1_ENABLE),)
  # WAR1_BUILD switches dlmalloc into the big-arena layout (24 MB main +
  # 13 MB low). Non-war1 builds stay on the slim 1 MB / 1 MB arenas so
  # the menu / synth-rt examples don't blow past CODE region budget.
  CFLAGS += -DWAR1_BUILD=1
  WAR1_EXTRA_DEPS := examples/war1/war1_script_catalog.h examples/war1/war1_asset_catalog.h
  # Wrap __cxa_throw so we can see what type was thrown when libstdc++ code
  # paths throw (filesystem_error, bad_alloc, etc.) under -fno-exceptions.
  # Also wrap fs::absolute (both overloads): on arm-none-eabi libstdc++ it
  # refuses relative paths with ec=ENOTRECOVERABLE (no getcwd), which then
  # throws through fs::absolute(path) — Stratagus calls that on every Load.
  WAR1_LINK_WRAP := -Wl,--wrap=__cxa_throw \
                    -Wl,--wrap=_ZNSt10filesystem8absoluteERKNS_7__cxx114pathE \
                    -Wl,--wrap=_ZNSt10filesystem8absoluteERKNS_7__cxx114pathERSt10error_code \
                    -Wl,--wrap=_ZNSt10filesystem18create_directoriesERKNS_7__cxx114pathE \
                    -Wl,--wrap=_ZNSt10filesystem18create_directoriesERKNS_7__cxx114pathERSt10error_code \
                    -Wl,--wrap=localeconv
endif

# Stratagus (Warcraft 1 port) C++ compile rule
build/war1/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(WAR1_CXX_FLAGS) -c $< -o $@

# Lua 5.3 C compile rule (part of war1 build)
build/war1/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(WAR1_LUA_CFLAGS) -c $< -o $@

# War1 asset objcopy: .pc8 → .o
build/war1/%.o: %.pc8
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# War1 sound asset objcopy: .pcm → .pcm.o (keep ext to avoid colliding
# with similarly-named .pc8 / .lua siblings under build/war1/...).
# These land in CODE/.data alongside the pc8 graphics blobs; the libc
# shim heap was shrunk by ~3MB to keep CODE's bss budget happy.
build/war1/%.pcm.o: %.pcm
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# War1 Lua-script objcopy: .lua → .o
build/war1/%.o: %.lua
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# War1 campaign map objcopy: .sms → .sms.o (keep ext — 01.sms + 01.smp
# would otherwise collide on 01.o in the same directory).
build/war1/%.sms.o: %.sms
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# War1 map-presentation objcopy: .smp → .smp.o (plain Lua)
build/war1/%.smp.o: %.smp
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# War1 mission-intro / dialog objcopy: .txt → .txt.o (briefing prose)
build/war1/%.txt.o: %.txt
	@mkdir -p $(dir $@)
	$(CROSS)objcopy -I binary -O elf32-littlearm -B arm $< $@

# Regenerate Lua script catalog when the script/campaign set changes
examples/war1/war1_script_catalog.h: scripts/war1_embed_lua.py $(WAR1_LUA_SCRIPT_SRCS) $(WAR1_SMS_SCRIPT_SRCS) $(WAR1_SMP_SCRIPT_SRCS) $(WAR1_TXT_SCRIPT_SRCS)
	python scripts/war1_embed_lua.py

# Asset catalog is produced by war1_assets.py (which also does the PNG->pc8
# conversion). The generated file is checked in; regenerate by running the
# script manually — we don't wire it to a generic build dep because it
# needs the data.War1gus path passed in.

$(TARGET).elf: $(MT32_EXTRA_DEPS) $(WAR1_EXTRA_DEPS) $(ASM_SRCS) $(ARM_OPT_SRCS) $(LIB_SRCS) $(VGM_SRCS) $(NE10_SRCS) $(FATFS_OBJS) $(MT32_OBJS) $(SC55_OBJS) $(WAR1_OBJS) $(ASSET_OBJS) $(GAME) $(LDSCRIPT)
	@mkdir -p build
	@# Windows CreateProcess caps argv at ~32 KB; with guisan added the link
	@# command exceeds it. Use GNU make's $(file) to write a response file
	@# (no shell involved, no length limit), then pass via @file to gcc.
	$(file >build/link_inputs.rsp,$(ASM_SRCS) $(ARM_OPT_SRCS) $(LIB_SRCS) $(VGM_SRCS) $(NE10_SRCS) $(FATFS_OBJS) $(GAME) $(MENU_SRCS) $(MT32_OBJS) $(SC55_OBJS) $(WAR1_OBJS) $(ASSET_OBJS))
	$(CC) $(CFLAGS) -Wno-error -Wno-unused-parameter -Wno-sign-compare $(LDFLAGS) -I lib/arm_opt -I third_party/ne10 $(WAR1_LINK_WRAP) -o $@ @build/link_inputs.rsp $(if $(WAR1_ENABLE),-Wl$(comma)--start-group -lstdc++ -lc -lm -lgcc -Wl$(comma)--end-group,-lgcc -lm)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "--- Built $@ (game: $(GAME)) ---"
	@$(SIZE) $<

build/boot.scr: scripts/boot.cmd
	@mkdir -p build
	mkimage -C none -A arm -T script -d $< $@
	@echo "--- Built $@ ---"

boot: $(TARGET).bin build/boot.scr
	@echo "Ready. Copy build/jupiter.bin and build/boot.scr to FAT partition."

sdcard: boot
ifndef MNT
	$(error Set MNT= to your mounted FAT partition, e.g. make sdcard MNT=/mnt)
endif
	cp $(TARGET).bin $(MNT)/jupiter.bin
	cp build/boot.scr $(MNT)/boot.scr
	sync
	@echo "--- Copied to $(MNT) ---"

clean:
	rm -rf build/

size: $(TARGET).elf
	$(SIZE) $<
