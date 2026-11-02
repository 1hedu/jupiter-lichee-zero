#!/usr/bin/env python3
"""Patch the vendored Nuked-SC55 (third_party/nuked_sc55) for bare metal.

Upstream MCU_PostSample() writes into an SDL audio ring that only exists
after MCU_OpenAudio() — which the Jupiter driver never calls. The driver
(lib/nuked_sc55_driver.cpp) instead expects a MCU_SetSampleCallback()
hook. This script injects that hook into mcu.cpp, idempotently, so the
submodule can stay pinned at pristine upstream.

Run automatically by the build (build/sc55/.prepared rule).
"""
import sys
from pathlib import Path

PATH = "third_party/nuked_sc55/src/mcu.cpp"
MARK = "jupiter_sample_cb"

HOOK = """\
/* --- Jupiter SDK bare-metal hook (auto-applied by scripts/prepare_sc55.py).
 * The SDK driver registers a sink here; when set, samples bypass the SDL
 * ring entirely (which is never allocated on bare metal). --- */
static void (*jupiter_sample_cb)(int16_t l, int16_t r);
extern "C" void MCU_SetSampleCallback(void (*cb)(int16_t l, int16_t r))
{
    jupiter_sample_cb = cb;
}

"""

DISPATCH = """\
    if (jupiter_sample_cb) {
        jupiter_sample_cb((int16_t)sample[0], (int16_t)sample[1]);
        return;
    }
"""

# Earlier PINE snapshots carried this board-only hook in the dependency.
# Migrate only that exact known patch before applying the shared hook;
# adding another MCU_SetSampleCallback would otherwise fail to link.
LEGACY_HOOK = """\
#ifdef JUPITER_BOARD_PINE64
static void (*pine64_sample_callback)(int16_t, int16_t);
extern "C" void MCU_SetSampleCallback(void (*callback)(int16_t, int16_t))
{
    pine64_sample_callback = callback;
}
#endif

"""
LEGACY_DISPATCH = """\
#ifdef JUPITER_BOARD_PINE64
    if (pine64_sample_callback) pine64_sample_callback((int16_t)sample[0], (int16_t)sample[1]);
#else
    sample_buffer[sample_write_ptr + 0] = sample[0];
    sample_buffer[sample_write_ptr + 1] = sample[1];
    sample_write_ptr = (sample_write_ptr + 2) % audio_buffer_size;
#endif
"""
SDL_DISPATCH = """\
    sample_buffer[sample_write_ptr + 0] = sample[0];
    sample_buffer[sample_write_ptr + 1] = sample[1];
    sample_write_ptr = (sample_write_ptr + 2) % audio_buffer_size;
"""
LEGACY_MAIN = """\
#ifdef JUPITER_BOARD_PINE64
#define main nuked_sc55_desktop_main
#endif
"""

def prepare(src):
    """Return fully prepared source, or fail before changing the file."""
    anchor_main = "int main(int argc, char *argv[])"
    renamed_main = "int sc55_upstream_main(int argc, char *argv[])"
    if MARK in src:
        if (src.count(HOOK) != 1 or src.count(DISPATCH) != 1 or
                src.count("void MCU_SetSampleCallback(") != 1 or
                src.count(renamed_main) != 1 or anchor_main in src or
                "pine64_sample_callback" in src):
            raise ValueError("incomplete or conflicting existing callback patch")
        return src

    if "pine64_sample_callback" in src:
        if any(src.count(part) != 1 for part in
               (LEGACY_HOOK, LEGACY_DISPATCH, LEGACY_MAIN)):
            raise ValueError("unrecognized legacy PINE callback patch")
        src = src.replace(LEGACY_HOOK, "", 1)
        src = src.replace(LEGACY_DISPATCH, SDL_DISPATCH, 1)
        src = src.replace(LEGACY_MAIN, "", 1)
    if "MCU_SetSampleCallback" in src:
        raise ValueError("unrecognized existing MCU_SetSampleCallback")

    anchor_fn = "void MCU_PostSample(int *sample)"
    if src.count(anchor_fn) != 1:
        raise ValueError("MCU_PostSample not unique — upstream layout changed")
    src = src.replace(anchor_fn, HOOK + anchor_fn, 1)

    anchor_buf = "    sample_buffer[sample_write_ptr + 0] = sample[0];"
    if src.count(anchor_buf) != 1:
        raise ValueError("sample_buffer write not unique")
    src = src.replace(anchor_buf, DISPATCH + anchor_buf, 1)

    # Upstream's SDL standalone entry point collides with the SDK
    # example's main() at link time — rename it out of the way.
    if src.count(anchor_main) != 1:
        raise ValueError("upstream main() not unique")
    return src.replace(anchor_main, renamed_main, 1)

def main():
    try:
        path = Path(PATH)
        src = path.read_text()
        prepared = prepare(src)
        if prepared != src:
            path.write_text(prepared)
            print("prepare_sc55: hook applied to " + PATH)
    except (OSError, ValueError) as exc:
        sys.stderr.write("prepare_sc55: " + str(exc) + "\n")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
