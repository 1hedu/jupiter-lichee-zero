#!/usr/bin/env python3
"""
war1_assets.py — batch-convert War1gus PNG graphics to .pc8 blobs and emit
the Stratagus-path → blob catalog.

Walks data.War1gus/graphics/ and converts every .png that's in one of the
categories listed below. Outputs mirror the source tree under
examples/war1/assets/. Generates examples/war1/war1_asset_catalog.h which
maps the Stratagus-side path (e.g. "tilesets/forest/terrain.png") to the
objcopy-generated blob symbols, so CGraphic::New can resolve image paths
at runtime.

Usage:
    scripts/war1_assets.py /path/to/data.War1gus/graphics/
"""
import sys, re, pathlib, subprocess

SCRIPT_DIR = pathlib.Path(__file__).parent
REPO_ROOT = SCRIPT_DIR.parent
CONVERTER = SCRIPT_DIR / "war1_convert.py"
ASSETS_OUT = REPO_ROOT / "examples" / "war1" / "assets"
CATALOG_H = REPO_ROOT / "examples" / "war1" / "war1_asset_catalog.h"

# Categories to include. Matches the Stratagus-side paths in War1gus's
# DefineTileset/DefineUnitType/DefineMissileType image fields.
CATEGORIES = [
    "tilesets/forest",      # terrain, fog, portrait_icons, human/, orc/, neutral/
    "tilesets/swamp",
    "tilesets/dungeon",
    "human/units",
    "orc/units",
    "missiles",
    "neutral",              # shared neutral things (gold mine etc. may live here)
    "ui",                   # HUD panels, buttons, icons, cursors, percent_complete
]

# Skip large intermission graphics: victory/defeat scenes + hot_keys etc.
# stay out (20-65 KB each, unused mid-game, would push bss past the 40 MB
# CODE region). ui/title_screen.png + ui/logo.png ARE in: the canonical
# boot splash (ShowTitleScreens) and the title menu background
# (ImageWidget(bckground) in guichan.lua) both reference them.
EXCLUDE = {
    "ui/hot_keys.png",
    "ui/top_of_title_screen.png", "ui/bottom_of_title_screen.png",
    "ui/victory_scene.png", "ui/defeat_scene.png",
    "ui/victory_text.png", "ui/defeat_text.png",
    # Stray double-extension file from an upstream batch tool — not a
    # real PNG (429 bytes, no IHDR/PLTE) and never referenced by any
    # Lua. Excluding so the catalog doesn't pick up a stale .pc8 on
    # mtime-cache hits.
    "ui/palette_batch_titlescreen.png.png",
}

def sanitize_sym_path(p: str) -> str:
    # objcopy generates symbol names by replacing non-alnum with '_' in the
    # path passed on its cmd line. We objcopy from repo-relative paths like
    # "examples/war1/assets/tilesets/forest/terrain.pc8". Build expected symbol.
    return "_binary_" + re.sub(r"[^A-Za-z0-9]", "_", p)


def main():
    if len(sys.argv) != 2:
        print("usage: war1_assets.py /path/to/data.War1gus/graphics/", file=sys.stderr)
        sys.exit(1)
    graphics_root = pathlib.Path(sys.argv[1])
    if not graphics_root.is_dir():
        print(f"not a directory: {graphics_root}", file=sys.stderr)
        sys.exit(1)

    converted = []  # list of (stratagus_path, repo_pc8_path)
    total_in = 0
    total_out = 0
    for cat in CATEGORIES:
        base = graphics_root / cat
        if not base.is_dir():
            print(f"[skip] missing {base}", file=sys.stderr)
            continue
        for png in sorted(base.rglob("*.png")):
            rel = png.relative_to(graphics_root).as_posix()       # e.g. "tilesets/forest/terrain.png"
            if rel in EXCLUDE:
                continue
            out_rel = rel[:-len(".png")] + ".pc8"                 # "tilesets/forest/terrain.pc8"
            out_path = ASSETS_OUT / out_rel
            out_path.parent.mkdir(parents=True, exist_ok=True)
            # Skip rebuild if .pc8 is newer than .png (handy iteration)
            if out_path.exists() and out_path.stat().st_mtime > png.stat().st_mtime:
                pass
            else:
                r = subprocess.run([sys.executable, str(CONVERTER), str(png), str(out_path)],
                                   capture_output=True, text=True)
                if r.returncode != 0:
                    print(f"[fail] {rel}: {r.stderr.strip()}", file=sys.stderr)
                    continue
            total_in += png.stat().st_size
            total_out += out_path.stat().st_size
            # Repo-relative path used by objcopy symbol mangling
            repo_rel = out_path.relative_to(REPO_ROOT).as_posix()
            converted.append((rel, repo_rel))

    # Root-level graphics/ PNGs (briefing animation strips 425-431, orc
    # victory anim 460, per-mission map previews hmap01..12 / omap01..12).
    # These live directly at data.War1gus/graphics/ — no subdirectory —
    # and the upstream Briefing/CreateEndingStep callers reference them
    # as "graphics/<file>.png" or "../graphics/<file>.png". Catalog key
    # follows the no-`../` form; CGraphic resolves either by stripping
    # leading "../".
    for png in sorted(graphics_root.glob("*.png")):
        strat_path = "graphics/" + png.name              # what Lua sees
        out_rel = "graphics/" + png.stem + ".pc8"
        out_path = ASSETS_OUT / out_rel
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if out_path.exists() and out_path.stat().st_mtime > png.stat().st_mtime:
            pass
        else:
            r = subprocess.run([sys.executable, str(CONVERTER), str(png), str(out_path)],
                               capture_output=True, text=True)
            if r.returncode != 0:
                print(f"[fail] {strat_path}: {r.stderr.strip()}", file=sys.stderr)
                continue
        total_in += png.stat().st_size
        total_out += out_path.stat().st_size
        repo_rel = out_path.relative_to(REPO_ROOT).as_posix()
        converted.append((strat_path, repo_rel))

    # contrib/graphics/ — war1gus's enhancement assets that the Lua
    # references via paths like "contrib/graphics/ui/icon-food.png" or
    # "contrib/graphics/missiles/blood-pool.png". Same recursive walk
    # pattern as the categories above. Paths preserve the leading
    # "contrib/graphics/" so they match what scripts/ui.lua + the
    # DefineMissileType/DefineUnitType calls ask for at runtime.
    war1gus_root_pre = graphics_root.parent
    contrib_gfx = war1gus_root_pre / "contrib" / "graphics"
    if contrib_gfx.is_dir():
        for png in sorted(contrib_gfx.rglob("*.png")):
            rel = png.relative_to(war1gus_root_pre).as_posix()  # e.g. "contrib/graphics/ui/icon-food.png"
            if rel in EXCLUDE:
                continue
            out_rel = rel[:-len(".png")] + ".pc8"
            out_path = ASSETS_OUT / out_rel
            out_path.parent.mkdir(parents=True, exist_ok=True)
            if out_path.exists() and out_path.stat().st_mtime > png.stat().st_mtime:
                pass
            else:
                r = subprocess.run([sys.executable, str(CONVERTER), str(png), str(out_path)],
                                   capture_output=True, text=True)
                if r.returncode != 0:
                    print(f"[fail] {rel}: {r.stderr.strip()}", file=sys.stderr)
                    continue
            total_in += png.stat().st_size
            total_out += out_path.stat().st_size
            repo_rel = out_path.relative_to(REPO_ROOT).as_posix()
            converted.append((rel, repo_rel))

    # Fonts live at data.War1gus/contrib/fonts/, NOT under graphics/. They
    # need to be in the catalog because fonts.lua does CGraphic:New(
    # "contrib/fonts/<name>.png", w, h). Without entries here, CFont's G
    # has no pc8 backing and CFont::drawString renders nothing.
    war1gus_root = graphics_root.parent  # graphics/ is under data.War1gus/
    fonts_src = war1gus_root / "contrib" / "fonts"
    if fonts_src.is_dir():
        for png in sorted(fonts_src.glob("*.png")):
            strat_path = ("contrib/fonts/" + png.name)              # what Lua sees
            out_rel = "contrib/fonts/" + png.stem + ".pc8"
            out_path = ASSETS_OUT / out_rel
            out_path.parent.mkdir(parents=True, exist_ok=True)
            if out_path.exists() and out_path.stat().st_mtime > png.stat().st_mtime:
                pass
            else:
                r = subprocess.run([sys.executable, str(CONVERTER), str(png), str(out_path)],
                                   capture_output=True, text=True)
                if r.returncode != 0:
                    print(f"[fail] {strat_path}: {r.stderr.strip()}", file=sys.stderr)
                    continue
            total_in += png.stat().st_size
            total_out += out_path.stat().st_size
            repo_rel = out_path.relative_to(REPO_ROOT).as_posix()
            converted.append((strat_path, repo_rel))

    # Emit catalog header
    lines = [
        "/* AUTO-GENERATED by scripts/war1_assets.py — do not edit. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]
    # extern decls
    for _, repo_rel in converted:
        s = sanitize_sym_path(repo_rel)
        lines.append(f"extern const uint8_t {s}_start[];")
        lines.append(f"extern const uint8_t {s}_end[];")
    lines += [
        "",
        "typedef struct {",
        "    const char *path;           /* Stratagus-side path (as seen in Lua) */",
        "    const uint8_t *data;",
        "    const uint8_t *data_end;",
        "} war1_asset_entry_t;",
        "",
        f"#define WAR1_ASSET_COUNT {len(converted)}",
        "static const war1_asset_entry_t WAR1_ASSETS[] = {",
    ]
    for strat_path, repo_rel in converted:
        s = sanitize_sym_path(repo_rel)
        lines.append(f'    {{ "{strat_path}", {s}_start, {s}_end }},')
    lines += [
        "};",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
    ]
    CATALOG_H.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {CATALOG_H} with {len(converted)} entries")
    print(f"total PNG input : {total_in/1024:.1f} KB")
    print(f"total PC8 output: {total_out/1024:.1f} KB")


if __name__ == "__main__":
    main()
