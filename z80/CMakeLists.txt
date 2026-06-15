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

find_program(FOUND_SJASMPLUS sjasmplus)
if(NOT FOUND_SJASMPLUS)
    message(WARNING "sjasmplus not found! Make sure it is in the PATH. Skipping z80 assembly.
(Will install the files that were committed in repository instead)")
    # Cant build so install the files that were committed in repository
    install(FILES
            zqloader.exp
            zqloader.tap
            zqloader128.tap
            snapshotregs.bin
            DESTINATION z80
            )
    return()
else()
    message("sjasmplus found at ${FOUND_SJASMPLUS}")
endif()

get_filename_component(SJASMPLUS_PATH  ${FOUND_SJASMPLUS}  DIRECTORY)       # default for PATH_TO_SJASMPLUS
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
            zqloader.exp
            zqloader.tap
            zqloader128.tap
            snapshotregs.bin
            DESTINATION z80
            )
    return()
else()
    message("sjasmplus examples found at ${SJASMPLUS_PATH}/examples")
endif()



foreach(which "48" "128")
    add_custom_target(zqloader_z80_${which}k
      ALL DEPENDS  ${CMAKE_CURRENT_BINARY_DIR}/zqloader${which}.tap
      SOURCES zqloader.z80asm
      )

    add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/zqloader${which}.tap
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm
        COMMAND  "sjasmplus"
        ARGS 
        "--fullpath"
        "-i${SJASMPLUS_PATH}/examples/BasicLib"             # BasicLib, needed to generate BASIC
        "--exp=zqloader${which}.exp"                        # export file, has symbols
        "--lst"                                             # lst file, not really needed
        "--syntax=abf"
        "-DBANKSWITCH${which}"
#        "--define BANKSWITCH${which}"                      # not recognized?? Bug sjasmplys?
        "--color=on"
        "${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm"
        COMMENT "Assembling ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm to  ${CMAKE_CURRENT_BINARY_DIR}/zqloader${which}.tap"
    )
endforeach()

install(FILES  
        ${CMAKE_CURRENT_BINARY_DIR}/zqloader.exp  
        ${CMAKE_CURRENT_BINARY_DIR}/zqloader48.tap
        ${CMAKE_CURRENT_BINARY_DIR}/zqloader128.tap
        ${CMAKE_CURRENT_BINARY_DIR}/snapshotregs.bin
        DESTINATION z80
        )
