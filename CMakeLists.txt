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
add_executable(hp_tracker_gui
    gui_main.cpp
    ${LOGIC_SOURCES}
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(hp_tracker_gui PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(hp_tracker_gui PRIVATE SDL2::SDL2)

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
    )
    # High-DPI rendering on Retina.
    set(MACOSX_BUNDLE_HIGH_RESOLUTION_CAPABLE TRUE)

    install(TARGETS hp_tracker_gui BUNDLE DESTINATION .)
endif()
