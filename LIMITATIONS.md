# Limitations & caveats

What the Jupiter SDK does **not** do, hardware features that are
present-but-unwired, and known rough edges. Read this before committing
to a project.

## Verification status

Many SDK features are built, compile clean, and have demos — but not
all of them have been exercised end-to-end against the relevant
external hardware. Distinguishing what's been *run on silicon* from
what's *written and built but unverified*:

### Verified on real hardware

- LCD output (480×272 panel) — every visual example
- N64 controller bit-bang — works in input demos + all editors
- SDMMC0 — SD reads (boot), FatFs mounts, raw-block music pack
- CedarVE H.264 decode — `cedar_video` plays back from SD
- CedarVE H.264 encode — `cedar_snes` pipeline (encode → decode → tiles)
- YM3438 hardware FM chip — `opn2_hw_*` examples
- N64 Controller Pak read/write — `cpak_browser`, `wc1_save`
  (round-tripped against real OEM carts)
- Warcraft 1 port — campaign plays end-to-end on hardware
- Audio mixer + all bundled chip emulators (NES APU, GB APU, Munt,
  Nuked-SC55, Nuked-OPN2) — confirmed audible from the codec

### Built but NOT yet tested with the external hardware they target

- **MIDI I/O (`lib/midi.c`, UART1 driver)** — written, compiles, no
  byte has yet left or arrived on the wire. The DIN-5 breakout per
  `docs/MIDI_HW_GUIDE.md` hasn't been built. **All MIDI-related
  claims below are software-side verification only:**
  - FB-01 / MT-32 / WaveTerm editors build correct SysEx packets and
    log them to UART0 — confirmed by inspecting the hex log.
  - Whether the synth on the other end accepts those packets, and
    whether response dumps come back through the opto-isolator into
    our RX ring buffer, is **untested**.
  - The "drive external hardware synths" and "bidirectional dump
    request/receive" framing in the README is the design intent,
    not a measured result.
- **YM3438 fed by incoming MIDI** — the Genesis MIDI Module devlog
  describes a voice allocator mapping MIDI channels → YM3438 FM
  channels. Not implemented. The chip itself works (via VGM files);
  the MIDI-driven path doesn't exist yet.
- **`waveterm_vdp` on actual Sega Genesis hardware** — designed
  against the Genesis VDP feature set + tile constraints. Runs on
  the V3s via the Genesis renderer (`lib/genesis.c`). Has not been
  cross-compiled for 68k / run on a real Genesis. Prototype.

### Known-broken / stale

- `examples/cedar_decode_test` — fixed in this audit pass; was
  built against an older `cedar_h264_decode` signature and didn't
  link since the initial commit.

If you build on top of any "unverified" item, expect to spend time
on bring-up that the SDK examples can't shortcut.

## Hardware features not exposed by the SDK

The V3s ships with a number of peripherals that the SDK doesn't yet
provide a driver for. They're not impossible to use — you can poke
registers via `v3s.h` — but no SDK API exists.

| Peripheral | Status |
|---|---|
| **MIPI CSI** (camera) | Pin-reserved (PE20). No driver. |
| **NAND flash** | No driver. |
| **OTG USB** (device + host) | No driver. |
| **ADC** | Thermal sensor only; no general user channels. |
| **General-purpose I²C** | Bit-bang for Si5351 clock prog only. No bus driver. |
| **General-purpose SPI** | None. SD card uses the SDMMC controller, not SPI. |
| **Watchdog** | Registers known, no API. |
| **PWM** | None. |
| **Generalized GPIO port API** | None — controllers are hard-coded to specific pins. |

## Software / runtime limitations

- **Single core.** V3s is one Cortex-A7. No SMP. NEON helps a lot, but
  there is no second CPU to offload work to.
- **No filesystem on the boot partition.** The boot partition is a
  flat FAT volume that U-Boot reads `boot.scr` and `jupiter.bin`
  from — that's it. Application FS access uses the *second* FAT
  partition, mounted via FatFs as `0:`.
- **No dynamic loading.** No `dlopen`, no `.so` / `.dll`, no kernel
  modules. Everything is statically linked into `jupiter.bin`.
- **No threading.** Bare-metal single-thread + IRQ handlers. No
  pthreads, no scheduler.
- **No networking.** No TCP/IP stack, no Wi-Fi, no Ethernet driver.
  V3s has no MAC anyway.
- **No exception handling beyond what bare-metal allows.** C++ code is
  built `-fno-exceptions -fno-rtti`. `throw` / `try` won't unwind.
  The Stratagus port wraps `__cxa_throw` to log + exit instead.
- **No `dlmalloc::trim`.** `MORECORE_CANNOT_TRIM = 1`; once memory is
  given to the heap, it stays in the heap. The arena sizes are
  conservative (1 MB / 1 MB for non-WC1 builds, 24 MB / 13 MB for WC1).
- **24-bit DAC mode is partial.** `audio_init_24bit()` initializes the
  codec for 24-bit but the mixer pipeline is still 16-bit; samples are
  zero-padded to 24-bit on the way out.

## Build / toolchain caveats

- **arm-none-eabi-gcc 14.2** is the tested toolchain. Older versions
  (11.x) work for plain C examples but the Stratagus port relies on
  C++17 / C++20 library features.
- **WSL on Windows** is the supported development platform (the `wsl
  python3 …` invocations in the asset-pipeline scripts assume this).
  Native Linux works; macOS is untested.
- **`-DWAR1_BUILD=1`** is required for any build that links the
  Stratagus / Warcraft 1 port — that flag bumps `dlmalloc`'s static
  arenas from 1 MB / 1 MB to 24 MB / 13 MB. The Makefile sets it
  automatically when the `GAME` path contains `war1`. Your own ports
  with similar memory needs should set it too.
- **CODE region is 40 MB.** Builds with very large bss (full Stratagus,
  multiple synth emulators, etc.) can run out. Move large zero-init
  arrays into `__attribute__((section(".bss_low")))` to put them in the
  separate 14 MB BSS_LOW region.

## SDK API limitations

- **No game-loop framework.** The SDK is library-of-functions, not a
  framework — there's a `template/game.c` skeleton but no guidance
  beyond it. You write your own `main()` and frame loop.
- **No asset pipeline tooling beyond Warcraft 1.** PNG → pc8, WAV →
  PCM, OGV → vbin scripts exist (under `scripts/`) but they're scoped
  to the WC1 port. Reuse / generalization is on you.
- **No diagnostics framework.** UART `printf`-style debug only. No GDB
  stub, no register-dump utilities, no automated post-mortem.
- **Audio: single sample-rate per session.** `audio_set_rate()` works
  but stops the DMA, requires mix_buf re-prefill and `audio_start()`
  again. Don't call it mid-cinematic.
- **Hard ceiling: 16 MB boot partition.** `jupiter.bin` plus any
  embedded assets must fit. WC1's main binary is ~14 MB; assets stream
  from the second FAT partition (and from raw blocks past it for the
  music pack).

## Display caveats

- **Fixed 480×272 panel.** The TCON config is hardcoded for the
  reference panel. Other panels work but you'll need to adjust the
  pixel-clock divider, hsync / vsync timing, and `LCD_W` / `LCD_H` in
  `include/v3s.h`.
- **No hardware tile engine.** DE2 supports tiled framebuffer modes
  but the SDK uses linear ARGB only. The "tile" renderers
  (`lib/tiles.c`) are software composition.
- **No hardware scaler.** V3s VSU was disabled in silicon (the
  `scaler_probe` example confirms this). Software scaling only.
- **No alpha output to HDMI.** V3s has no HDMI silicon — the LCD
  parallel RGB interface is the only output.

## Audio caveats

- **No ADC.** Codec is DAC-only. No mic / line-in.
- **DMA underrun on long click handlers.** The mix-buf top-up is now
  ISR-driven (`audio_dma_isr` calls `audio_mix` after each refill), so
  this is largely fixed. If you write a tight CPU-burning loop with
  IRQs masked, you'll still hear it skip.
- **MIDI rendering is CPU-heavy.** Both vendored synths (Munt for
  MT-32, Nuked-SC55 for SC-55) are accurate but expensive. They run
  comfortably on the 1.2 GHz Cortex-A7 at the audio rates they target,
  but leave little headroom for other CPU work in the same example —
  most synth examples stop short of also running a game-ish loop.

## Storage caveats

- **SDMMC0 only.** SDMMC1 / SDMMC2 controllers are not driven.
- **PIO read/write only — no DMA.** SD throughput is CPU-bound (~5–10
  MB/s sustained). Acceptable for music / video streaming but slow
  for bulk asset loads.
- **No write-leveling, no journaling, no power-loss protection.** A
  hard reset during an SD write may corrupt the FAT. Save game files
  are atomic-rename-via-tempfile in the WC1 port; use the same pattern
  for your own writes.

## Known rough edges in shipped examples

- The 5+ MIDI / synth examples each hand-roll their audio init; could
  use the new `audio_quickstart()` helper but haven't been migrated.
- Some `mt32_*` examples depend on heap behavior that pre-dated dlmalloc
  (`heap_reset`, `heap_rewind`); the SDK now stubs those as no-ops, so
  per-track memory is leaked instead of recycled. Reboot for next track.
- `scaler_probe` is for SDK developers, not users — confirms VSU is
  not in silicon.
- **WC1 long-session degradation: ARGB widget leak (~1.2 MB per
  distinct briefing).** The Stratagus port routes briefing artwork
  through `gcn::ImageWidget` / `JupiterImage`; widgets are never
  destructed (tolua `pushusertype` without `takeownership` + no C++
  delete path), so each distinct mission's briefing leaks its bg
  buffer + map buffer + animation strips, totaling ~1.2 MB per
  briefing-with-video. With ~10.5 MB headroom over the ~26 MB baseline,
  arena exhaustion → `morecore` returns -1 → unguarded `new` failure
  → `_exit`'s `for(;;);` hang after ~8 distinct briefings.

  Mitigations shipped (commit on top of `0c94a25`):
  - **B1** — `CGraphic::New` dedups by `(path, w, h)` via `std::map`
    cache (was: fresh CGraphic per call → `g_pc8_registry` stale
    pointers + 2048-slot overflow → "wrong-sprite blits" and
    "all-black terrain" symptoms).
  - **C2** — `arg_to_image` caches `JupiterImage` by `(CGraphic*, outW,
    outH)`. Same image across many briefings now shares one ARGB
    buffer instead of N — eliminates the *replay* leak.
  - **A1** — `LuaActionListener` pushed without `takeownership` so
    GC can't free it while widgets still hold raw pointers; closes
    the listener UAF that earlier registry-clear changes had opened.
  - **D** — Briefing's `listener` local-scoped instead of leaking to
    `_G.listener` and getting overwritten by the next briefing.

  Not yet fixed:
  - **C1** — atomic widget-subtree teardown for dead menus. The
    obvious implementation hangs the game on next cursor move because
    `gcn::FocusHandler` holds 7 widget pointers (mFocused / mDragged
    / mLastWidgetWithMouse / mLastWidgetWithModalFocus /
    mLastWidgetWithModalMouseInputFocus / mLastWidgetPressed / its
    `mWidgets` list) that go dangling, plus `ContainerListener` and
    death-listener registrations. The sweep function (and its call
    site in `mainloop.cpp`) is compiled in but not invoked. Re-enable
    after the cross-reference cleanup is wired (see the WC1
    `menu_lifetime_diagnosis.md` doc).

  Net: a single campaign playthrough (~24 distinct briefings) will
  still eventually OOM-hang. Repeated mission loads of the *same*
  mission no longer leak. Power-cycle between long sessions if you
  intend to grind many distinct briefings back-to-back.

## Out of scope (deliberate)

These won't be added to the core SDK:

- **Multiplayer networking.** No NIC and no TCP/IP stack on bare-metal
  single-thread. The Stratagus port disables Wargus's network code.
- **GUI toolkit beyond what the WC1 port pulls in (guisan).** The SDK
  is a library, not a framework.
- **Live-update / OTA mechanism.** Re-flash the SD card.
- **Battery / power-management API.** No PMIC driver beyond the basics
  needed to bring up the CPU.
