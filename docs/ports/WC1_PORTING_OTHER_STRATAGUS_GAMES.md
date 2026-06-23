# Porting other Stratagus-engine games to Jupiter

How close the Jupiter port of Stratagus is to running another game built
on the same engine. Companion to `WC1_PORT_AUDIT.md`.

## Stratagus-game catalog

AoE is **not** a Stratagus game (it's on Microsoft's Genie engine). The
real Stratagus catalog is small:

| Game | Status upstream | Notes |
|---|---|---|
| **Wargus** (Warcraft 2 Tides of Darkness + Beyond the Dark Portal) | mature, actively maintained | Stratagus was originally built FOR Wargus |
| **War1gus** (Warcraft 1) | mature | What this Jupiter port targets |
| **Stargus** (StarCraft 1) | partial / unmaintained | Engine fights StarCraft's pixel-aspect math |
| **Doomgus**, **Aleona's Tales**, **Bos Wars**, **3stages** | hobby-tier | Mostly demo content |

Wargus is the realistic next target.

## What's locked to WC1 in this port

These would need to be redone per game:

- `examples/war1/scripts/*` — direct copy of war1gus's Lua tree (35
  files). A different game has its own scripts/ tree; replace
  wholesale — same loader.
- `examples/war1/war1_asset_catalog.h` + `war1_script_catalog.h` —
  auto-generated registries naming `/videos/hmap01.vbin` etc.
- `examples/war1/assets/` — pc8 graphics rebuilt from the source PNG
  tree.
- `scripts/war1_*` Python pipeline (asset converters, sound packer,
  video pack). Mostly path / name scoped.
- `war1_bridge.cpp`'s in-game-menu paths, briefing flow tweaks,
  race-prefix HUD wiring (orc / human panels).

## What's already generic

These are engine-level — already work for any Stratagus game:

- `third_party/stratagus/src/*` — entire engine. War1gus and Wargus run
  the same Stratagus binary on desktop, so they'd run the same Jupiter
  binary too.
- `third_party/stratagus_port/war1_link_stubs.cpp` — except the file
  name, every adapter (font glyphs, glyph clip, framebuffer ops,
  FatFs syscalls, music streaming, cinematics) is engine-level, not
  game-level.
- `third_party/stratagus_port/war1_widgets.cpp` /
  `war1_widget_bindings.cpp` — guisan + tolua bindings, generic.
- `third_party/stratagus_port/war1_tolua_bindings.cpp` — generated
  from upstream `stratagus.pkg`, identical for any Stratagus game.
- `lib/audio.c`, `lib/cedar.c`, `lib/sdmmc.c`, FatFs, dlmalloc —
  Jupiter SDK runtime.
- `Movie::Load` → `war1_movie_play` → vbin / cedar — works for any
  source video.
- Boot / init in `examples/war1/main.c` — name aside, the sequence is
  generic.

## Estimated cost per target game

### Wargus (Warcraft 2)

Closest target. Roughly 1–2 weeks of focused work.

1. **Asset conversion (~1 week)**: run a `war2_assets.py` pass over
   Wargus's `data.Wargus/graphics/`. WC2 is bigger — 5 races × more
   units × more buildings × more frames. Expect 50–100 MB of pc8
   (vs WC1's ~15 MB). Won't fit in the 16 MB boot partition; pc8s go
   on the FatFs partition like we already do for music / intros /
   videos.
2. **Lua bundle**: copy `wargus/scripts/` into `examples/war2/scripts/`,
   regenerate `war2_script_catalog.h`.
3. **Cinematics**: 30+ Wargus OGVs → vbin via existing pipeline.
4. **Music**: Wargus uses MIDI. The Jupiter SDK already has
   Munt / MT-32 and Nuked SC-55 backends working; needs wiring up to
   Stratagus's `PlayMusic`. War1gus only used PCM tracks.
5. **HUD assets**: Wargus's panel art is bigger and uses 5 races worth
   of icons; the prescaling step in `examples/war1/assets/ui/` would
   be redone.
6. **Engine tweaks**: probably <20 LOC of `script_*` adjustments.

### Stargus (StarCraft 1)

Higher cost. Stargus uses different aspect logic (320×200 stretched
doesn't map well to 480×272), has parasitic units / cloak FX that
exercise corners of the engine we haven't tested, and the asset
extraction pipeline (.mpq) is different. Plausible but messier.

## Hard limits

- **64 MB DRAM total.** Current war1 build is ~55 MB (mostly bss).
  Wargus's bigger `UnitTypeVar` / `Player` tables might push past
  64 MB; would need to thin out or shift more buffers to FAT-streamed.
- **16 MB boot partition** for `jupiter.bin`. Code+data is currently
  ~16 MB; bigger ports need most assets on the FAT partition (we
  already do that for music / intros / videos).
- **No multiplayer.** Wargus's campaign is single-player so this
  doesn't matter, but Battle.net-style skirmish would.

## Headline

The engine is **~95% game-agnostic**. Bringing up Wargus is mostly an
asset / Lua / build-pipeline job. The actual C++ code changes would be
small — wiring MIDI music and adding a Wargus-specific boot example.
