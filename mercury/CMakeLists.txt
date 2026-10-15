# Mercury — Pico polygon coprocessor
#
# Defaults: Pico 2 W (RP2350) + SpotPear Pico HDMI board.
#   cmake ..                                   Pico 2 W + SpotPear (PicoDVI)
#   cmake -DMERCURY_DVI_BOARD=DVI_SOCK ..      Pico 2 W + DVI Sock (HSTX)
#   cmake -DPICO_BOARD=pico ..                 RP2040 Pico (PicoDVI)
#   cmake -DMERCURY_ROUTE_DVP=ON ..            either chip: PIO parallel DVP
# The DVI route follows chip + board: HSTX (RP2350 hardware TMDS) when
# the board's pairs sit on GP12-19, else PicoDVI (PIO, needs
# external/PicoDVI). DVI feeds a TC358743; DVP feeds a TC358748.
cmake_minimum_required(VERSION 3.13)

# Must precede the SDK import: the board picks the chip (pico2_w -> rp2350)
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2_w CACHE STRING "Board (pico2_w, pico2, pico)")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mercury C CXX ASM)
set(CMAKE_C_STANDARD 11)

option(MERCURY_ROUTE_DVP "Output parallel DVP via PIO instead of DVI" OFF)
# DVI connector board — see the presets in include/jupiter32x.h
set(MERCURY_DVI_BOARD "SPOTPEAR" CACHE STRING
    "DVI board: SPOTPEAR, DVI_SOCK, AMY, LEGACY_GP0")

pico_sdk_init()

if(PICO_PLATFORM MATCHES "^rp2350")
    set(MERCURY_RP2350 ON)
endif()
# HSTX only reaches GP12-19: usable for boards wired entirely there
if(MERCURY_RP2350 AND MERCURY_DVI_BOARD MATCHES "^(DVI_SOCK|AMY)$")
    set(MERCURY_USE_HSTX ON)
endif()

if(MERCURY_ROUTE_DVP)
    set(MERCURY_OUT_SRC src/csi_out.c)
elseif(MERCURY_USE_HSTX)
    # RP2350: HSTX TMDS-encodes in hardware — no PicoDVI needed
    set(MERCURY_OUT_SRC src/hstx_dvi_out.c src/hstx_dvi_stream.c)
else()
    # PicoDVI library (PIO serialiser) — clone into external/PicoDVI or set PICODVI_PATH
    set(PICODVI_PATH ${CMAKE_CURRENT_LIST_DIR}/external/PicoDVI/software
        CACHE PATH "Path to PicoDVI software directory")
    add_subdirectory(${PICODVI_PATH}/libdvi libdvi)
    set(MERCURY_OUT_SRC src/dvi_out.c)
endif()

add_executable(mercury
    src/mercury_main.c
    src/mercury_raster.c
    src/mercury_displaylist.c
    src/mercury_spi.c
    ${MERCURY_OUT_SRC}
)

target_include_directories(mercury PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(mercury
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_sync
)

if(MERCURY_ROUTE_DVP)
    pico_generate_pio_header(mercury ${CMAKE_CURRENT_LIST_DIR}/pio/csi_out.pio)
    target_compile_definitions(mercury PRIVATE MERCURY_ROUTE_DVP=1)
    target_link_libraries(mercury hardware_pio hardware_dma)
elseif(MERCURY_USE_HSTX)
    # clk_hstx = clk_sys and HSTX shifts 2 bits/cycle, so DVI's 252 Mbps
    # lane rate needs clk_sys = 126 MHz exactly (12 MHz XOSC x 126 = 1512
    # MHz VCO, / 6 / 2). The RP2350 default of 150 MHz would give a
    # non-standard ~71 Hz 640x480 that bridges/monitors may reject.
    target_compile_definitions(mercury PRIVATE
        MERCURY_ROUTE_HSTX=1
        MERCURY_DVI_BOARD_${MERCURY_DVI_BOARD}=1
        SYS_CLK_HZ=126000000
        PLL_SYS_VCO_FREQ_HZ=1512000000
        PLL_SYS_POSTDIV1=6
        PLL_SYS_POSTDIV2=2
    )
    target_link_libraries(mercury hardware_dma hardware_irq)
else()
    # PicoDVI 8bpp RGB332 config; clk_sys = 252 MHz set at runtime by
    # j32x_scanout_preinit (dvi_out.c)
    target_compile_definitions(mercury PRIVATE
        MERCURY_DVI_BOARD_${MERCURY_DVI_BOARD}=1
        DVI_VERTICAL_REPEAT=1
        DVI_SYMBOLS_PER_WORD=2
    )
    target_link_libraries(mercury libdvi hardware_vreg)
endif()

pico_enable_stdio_usb(mercury 1)
pico_enable_stdio_uart(mercury 0)

pico_add_extra_outputs(mercury)
