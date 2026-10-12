cmake_minimum_required(VERSION 3.16)
project(DnDHpTracker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

set(LOGIC_SOURCES creature.cpp encounter.cpp difficulty.cpp)

# Console demo
add_executable(console_demo main.cpp ${LOGIC_SOURCES})

# Rules tests (no UI): ./build/logic_tests
add_executable(logic_tests logic_tests.cpp ${LOGIC_SOURCES})

# ImGui GUI
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
find_package(Threads REQUIRED)
add_executable(hp_tracker_gui
    gui_main.cpp
    player_server.cpp
    ${CMAKE_SOURCE_DIR}/third_party/qrcodegen/qrcodegen.cpp
    ${LOGIC_SOURCES}
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(hp_tracker_gui PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/third_party/httplib ${CMAKE_SOURCE_DIR}/third_party/qrcodegen)
# Player screens: a small web server (cpp-httplib) on a background thread.
target_link_libraries(hp_tracker_gui PRIVATE Threads::Threads)
if(WIN32)
    # SDL2main provides WinMain; WIN32_EXECUTABLE means no console window.
    target_link_libraries(hp_tracker_gui PRIVATE SDL2::SDL2main SDL2::SDL2 ws2_32)
    # cpp-httplib needs the Windows 10 API.
    target_compile_definitions(hp_tracker_gui PRIVATE _WIN32_WINNT=0x0A00)
    set_target_properties(hp_tracker_gui PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME "DnD HP Tracker")
    # The app icon (windows/icon/hptracker.ico) is compiled in as a resource.
    enable_language(RC)
    set(APP_ICON ${CMAKE_SOURCE_DIR}/windows/icon/hptracker.ico)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/windows/app.rc PROPERTIES OBJECT_DEPENDS ${APP_ICON})
    target_sources(hp_tracker_gui PRIVATE ${CMAKE_SOURCE_DIR}/windows/app.rc)
    target_include_directories(hp_tracker_gui PRIVATE ${CMAKE_SOURCE_DIR}/windows)
    # Put SDL2.dll beside the built .exe so it runs straight from the build folder.
    add_custom_command(TARGET hp_tracker_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:hp_tracker_gui>)
else()
    target_link_libraries(hp_tracker_gui PRIVATE SDL2::SDL2)
endif()

# MinGW: bake the C++ runtime into the .exe so only SDL2.dll ships beside it.
if(MINGW)
    foreach(t console_demo logic_tests hp_tracker_gui)
        target_link_options(${t} PRIVATE -static-libgcc -static-libstdc++)
        target_link_libraries(${t} PRIVATE -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
    endforeach()
endif()

# ---- macOS app bundle ------------------------------------------------------
# `cmake --install build --prefix dist` produces dist/"DnD HP Tracker.app".
# Run ./package_mac.sh to also copy SDL inside it and make a shareable zip.
if(APPLE)
    set_target_properties(hp_tracker_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "DnD HP Tracker"
        MACOSX_BUNDLE_BUNDLE_NAME "DnD HP Tracker"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hotelworkshop.hptracker"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_STRING "DnD HP Tracker"
        MACOSX_BUNDLE_COPYRIGHT ""
        MACOSX_BUNDLE_ICON_FILE "hptracker.icns"
    )
    # The app icon (macos/icon/hptracker.icns) goes into the bundle's Resources.
    set(APP_ICNS ${CMAKE_SOURCE_DIR}/macos/icon/hptracker.icns)
    set_source_files_properties(${APP_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(hp_tracker_gui PRIVATE ${APP_ICNS})
    # High-DPI rendering on Retina.
    set(MACOSX_BUNDLE_HIGH_RESOLUTION_CAPABLE TRUE)

    install(TARGETS hp_tracker_gui BUNDLE DESTINATION .)
endif()
