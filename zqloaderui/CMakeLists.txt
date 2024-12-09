cmake_minimum_required(VERSION 3.16)

project(zqloaderui VERSION 0.1 LANGUAGES CXX)



#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5      REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)


if (NOT Qt${QT_VERSION_MAJOR}_FOUND)
    message("QT${QT_VERSION_MAJOR} not found! Skip building qt/User interface project")
    return()
else()
    message("QT${QT_VERSION_MAJOR} found at: ${Qt${QT_VERSION_MAJOR}_DIR}")
endif()

set(PROJECT_SOURCES
        main.cpp
        dialog.cpp
        dial.h
        dialog.h
        dialog.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(zqloaderui
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET zqloaderlib APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(zqloaderui SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(zqloaderui
            ${PROJECT_SOURCES}
        )
    endif()
endif()
#target_include_directories(zqloaderlib PRIVATE .)
target_link_libraries(zqloaderui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets zqloaderlib)
target_include_directories(zqloaderui PRIVATE . ..)


# We need c++17
target_compile_features(zqloaderui PUBLIC cxx_std_17)
set_target_properties(zqloaderui PROPERTIES AUTOMOC TRUE AUTORCC TRUE AUTOUIC TRUE)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.zqloaderui)
endif()
set_target_properties(zqloaderui PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS zqloaderui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(zqloaderui)
endif()
