# Jupiter Mars — Pico polygon coprocessor with PicoDVI output
cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(jupiter_mars C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# PicoDVI library — clone into external/PicoDVI or set PICODVI_PATH
set(PICODVI_PATH ${CMAKE_CURRENT_LIST_DIR}/external/PicoDVI/software
    CACHE PATH "Path to PicoDVI software directory")
add_subdirectory(${PICODVI_PATH}/libdvi libdvi)

add_executable(jupiter_mars
    src/mars_main.c
    src/mars_raster.c
    src/mars_displaylist.c
    src/mars_spi.c
    src/dvi_out.c
)

# Keep the PIO CSI source for future analog MIPI route
# To switch: replace dvi_out.c with csi_out.c (or mipi_out.c)

target_include_directories(jupiter_mars PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# PicoDVI 8bpp RGB332 config
target_compile_definitions(jupiter_mars PRIVATE
    DVI_VERTICAL_REPEAT=1
    DVI_SYMBOLS_PER_WORD=2
)

target_link_libraries(jupiter_mars
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    libdvi
)

pico_enable_stdio_usb(jupiter_mars 1)
pico_enable_stdio_uart(jupiter_mars 0)

pico_add_extra_outputs(jupiter_mars)
