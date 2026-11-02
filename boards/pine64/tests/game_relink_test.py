#!/usr/bin/env python3
"""Exercise the actual board.mk dependency graph with a modeled compiler.

This verifies GAME changes and stale-output handling, not generated ARM code.
Both game objects predate the first ELF, so a different existing object alone
cannot force the relink: the real GAME stamp must do that work.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess

repo = Path(__file__).resolve().parents[3]
fixture = repo / "build/pine64/reconciliation-tests/game-fixture"
shutil.rmtree(fixture, ignore_errors=True)
fixture.mkdir(parents=True)
fake = fixture / "compiler.py"
fake.write_text('''#!/usr/bin/env python3
import json, pathlib, sys
root = pathlib.Path(__file__).parent
args = sys.argv[1:]
with (root / "calls.jsonl").open("a") as f: f.write(json.dumps(args) + "\\n")
out = pathlib.Path(args[args.index("-o") + 1])
out.write_text(json.dumps(args))
if "-c" not in args and (root / "FAIL_LINK").exists(): sys.exit(1)
''')
fake.chmod(0o755)
size = fixture / "fake-size"
size.write_text("#!/bin/sh\nexit 0\n")
size.chmod(0o755)
for name in ("a", "b"):
    (fixture / (name + ".c")).write_text("void fixture_" + name + "(void) {}\n")

build = (fixture / "objects").relative_to(repo)
target = build / "jupiter.elf"
empty_groups = [name + "=" for name in
                ("BOARD_SRCS", "USB_SRCS", "LIB_SRCS", "VGM_SRCS", "NE10_SRCS",
                 "FATFS_SRCS", "MT32_SRCS", "SC55_SRCS", "MENU_SRCS", "ASSETS")]
base = ["make", "--no-print-directory", "-f", "boards/pine64/board.mk",
        "BUILD=" + str(build), "CC=" + str(fake), "CROSS=" + str(fixture / "fake-")]

def invoke(game, goals, success=True):
    game_path = (fixture / (game + ".c")).relative_to(repo)
    result = subprocess.run(base + empty_groups + ["GAME=" + str(game_path)] +
                            [str(g) for g in goals], cwd=repo,
                            capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise AssertionError(result.stdout + result.stderr)

def calls():
    return [json.loads(line) for line in (fixture / "calls.jsonl").read_text().splitlines()]

objects = [build / (str((fixture / (game + ".c")).relative_to(repo)) + ".o")
           for game in ("a", "b")]
invoke("a", objects)
for game in ("a", "b", "a"):
    old_count = len(calls())
    invoke(game, [target])
    args = json.loads((repo / target).read_text())
    assert any(arg.endswith("/" + game + ".c.o") for arg in args)
    assert len(calls()) == old_count + 1 and "-c" not in calls()[-1]
old_count = len(calls())
invoke("a", [target])
assert len(calls()) == old_count, "Unchanged GAME unexpectedly rebuilt"
for suffix in (".bin", ".uImage"):
    (repo / target.with_suffix(suffix)).write_bytes(b"stale output")
(fixture / "FAIL_LINK").touch()
invoke("b", [target], success=False)
assert not any((repo / target.with_suffix(suffix)).exists()
               for suffix in (".elf", ".bin", ".uImage"))
print("Build graph PASS: existing GAME A/B/A objects relink correctly; unchanged GAME is stable; failed link removes all stale images")
