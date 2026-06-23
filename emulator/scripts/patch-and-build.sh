#!/bin/bash
# patch-and-build.sh — Wire our V3s SoC + lichee-zero machine into QEMU,
# configure if needed, then build qemu-system-arm.
#
# Run from WSL. Sources live in /mnt/c/Users/darek/licheeEmu/src/ and are
# copied (not symlinked, to avoid /mnt/c permission confusion) into the
# QEMU tree at ~/licheeEmu-build/qemu/ on every invocation — edit the
# copies in /mnt/c and re-run this script.
set -euo pipefail
# Ensure failures inside pipelines propagate (the top-level invocation often
# pipes our output through `tail` — without pipefail, a configure failure
# silently returns the pipe's tail exit code, which is 0).

LICHEE_SRC="/mnt/c/Users/darek/licheeEmu/src"
QEMU_DIR="$HOME/licheeEmu-build/qemu"
BUILD_DIR="$QEMU_DIR/build"

[ -d "$QEMU_DIR" ] || { echo "QEMU not cloned at $QEMU_DIR"; exit 1; }
cd "$QEMU_DIR"

echo "==> Copying V3s/lichee-zero sources into QEMU tree"
install -Dm644 "$LICHEE_SRC/include/hw/arm/allwinner-v3s.h" \
               "$QEMU_DIR/include/hw/arm/allwinner-v3s.h"
install -Dm644 "$LICHEE_SRC/include/hw/display/allwinner-v3s-display.h" \
               "$QEMU_DIR/include/hw/display/allwinner-v3s-display.h"
install -Dm644 "$LICHEE_SRC/hw/arm/allwinner-v3s.c" \
               "$QEMU_DIR/hw/arm/allwinner-v3s.c"
install -Dm644 "$LICHEE_SRC/hw/arm/lichee-zero.c" \
               "$QEMU_DIR/hw/arm/lichee-zero.c"
install -Dm644 "$LICHEE_SRC/hw/display/allwinner-v3s-display.c" \
               "$QEMU_DIR/hw/display/allwinner-v3s-display.c"
install -Dm644 "$LICHEE_SRC/include/hw/gpio/allwinner-v3s-pio.h" \
               "$QEMU_DIR/include/hw/gpio/allwinner-v3s-pio.h"
install -Dm644 "$LICHEE_SRC/hw/gpio/allwinner-v3s-pio.c" \
               "$QEMU_DIR/hw/gpio/allwinner-v3s-pio.c"
install -Dm644 "$LICHEE_SRC/include/hw/timer/allwinner-v3s-hstimer.h" \
               "$QEMU_DIR/include/hw/timer/allwinner-v3s-hstimer.h"
install -Dm644 "$LICHEE_SRC/hw/timer/allwinner-v3s-hstimer.c" \
               "$QEMU_DIR/hw/timer/allwinner-v3s-hstimer.c"
install -Dm644 "$LICHEE_SRC/include/hw/input/virt-n64-pad.h" \
               "$QEMU_DIR/include/hw/input/virt-n64-pad.h"
install -Dm644 "$LICHEE_SRC/hw/input/virt-n64-pad.c" \
               "$QEMU_DIR/hw/input/virt-n64-pad.c"
install -Dm644 "$LICHEE_SRC/include/hw/dma/allwinner-v3s-dma.h" \
               "$QEMU_DIR/include/hw/dma/allwinner-v3s-dma.h"
install -Dm644 "$LICHEE_SRC/hw/dma/allwinner-v3s-dma.c" \
               "$QEMU_DIR/hw/dma/allwinner-v3s-dma.c"
install -Dm644 "$LICHEE_SRC/include/hw/audio/allwinner-v3s-codec.h" \
               "$QEMU_DIR/include/hw/audio/allwinner-v3s-codec.h"
install -Dm644 "$LICHEE_SRC/hw/audio/allwinner-v3s-codec.c" \
               "$QEMU_DIR/hw/audio/allwinner-v3s-codec.c"
install -Dm644 "$LICHEE_SRC/include/hw/misc/allwinner-v3s-cedar.h" \
               "$QEMU_DIR/include/hw/misc/allwinner-v3s-cedar.h"
install -Dm644 "$LICHEE_SRC/hw/misc/allwinner-v3s-cedar.c" \
               "$QEMU_DIR/hw/misc/allwinner-v3s-cedar.c"
install -Dm644 "$LICHEE_SRC/include/hw/audio/virt-ym3438.h" \
               "$QEMU_DIR/include/hw/audio/virt-ym3438.h"
install -Dm644 "$LICHEE_SRC/hw/audio/virt-ym3438.c" \
               "$QEMU_DIR/hw/audio/virt-ym3438.c"

# Vendored Nuked-OPN2 (libvgm flavor) — includes resolve via the original
# libvgm directory layout (../../stdtype.h, ../snddef.h).
install -Dm644 "$LICHEE_SRC/third_party/nuked_opn2/stdtype.h" \
               "$QEMU_DIR/hw/audio/nuked_opn2/stdtype.h"
install -Dm644 "$LICHEE_SRC/third_party/nuked_opn2/emu/snddef.h" \
               "$QEMU_DIR/hw/audio/nuked_opn2/emu/snddef.h"
install -Dm644 "$LICHEE_SRC/third_party/nuked_opn2/emu/cores/ym3438.c" \
               "$QEMU_DIR/hw/audio/nuked_opn2/emu/cores/ym3438.c"
install -Dm644 "$LICHEE_SRC/third_party/nuked_opn2/emu/cores/ym3438.h" \
               "$QEMU_DIR/hw/audio/nuked_opn2/emu/cores/ym3438.h"
install -Dm644 "$LICHEE_SRC/third_party/nuked_opn2/emu/cores/ym3438_int.h" \
               "$QEMU_DIR/hw/audio/nuked_opn2/emu/cores/ym3438_int.h"

echo "==> Wiring meson.build / Kconfig (idempotent)"

# hw/arm/meson.build — add our files behind CONFIG_LICHEE_ZERO
if ! grep -q "CONFIG_LICHEE_ZERO" hw/arm/meson.build; then
    sed -i "/CONFIG_ALLWINNER_H3.*orangepi.c/a arm_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s.c', 'lichee-zero.c'))" \
        hw/arm/meson.build
fi

# hw/display/meson.build — compile our display device when LICHEE_ZERO selected
if ! grep -q "allwinner-v3s-display" hw/display/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s-display.c'))" \
        >> hw/display/meson.build
fi

# hw/gpio/meson.build — PIO
if ! grep -q "allwinner-v3s-pio" hw/gpio/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s-pio.c'))" \
        >> hw/gpio/meson.build
fi

# hw/timer/meson.build — HSTimer
if ! grep -q "allwinner-v3s-hstimer" hw/timer/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s-hstimer.c'))" \
        >> hw/timer/meson.build
fi

# hw/input/meson.build — virtual N64 pad
if ! grep -q "virt-n64-pad" hw/input/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('virt-n64-pad.c'))" \
        >> hw/input/meson.build
fi

# hw/dma/meson.build — V3s DMA engine
if ! grep -q "allwinner-v3s-dma" hw/dma/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s-dma.c'))" \
        >> hw/dma/meson.build
fi

# hw/audio/meson.build — V3s codec
if ! grep -q "allwinner-v3s-codec" hw/audio/meson.build; then
    echo "system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: files('allwinner-v3s-codec.c'))" \
        >> hw/audio/meson.build
fi

# hw/audio/meson.build — virt-ym3438 + Nuked-OPN2.
# Idempotent BEGIN/END marker block: we strip and re-emit so include path
# / source list stay in sync with what the wrapper expects.
sed -i '/# >>> licheeEmu ym3438 BEGIN/,/# <<< licheeEmu ym3438 END/d' \
    hw/audio/meson.build
cat >> hw/audio/meson.build <<'EOF'
# >>> licheeEmu ym3438 BEGIN
ym3438_inc = include_directories('nuked_opn2/emu/cores')
system_ss.add(when: 'CONFIG_LICHEE_ZERO', if_true: [
    files('virt-ym3438.c',
          'nuked_opn2/emu/cores/ym3438.c'),
    declare_dependency(include_directories: ym3438_inc)])
# <<< licheeEmu ym3438 END
EOF

# hw/misc/meson.build — V3s Cedar VE. Wire libavcodec/libavutil if pkg-config
# can find them; otherwise the device compiles in stub mode (decode returns
# error and guest sees green-fail frames as before).
# Use sed-stripable BEGIN/END markers so we can re-emit the block on every
# run instead of relying on a one-shot idempotency guard.
sed -i '/# >>> licheeEmu cedar BEGIN/,/# <<< licheeEmu cedar END/d' \
    hw/misc/meson.build
cat >> hw/misc/meson.build <<'EOF'
# >>> licheeEmu cedar BEGIN
cedar_libav = []
libavcodec_dep = dependency('libavcodec', required: false)
libavutil_dep  = dependency('libavutil',  required: false)
if libavcodec_dep.found() and libavutil_dep.found()
  # declare_dependency lets us bundle the libs AND the -D macro into one
  # object that system_ss.add accepts as a single dependency entry.
  cedar_libav = declare_dependency(
    dependencies: [libavcodec_dep, libavutil_dep],
    compile_args: ['-DCONFIG_LIBAVCODEC=1'])
endif
system_ss.add(when: 'CONFIG_LICHEE_ZERO',
              if_true: [files('allwinner-v3s-cedar.c'), cedar_libav])
# <<< licheeEmu cedar END
EOF

# hw/arm/Kconfig — add CONFIG_LICHEE_ZERO (selects ALLWINNER_H3 family for CCU/etc)
if ! grep -q "config LICHEE_ZERO" hw/arm/Kconfig; then
    cat >> hw/arm/Kconfig <<'EOF'

config LICHEE_ZERO
    bool
    default y
    depends on TCG && ARM
    select ALLWINNER_A10_PIT
    select ALLWINNER_H3   # reuse CCU, sysctrl, cpucfg, SID device models
    select ARM_GIC
    select SERIAL_MM
    select UNIMP
EOF
fi

# Make the device default-on so -M lichee-zero works out of the box
if ! grep -q "CONFIG_LICHEE_ZERO" configs/devices/arm-softmmu/default.mak; then
    echo "# CONFIG_LICHEE_ZERO=n" >> configs/devices/arm-softmmu/default.mak
fi

echo "==> Configure (first time only)"
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    ../configure \
        --target-list=arm-softmmu \
        --enable-sdl \
        --disable-docs \
        --disable-werror \
        --disable-capstone
    cd "$QEMU_DIR"
fi

echo "==> Build qemu-system-arm"
ninja -C "$BUILD_DIR" qemu-system-arm

echo
echo "==> Done. Binary: $BUILD_DIR/qemu-system-arm"
"$BUILD_DIR/qemu-system-arm" -M help | grep -i lichee || true
