add_executable(xm_core_tests
    main.cpp
    test_geometry.cpp
    test_document.cpp
    dsh_test.hpp
)

target_link_libraries(xm_core_tests PRIVATE xm_core)
target_include_directories(xm_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# The bundled runner is the test executable's main().
add_test(NAME xm_core_tests COMMAND xm_core_tests)
