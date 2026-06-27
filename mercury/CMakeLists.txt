# Mercury — Pico polygon coprocessor with PicoDVI output
cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mercury C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# PicoDVI library — clone into external/PicoDVI or set PICODVI_PATH
set(PICODVI_PATH ${CMAKE_CURRENT_LIST_DIR}/external/PicoDVI/software
    CACHE PATH "Path to PicoDVI software directory")
add_subdirectory(${PICODVI_PATH}/libdvi libdvi)

add_executable(mercury
    src/mercury_main.c
    src/mercury_raster.c
    src/mercury_displaylist.c
    src/mercury_spi.c
    src/dvi_out.c
)

# Keep the PIO CSI source for future analog MIPI route
# To switch: replace dvi_out.c with csi_out.c (or mipi_out.c)

target_include_directories(mercury PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# PicoDVI 8bpp RGB332 config
target_compile_definitions(mercury PRIVATE
    DVI_VERTICAL_REPEAT=1
    DVI_SYMBOLS_PER_WORD=2
)

target_link_libraries(mercury
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    libdvi
)

pico_enable_stdio_usb(mercury 1)
pico_enable_stdio_uart(mercury 0)

pico_add_extra_outputs(mercury)
