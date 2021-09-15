cmake_minimum_required(VERSION 3.13)

add_library(util INTERFACE)

target_sources(util
INTERFACE
    work_meter.cpp
)

target_link_libraries(util 
INTERFACE
)

