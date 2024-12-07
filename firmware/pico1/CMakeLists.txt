cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico1 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico1
	       zx_dma_pico1.c
)

target_include_directories(pico1 PRIVATE ../firmware-common)

target_link_libraries(pico1
		      pico_stdlib
	              hardware_gpio
)

pico_add_extra_outputs(pico1)
