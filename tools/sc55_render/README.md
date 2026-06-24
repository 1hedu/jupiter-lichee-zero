# sc55_render — Nuked-SC55 offline MIDI renderer

Used by the WC1 build pipeline to render Warcraft 1's MIDIs through
Roland SC-55 emulation into PCM WAVs that get packed into the SD
music image. See [`docs/ports/WC1_BUILD_GUIDE.md`](../../docs/ports/WC1_BUILD_GUIDE.md)
section 3d for the full pipeline.

## Status

**The build/ directory is not tracked in this repo.** You need to
compile `sc55_render` yourself before `render_all.sh` will work.

The renderer is a small driver that wraps the `third_party/nuked_sc55/`
emulator core, plus the Roland SC-55 control + PCM ROMs (which must
be supplied separately — they're copyrighted Roland firmware).

A buildable source tree was used to produce the binary that originally
sat in `build/`; that source isn't currently in the public SDK repo.
TODO for a future cleanup pass: vendor the driver source into
`tools/sc55_render/src/` and add a Makefile here that builds it
against `third_party/nuked_sc55/`.

## Workaround until the source lands

Until that's done, options to produce the rendered WAVs are:
- Build Nuked-SC55 on a Linux host yourself (the upstream
  `nuked-sc55-mk2` repo has a `sc55_render`-equivalent demo target),
  and adapt its output filenames to match what
  `scripts/sd_pack_music.py` expects (00.wav, 01.wav, …).
- Or render the MIDIs through any SC-55-compatible synth (real
  hardware, BASSMIDI with an SC-55 soundfont, Munt-with-CM32L if you
  don't mind the wrong synth model) and save to 11025 Hz mono 16-bit
  WAV named `00.wav` … `44.wav` in `tools/sc55_render/out/`.

Then `python scripts/sd_pack_music.py tools/sc55_render/out/ build/wc1_music.img`
picks up from there.
