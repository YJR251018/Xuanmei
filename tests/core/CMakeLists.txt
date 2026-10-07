add_executable(xm_core_tests
    main.cpp
    test_geometry.cpp
    test_document.cpp
    test_native_analyzer.cpp
    test_dxf_writer.cpp
    test_dxf_reader.cpp
    dsh_test.hpp
)

set_target_properties(xm_core_tests PROPERTIES
    DEBUG_POSTFIX "d"
    RELWITHDEBINFO_POSTFIX "rd"
    MINSIZEREL_POSTFIX "s")

target_link_libraries(xm_core_tests PRIVATE xm_core)
target_include_directories(xm_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# The bundled runner is the test executable's main().
add_test(NAME xm_core_tests COMMAND xm_core_tests)
