cmake_minimum_required(VERSION 3.25)

project(KnotEngine VERSION 0.1.0 LANGUAGES CXX)

get_filename_component(KNOTENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(CMAKE_GENERATOR_PLATFORM)
    set(KNOTENGINE_OUTPUT_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(KNOTENGINE_OUTPUT_PLATFORM "x64")
else()
    set(KNOTENGINE_OUTPUT_PLATFORM "Win32")
endif()
set(KNOTENGINE_BIN_DIR "${KNOTENGINE_ROOT}/Bin/${KNOTENGINE_OUTPUT_PLATFORM}")

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT WIN32)
    message(FATAL_ERROR "KnotEngine currently targets Windows and DirectX 11.")
endif()

find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(KNOTENGINE_SHADER_FILES
    "${KNOTENGINE_ROOT}/Shaders/Common.hlsl"
)

set_source_files_properties(${KNOTENGINE_SHADER_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    VS_TOOL_OVERRIDE "None"
)

file(GLOB_RECURSE KNOTENGINE_SOURCE_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Source/*.cpp"
)

file(GLOB_RECURSE KNOTENGINE_HEADER_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Source/*.h"
)

add_executable(KnotEngine WIN32
    "${KNOTENGINE_ROOT}/main.cpp"
    "${KNOTENGINE_ROOT}/pch.cpp"
    "${KNOTENGINE_ROOT}/pch.h"
    ${KNOTENGINE_SOURCE_FILES}
    ${KNOTENGINE_HEADER_FILES}
    ${KNOTENGINE_SHADER_FILES}
)

target_compile_features(KnotEngine PRIVATE cxx_std_20)

target_include_directories(KnotEngine PRIVATE
    "${KNOTENGINE_ROOT}"
    "${KNOTENGINE_ROOT}/Source"
    "${KNOTENGINE_ROOT}/Source/Core"
    "${KNOTENGINE_ROOT}/Source/Engine"
    "${KNOTENGINE_ROOT}/Source/Editor"
)

target_compile_definitions(KnotEngine PRIVATE
    WITH_EDITOR=1
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(KnotEngine PRIVATE /utf-8 /MP /EHa)
endif()

target_precompile_headers(KnotEngine PRIVATE
    "${KNOTENGINE_ROOT}/pch.h"
)

target_link_libraries(KnotEngine PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    d3dcompiler
    dxgi
)

set_target_properties(KnotEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${KNOTENGINE_BIN_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${KNOTENGINE_ROOT}"
)

foreach(CONFIG_TYPE Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
    set_target_properties(KnotEngine PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}" "${KNOTENGINE_BIN_DIR}/${CONFIG_TYPE}"
    )
endforeach()
