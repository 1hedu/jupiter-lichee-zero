# PINE A64+ 2GB, AArch32 EL1 payload behind the validated SD2 firmware.
# Keep board objects separate from the original V3s build.
CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
CXX := $(CROSS)g++
OBJCOPY := $(CROSS)objcopy
GAME ?= examples/menu/main.c
BUILD := build/pine64
TARGET := $(BUILD)/jupiter
ARCH := -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -mthumb
CPPFLAGS := -DJUPITER_BOARD_PINE64=1 -DHAVE_STDINT_H \
 -Iinclude -Ilib -Ilib/arm_opt -Ilibvgm -Ilibvgm/emu -Ilibvgm/emu/cores \
 -Ithird_party/fatfs -Ithird_party/ne10 -Iboards/pine64 \
 -Ithird_party/munt/mt32emu/src -Iboards/pine64/munt -Iexamples/mt32_poc \
 -Ithird_party/munt/mt32emu/src/srchelper/srctools/include \
 -Ithird_party/math_neon -Ithird_party/nuked_sc55/src -Ithird_party/nuked_sc55_stubs \
 -DMT32EMU_WITH_INTERNAL_RESAMPLER=1
CFLAGS := $(ARCH) -nostdlib -ffreestanding -O3 -fno-common \
 -ffunction-sections -fdata-sections -Wall -Wextra \
 -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function \
 -MMD -MP
CXXFLAGS := $(filter-out -ffreestanding,$(CFLAGS)) -std=c++11 -fno-exceptions \
 -fno-threadsafe-statics -fno-use-cxa-atexit -ffp-contract=off
LDFLAGS := $(ARCH) -nostdlib -Tboards/pine64/jupiter.ld \
 -Wl,--gc-sections,-Map,$(TARGET).map

BOARD_SRCS := boards/pine64/start.S boards/pine64/mmu.c \
 boards/pine64/irq.c boards/pine64/timer.c boards/pine64/sdmmc.c \
 boards/pine64/video.c boards/pine64/hstimer.c boards/pine64/sram.c \
 boards/pine64/audio_hw.c boards/pine64/input.c boards/pine64/runtime.c \
 boards/pine64/assets.c boards/pine64/cedar_ipcm.c boards/pine64/demo_music.c \
 boards/pine64/mercury_host.c
-include boards/pine64/usb.mk

LIB_SRCS := lib/uart.c lib/midi.c lib/mem.c lib/tiles.c lib/sprite.c \
 lib/snes.c lib/genesis.c lib/audio.c lib/nes.c lib/gb.c lib/sheet.c \
 lib/libc_shim.c lib/cedar.c lib/cedar_enc.c lib/si5351.c lib/ym3438_hw.c \
 lib/cpak.c lib/cpakfs.c lib/n64_joybus.c third_party/dlmalloc/dlmalloc_jupiter.c \
 lib/sprite_neon.S lib/tiles_neon.S lib/mode7_neon.S \
 lib/arm_opt/memcpy.S lib/arm_opt/memset.S lib/arm_opt/strcmp.S \
 lib/arm_opt/strlen.S lib/arm_opt/memchr.S
VGM_SRCS := libvgm/vgm_player.c libvgm/emu/cores/np_nes_apu.c \
 libvgm/emu/cores/np_nes_dmc.c libvgm/emu/cores/gb.c \
 libvgm/emu/cores/sn76489.c libvgm/emu/cores/ym3438.c lib/nuked_opn2_shim.c
NE10_SRCS := third_party/ne10/NE10_fft.c third_party/ne10/NE10_fft_float32.c \
 third_party/ne10/NE10_fft_float32.neonintrinsic.c third_party/ne10/NE10_fft_generic_float32.c
FATFS_SRCS := third_party/fatfs/ff.c third_party/fatfs/diskio.c third_party/fatfs/ffunicode.c
MT32_DIR := third_party/munt/mt32emu/src
MT32_SRCS := $(wildcard $(MT32_DIR)/*.cpp) $(MT32_DIR)/c_interface/c_interface.cpp \
 $(MT32_DIR)/sha1/sha1.cpp $(MT32_DIR)/srchelper/InternalResampler.cpp \
 $(wildcard $(MT32_DIR)/srchelper/srctools/src/*.cpp) \
 lib/cpp_runtime.cpp lib/mt32_math_shim.c lib/mt32_lut_dump.cpp \
 $(addprefix third_party/math_neon/,math_sinf.c math_cosf.c math_expf.c \
 math_logf.c math_log10f.c math_powf.c math_sqrtf.c math_sincosf.c math_floorf.c math_fabsf.c)
SC55_SRCS := $(addprefix third_party/nuked_sc55/src/,mcu.cpp mcu_opcodes.cpp \
 mcu_interrupt.cpp mcu_timer.cpp pcm.cpp submcu.cpp) \
 lib/nuked_sc55_driver.cpp lib/nuked_sc55_lcd_stub.cpp
MENU_SRCS := $(if $(findstring menu,$(GAME)),$(wildcard examples/menu/ex_*.c))
SRCS := $(BOARD_SRCS) $(USB_SRCS) $(LIB_SRCS) $(VGM_SRCS) $(NE10_SRCS) \
 $(FATFS_SRCS) $(MT32_SRCS) $(SC55_SRCS) $(GAME) $(MENU_SRCS)
OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(SRCS)))
ASSETS := tools/pulseman.argb tools/vinci.argb tools/celebi.argb tools/ff6soldier.argb \
 $(wildcard examples/mt32_rt/midi/*.mid)
ASSET_OBJS := $(addprefix $(BUILD)/assets/,$(addsuffix .o,$(ASSETS)))

# Match the newest SDK's GAME-change protection in this separate build
# directory: object timestamps alone cannot identify the last linked game.
GAME_STAMP := $(BUILD)/.game_stamp
ifneq ($(strip $(shell cat $(GAME_STAMP) 2>/dev/null)),$(strip $(GAME)))
  $(shell mkdir -p $(BUILD))
  $(file >$(GAME_STAMP),$(GAME))
endif

# The checked-in boards/pine64/munt/config.h supplies the static Munt
# configuration even in a fresh submodule checkout. RTTI stays enabled in
# this board build, so it needs no dynamic_cast source patch.
SC55_PREP := $(BUILD)/sc55/.prepared
$(SC55_PREP): scripts/prepare_sc55.py third_party/nuked_sc55/src/mcu.cpp
	@mkdir -p $(dir $@)
	python3 scripts/prepare_sc55.py
	@touch $@
$(addprefix $(BUILD)/,$(addsuffix .o,$(filter third_party/nuked_sc55/%,$(SC55_SRCS)))): $(SC55_PREP)

$(OBJS): boards/pine64/board.mk

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all boot clean size check
all: $(TARGET).uImage
boot: $(TARGET).uImage $(BUILD)/boot.scr
$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ARCH) -c $< -o $@
$(BUILD)/assets/%.o: %
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm \
	 --rename-section .data=.rodata.assets,alloc,load,readonly,data,contents $< $@
$(TARGET).elf: $(OBJS) $(ASSET_OBJS) boards/pine64/jupiter.ld $(GAME_STAMP)
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).uImage
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(ASSET_OBJS) \
	 -Wl,--start-group -lstdc++ -lm -lc -lgcc -Wl,--end-group
	$(CROSS)size $@
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
$(TARGET).uImage: $(TARGET).bin
	mkimage -A arm -O linux -T kernel -C none -a 0x41000000 -e 0x41000000 \
	 -n Jupiter-PINE64-Menu -d $< $@
$(BUILD)/boot.scr: boards/pine64/boot.cmd
	@mkdir -p $(dir $@)
	mkimage -A arm -T script -C none -n Jupiter-PINE64-Menu -d $< $@
size: $(TARGET).elf
	$(CROSS)size $<
check:
	bash boards/pine64/tests/run_cpu_smoke.sh
	bash boards/pine64/tests/run_sdmmc_tests.sh
	bash boards/pine64/tests/run_usb_keyboard_tests.sh
	bash boards/pine64/tests/run_usb_host_qemu.sh
clean:
	rm -rf $(BUILD)
-include $(OBJS:.o=.d)
