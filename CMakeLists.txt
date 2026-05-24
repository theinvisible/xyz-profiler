cmake_minimum_required(VERSION 3.21)

project(xyz-profiler
    VERSION 0.1.0
    DESCRIPTION "DVD / Blu-ray collection manager (XYZ-Profiler)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Default Qt installation paths — overridden by -DCMAKE_PREFIX_PATH=... or
# the Qt6_DIR env var if those are set.
if(NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    if(WIN32)
        set(_xyz_qt_candidates
            "H:/Qt/6.11.1/msvc2022_64"
            "C:/Qt/6.11.1/msvc2022_64"
            "C:/Qt/6.6.0/msvc2019_64"
        )
        foreach(_qt_path IN LISTS _xyz_qt_candidates)
            if(EXISTS "${_qt_path}/lib/cmake/Qt6/Qt6Config.cmake")
                set(CMAKE_PREFIX_PATH "${_qt_path}" CACHE PATH "Qt6 install prefix")
                message(STATUS "xyz-profiler: using Qt at ${_qt_path}")
                break()
            endif()
        endforeach()
        unset(_xyz_qt_candidates)
        unset(_qt_path)
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Concurrent
    Gui
    Qml
    Quick
    QuickControls2
    Sql
    Test
)
# Network/Xml come in with the TMDb client (Roadmap phase 5).

qt_standard_project_setup()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
