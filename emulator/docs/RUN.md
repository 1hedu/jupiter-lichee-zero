# Running payloads

Once you have `qemu-system-arm` built (see [`BUILD.md`](BUILD.md)),
running a Jupiter bare-metal binary is one shell line. This doc covers
the common knobs.

## The convenience wrapper

```bash
./scripts/run.sh [-g] [path/to/payload.bin]
```

- `-g` → graphical SDL window. Default is `-display none` (serial only).
- The default payload path is
  `/mnt/c/Users/darek/jupiter/combined/build/jupiter.bin` — adjust to
  taste, or pass an explicit path.
- Auto-detects PulseAudio (`pactl info`); falls back to `-audio none`
  if PA isn't reachable (headless SSH, plain WSL without WSLg).
- Auto-attaches `docs/sd.img` as `if=sd,format=raw` if present.

The wrapper always passes `-icount shift=auto,align=off,sleep=on` —
this is **required** for the N64 controller protocol to decode bits
correctly (4 µs cells). See [`ARCHITECTURE.md`](ARCHITECTURE.md#why-icount).

## Direct invocation

If you want to bypass the wrapper:

```bash
~/licheeEmu-build/qemu/build/qemu-system-arm \
    -M lichee-zero \
    -m 64 \
    -kernel docs/menu.bin \
    -icount shift=auto,align=off,sleep=on \
    -audio pa,model=allwinner-v3s-codec \
    -drive if=sd,file=docs/sd.img,format=raw \
    -serial stdio \
    -display sdl
```

The `-m 64` is required — the lichee-zero machine init aborts if RAM is
not exactly 64 MiB:

```c
if (machine->ram_size != 64 * MiB) {
    error_report("lichee-zero requires exactly 64 MiB of RAM ...");
    exit(1);
}
```

## Headless mode

For CI or remote-shell debugging:

```bash
~/licheeEmu-build/qemu/build/qemu-system-arm \
    -M lichee-zero -m 64 \
    -kernel docs/menu.bin \
    -icount shift=auto,align=off,sleep=on \
    -audio none,model=allwinner-v3s-codec \
    -display none -serial stdio
```

You'll get UART0 output on stdout. Most Jupiter examples print
diagnostics there (`[ym3438] ready`, `[fatfs] mount ok`, etc.) so this
is plenty for most debugging.

## Audio backends

The codec device (`allwinner-v3s-codec`) tries to open a host audio
output stream during realize. The backend is whatever `-audio` selects:

| `-audio …` | When to use |
|------------|-------------|
| `pa,model=allwinner-v3s-codec` | WSLg, Linux desktop with PulseAudio/PipeWire |
| `none,model=allwinner-v3s-codec` | Headless, CI, or audio-broken environment |

The same applies to the YM3438 path (`virt-ym3438`). Both devices
**tolerate** a NULL voice (no backend / open failed) — the rest of the
machine still boots, and writes to the chips are silently dropped on
the floor.

## SD images

A 1 GiB sparse SD image with the right partition layout is provided as
`docs/sd.img` (gitignored — build it yourself). To create one:

```bash
./scripts/make-sd-image.sh        # blank
./scripts/populate-fat.sh         # copy /videos and /intros from Jupiter
# OR for the war1 music pack:
./scripts/make-music-sd.sh
```

Layout:

| LBA range          | Type    | Purpose                                |
|--------------------|---------|----------------------------------------|
| 2048..4095         | Linux   | 1 MiB dummy (SPL placeholder, unused)  |
| 4096..524287       | FAT16   | ~254 MiB — what `0:/` mounts to        |
| 524288+            | (free)  | WC1 music pack splice                  |

**Why partition 2 and not 1?** Jupiter's `third_party/fatfs/diskio.c`
maps drive `0:/` to physical partition #2. Real V3s boards put SPL +
U-Boot on partition #1, FAT16 on partition #2; we mirror that.

## Keyboard mapping

The `virt-n64-pad` translates host keys to N64 button bits. The current
map (from `src/hw/input/virt-n64-pad.c`):

| Host key      | N64 button      |
|---------------|-----------------|
| ↑ ↓ ← →       | D-pad           |
| Z             | A               |
| X             | B               |
| Shift (left)  | Z trigger       |
| Enter         | Start           |
| Q / W         | L / R           |
| I / K / J / L | C-Up/Down/Left/Right |

There's no analog stick mapping yet — the BTN response packs zero in
both stick bytes.

## What each demo binary does

See [`TEST_BINARIES.md`](TEST_BINARIES.md).

## Troubleshooting

### Black SDL window, no menu

- Check serial output. If it's blank, the kernel didn't start — wrong
  load address or corrupt `.bin`.
- If serial shows `[main]` traces but the SDL stays black, the display
  pipeline didn't get enabled. The display device gates scan-out on
  TCON_GCTL bit 31 + TCON0_CTL bit 31. If the guest hasn't flipped both
  yet, you see the framebuffer init color.

### Menu shows but D-pad doesn't move the cursor

- You're missing `-icount`. Without it, Joybus bit decode collapses to
  always-`0` and the pad reports no buttons.
- Confirm with `[n64] poll: 00000000` repeating in serial output — that
  means the protocol decoded but the pad reported no buttons. If it
  prints nothing, the protocol decode itself is failing.

### Cinematics start but the SDL stays on the previous frame

Cedar VE was built without libavcodec (stub mode). Install
`libavcodec-dev libavutil-dev` and rebuild.

### Audio is very stuttery

- WSLg PulseAudio over Hyper-V can be choppy under high CPU load.
  Try `-audio pa,server=...` to point at a different daemon, or fall
  back to `-audio none` if you only care about visual demos.
- The codec sample-rate is decoded from `PLL_AUDIO_CTRL`; if the guest
  reprograms PLL_AUDIO mid-stream we don't reopen the host voice.
  Restart the payload.

### Long boot logs end without an error

That's the end of the log. The emulator hasn't crashed; it's running
an idle loop or polling input. If you expect more output, send input
(keyboard for menu, RESET for some examples).
