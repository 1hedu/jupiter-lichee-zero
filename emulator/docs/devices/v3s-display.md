# `allwinner-v3s-display` — DE2 + Mixer0 + TCON0

Source: [`src/hw/display/allwinner-v3s-display.c`](../../src/hw/display/allwinner-v3s-display.c)
Header: [`src/include/hw/display/allwinner-v3s-display.h`](../../src/include/hw/display/allwinner-v3s-display.h)

The display pipeline as one device because Jupiter's `video.c` treats
it as one. Models the bits you need to scan a 480×272 framebuffer to a
QemuConsole, not the full DE2 spec.

## Three MMIO regions

| Region   | Base       | Size  | Purpose                       |
|----------|-----------:|------:|-------------------------------|
| DE2 top  | 0x01000000 | 256 B | Top-level clock gates         |
| Mixer0   | 0x01100000 | 48 KiB | Layers + blender             |
| TCON0    | 0x01C0C000 | 512 B | Timing controller             |

Each region is its own `MemoryRegion` and its writes go to a separate
register array (`de2[]`, `mix[]`, `tcon[]`). The device is mapped at
all three bases via `sysbus_mmio_map(...)` calls 0..2.

## Mixer registers we honor

| Offset           | Name             | Effect                                 |
|------------------|------------------|----------------------------------------|
| 0x0000           | `MIX_GLB_CTL`    | Bit 0 = enable mixer                   |
| 0x0008           | `MIX_GLB_DBUF`   | Bit 0 = commit double-buffer (no-op for us) |
| 0x000C           | `MIX_GLB_SIZE`   | WH(w-1, h-1) — we read but mostly hardcode 480×272 |
| 0x2000 + 0x30·n  | `VI0_ATTR(n)`    | Bit 0 = layer enable; format bits 8–12 |
| 0x2018 + 0x30·n  | `VI0_TOP_LADDR0(n)` | Guest-side framebuffer DRAM address |
| 0x4000 + 0x20·n  | `UI0_ATTR(n)`    | Bit 0 = enable; bits 24–31 = global alpha |
| 0x4010 + 0x20·n  | `UI0_ADDR(n)`    | UI overlay framebuffer DRAM address    |

VI0 = the game world layer (XRGB8888). UI0 = the per-pixel alpha
overlay (ARGB8888). We composite UI0 over VI0 only when VI0_ATTR and
UI0_ATTR both have their enable bit set.

Other channels and most format bits are recorded but not interpreted.

## TCON registers we gate scan-out on

| Offset | Name        | Effect                                |
|--------|-------------|---------------------------------------|
| 0x00   | `TCON_GCTL` | Bit 31 = global enable                |
| 0x40   | `TCON0_CTL` | Bit 31 = channel 0 enable             |

If both bits aren't set when the QEMU display update callback fires,
we draw black. This matches Jupiter's bringup sequence (set both, then
let the framebuffer fill with content).

## Scan-out

QEMU registers a `GraphicHwOps` with an `update` callback. Each
invocation:

1. Check display is active (TCON enables both set). Else fill black,
   return.
2. Read VI0_TOP_LADDR0(0). Use it to pull `LCD_W * LCD_H * 4` bytes from
   guest DRAM into a `pixman_image_t`.
3. If UI0 enabled, read UI0_ADDR(0); composite ARGB-over-XRGB onto the
   pixman buffer in-place.
4. `dpy_gfx_update_full` to push to the QemuConsole.

The image is 480×272 fixed because Jupiter's `video.c` configures TCON0
for that timing. We don't read the timing registers.

## Known limitations

- **Format bits in VI0_ATTR are ignored.** We always interpret the VI
  buffer as XRGB8888. If a payload uses YUV or RGB565 here, it'll show
  garbage.
- **Single VI channel, single UI channel.** Real DE2 has multiple
  channels of each; we read channel 0 only.
- **No scaling, no rotation, no chroma keying.** Rectangular blit only.
- **No VBLANK / HSYNC interrupts.** TCON IRQ is unwired. The guest
  uses HSTimer for frame timing instead, which is fine.
- **Performance.** Every `update` walks the full framebuffer through
  `address_space_read`. On a fast host this is invisible; on a heavily
  loaded VM it can cap at ~30 FPS.
