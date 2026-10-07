add_executable(xm_app_tests
    main.cpp
    test_workbench_session.cpp
)

set_target_properties(xm_app_tests PROPERTIES
    DEBUG_POSTFIX "d"
    RELWITHDEBINFO_POSTFIX "rd"
    MINSIZEREL_POSTFIX "s")

target_link_libraries(xm_app_tests PRIVATE xm_app_support)
target_include_directories(xm_app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../core")

add_test(NAME xm_app_tests COMMAND xm_app_tests)
