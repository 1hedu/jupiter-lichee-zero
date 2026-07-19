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

def main():
    src = open(PATH).read()
    if MARK in src:
        return 0  # already applied

    anchor_fn = "void MCU_PostSample(int *sample)"
    if anchor_fn not in src:
        sys.stderr.write("prepare_sc55: MCU_PostSample not found — "
                         "upstream layout changed, patch manually\n")
        return 1
    src = src.replace(anchor_fn, HOOK + anchor_fn, 1)

    anchor_buf = "    sample_buffer[sample_write_ptr + 0] = sample[0];"
    if anchor_buf not in src:
        sys.stderr.write("prepare_sc55: sample_buffer write not found\n")
        return 1
    src = src.replace(anchor_buf, DISPATCH + anchor_buf, 1)

    # Upstream's SDL standalone entry point collides with the SDK
    # example's main() at link time — rename it out of the way.
    anchor_main = "int main(int argc, char *argv[])"
    if anchor_main not in src:
        sys.stderr.write("prepare_sc55: upstream main() not found\n")
        return 1
    src = src.replace(anchor_main,
                      "int sc55_upstream_main(int argc, char *argv[])", 1)

    open(PATH, "w").write(src)
    print("prepare_sc55: hook applied to " + PATH)
    return 0

if __name__ == "__main__":
    sys.exit(main())
