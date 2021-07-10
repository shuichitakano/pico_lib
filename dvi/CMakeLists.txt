#cmake_minimum_required(VERSION 3.13)

add_library(dvi INTERFACE)

target_sources(dvi
INTERFACE
	dvi.cpp
    timing.cpp
    dma.cpp
    data_packet.cpp
    tmds_encode.cpp
    tmds_encode.S
)

target_include_directories(dvi 
INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(dvi 
INTERFACE
    pico_base_headers
    pico_util
    hardware_dma
    hardware_interp
    hardware_pio
    hardware_gpio
    hardware_pwm
)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

pico_generate_pio_header(dvi ${CMAKE_CURRENT_LIST_DIR}/dvi_serialiser.pio)
