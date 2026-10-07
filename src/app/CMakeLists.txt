find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

add_library(xm_app_support STATIC
    workbench_session.hpp
    workbench_session.cpp
)

target_include_directories(xm_app_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(xm_app_support PUBLIC xm_core)

add_executable(cdr2dxf_workbench
    main.cpp
    main_window.hpp
    main_window.cpp
    preview_view.hpp
    preview_view.cpp
)

set_target_properties(cdr2dxf_workbench PROPERTIES
    DEBUG_POSTFIX "d"
    RELWITHDEBINFO_POSTFIX "rd"
    MINSIZEREL_POSTFIX "s")

target_link_libraries(cdr2dxf_workbench PRIVATE xm_app_support Qt6::Widgets)

# This toolchain already supplies /source-charset:utf-8. Qt's default -utf-8
# adds the same source setting plus an execution setting, and recent MSVC
# rejects the pair as D8016. Keep the existing project-wide source setting.
set_property(TARGET cdr2dxf_workbench PROPERTY QT_NO_UTF8_SOURCE ON)

if(WIN32)
    add_custom_command(TARGET cdr2dxf_workbench POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
            "$<TARGET_FILE_DIR:cdr2dxf_workbench>/platforms"
        COMMENT "Copying the minimal Qt runtime beside cdr2dxf_workbench")
endif()

add_test(NAME xm_app_selftest COMMAND cdr2dxf_workbench --selftest)
add_test(NAME xm_app_ui_smoketest COMMAND cdr2dxf_workbench --ui-smoketest)
