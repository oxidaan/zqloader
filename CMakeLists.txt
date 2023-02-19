cmake_minimum_required(VERSION 3.7)



project(zqloader VERSION 0.1.0 LANGUAGES CXX)



file(GLOB INCLUDE
    "zqloader/*.h"
    "*.h"
    )


set(SOURCE
    zqloader.cpp
)



add_executable(${PROJECT_NAME}  ${SOURCE} ${INCLUDE} ${DOCUMENTATION} )
target_include_directories(${PROJECT_NAME} PRIVATE miniaudio)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "NDEBUG;"
    "_CONSOLE;"
    "UNICODE;"
    "_UNICODE"
)

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

if(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall 
        -pedantic
        -Werror
        -Wno-error=deprecated-declarations
        -Wno-sign-compare
    )
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)





if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()
if(MSVC)
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


