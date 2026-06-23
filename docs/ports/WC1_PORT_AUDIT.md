# Warcraft 1 / Stratagus port — subsystem audit

Status of every Stratagus + War1gus subsystem in the Jupiter SDK port,
auditing what's verbatim upstream vs. what's been modified, replaced,
stubbed, or skipped.

Status legend:

- **1:1** — upstream file in the build, no edits
- **port-mod** — upstream file with Jupiter-port edits (logic intact)
- **real-stub** — port-side functional replacement
- **empty-stub** — silent `{}` no-op
- **missing** — referenced but not provided
- **out-of-scope** — deliberately skipped (no replacement intended)

Tree references:

| Path | Role |
|---|---|
| `third_party/stratagus/src/` | Upstream Stratagus, filtered into the build by the Makefile's `WAR1_CPP_SRCS` exclusion list |
| `third_party/stratagus_port/` | Adapter shims (`war1_link_stubs.cpp`, `war1_bridge.cpp`, `war1_widgets.cpp`, `war1_widget_bindings.cpp`, `war1_tolua_bindings.cpp`, `war1_stub_bodies.cpp`) |
| `examples/war1/` | Boot stub + Lua scripts + asset catalogs |
| `lib/` | Jupiter SDK runtime (cedar H.264, audio mixer, FatFs glue, sdmmc, etc.) |
| `third_party/guisan_jupiter/` | guisan backend (Graphics / Image / Input) |
| `third_party/fatfs/` | Vendored ChaN FatFs R0.15a |

---

## Engine core (`stratagus/`)

| File | Status | Notes |
|---|---|---|
| stratagus.cpp | port-mod | `stratagusMain`, exit-via-setjmp recovery, UART diagnostics |
| mainloop.cpp | port-mod | Diagnostic prints, `Video.FillRectangle` moved out of `GameRunning` branch |
| title.cpp | port-mod | `MouseButtons = 0` scrub on splash exit |
| script.cpp | port-mod | `war1_asset_exists()` lookup before `fs::exists`; FatFs save-dir listing for `~save` |
| iolib.cpp | port-mod | Embedded-asset check + diagnostic `CanAccess` probes |
| script_player.cpp | port-mod | Minor; +32 LOC vs upstream |
| parameters.cpp, util.cpp, translate.cpp, construct.cpp, groups.cpp, player.cpp, selection.cpp | 1:1 | |
| replay.cpp, results.cpp | empty-stub | Headers compile, no logic — networking-tier features |
| `InitNetwork1` / `ExitNetwork1` / `InitOnlineService` / `DeInitOnlineService` | empty-stub | Networking out-of-scope |

## Game logic

| Subsystem | Status | Notes |
|---|---|---|
| `action/` (20 FSM files) | 1:1 | Every action FSM verbatim. `actions.cpp` 1:1 |
| `pathfinder/astar.cpp` | port-mod | Bare-metal layout / perf tweaks |
| `pathfinder/pathfinder.cpp`, `script_pathfinder.cpp` | 1:1 | |
| `missile/` (20 subclass files + missile.cpp + missileconfig.cpp) | 1:1 | |
| `script_missile.cpp` | 1:1 | |
| `spell/` (12 effect files + spells.cpp) | 1:1 | `spell_luacallback.cpp` dropped |
| `script_spell.cpp` | 1:1 | |
| `animation/` (22 action files) | 1:1 | `animation.cpp` port-mod (minor logic tweaks) |
| `unit/unit.cpp` | port-mod | Bare-metal heap/state tweaks (97K file) |
| `unit/unittype.cpp` | port-mod | Function-local-static for `UnitTypeMap` |
| `unit/unit_draw.cpp` | port-mod | pc8-blit routing, no SDL |
| `unit/upgrade.cpp` | port-mod | Function-local-static for `Depends` |
| `unit/depend.cpp` | port-mod | Same — bare-metal init order |
| `unit/unit_find.cpp`, `unit_manager.cpp`, `unit_save.cpp`, `unitptr.cpp`, `build.cpp` | 1:1 | |
| `script_unit.cpp`, `script_unittype.cpp` | 1:1 | |

## Map / FoW / AI

| Subsystem | Status | Notes |
|---|---|---|
| `map/` (map, mapfield, tileset, fov, fow, fow_utils, map_wall, map_radar, map_fog, map_draw) | 1:1 | |
| `map/minimap.cpp` | port-mod | pc8 sample bridge for terrain; SDL surface paths replaced |
| `map/script_map.cpp`, `script_tileset.cpp` | 1:1 | |
| `ai/` (ai, ai_building, ai_force, ai_magic, ai_plan, ai_resource, script_ai) | 1:1 | All ported (early plan to skip AI was reversed) |

## UI / HUD

| Subsystem | Status | Notes |
|---|---|---|
| `ui/` button_checks, contenttype, mainscr, mouse, ui, botpanel, icons, statusline, interface | 1:1 | |
| `ui/widgets.cpp` | filter-out | Replaced by `war1_widgets.cpp` (guisan + Jupiter backend, ScrollingWidget / CImageButton / MultiLineLabel) |

## Sound

| Subsystem | Status | Notes |
|---|---|---|
| `sound/sound.cpp` | port-mod | Function-local-static `SampleMap` / `ChannelMap` |
| `sound/music.cpp` | port-mod | `CallbackMusicTrigger` direct-call (no SDL_Event), 511-byte buffer rounding |
| `sound/sound_server.cpp`, `sound_id.cpp`, `unitsound.cpp`, `script_sound.cpp` | 1:1 | |
| `sound/ogg.cpp` | out-of-scope | No Theora / Vorbis on bare-metal |
| `Mix_*` (SDL_mixer API) | real-stub | `war1_bridge.cpp` shim — PCM catalog + music streaming |

## Video / Graphics

| Subsystem | Status | Notes |
|---|---|---|
| `video/color.cpp` | 1:1 | Filter-out removed in `6c7ce04`; `InterpolateColor` + `lerp` excised (port has alpha-preserving variant) |
| `video/cursor.cpp` | 1:1 | |
| `video/font.cpp`, `linedraw.cpp` | filter-out | Replaced by port glyph / line primitives in `war1_link_stubs.cpp` |
| `video/sdl.cpp` | missing | Replaced by `guisan_jupiter` backend + `g_war1_back_fb` FB ops |
| `video/png.cpp`, `mng.cpp` | missing | Runtime uses pc8; PNG only via offline conversion |
| `CGraphic` family (`Load`, `New`, `Resize`, `DrawFrameClip`) | real-stub | `war1_link_stubs.cpp` + `war1_bridge.cpp` pc8 path |
| `CGraphic::SetPaletteColor` | real-stub | Lazy-malloc'd writable palette + epoch (`cd148a7`) |
| `CPlayerColorGraphic::Clone` | empty-stub | Unused on bare-metal |
| `CVideo::FillRectangleClip`, `DrawHLineClip`, `DrawVLineClip`, `DrawRectangleClip`, `DrawTransRectangle` | real-stub | Direct framebuffer writes; `DrawTransRectangle` paints the minimap viewport box |
| `CVideo::DrawCircleClip`, `DrawEllipseClip`, `FillCircleClip`, `DrawTransCircleClip`, `FillTransRectangleClip`, `FillTransCircleClip` | empty-stub | Unused by WC1 |
| `CVideo::DrawLineClip`, `RealizeVideoMemory`, `ColorCycle`, `ToggleFullScreen`, `SaveMapPNG` | empty-stub | |
| `CVideo::ClearScreen` | empty-stub | War1 paints over each frame |

## Movies / Cinematics

| Subsystem | Status | Notes |
|---|---|---|
| `Movie::Load` | port-mod | Out-of-line in `war1_bridge.cpp` → `war1_movie_play` (cedar H.264 + FatFs vbin) |
| `Movie::IsPlaying` | port-mod | Always returns false — `Load` is synchronous |
| `Movie::getWidth/Height/free/getPixel/putPixel/convertToDisplayFormat` | empty-stub | Image interface unused |
| `PlayMovie` | port-mod | Routes to `war1_movie_play` |
| `lib/cedar.c` | port-mod | Buffers relocated to `0x43E00000-0x43F00000` VRAM gap (out of bss) |
| Asset pipeline (`scripts/wc1_video_pack.py`, `scripts/wc1_videos_convert.py`) | port-only | OGV → vbin (H.264 + s16le PCM) |

## Save / Load

| Subsystem | Status | Notes |
|---|---|---|
| `savegame.cpp`, `loadgame.cpp`, `replay.cpp`, `trigger.cpp` (in `game/`) | 1:1 | |
| FatFs (`third_party/fatfs/`) | port-only | Vendored R0.15a; `diskio.c` over `lib/sdmmc.c` |
| `_open / _read / _write / _lseek / _close / _fstat / _stat / _unlink / _mkdir` syscalls | real-stub | `/`-rooted paths route to FatFs `0:` volume |
| Save round-trip | works | Mid-mission save / restore confirmed on real HW |

## Lua / Scripting

| Subsystem | Status | Notes |
|---|---|---|
| `script_*.cpp` (every binding) | 1:1 except `script.cpp` + `script_player.cpp` | |
| Lua VM (vendored 5.4) | 1:1 | |
| War1gus Lua scripts (35 files in `examples/war1/scripts/`) | 1:1 except 4 | Modified: `stratagus.lua` (5.1 compat polyfills + cinematic title list), `guichan.lua` (Solo / Load / Replay / MultiPlayer / Editor button handling), `ui.lua` (`SCREEN_W=480` hardcode), `menus/campaign.lua` (briefing prints, anim half-pixel, action2 widget flow). Added: `wc1-config.lua` |
| `tolua` bindings (`war1_tolua_bindings.cpp` ~15K LOC) | port-only | All major classes bound. **Missing**: `DropDownWidget`, `LuaListModel`, editor bindings |

## Input

| Subsystem | Status | Notes |
|---|---|---|
| `WaitEventsOneFrame` | real-stub | N64 stick → cursor, BTN_B → LMB, BTN_A → RMB, BTN_START → in-game menu toggle |
| `ui/mouse.cpp` | 1:1 | Receives via `InputMouseMove / Press / Release` |
| Native SDL events | missing | No `SDL_PollEvent` chain |
| Hardware cursor | empty-stub | War1 uses software cursor |

## Networking / Multiplayer

| Subsystem | Status |
|---|---|
| `network/*.cpp`, `online_service.cpp`, `netconnect.cpp` | out-of-scope |
| `NetworkCommands`, `NetworkEvent`, `NetworkSendSelection`, `IsNetworkGame` | empty-stub |
| `CHost` / `CTCPSocket` / `CUDPSocket` | empty-stub |

## Editor

| Subsystem | Status |
|---|---|
| `editor/*.cpp` (editloop, editor, editor_brush, etc.) | out-of-scope |
| `EditorCclRegister` | empty-stub |
| `Editor.Running` | Lua-side `{Running = NotRunning}` shim |

## Adapter shims (port-only)

| File | LOC | Role |
|---|---|---|
| `war1_link_stubs.cpp` | ~2057 | Linker satisfaction + glyph blits + framebuffer ops |
| `war1_bridge.cpp` | ~1300 | pc8 registry + `Mix_*` shim + music streaming + cinematic player + `Movie::Load` |
| `war1_widgets.cpp` | ~700 | guisan widgets (Container / Image / Button / Scroll / MultiLineLabel) |
| `war1_widget_bindings.cpp` | ~1500 | Lua / tolua bindings for guisan |
| `war1_tolua_bindings.cpp` | ~15000 | Auto-generated tolua wrapper for Stratagus engine API |
| `war1_stub_bodies.cpp` | ~280 | Misc no-ops (zlib / bzlib / MNG) |
| `lib/audio.c`, `lib/cedar.c` | 1186 + 551 | PCM mixer + H.264 decoder |
| `third_party/guisan_jupiter/` | ~600 | guisan backend (Graphics / Image / Input) |

---

## Headline summary

- **1:1 with upstream**: every action FSM, missile, spell, animation script,
  AI, map / FoW / tileset, savegame, sound mixer, font_color / cursor, and
  most UI behavior code.
- **Surgical port-mods (~15 files)**: bare-metal init order (function-local
  statics in `unittype` / `depend` / `sound`), glyph clip in font, pc8
  routing in `unit_draw`, cedar buffer relocation, splash `MouseButtons`
  scrub, `Movie` out-of-line.
- **Functional replacements (real-stubs)**: graphics primitives, audio
  mixer / SDL_mixer, FatFs syscalls, guisan backend, `Movie::Load`.
- **Empty stubs (silent no-ops)**: all networking, editor, replay /
  results, hardware cursor, MNG, PNG output, color cycle, fullscreen
  toggle, screenshot.
- **Out of scope**: multiplayer, editor, online service, OGV / Theora /
  Vorbis decoding (replaced by vbin / H.264).
- **Lua deltas**: 4 of 35 scripts touched (`stratagus.lua` / `guichan.lua`
  / `ui.lua` / `menus/campaign.lua`); one new file (`wc1-config.lua`).

The port is essentially **upstream Stratagus + War1gus** with the desktop
I/O layer (SDL / SDL_mixer / SDL_image / Theora / networking / editor)
replaced by Jupiter-native equivalents (cedar / dlmalloc / FatFs /
N64-pad) and a thin tolua + guisan layer to keep the Lua scripts
unchanged. The only gameplay-relevant gap is networked multiplayer.
