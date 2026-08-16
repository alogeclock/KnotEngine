cmake_minimum_required(VERSION 3.25)

set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Shipping" CACHE STRING "KnotEngine build configurations" FORCE)

project(KnotEngine VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "Compiler flags for Development builds" FORCE)
set(CMAKE_CXX_FLAGS_SHIPPING "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Compiler flags for Shipping builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}" CACHE STRING "Linker flags for Development builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "Linker flags for Shipping builds" FORCE)

set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT RelWithDebInfo Release)
set(CMAKE_MAP_IMPORTED_CONFIG_SHIPPING Release)

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

set(KNOTENGINE_RESOURCE_FILES
    "${KNOTENGINE_ROOT}/KnotEngine.rc"
)

file(GLOB_RECURSE KNOTENGINE_CONTENT_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Contents/*"
)

file(GLOB_RECURSE KNOTENGINE_SETTING_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Settings/*"
)

set_source_files_properties(${KNOTENGINE_SHADER_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    VS_TOOL_OVERRIDE "None"
)

set_source_files_properties(${KNOTENGINE_CONTENT_FILES} ${KNOTENGINE_SETTING_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
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
    ${KNOTENGINE_CONTENT_FILES}
    ${KNOTENGINE_RESOURCE_FILES}
    ${KNOTENGINE_SETTING_FILES}
    ${KNOTENGINE_SHADER_FILES}
)

target_compile_features(KnotEngine PRIVATE cxx_std_20)

target_precompile_headers(KnotEngine PRIVATE
    "${KNOTENGINE_ROOT}/pch.h"
)

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
    $<$<CONFIG:Debug>:KNOT_BUILD_DEBUG=1>
    $<$<CONFIG:Development>:KNOT_BUILD_DEVELOPMENT=1>
    $<$<CONFIG:Shipping>:KNOT_BUILD_SHIPPING=1>
)

if(MSVC)
    target_compile_options(KnotEngine PRIVATE
        /utf-8
        /MP
        /EHa
    )
endif()

target_link_libraries(KnotEngine PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    d3dcompiler
    dxgi
)

set_target_properties(KnotEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${KNOTENGINE_BIN_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:KnotEngine>"
)

add_custom_command(TARGET KnotEngine POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${KNOTENGINE_ROOT}/Contents"
        "$<TARGET_FILE_DIR:KnotEngine>/Contents"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${KNOTENGINE_ROOT}/Shaders"
        "$<TARGET_FILE_DIR:KnotEngine>/Shaders"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${KNOTENGINE_ROOT}/Settings"
        "$<TARGET_FILE_DIR:KnotEngine>/Settings"
    COMMENT "Copying KnotEngine runtime files"
    VERBATIM
)

foreach(CONFIG_TYPE Debug Development Shipping)
    string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
    set_target_properties(KnotEngine PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}" "${KNOTENGINE_BIN_DIR}/${CONFIG_TYPE}"
    )
endforeach()
