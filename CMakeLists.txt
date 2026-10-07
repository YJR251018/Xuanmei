# The project itself needs no CMake newer than 3.21. The 3.25 requirement lives
# in CMakePresets.json (cmakeMinimumRequired) and applies only when this project
# is configured through a preset, because that is the only time CMake parses the
# presets file: the file declares presets schema version 6, which CMake
# understands from 3.25 onwards. Keeping the two versions independent is what
# lets CMake 3.21-3.24 still configure the project directly with
# `cmake -S . -B build`; raising this to 3.25 would reject those configurations
# for a limitation that only the presets file has.
cmake_minimum_required(VERSION 3.21)

project(CDR2DXFWorkbench
    VERSION 0.1.0
    DESCRIPTION "CDR to DXF geometry conversion and analysis workbench"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Warnings are errors in this project: the geometry core must not accumulate
# silent numeric or conversion defects.
if(MSVC)
    # Note: CMake already adds /source-charset:utf-8 on MSVC (CMAKE_CXX_FLAGS /
    # CMAKE_MSVC_RUNTIME_LIBRARY handling), so adding /utf-8 here would collide
    # with it (reporting D8016: incompatible command line options).
    add_compile_options(/W4 /permissive- /EHsc /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Keep executables and libraries in predictable locations so the acceptance
# commands in docs/MVP_DESIGN.md stay stable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config generators (Visual Studio) append a per-configuration
# subdirectory; force a flat layout instead.
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()

# The flat multi-config output layout keeps the documented Release paths
# stable, but different configurations must not overwrite the same file:
# otherwise a Debug build can leave a debug-linked .exe that a subsequent
# incremental Release build incorrectly considers up to date.
set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELWITHDEBINFO_POSTFIX "rd")
set(CMAKE_MINSIZEREL_POSTFIX "s")

enable_testing()

# The Qt 6 Widgets application layer is added slice by slice; it stays off until
# src/app exists so that the core layer can be configured and tested on its own.
option(XM_BUILD_APP "Build the Qt 6 Widgets application" OFF)

add_subdirectory(src/core)
add_subdirectory(tests/core)

if(XM_BUILD_APP)
    add_subdirectory(src/app)
    add_subdirectory(tests/app)
endif()
