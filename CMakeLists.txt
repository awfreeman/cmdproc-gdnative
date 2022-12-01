# min version could probably be less than this.
cmake_minimum_required(VERSION 3.20)
add_subdirectory(zip)
if (WIN32)
    set(PLATFORM windows)
else()
    set(PLATFORM linuxbsd)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/project/gdnative/${PLATFORM}")

project (gddl)

find_package(CURL REQUIRED)

set(SOURCES
    src/gddl.c
)

if(WIN32)
    add_compile_options(
        -DWIN32
        -D_WIN32
        -D_WINDOWS
#        -W3 # warning
#        -GR # runtime info
        -D_CRT_SECURE_NO_WARNINGS
    )

    if(CMAKE_BUILD_TYPE MATCHES DEBUG)
        add_compile_options(
#            -EHsc # c only exception handling
            -D_DEBUG 
#            -MDd # library thing
        )
    else()
        add_compile_options(
            -O2
#            -EHsc
            -DNDEBUG
#            -MD
        )
    endif()

    add_link_options(
        -LIBPATH:"${CMAKE_CURRENT_SOURCE_DIR}/project/gdnative/${PLATFORM}"
    )
else()
    add_compile_options(
        -march=x86-64
        -DCURL_STATICLIB
    )
    if(CMAKE_BUILD_TYPE MATCHES DEBUG)
        add_compile_options(
            -fPIC
            -g3
            -Og
        )
    else()
        add_compile_options(
            -fPIC
            -g
            -O3
        )
    endif()

    add_link_options(
        -march=x86-64
        -Wl,--no-undefined
    )
    #might have to add search path...
endif()

include_directories(godot-headers)
include_directories(${CURL_INCLUDE_DIRS})

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES} zip)
