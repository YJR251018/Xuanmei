# Core geometry layer: C++20 only, no Qt, no third-party types (ADR-001).
add_library(xm_core STATIC
    geometry.hpp
    entity.hpp
    entity.cpp
)

target_include_directories(xm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(MSVC)
    target_compile_options(xm_core PRIVATE /WX)
endif()
