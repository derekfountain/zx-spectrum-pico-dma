cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(zx_dma_rp2350b C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(zx_dma_rp2350b
zx_dma_rp2350b.c
)

target_link_libraries(zx_dma_rp2350b
		      pico_stdlib
		      hardware_gpio
)

pico_add_extra_outputs(zx_dma_rp2350b)

