cmake_minimum_required(VERSION 3.21)

project(FlowWheelNative VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(flowwheel_native_settings STATIC
    src/Settings.cpp
)

add_library(flowwheel_native_core STATIC
    src/MouseHook.cpp
    src/ScrollEngine.cpp
)

target_include_directories(flowwheel_native_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(flowwheel_native_core
    PUBLIC
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

if (MSVC)
    target_compile_options(flowwheel_native_core PRIVATE /W4 /permissive-)
endif()

target_link_libraries(flowwheel_native_core PRIVATE
    User32
)

target_include_directories(flowwheel_native_settings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(flowwheel_native_settings
    PUBLIC
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

if (MSVC)
    target_compile_options(flowwheel_native_settings PRIVATE /W4 /permissive-)
endif()

target_link_libraries(flowwheel_native_settings PRIVATE
    Advapi32
    Shell32
)

add_executable(FlowWheelNative WIN32
    FlowWheel.rc
    app.manifest
    src/main.cpp
    src/TrayApp.cpp
    src/SettingsWindow.cpp
)

set_target_properties(FlowWheelNative PROPERTIES OUTPUT_NAME FlowWheel)

target_include_directories(FlowWheelNative PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(FlowWheelNative PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if (MSVC)
    target_compile_options(FlowWheelNative PRIVATE /W4 /permissive-)
endif()

target_link_libraries(FlowWheelNative PRIVATE
    flowwheel_native_core
    flowwheel_native_settings
    User32
    Gdi32
    Shell32
    Advapi32
    Comctl32
)
