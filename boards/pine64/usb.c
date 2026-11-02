/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PHY register sequencing adapted from U-Boot phy-sun4i-usb.c:
 * Copyright (C) 2017 Jagan Teki <jagan@amarulasolutions.com>
 * Copyright (C) 2015 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2014 Roman Byshko <rbyshko@gmail.com>
 *
 * PINE A64+ OHCI keyboard host. The two external USB sockets each have
 * one EHCI/OHCI pair. We give both to their OHCI companion and run one
 * TinyUSB host at a time. USB service runs in ordinary foreground code.
 *
 * Hardware definitions checked against U-Boot v2025.10:
 * drivers/clk/sunxi/clk_a64.c, drivers/phy/allwinner/phy-sun4i-usb.c,
 * drivers/usb/host/ehci.h, and sun50i-a64{-pine64,}.dts[i]. PINE64 has
 * fixed USB VBUS power; its device tree specifies no VBUS GPIO/regulator.
 */
#include "jupiter.h"
#include "peripherals.h"
#include "usb.h"
#include "tusb.h"
#include "host/hcd.h"

#define USB_BUS_BITS        (BIT(24) | BIT(25) | BIT(28) | BIT(29))
#define USB_CLOCK_BITS      (BIT(8) | BIT(9) | BIT(16) | BIT(17))
#define USB_PHY_RESET_BITS  (BIT(0) | BIT(1))
#define USB_BUS_GATE        REG32(0x01c20060u)
#define USB_BUS_RESET       REG32(0x01c202c0u)
#define USB_CLOCK           REG32(0x01c200ccu)
#define USB_PHY_SERIAL      0x01c19410u
#define USB_PHY_ROUTE       REG32(0x01c19420u)

static unsigned selected_port;
static uint32_t counter_khz;
static uint32_t selected_since;
static uint8_t port_available[2];
static bool initialized, host_ready, poll_enabled, host_fault, polling;

uintptr_t pine64_usb_ohci_base(void)
{
    return selected_port ? 0x01c1b400u : 0x01c1a400u;
}

uint32_t tusb_time_millis_api(void)
{
    return counter_khz ? (uint32_t)(pine64_usb_counter() / counter_khz) : 0;
}

/* No dependence on a USB frame clock, so reset/resume waits also work
 * before OHCI begins producing SOF. An iteration bound covers a stopped
 * or incorrectly configured system counter. */
void tusb_time_delay_ms_api(uint32_t ms)
{
    uint32_t start = tusb_time_millis_api();
    uint32_t bound = 100000000u;
    while ((uint32_t)(tusb_time_millis_api() - start) < ms) {
        if (!--bound) {
            pine64_usb_hcd_fault("system counter stopped during USB delay");
            return;
        }
    }
}

bool pine64_usb_wait32(volatile uint32_t *reg, uint32_t mask,
                       uint32_t value, uint32_t timeout_ms)
{
    uint32_t start = tusb_time_millis_api();
    uint32_t bound = 10000000u;
    do {
        pine64_usb_dma_barrier();
        if ((*reg & mask) == value)
            return true;
        if (!--bound)
            break;
    } while ((uint32_t)(tusb_time_millis_api() - start) < timeout_ms);
    return false;
}

void pine64_usb_hcd_fault(const char *reason)
{
    if (!host_fault) {
        uart_puts("[usb] ");
        uart_puts(reason);
        uart_puts("; restarting keyboard host\n");
    }
    host_fault = true;
}

void hcd_int_enable(uint8_t rhport)
{
    (void)rhport;
    pine64_usb_dma_barrier();
    poll_enabled = true;
}

void hcd_int_disable(uint8_t rhport)
{
    (void)rhport;
    poll_enabled = false;
    pine64_usb_dma_barrier();
}

static void phy_write(unsigned phy, unsigned address, unsigned data, unsigned bits)
{
    volatile uint8_t *serial_byte = (volatile uint8_t *)USB_PHY_SERIAL;
    uint8_t strobe = (uint8_t)(1u << (phy * 2u));
    REG32(USB_PHY_SERIAL) = 0; /* A33 and newer serial PHY interface. */
    for (unsigned i = 0; i < bits; ++i) {
        uint32_t value = REG32(USB_PHY_SERIAL);
        REG32(USB_PHY_SERIAL) = (value & ~0xff00u) | ((address + i) << 8);
        uint8_t byte = *serial_byte;
        byte = (uint8_t)((byte & ~(0x80u | strobe)) | ((data & 1u) << 7));
        *serial_byte = byte;
        pine64_usb_dma_barrier();
        *serial_byte = (uint8_t)(*serial_byte | strobe);
        pine64_usb_dma_barrier();
        *serial_byte = (uint8_t)(*serial_byte & ~strobe);
        data >>= 1;
    }
    pine64_usb_dma_barrier();
}

static bool ehci_to_companion(unsigned port)
{
    uintptr_t base = port ? 0x01c1b000u : 0x01c1a000u;
    uint32_t length = REG32(base) & 0xffu;
    if (length < 0x10u || length > 0x40u || (length & 3u))
        return false;
    uintptr_t op = base + length;
    REG32(op + 0x08) = 0; /* No EHCI interrupts or transfer schedules. */
    REG32(op) &= ~(BIT(0) | BIT(4) | BIT(5));
    if (!pine64_usb_wait32((volatile uint32_t *)(op + 4), BIT(12), BIT(12), 100))
        return false;
    REG32(op) = BIT(1);
    if (!pine64_usb_wait32((volatile uint32_t *)op, BIT(1), 0, 100))
        return false;
    REG32(op + 0x40) = 0; /* CONFIGFLAG: all ports routed to companion. */
    uint32_t status = REG32(op + 0x44);
    status &= ~(BIT(1) | BIT(3) | BIT(5) | BIT(8)); /* W1C changes/reset. */
    REG32(op + 0x44) = status | BIT(12) | BIT(13); /* Power + OHCI owner. */
    pine64_usb_dma_barrier();
    return true;
}

static bool prepare_hardware(void)
{
    /* Stop all old bootloader schedules before any descriptor reuse. */
    USB_BUS_GATE |= USB_BUS_BITS;
    USB_BUS_RESET &= ~USB_BUS_BITS;
    USB_CLOCK = (USB_CLOCK | USB_CLOCK_BITS) & ~USB_PHY_RESET_BITS;
    pine64_usb_dma_barrier();
    tusb_time_delay_ms_api(1);
    USB_CLOCK |= USB_PHY_RESET_BITS;
    USB_BUS_RESET |= USB_BUS_BITS;
    pine64_usb_dma_barrier();
    tusb_time_delay_ms_api(1);

    USB_PHY_ROUTE &= ~BIT(0); /* PHY0 host route, not the MUSB device path. */
    for (unsigned port = 0; port < 2; ++port) {
        uintptr_t pmu = port ? 0x01c1b800u : 0x01c1a800u;
        REG32(pmu + 0x10) &= ~BIT(1); /* Clear analog PHY SIDDQ. */
        if (!port)
            phy_write(port, 0x0c, 1, 1); /* PHY0 45-ohm calibration. */
        phy_write(port, 0x20, 0x14, 5);  /* A64 TX magnitude/rate. */
        phy_write(port, 0x2a, 3, 2);     /* A64 disconnect threshold. */
        REG32(pmu) |= 0x701u;           /* AHB bursts + ULPI passby. */
        pine64_usb_dma_barrier();
        port_available[port] = ehci_to_companion(port);
        if (!port_available[port]) {
            uart_puts("[usb] EHCI companion handoff failed for port ");
            uart_putdec(port + 1u);
            uart_puts("\n");
        }
        /* OHCI is held in reset until its descriptors are initialized. */
        USB_BUS_RESET &= ~BIT(28u + port);
    }
    pine64_usb_dma_barrier();
    return port_available[0] || port_available[1];
}

static void stop_host(void)
{
    if (tuh_inited())
        (void)tuh_deinit(0);
    /* Hardware reset is the final DMA fence even if HCR timed out. */
    USB_BUS_RESET &= ~BIT(28u + selected_port);
    pine64_usb_dma_barrier();
    host_ready = false;
    poll_enabled = false;
    pine64_usb_keyboard_clear();
}

static bool start_host(unsigned port)
{
    selected_port = port;
    host_fault = false;
    selected_since = tusb_time_millis_api();
    if (!port_available[port])
        return false;
    USB_BUS_RESET |= BIT(28u + port);
    pine64_usb_dma_barrier();
    tusb_time_delay_ms_api(1);
    const tusb_rhport_init_t init = { .role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL };
    host_ready = tuh_rhport_init(0, &init);
    if (!host_ready || host_fault) {
        uart_puts("[usb] OHCI initialization failed for port ");
        uart_putdec(port + 1u);
        uart_puts("\n");
        stop_host();
        return false;
    }
    return true;
}

void pine64_usb_init(void)
{
    if (initialized)
        return;
    initialized = true;
    uint32_t frequency;
    __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(frequency));
    if (frequency < 1000000u || frequency > 100000000u || frequency % 1000u) {
        uart_puts("[usb] invalid generic counter frequency; USB disabled\n");
        return;
    }
    counter_khz = frequency / 1000u;
    uint64_t first = pine64_usb_counter();
    unsigned bound = 1000000u;
    while (pine64_usb_counter() == first && --bound)
        ;
    if (!bound) {
        uart_puts("[usb] generic counter did not advance; USB disabled\n");
        counter_khz = 0;
        return;
    }
    pine64_usb_keyboard_clear();
    if (!prepare_hardware())
        return;
    tuh_hid_set_default_protocol(HID_PROTOCOL_BOOT);
    uart_puts("[usb] polling both ports for a directly connected boot keyboard\n");
    if (!start_host(port_available[0] ? 0u : 1u))
        (void)start_host(selected_port ^ 1u);
}

void pine64_usb_poll(void)
{
    if (!initialized || !counter_khz || polling ||
        (!port_available[0] && !port_available[1]))
        return;
    polling = true;
    if (host_ready) {
        if (poll_enabled) {
            pine64_usb_dma_barrier();
            hcd_int_handler(0, false);
            pine64_usb_dma_barrier();
        }
        tuh_task_ext(0, false);
        pine64_usb_keyboard_poll();
    }
    uint32_t age = tusb_time_millis_api() - selected_since;
    bool connected = host_ready && hcd_port_connect_status(0);
    /* Empty socket: rotate quickly. A device gets four seconds for
     * enumeration before we retry on the other socket. This also keeps
     * unsupported devices from blocking a keyboard in the other port. */
    if (host_fault || (!pine64_usb_connected() &&
        age >= (connected ? 4000u : 750u))) {
        unsigned next = selected_port ^ 1u;
        if (!port_available[next])
            next = selected_port;
        stop_host();
        (void)start_host(next);
    }
    polling = false;
}
