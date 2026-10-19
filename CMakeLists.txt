cmake_minimum_required(VERSION 3.21)

project(xyz-profiler
    VERSION 0.3.0
    DESCRIPTION "DVD / Blu-ray collection manager (XYZ-Profiler)"
    HOMEPAGE_URL "https://github.com/theinvisible/xyz-profiler"
    LANGUAGES CXX
)

set(PROJECT_LICENSE "GPL-3.0-or-later")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 QUIET COMPONENTS
    Core Concurrent Gui Network Widgets Sql Svg Test LinguistTools)

if(NOT Qt6_FOUND)
    set(_qt_search_roots "H:/Qt" "C:/Qt")
    set(_qt_found_dir "")
    foreach(_qt_root IN LISTS _qt_search_roots)
        file(GLOB _qt_versions LIST_DIRECTORIES true "${_qt_root}/6.*")
        # NATURAL, or "6.9.x" would sort above "6.12.x".
        list(SORT _qt_versions COMPARE NATURAL ORDER DESCENDING)
        foreach(_qt_ver IN LISTS _qt_versions)
            file(GLOB _qt_kits LIST_DIRECTORIES true
                 "${_qt_ver}/msvc*_64" "${_qt_ver}/mingw*_64")
            foreach(_qt_kit IN LISTS _qt_kits)
                if(EXISTS "${_qt_kit}/lib/cmake/Qt6/Qt6Config.cmake")
                    set(_qt_found_dir "${_qt_kit}")
                    break()
                endif()
            endforeach()
            if(_qt_found_dir)
                break()
            endif()
        endforeach()
        if(_qt_found_dir)
            break()
        endif()
    endforeach()
    if(_qt_found_dir)
        set(Qt6_DIR "${_qt_found_dir}/lib/cmake/Qt6" CACHE PATH "Qt6 config dir" FORCE)
        # Drop prefixes whose Qt install has since been removed (e.g. the
        # previous version after an upgrade) so they don't pile up in the cache.
        set(_qt_prefixes "")
        foreach(_p IN LISTS CMAKE_PREFIX_PATH)
            if(EXISTS "${_p}")
                list(APPEND _qt_prefixes "${_p}")
            endif()
        endforeach()
        set(CMAKE_PREFIX_PATH ${_qt_prefixes})
        list(PREPEND CMAKE_PREFIX_PATH "${_qt_found_dir}")
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "" FORCE)
        message(STATUS "Auto-detected Qt6 at ${_qt_found_dir}")
        find_package(Qt6 6.5 REQUIRED COMPONENTS
            Core Concurrent Gui Network Widgets Sql Svg Test LinguistTools)
    else()
        message(FATAL_ERROR
            "Qt6 not found — set Qt6_DIR / CMAKE_PREFIX_PATH or drop a Qt "
            "install under H:/Qt or C:/Qt.")
    endif()
endif()

qt_standard_project_setup()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
