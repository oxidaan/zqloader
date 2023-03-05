cmake_minimum_required(VERSION 3.7)

project(zqloader_z80)

find_program(HAVE_SJASMPLUS sjasmplus)
cmake_path(GET HAVE_SJASMPLUS PARENT_PATH SJASMPLUS_PATH)       # default for PATH_TO_SJASMPLUS
set(PATH_TO_SJASMPLUS ${SJASMPLUS_PATH} CACHE PATH "Location of sjasmplus (directory)")


if (NOT IS_DIRECTORY ${PATH_TO_SJASMPLUS}/examples)
    message("Please give directory of sjasmplus as parameter PATH_TO_SJASMPLUS; examples must be there (now: ${PATH_TO_SJASMPLUS})")
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
#    "-i${PATH_TO_SJASMPLUS}/examples/TapLib"           // not (yet) used (anymore)
    "-i${PATH_TO_SJASMPLUS}/examples/BasicLib"
    "--exp=zqloader.exp"
    "--lst"
    "--syntax=abf"
    "--color=on"
    "${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm"
    COMMENT "Assembling ${CMAKE_CURRENT_SOURCE_DIR}/zqloader.z80asm to  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap"
)


install(FILES zqloader.exp  ${CMAKE_CURRENT_BINARY_DIR}/zqloader.tap snapshotregs.bin
        DESTINATION bin
        )
