# ==============================================================================
# PROJECT:         zqloader
# FILE:            CMakeLists.txt
# DESCRIPTION:     CMake Project for zqloader
#
# Copyright (c) 2024 Daan Scherft [Oxidaan]
# This project uses the MIT license. See LICENSE.txt for details.
# ==============================================================================
# Main zqloader
# defines targets for zqloaderlib and zqloader (console) application
# includes zqloaderui and the z80 sjasmplus subdirecory projects

cmake_minimum_required(VERSION 3.12)



project(zqloader VERSION 2.0.0 LANGUAGES CXX)

add_subdirectory(z80)

set(TARGET_LIB ${PROJECT_NAME}lib)
set(TARGET_EXE ${PROJECT_NAME})

file(GLOB INCLUDE
    "zqloader/*.h"
    "*.h"
    )


set(SOURCE_LIB
    z80snapshot_loader.cpp
    datablock.cpp
    enumstreamer.cpp
    miniaudio.cpp
    samplesender.cpp
    spectrum_loader.cpp
    symbols.cpp
    taploader.cpp
    taptoturboblocks.cpp
    test.cpp
    tools.cpp
    turboblocks.cpp
    tzxloader.cpp
    sampletowav.cpp
    pulsers.cpp
    zqloader.cpp
)


set(DOCUMENTATION
    README.md
    LICENSE.txt
)    


# Common library containing bulk of code
add_library(${TARGET_LIB} SHARED  ${SOURCE_LIB} ${INCLUDE} ${DOCUMENTATION} )

# We need c++17
target_compile_features(${TARGET_LIB} PUBLIC cxx_std_17)

# Here lives miniadio.h
target_include_directories(${TARGET_LIB} PRIVATE miniaudio)

target_compile_definitions(${TARGET_LIB} PUBLIC
    "NDEBUG;"
    "_CONSOLE;"
    "UNICODE;"
    "_UNICODE"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(GCC true)
endif()

if(MSVC)
    target_compile_options(${TARGET_LIB} PUBLIC
        /permissive-;
        /MP;                # multi processor build
        /Oi;                # intrincic functions
        /Gy;                # function level linking
        /W4;                # warning level
        /WX;                # warnings are errors
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        /Zc:__cplusplus;    # fix bug to recognise __cplusplus
        ${DEFAULT_CXX_EXCEPTION_HANDLING}
    )
    target_link_options(${TARGET_LIB} PUBLIC
        /DEBUG;
        /SUBSYSTEM:CONSOLE;
        /OPT:REF;           # Remove unreferenced
        /OPT:ICF            # identical COMDAT folding
    )
endif()

if (GCC) 
    target_compile_options(${TARGET_LIB} PUBLIC 
        -Wall 
        -pedantic
        -Werror
        -Wno-error=deprecated-declarations
        -Wno-sign-compare
    )
endif()


if(UNIX)
    target_link_libraries(${TARGET_LIB} PRIVATE dl pthread)
endif()
if(WIN32)
    target_link_libraries(${TARGET_LIB} PRIVATE ws2_32 DbgHelp psapi)
endif()

# include ui/qt application
add_subdirectory(zqloaderui)

# commandline/console application
set(SOURCE_EXE main.cpp)

if(MSVC)
    set(SOURCE_EXE ${SOURCE_EXE}
    zqloader.rc)
endif()

# The console application
add_executable(${TARGET_EXE}  ${SOURCE_EXE} ${INCLUDE} )
target_link_libraries(${TARGET_EXE} ${TARGET_LIB})


#installation
include(GNUInstallDirs)
install(TARGETS ${TARGET_EXE} ${TARGET_LIB}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
       )

