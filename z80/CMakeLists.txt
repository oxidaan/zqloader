# ==============================================================================
# PROJECT:         zqloader_z80
# FILE:            CMakeLists.txt
# DESCRIPTION:     CMake project for z80/sjasmplus project for zqloader.
#
# Copyright (c) 2024 Daan Scherft [Oxidaan]
# This project uses the MIT license. See LICENSE.txt for details.
# ==============================================================================



cmake_minimum_required(VERSION 3.12)

project(zqloader_z80)

find_program(HAVE_SJSASMPLUS sjasmplus)
if(NOT HAVE_SJSASMPLUS)
    message(WARNING "sjasmplus not found! Make sure it is in the PATH. Skipping z80 assembly.
(Will install the files that were committed in repository instead)")
    # Cant build so install the files that were committed in repository
    install(FILES
            z80/zqloader.exp
            z80/zqloader.tap
            z80/snapshotregs.bin
            DESTINATION bin
            )
    return()
else()
    message("sjasmplus found at ${HAVE_SJSASMPLUS}")
endif()

get_filename_component(SJASMPLUS_PATH  ${HAVE_SJSASMPLUS}  DIRECTORY)       # default for PATH_TO_SJASMPLUS
set(PATH_TO_SJASMPLUS ${SJASMPLUS_PATH} CACHE PATH "Location of sjasmplus (directory)")

# The directory location of the found binary is not necessary the location of the sjasmplus examples.
# Try the cached variable PATH_TO_SJASMPLUS first.
# Or give with cmake -DPATH_TO_SJASMPLUS=path/to/sjasmplus
if (IS_DIRECTORY ${PATH_TO_SJASMPLUS}/examples)
    set(SJASMPLUS_PATH ${PATH_TO_SJASMPLUS})
endif()

if (NOT IS_DIRECTORY ${SJASMPLUS_PATH}/examples)
    message(WARNING "Sjasmplus examples not found. Skipping z80 assembly.
Please give directory of sjasmplus as parameter PATH_TO_SJASMPLUS; examples must be there (now: ${SJASMPLUS_PATH})
Eg: cmake -DPATH_TO_SJASMPLUS=path/to/sjasmplus
(Will install the files that were committed in repository instead)")
    # Cant build so install the files that were committed in repository
    install(FILES
            z80/zqloader.exp
            z80/zqloader.tap
            z80/snapshotregs.bin
            DESTINATION bin
            )
    return()
else()
    message("sjasmplus examples found at ${SJASMPLUS_PATH}/examples")
endif()

add_custom_target(zqloader_z80
  ALL DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap
  SOURCES zqloader.z80asm
  )

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm
    COMMAND  "sjasmplus"
    ARGS 
    "--fullpath"
    "-i${SJASMPLUS_PATH}/examples/BasicLib"             # BasicLib, needed to generate BASIC
    "--exp=zqloader.exp"                                # export file, has symbols
    "--lst"                                             # lst file, not really needed
    "--syntax=abf"
    "--color=on"
    "${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm"
    COMMENT "Assembling ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm to  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap"
)

install(FILES  
        ${CMAKE_CURRENT_BINARY_DIR}/zqloader.exp  
        ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap  
        ${CMAKE_CURRENT_BINARY_DIR}/snapshotregs.bin
        DESTINATION bin
        )
