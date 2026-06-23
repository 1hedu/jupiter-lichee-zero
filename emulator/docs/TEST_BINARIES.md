# Test binaries

Pre-built bare-metal payloads shipped under `docs/` for smoke-testing
the emulator. All are flat binaries built with the
[Jupiter SDK](https://github.com/) and meant to be loaded with
`-kernel <file>.bin` at `0x41000000`.

`docs/*.bin` is in `.gitignore` — these binaries are committed for
release-time reproducibility but you can regenerate any of them from
Jupiter sources using `make GAME=<path>`.

## Running them

```bash
./scripts/run.sh -g docs/<name>.bin
```

For binaries that need an SD card (`sdmmc.bin`, `sdmmc_music.bin`,
`war1.bin`, `cpak_browser.bin`), make sure `docs/sd.img` exists first
— see [`RUN.md`](RUN.md#sd-images).

## Index

| Binary             | Size | Exercises | Expected output |
|--------------------|------|-----------|-----------------|
| `menu.bin`         | 12 MB | Display + N64 pad + the union of every example below | SDL window: scrollable example launcher. ↑/↓ keys to navigate, Z to launch. |
| `war1.bin`         | 15 MB | Everything: display, audio (codec + Nuked-OPN2 path via Stratagus port), SDMMC FAT16, Cedar VE H.264, N64 pad | Stratagus engine boots, loads Lua, plays intro cinematic, reaches main menu. |
| `jupiter.bin`      | 1.0 MB | The Jupiter SDK self-test entry point | Quick smoke test — UART prints, display init, no input loop. |
| `colorbars.bin`    | 196 KB | Display pipeline (DE2 + Mixer0 + TCON0) | SMPTE-style colorbar pattern in the SDL window. No input. |
| `hstimer_raster.bin` | 192 KB | HSTimer + display (raster-effect timing using HSTimer ch0) | Animated horizontal-scanline color washes. |
| `n64_diag.bin`     | 192 KB | virt-n64-pad protocol decode | Continuously prints `poll=N connected=… buttons=…` over UART. Press D-pad/buttons; bits should toggle. Source in [`n64_diag_main.c`](n64_diag_main.c). |
| `cpak_browser.bin` | 230 KB | virt-n64-pad Controller Pak (cmds 0x02/0x03) | Hex viewer over the 32 KB pak, scrollable with the D-pad. |
| `opn2_rt.bin`      | 674 KB | virt-ym3438 (real-time FM synth driven from input) | Press buttons, hear FM tones. Requires `-audio pa,…`. |
| `sdmmc.bin`        | 207 KB | SDMMC0 + FAT16 mount | Lists root directory of `0:/` over UART. Requires populated SD image. |
| `sdmmc_music.bin`  | 213 KB | SDMMC0 raw block reads of music pack at LBA 524288 | Streams WC1 music pack header, prints track count. Requires `make-music-sd.sh` SD image. |
| `n64_diag_main.c`  | 2.7 KB | Source for `n64_diag.bin` | Not a binary — kept here as an easy-to-modify reproducer for input bugs. |

## What each binary tells you when it fails

If a binary doesn't behave as expected, the failure mode usually points
at a specific subsystem:

| Symptom                                                  | Likely culprit |
|----------------------------------------------------------|----------------|
| Black SDL window for `colorbars.bin`                     | Display pipeline didn't enable (`TCON_GCTL` / `TCON0_CTL`). Check serial for `[video]` traces. |
| `menu.bin` boots but D-pad doesn't move cursor           | `-icount` missing → Joybus bit decode collapses. Use `scripts/run.sh` (it adds icount). |
| `cpak_browser.bin` loops with `[cpak] CRC error`         | Pad TX timing off. Most likely cause: virt-n64-pad's per-bit timer is firing too far apart in virtual time. |
| `sdmmc.bin` prints `[fatfs] mount failed rc=13`          | SD image partition layout wrong. Re-run `scripts/make-sd-image.sh`. |
| `war1.bin` cinematic plays audio but the SDL stays still | Cedar VE was built without libavcodec (stub mode). Install `libavcodec-dev` and rebuild. |
| `opn2_rt.bin` is silent                                  | No host audio backend. Use `-audio pa,…` if PulseAudio is up; otherwise expected. |
| Any binary aborts with `error: Could not load -kernel`   | Wrong path or zero-size file. Check `ls -la docs/<name>.bin`. |
