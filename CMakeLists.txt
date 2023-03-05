cmake_minimum_required(VERSION 3.7)



project(zqloader VERSION 0.1.0 LANGUAGES CXX)

find_program(HAVE_SJSASMPLUS sjasmplus)
if(NOT HAVE_SJSASMPLUS)
    message("sjasmplus not found! Make sure it is in the PATH. Skipping z80 assembly.")
else()
    message("sjasmplus found at ${HAVE_SJSASMPLUS}")
    add_subdirectory(z80)
endif()

file(GLOB INCLUDE
    "zqloader/*.h"
    "*.h"
    )


set(SOURCE
    z80snapshot_loader.cpp
    datablock.cpp
    enumstreamer.cpp
    main.cpp
    miniaudio.cpp
    samplesender.cpp
    spectrum_loader.cpp
    symbols.cpp
    taploader.cpp
    taptoturboblocks.cpp
    test.cpp
    turboblock.cpp
    tzxloader.cpp
)

set(DOCUMENTATION
    README.md
    LICENSE.txt
)    

add_executable(${PROJECT_NAME}  ${SOURCE} ${INCLUDE} ${DOCUMENTATION} )

# We need c++17
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Here lives miniadio.h
target_include_directories(${PROJECT_NAME} PRIVATE miniaudio)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "NDEBUG;"
    "_CONSOLE;"
    "UNICODE;"
    "_UNICODE"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(GCC true)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /permissive-;
        /MP;
        /Oi;
        /Gy;
        /W4;
        /WX;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        /Zc:__cplusplus;
        ${DEFAULT_CXX_EXCEPTION_HANDLING}
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        /DEBUG;
        /SUBSYSTEM:CONSOLE;
        /OPT:REF;
        /OPT:ICF
    )
endif()

if (GCC) 
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall 
        -pedantic
        -Werror
        -Wno-error=deprecated-declarations
        -Wno-sign-compare
    )
endif()







if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 DbgHelp psapi)
endif()

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
    /DEBUG;
    /SUBSYSTEM:CONSOLE;
    /OPT:REF;
    /OPT:ICF
    )
endif()


install(TARGETS ${PROJECT_NAME} 
RUNTIME DESTINATION bin
LIBRARY DESTINATION lib
       )

