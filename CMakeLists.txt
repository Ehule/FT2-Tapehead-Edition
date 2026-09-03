cmake_minimum_required(VERSION 3.7)

project(Tapehead)

find_package(SDL2 REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/release/other/")

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
    add_compile_options(-O3)
    add_compile_options(-ffast-math)
endif()

file(GLOB TAPEHEAD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rtmidi/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfxdata/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mixer/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scopes/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/modloaders/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/smploaders/*.c"
)

add_executable(Tapehead ${TAPEHEAD_SOURCES})

if(WIN32)
    target_sources(Tapehead PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ft2-clone.rc")
endif()

set(TAPEHEAD_RUNTIME_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/release/other/FT2.CFG"
    "${CMAKE_CURRENT_SOURCE_DIR}/release/other/tapehead.ini"
    "${CMAKE_CURRENT_SOURCE_DIR}/release/other/palette.pal"
    "${CMAKE_CURRENT_SOURCE_DIR}/release/other/fastTracksLogoBadges.bmp"
    "${CMAKE_CURRENT_SOURCE_DIR}/release/other/tapeheadSplash.png"
)

set(TAPEHEAD_OPTIONAL_APP_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfxdata/icon/tapehead/tapehead-icon.bmp")

foreach(asset IN LISTS TAPEHEAD_RUNTIME_ASSETS)
    if(NOT EXISTS "${asset}")
        message(FATAL_ERROR "Missing required Tapehead runtime asset: ${asset}")
    endif()
endforeach()

add_custom_command(TARGET Tapehead POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TAPEHEAD_RUNTIME_ASSETS}
        "$<TARGET_FILE_DIR:Tapehead>"
    COMMENT "Copying Tapehead runtime assets"
)

if((WIN32 OR (UNIX AND NOT APPLE)) AND EXISTS "${TAPEHEAD_OPTIONAL_APP_ICON}")
    add_custom_command(TARGET Tapehead POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TAPEHEAD_OPTIONAL_APP_ICON}"
            "$<TARGET_FILE_DIR:Tapehead>"
        COMMENT "Copying optional Tapehead application icon"
    )
endif()

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TAPEHEAD_SDL_RUNTIME
            "${CMAKE_CURRENT_SOURCE_DIR}/release/win64/SDL2.dll")
    else()
        set(TAPEHEAD_SDL_RUNTIME
            "${CMAKE_CURRENT_SOURCE_DIR}/release/win32/SDL2.dll")
    endif()

    if(EXISTS "${TAPEHEAD_SDL_RUNTIME}")
        add_custom_command(TARGET Tapehead POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TAPEHEAD_SDL_RUNTIME}"
                "$<TARGET_FILE_DIR:Tapehead>"
            COMMENT "Copying SDL2 runtime for portable Windows use")
    endif()
endif()

target_include_directories(Tapehead SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS})

if("${SDL2_LIBRARIES}" STREQUAL "")
    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

find_package(Threads REQUIRED)

target_link_libraries(Tapehead
    PRIVATE m Threads::Threads ${SDL2_LIBRARIES})

target_compile_definitions(Tapehead
    PRIVATE HAS_MIDI)

if(UNIX)
    if(APPLE)
        find_library(COREAUDIO CoreAudio REQUIRED)
        find_library(COREFOUNDATION CoreFoundation REQUIRED)
        find_library(COREMIDI CoreMIDI REQUIRED)
        find_library(ICONV iconv REQUIRED)
        target_link_libraries(Tapehead
            PRIVATE ${COREAUDIO} ${COREMIDI} ${COREFOUNDATION} ${ICONV})
        target_compile_definitions(Tapehead
            PRIVATE __MACOSX_CORE__)
    else()
        # musl systems need musl-fts installed, others have it with glibc
        find_library(FTS fts)
        if (FTS)
            target_link_libraries(Tapehead PRIVATE fts)
        endif()
        target_link_libraries(Tapehead
            PRIVATE asound dl)
        target_compile_definitions(Tapehead
            PRIVATE __LINUX_ALSA__)
    endif()
endif()

install(TARGETS Tapehead
    RUNTIME DESTINATION bin)

install(FILES ${TAPEHEAD_RUNTIME_ASSETS}
    DESTINATION bin)

if((WIN32 OR (UNIX AND NOT APPLE)) AND EXISTS "${TAPEHEAD_OPTIONAL_APP_ICON}")
    install(FILES "${TAPEHEAD_OPTIONAL_APP_ICON}"
        DESTINATION bin)
endif()

if(UNIX AND NOT APPLE)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/release/other/tapehead.desktop"
        DESTINATION share/applications
        RENAME tapehead.desktop)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gfxdata/icon/tapehead/png/tapehead-512.png")
        install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gfxdata/icon/tapehead/png/tapehead-512.png"
            DESTINATION share/icons/hicolor/512x512/apps
            RENAME tapehead.png)
    endif()
endif()
