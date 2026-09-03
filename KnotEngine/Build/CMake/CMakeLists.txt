cmake_minimum_required(VERSION 3.25)

# Visual Studio Output 창에서 vcpkg의 지역화된 메시지가 깨지지 않도록 영어 출력을 사용한다.
set(ENV{VSLANG} "1033")

# Visual Studio에서 사용할 빌드 구성을 세 가지로 제한한다.
set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Shipping" CACHE STRING "KnotEngine build configurations" FORCE)

# KnotEngine 프로젝트를 만들고, C++와 Windows 리소스 컴파일러를 활성화한다.
project(KnotEngine VERSION 0.1.0 LANGUAGES CXX RC)

# 빌드 구성에 따라 컴파일러 및 링커 플래그를 설정한다.
# - Debug 구성은 디버깅 심볼을 포함하고 최적화를 비활성화한다.
# - Development 구성은 디버깅 심볼을 포함하고 일부 최적화를 활성화한다.
# - Shipping 구성은 디버깅 심볼을 제거하고 최적화를 활성화한다.
set(CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "Compiler flags for Development builds" FORCE)
set(CMAKE_CXX_FLAGS_SHIPPING "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Compiler flags for Shipping builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}" CACHE STRING "Linker flags for Development builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "Linker flags for Shipping builds" FORCE)

# Development과 Shipping 구성에서 CMake가 가져온 라이브러리를 올바르게 매핑하도록 설정한다.
set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT RelWithDebInfo Release)
set(CMAKE_MAP_IMPORTED_CONFIG_SHIPPING Release)

# 현재 CMakeLists.txt 위치에서 두 단계 위를 엔진 루트로 잡는다.
get_filename_component(KNOTENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# 제너레이터가 지정한 플랫폼을 우선 사용하고, 플랫폼 정보가 없다면 포인터 크기에 따라 64bit, 32bit를 결정한다.
if(CMAKE_GENERATOR_PLATFORM)
    set(KNOTENGINE_OUTPUT_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(KNOTENGINE_OUTPUT_PLATFORM "x64")
else()
    set(KNOTENGINE_OUTPUT_PLATFORM "Win32")
endif()

# 최종 실행 파일의 상위 출력 폴더를 만든다.
set(KNOTENGINE_BIN_DIR "${KNOTENGINE_ROOT}/Bin/${KNOTENGINE_OUTPUT_PLATFORM}")

# vcpkg 등의 툴체인 파일이 지정됐다면, 경로를 로그에 출력한다.
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Windows가 아닌 플랫폼에서는 프로젝트 생성을 중단한다.
if(NOT WIN32)
    message(FATAL_ERROR "KnotEngine currently targets Windows and DirectX 11.")
endif()

# Config 패키지 모드로 ImGui, nlohmann-json을 찾는다.
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Shaders 폴더 아래의 모든 셰이더 파일을 재귀적으로 수집한다.
file(GLOB_RECURSE KNOTENGINE_SHADER_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Shaders/*.hlsl"
    "${KNOTENGINE_ROOT}/Shaders/*.hlsli"
    "${KNOTENGINE_ROOT}/Shaders/*.fx"
    "${KNOTENGINE_ROOT}/Shaders/*.fxh"
)

# Windows 리소스 파일을 수집한다.
set(KNOTENGINE_RESOURCE_FILES
    "${KNOTENGINE_ROOT}/KnotEngine.rc"
    "${KNOTENGINE_ROOT}/resource.h"
)

# Contents 폴더 아래의 모든 콘텐츠 파일을 재귀적으로 수집한다.
file(GLOB_RECURSE KNOTENGINE_CONTENT_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Contents/*"
)

# Settings 폴더 아래의 모든 설정 파일을 재귀적으로 수집한다.
file(GLOB_RECURSE KNOTENGINE_SETTING_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Settings/*"
)

# 셰이더를 Visual Studio 프로젝트에는 표시하지만 빌드 대상으로 처리하지 않는다.
set_source_files_properties(${KNOTENGINE_SHADER_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    VS_TOOL_OVERRIDE "None"
)

# 콘텐츠와 설정 파일도 컴파일하지 않는다.
set_source_files_properties(${KNOTENGINE_CONTENT_FILES} ${KNOTENGINE_SETTING_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
)
# Source 아래의 C/C++ 구현 파일을 모두 수집한다.
file(GLOB_RECURSE KNOTENGINE_SOURCE_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Source/*.c"
    "${KNOTENGINE_ROOT}/Source/*.cc"
    "${KNOTENGINE_ROOT}/Source/*.cpp"
    "${KNOTENGINE_ROOT}/Source/*.cxx"
)

# Source 아래의 헤더와 인라인 구현 파일을 모두 수집한다.
file(GLOB_RECURSE KNOTENGINE_HEADER_FILES CONFIGURE_DEPENDS
    "${KNOTENGINE_ROOT}/Source/*.h"
    "${KNOTENGINE_ROOT}/Source/*.hh"
    "${KNOTENGINE_ROOT}/Source/*.hpp"
    "${KNOTENGINE_ROOT}/Source/*.hxx"
    "${KNOTENGINE_ROOT}/Source/*.inl"
    "${KNOTENGINE_ROOT}/Source/*.ipp"
)

# 수집한 모든 파일로 Windows GUI 실행 파일을 만든다. main.cpp와 pch.cpp, pch.h는 반드시 포함한다.
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

# 솔루션을 열었을 때 KnotEngine 프로젝트가 기본 시작 프로젝트가 되도록 설정한다.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KnotEngine)

# Source 디렉터리 구조를 Visual Studio의 Source 필터로 표현한다.
source_group(TREE "${KNOTENGINE_ROOT}/Source" PREFIX "Source" FILES
    ${KNOTENGINE_SOURCE_FILES}
    ${KNOTENGINE_HEADER_FILES}
)

# Contents, Settings, Shaders 폴더 구조도 Visual Studio의 필터로 표현한다.
source_group(TREE "${KNOTENGINE_ROOT}/Contents" PREFIX "Contents" FILES
    ${KNOTENGINE_CONTENT_FILES}
)

source_group(TREE "${KNOTENGINE_ROOT}/Settings" PREFIX "Settings" FILES
    ${KNOTENGINE_SETTING_FILES}
)

source_group(TREE "${KNOTENGINE_ROOT}/Shaders" PREFIX "Shaders" FILES
    ${KNOTENGINE_SHADER_FILES}
)

# Visual Studio에서 프로젝트를 열었을 때, main.cpp, pch.cpp, pch.h, KnotEngine.rc 파일이 최상위에 표시되도록 설정한다.
source_group("" FILES
    "${KNOTENGINE_ROOT}/main.cpp"
    "${KNOTENGINE_ROOT}/pch.cpp"
    "${KNOTENGINE_ROOT}/pch.h"
    ${KNOTENGINE_RESOURCE_FILES}
)

# Visual Studio가 생성한 CMake 관련 파일들은 프로젝트 필터에서 숨긴다.
source_group("Generated\\CMake" REGULAR_EXPRESSION
    ".*[/\\\\]CMakeFiles[/\\\\].*cmake_pch.*"
)

# C++20 표준을 사용하도록 설정한다.
target_compile_features(KnotEngine PRIVATE cxx_std_20)

# Precompiled Header를 사용하도록 설정한다.
target_precompile_headers(KnotEngine PRIVATE
    "${KNOTENGINE_ROOT}/pch.h"
)

# Include 디렉터리를 설정한다. Source, Core, Engine, Editor 폴더를 포함한다.
target_include_directories(KnotEngine PRIVATE
    "${KNOTENGINE_ROOT}"
    "${KNOTENGINE_ROOT}/Source"
    "${KNOTENGINE_ROOT}/Source/Core"
    "${KNOTENGINE_ROOT}/Source/Engine"
    "${KNOTENGINE_ROOT}/Source/Editor"
)

# 컴파일러 정의를 설정한다. WITH_EDITOR, NOMINMAX, WIN32_LEAN_AND_MEAN, UNICODE, _UNICODE를 정의하고, 
# 빌드 구성에 따라 KNOT_BUILD_DEBUG, KNOT_BUILD_DEVELOPMENT, KNOT_BUILD_SHIPPING를 정의한다.
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

# Visual Studio에서 UTF-8 인코딩, 병렬 빌드, 예외 처리, 전처리기 동작을 설정한다.
if(MSVC)
    target_compile_options(KnotEngine PRIVATE
        /utf-8
        /MP
        /EHa
        /Zc:preprocessor
    )
endif()

# 링커 라이브러리를 설정한다. ImGui, nlohmann-json, Direct3D 11, D3DCompiler, DXGI를 링크한다.
target_link_libraries(KnotEngine PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    d3dcompiler
    dxgi
)

# Visual Studio에서 실행 파일의 출력 디렉터리와 디버거 작업 디렉터리를 설정하고, Shipping 구성에서 인터프로시저 최적화를 활성화한다.
set_target_properties(KnotEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${KNOTENGINE_BIN_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:KnotEngine>"
    INTERPROCEDURAL_OPTIMIZATION_SHIPPING TRUE
)

# 빌드 후 명령어를 추가하여 Contents, Shaders, Settings 폴더를 실행 파일과 동일한 위치로 복사한다.
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

# Visual Studio에서 각 빌드 구성에 따라 실행 파일의 출력 디렉터리를 설정한다.
foreach(CONFIG_TYPE Debug Development Shipping)
    string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
    set_target_properties(KnotEngine PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER}" "${KNOTENGINE_BIN_DIR}/${CONFIG_TYPE}"
    )
endforeach()
