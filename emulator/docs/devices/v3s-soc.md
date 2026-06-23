# `allwinner-v3s` — SoC top level

Source: [`src/hw/arm/allwinner-v3s.c`](../../src/hw/arm/allwinner-v3s.c)
Header: [`src/include/hw/arm/allwinner-v3s.h`](../../src/include/hw/arm/allwinner-v3s.h)

The SoC container. Owns one Cortex-A7, the GIC, on-chip SRAMs, and every
peripheral device modeled in this repo. Instantiated by the
`lichee-zero` machine.

## What it ties together

| Member          | Type                       | Address space            |
|-----------------|----------------------------|--------------------------|
| `cpus[1]`       | Cortex-A7                  | —                        |
| `gic`           | `arm_gic` (rev 2)           | 0x01C81000 / 82000 / 84000 / 86000 |
| `timer`         | `allwinner-a10-pit`        | 0x01C20C00               |
| `ccu`           | `allwinner-h3-ccu`         | 0x01C20000               |
| `sysctrl`       | `allwinner-h3-sysctrl`     | 0x01C00000               |
| `cpucfg`        | `allwinner-cpucfg`         | 0x01C25C00               |
| `sid`           | `allwinner-sid`            | 0x01C14000               |
| `display`       | `allwinner-v3s-display`    | 0x01000000 + 0x01100000 + 0x01C0C000 |
| `pio`           | `allwinner-v3s-pio`        | 0x01C20800               |
| `hstimer`       | `allwinner-v3s-hstimer`    | 0x01C60000               |
| `dma`           | `allwinner-v3s-dma`        | 0x01C02000               |
| `codec`         | `allwinner-v3s-codec`      | 0x01C22C00               |
| `mmc0`          | `allwinner-sdhost-sun5i`   | 0x01C0F000               |
| `cedar`         | `allwinner-v3s-cedar`      | 0x01C0E000               |
| `ym3438`        | `virt-ym3438`              | (no MMIO; GPIO-driven)   |
| `sram_a1/a2/c`  | `MemoryRegion (RAM)`       | 0x00000000, 0x00044000, 0x01D00000 |

UART0/1/2 are wired via `serial_mm_init` directly at machine init time
(not held as struct members).

## Reset / handoff

- The SoC's `realize` instantiates every child, hooks up GIC SPIs,
  realizes the GIC, and maps each child's MMIO.
- The `lichee-zero` machine adds a `qemu_register_reset` handler that
  fires on machine reset and sets `cpu->pc = 0x41000000`. That's where
  U-Boot's `go 0x41000000` jumps after SPL on real hardware. We don't
  simulate SPL or U-Boot — the `-kernel` payload is just dropped at that
  address by `load_image_targphys`.

## SRAM sizing

V3s has smaller SRAMs than the H3 we borrowed CCU/sysctrl/cpucfg from:

- SRAM A1: 48 KiB at `0x00000000` (boot vectors, DMA scratch).
- SRAM A2: 16 KiB at `0x00044000`.
- SRAM C : 64 KiB at `0x01D00000`.

These are plain `memory_region_init_ram` regions; nothing special.

## Stub peripherals

Devices that exist on real silicon but no Jupiter payload touches yet
are registered with `create_unimplemented_device` so stray reads/writes
log a warning:

- SPI0 / SPI1
- OWA (S/PDIF)
- PWM
- KEYADC
- codec-analog
- TWI0 / TWI1
- USB OTG / USB PHY
- CSI0

When a payload starts hitting one of these, replace the stub with a
real model.

## Known limitations

- **Single-core only.** The V3s really is single-core (1× A7), so this
  is fidelity, not a limitation. Anything that assumes SMP from looking
  at a generic ARM machine will misbehave.
- **No security extensions.** GIC built without
  `has-security-extensions=true`; security state is permanently NS.
  Doesn't matter for bare-metal Jupiter.
- **No DTB.** We pass `0x41000000` directly to `cpu_set_pc`; no Linux
  boot wrapper, no DTB load, no `r0=0/r1=mach/r2=dtb` ARM Linux
  convention.
