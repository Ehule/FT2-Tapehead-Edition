cmake_minimum_required(VERSION 3.16)

project(tapesister_core VERSION 0.1.0 LANGUAGES C)

option(TAPESISTER_BUILD_SDL "Build the SDL2 TapeSister audition application" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(tapesister_core STATIC
    src/ft2_shared_ui.c
    src/ts_render.c
    src/ts_recipes.c
    src/ts_io.c
    src/ts_audition.c
    src/ts_ui.c
    src/ts_app.c
    src/ts_presentation.c
    src/ts_editor.c
    src/ts_preview.c
    src/ts_text_edit.c
    src/ts_file_browser.c
)
target_include_directories(tapesister_core PUBLIC include)
target_compile_definitions(tapesister_core PUBLIC
    TS_FACTORY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
if(NOT MSVC)
    find_package(Threads REQUIRED)
    target_link_libraries(tapesister_core PUBLIC m Threads::Threads)
endif()

if(MSVC)
    target_compile_options(tapesister_core PRIVATE /W4 /fp:strict)
else()
    target_compile_options(tapesister_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        -ffp-contract=off
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(tapesister_core_tests tests/test_core.c)
    target_link_libraries(tapesister_core_tests PRIVATE tapesister_core)
    target_compile_definitions(tapesister_core_tests PRIVATE
        TS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME tapesister_core_tests COMMAND tapesister_core_tests)

    add_executable(tapesister_io_tests tests/test_io.c)
    target_link_libraries(tapesister_io_tests PRIVATE tapesister_core)
    target_compile_definitions(tapesister_io_tests PRIVATE
        TS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME tapesister_io_tests COMMAND tapesister_io_tests)

    add_executable(tapesister_audition_tests tests/test_audition.c)
    target_link_libraries(tapesister_audition_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_audition_tests COMMAND tapesister_audition_tests)

    add_executable(tapesister_ui_tests tests/test_ui.c)
    target_link_libraries(tapesister_ui_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_ui_tests COMMAND tapesister_ui_tests)

    add_executable(tapesister_editor_tests tests/test_editor.c)
    target_link_libraries(tapesister_editor_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_editor_tests COMMAND tapesister_editor_tests)

    add_executable(tapesister_preview_tests tests/test_preview.c)
    target_link_libraries(tapesister_preview_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_preview_tests COMMAND tapesister_preview_tests)

    add_executable(tapesister_text_edit_tests tests/test_text_edit.c)
    target_link_libraries(tapesister_text_edit_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_text_edit_tests COMMAND tapesister_text_edit_tests)

    add_executable(tapesister_session_tests tests/test_session.c)
    target_link_libraries(tapesister_session_tests PRIVATE tapesister_core)
    target_compile_definitions(tapesister_session_tests PRIVATE
        TS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME tapesister_session_tests COMMAND tapesister_session_tests)

    add_executable(tapesister_rejected_edit_tests tests/test_rejected_edit.c)
    target_link_libraries(tapesister_rejected_edit_tests PRIVATE tapesister_core)
    target_compile_definitions(tapesister_rejected_edit_tests PRIVATE
        TS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME tapesister_rejected_edit_tests COMMAND tapesister_rejected_edit_tests)

    add_executable(tapesister_file_browser_tests tests/test_file_browser.c)
    target_link_libraries(tapesister_file_browser_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_file_browser_tests COMMAND tapesister_file_browser_tests)

    add_executable(tapesister_shared_ui_tests tests/test_shared_ui.c)
    target_link_libraries(tapesister_shared_ui_tests PRIVATE tapesister_core)
    add_test(NAME tapesister_shared_ui_tests COMMAND tapesister_shared_ui_tests)
endif()

if(TAPESISTER_BUILD_SDL)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_executable(tapesister src/ts_sdl_main.c)
        target_link_libraries(tapesister PRIVATE tapesister_core)
        if(TARGET SDL2::SDL2main)
            target_link_libraries(tapesister PRIVATE SDL2::SDL2main)
        endif()
        if(TARGET SDL2::SDL2)
            target_link_libraries(tapesister PRIVATE SDL2::SDL2)
        else()
            target_include_directories(tapesister SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(tapesister PRIVATE ${SDL2_LIBRARIES})
        endif()
        add_custom_command(TARGET tapesister POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                "$<TARGET_FILE_DIR:tapesister>/resources/recipes")
        if(BUILD_TESTING)
            add_test(NAME tapesister_sdl_smoke COMMAND tapesister --smoke-test)
            set_tests_properties(tapesister_sdl_smoke PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
        endif()
    else()
        message(WARNING "SDL2 not found: core/UI/mixer tests build, but the tapesister executable is disabled")
    endif()
endif()
