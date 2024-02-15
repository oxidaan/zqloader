cmake_minimum_required(VERSION 3.12)

project(zqloader_z80)

find_program(WHERE_IS_SJASMPLUS sjasmplus)
get_filename_component(SJASMPLUS_PATH  ${WHERE_IS_SJASMPLUS}  DIRECTORY)       # default for PATH_TO_SJASMPLUS
set(PATH_TO_SJASMPLUS ${SJASMPLUS_PATH} CACHE PATH "Location of sjasmplus (directory)")

# The directory location of the binary is not necessary the location of the sjasmplus examples.
# Try the cached variable PATH_TO_SJASMPLUS first.
# Or give with cmake -DPATH_TO_SJASMPLUS=path/to/sjasmplus
if (IS_DIRECTORY ${PATH_TO_SJASMPLUS}/examples)
    set(SJASMPLUS_PATH ${PATH_TO_SJASMPLUS})
endif()

if (NOT IS_DIRECTORY ${SJASMPLUS_PATH}/examples)
    message(FATAL_ERROR "Please give directory of sjasmplus as parameter PATH_TO_SJASMPLUS; examples must be there (now: ${SJASMPLUS_PATH})\nEg: cmake -DPATH_TO_SJASMPLUS=path/to/sjasmplus")
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
#    "-i${PATH_TO_SJASMPLUS}/examples/TapLib"           # TapLib not (yet) used (anymore)
    "-i${PATH_TO_SJASMPLUS}/examples/BasicLib"          # BasicLib, needed to generate BASIC
    "--exp=zqloader.exp"                                # export file, has symbols
    "--lst"                                             # lst file, not really needed
    "--syntax=abf"
    "--color=on"
    "${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm"
    COMMENT "Assembling ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm to  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap"
)


install(FILES zqloader.exp  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap snapshotregs.bin
        DESTINATION bin
        )
