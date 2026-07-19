# Mercury — Pico polygon coprocessor
#
# Two output routes, selected at configure time:
#   cmake ..                        → DVI route (default): PicoDVI bitbang,
#                                     feeds an HDMI→CSI bridge (TC358743)
#   cmake -DMERCURY_ROUTE_DVP=ON .. → parallel DVP route: PIO 8-bit bus +
#                                     PCLK/HREF/VSYNC, feeds a DVP→CSI
#                                     bridge (TC358748)
cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mercury C CXX ASM)
set(CMAKE_C_STANDARD 11)

option(MERCURY_ROUTE_DVP "Output parallel DVP via PIO instead of DVI" OFF)

pico_sdk_init()

if(MERCURY_ROUTE_DVP)
    set(MERCURY_OUT_SRC src/csi_out.c)
else()
    # PicoDVI library — clone into external/PicoDVI or set PICODVI_PATH
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
else()
    # PicoDVI 8bpp RGB332 config
    target_compile_definitions(mercury PRIVATE
        DVI_VERTICAL_REPEAT=1
        DVI_SYMBOLS_PER_WORD=2
    )
    target_link_libraries(mercury libdvi)
endif()

pico_enable_stdio_usb(mercury 1)
pico_enable_stdio_uart(mercury 0)

pico_add_extra_outputs(mercury)
