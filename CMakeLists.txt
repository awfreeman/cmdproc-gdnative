# min version could probably be less than this.
cmake_minimum_required(VERSION 3.15)
#
if (WIN32)
    set(PLATFORM windows)
else()
    set(PLATFORM linuxbsd)
endif()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
project (cmdshim)
set(SOURCES src/cmdshim.c)
add_library(${PROJECT_NAME} MODULE ${SOURCES})
include_directories(godot-headers)